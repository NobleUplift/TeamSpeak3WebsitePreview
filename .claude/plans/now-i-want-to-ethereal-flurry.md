# Plan: Open Graph Protocol support + description + img tag

## Context

The plugin currently fetches `/html/head/title` via XPath and formats the message as `"<title>" <[URL]...[/URL]>`. The goal is to:
1. Prefer Open Graph Protocol metadata (`og:title`) over `<title>`, with fallback
2. Include `og:description` alongside the title in the formatted message
3. Experiment with sending an HTML `<img>` tag using `og:image` (TS3 3.6.x renders chat via Qt's HTML-aware widget, so this may render as an actual image)

There is also an existing memory leak: `xmlXPathNewContext` is called but the context is never freed with `xmlXPathFreeContext`. This will be fixed as part of the changes.

---

## Files to Modify

- `ts3websitepreview/plugin.c` — all core changes
- `ts3websitepreviewtest/main.c` — mirror the same OG extraction logic for manual testing

---

## Step 1 — Add `pfn_xmlXPathFreeContext` function pointer (`plugin.c`)

In the `#ifdef _WIN32` block (~line 43–56), add:

```c
typedef void (*pfnXmlXPathFreeContext_t)(xmlXPathContextPtr);
static pfnXmlXPathFreeContext_t pfn_xmlXPathFreeContext;
```

In `ts3plugin_init()` after the other `GetProcAddress` calls for libxml2 (~line 231):

```c
pfn_xmlXPathFreeContext = (pfnXmlXPathFreeContext_t)GetProcAddress(hLibxml2, "xmlXPathFreeContext");
```

---

## Step 2 — Add `GetOGProperty()` helper (`plugin.c`, after `GetHTML`, before `ts3plugin_onTextMessageEvent`)

This function runs an XPath query for a given OG meta property and returns the `content` attribute value. Callers must free the returned string with `pfn_xmlFree`.

```c
static char* GetOGProperty(htmlDocPtr doc, xmlXPathContextPtr context, const char* property) {
    char xpath[128];
    xmlXPathObjectPtr result;
    char* value = NULL;

    snprintf(xpath, sizeof(xpath), "//meta[@property='%s']/@content", property);
    result = pfn_xmlXPathEvalExpression((const xmlChar*)xpath, context);
    if (result && !xmlXPathNodeSetIsEmpty(result->nodesetval)) {
        xmlNodePtr node = result->nodesetval->nodeTab[0];
        /* Attribute nodes: node->children is the text child holding the value */
        value = (char*)pfn_xmlNodeListGetString(doc, node->children, 1);
    }
    if (result) pfn_xmlXPathFreeObject(result);
    return value;
}
```

Key: XPath `//meta[@property='og:title']/@content` returns the attribute node. `node->children` on an attribute node is its text child — `xmlNodeListGetString` on that returns the string value. No new function pointer needed.

---

## Step 3 — Rewrite the HTML-parsing block in `ts3plugin_onTextMessageEvent()` (`plugin.c`)

Replace the current variables and XPath logic (~lines 496–572) with:

### Variable declarations (replace existing `keyword`, `result`, `context` etc.):
```c
char* og_title = NULL;
char* og_desc  = NULL;
char* og_image = NULL;
char* html_title = NULL;
const char* title = NULL;
char newMessage[4096];  /* was 1024 — increased to fit title + desc + img */
```

### After `pfn_htmlReadMemory` succeeds, extract OG properties then fall back:
```c
context = pfn_xmlXPathNewContext(doc);

og_title = GetOGProperty(doc, context, "og:title");
og_desc  = GetOGProperty(doc, context, "og:description");
og_image = GetOGProperty(doc, context, "og:image");

/* Fallback: HTML <title> element */
if (!og_title) {
    result = pfn_xmlXPathEvalExpression("/html/head/title", context);
    if (!xmlXPathNodeSetIsEmpty(result->nodesetval)) {
        xmlNodePtr titleNode = result->nodesetval->nodeTab[result->nodesetval->nodeNr - 1];
        html_title = (char*)pfn_xmlNodeListGetString(doc, titleNode->xmlChildrenNode, 1);
    }
    pfn_xmlXPathFreeObject(result);
}

pfn_xmlXPathFreeContext(context);  /* fixes existing leak */

title = og_title ? og_title : (html_title ? html_title : "(untitled)");
```

### Message formatting (Windows path):
```c
snprintf(newMessage, sizeof(newMessage),
    "\"%s\" <[URL]%s[/URL]>%s%s%s%s",
    title,
    url,
    og_desc  ? "\n"               : "",
    og_desc  ? og_desc            : "",
    og_image ? "\n<img src=\""    : "",
    og_image ? og_image           : "");
/* Close the img tag if og_image is present */
if (og_image) {
    strncat_s(newMessage, sizeof(newMessage), "\">", _TRUNCATE);
}
```

**Resulting message format (when all OG tags are present):**
```
"Page Title" <[URL]https://example.com[/URL]>
Short description of the page content
<img src="https://cdn.example.com/thumbnail.jpg">
```

### Cleanup (before `requestSendChannelTextMsg`):
```c
if (pfn_xmlFree) {
    if (og_title)   pfn_xmlFree(og_title);
    if (og_desc)    pfn_xmlFree(og_desc);
    if (og_image)   pfn_xmlFree(og_image);
    if (html_title) pfn_xmlFree(html_title);
}
pfn_xmlFreeDoc(doc);
free(chunk.memory);
```

---

## Step 4 — Mirror changes in `ts3websitepreviewtest/main.c`

The test app calls libxml2 directly (not via function pointers). Add an equivalent `GetOGProperty` that calls `xmlXPathEvalExpression`, `xmlXPathFreeObject`, `xmlNodeListGetString` directly. Update `_tmain` to:
- Call `GetOGProperty(doc, context, "og:title")` etc.
- Fall back to `<title>` if `og:title` is NULL
- Format and print the message in the same 4-field format
- Call `xmlXPathFreeContext(context)` to fix the same leak
- Free all extracted strings with `xmlFree`

---

## The `<img>` Experiment

TeamSpeak 3.6.x uses Qt 5.15.2. The chat window widget (`QTextEdit`/`QTextBrowser`) renders a subset of HTML, which **does** include `<img src="...">` tags — it will attempt to load the image from the URL. The experiment is to send the raw HTML tag and observe whether TS3 renders it as an inline image or displays it as plain text.

If TS3 strips `<img>` tags, the message simply shows the extra text harmlessly. No crash risk.

---

## Verification

1. Build with `build_plugin.bat` (both Win32 and x64)
2. Install the plugin in TS3 3.6.2
3. Send `[URL]https://www.youtube.com/watch?v=dQw4w9WgXcQ[/URL]` in a channel
4. Expected output: title from `og:title`, description from `og:description`, and an img tag using the YouTube thumbnail URL
5. Optionally build and run `ts3websitepreviewtest` against the same URL to verify OG extraction in isolation before installing
