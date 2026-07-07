#include "webparse.h"

#include <gumbo.h>

namespace {

// First descendant element (depth-first) with the given tag, or nullptr.
const GumboNode* findElement(const GumboNode* node, GumboTag tag) {
    if (node->type != GUMBO_NODE_ELEMENT)
        return nullptr;
    if (node->v.element.tag == tag)
        return node;
    const GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        const GumboNode* found = findElement(static_cast<const GumboNode*>(children->data[i]), tag);
        if (found)
            return found;
    }
    return nullptr;
}

// Depth-first search for a <meta property="<property>" content="…"> value.
QString findMetaProperty(const GumboNode* node, const QString& property) {
    if (node->type != GUMBO_NODE_ELEMENT)
        return QString();
    if (node->v.element.tag == GUMBO_TAG_META) {
        const GumboAttribute* prop =
            gumbo_get_attribute(&node->v.element.attributes, "property");
        if (prop && property.compare(QString::fromUtf8(prop->value), Qt::CaseInsensitive) == 0) {
            const GumboAttribute* content =
                gumbo_get_attribute(&node->v.element.attributes, "content");
            if (content)
                return QString::fromUtf8(content->value).simplified();
        }
    }
    const GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; ++i) {
        const QString v = findMetaProperty(static_cast<const GumboNode*>(children->data[i]), property);
        if (!v.isEmpty())
            return v;
    }
    return QString();
}

// RAII wrapper around a parsed document (Gumbo's C API needs paired create/destroy).
struct Parsed {
    GumboOutput* out;
    explicit Parsed(const QByteArray& html)
        : out(gumbo_parse_with_options(&kGumboDefaultOptions, html.constData(), html.size())) {}
    ~Parsed() { if (out) gumbo_destroy_output(&kGumboDefaultOptions, out); }
    Parsed(const Parsed&) = delete;
    Parsed& operator=(const Parsed&) = delete;
};

}  // namespace

namespace webparse {

QString extractTitle(const QByteArray& html) {
    Parsed doc(html);
    if (!doc.out)
        return QString();
    const GumboNode* title = findElement(doc.out->root, GUMBO_TAG_TITLE);
    if (title && title->v.element.children.length > 0) {
        const GumboNode* text = static_cast<const GumboNode*>(title->v.element.children.data[0]);
        if (text->type == GUMBO_NODE_TEXT || text->type == GUMBO_NODE_WHITESPACE)
            return QString::fromUtf8(text->v.text.text).simplified();
    }
    return QString();
}

QString extractOgProperty(const QByteArray& html, const QString& property) {
    Parsed doc(html);
    if (!doc.out)
        return QString();
    return findMetaProperty(doc.out->root, property);
}

}  // namespace webparse
