#ifdef _WIN32
#pragma warning (disable : 4100)  /* Disable Unreferenced parameter warning */
#include <windows.h>
#include <tchar.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "teamspeak/public_errors.h"
#include "teamspeak/public_errors_rare.h"
#include "teamspeak/public_definitions.h"
#include "teamspeak/public_rare_definitions.h"
#include "ts3_functions.h"

#include "curl.h"
#include "HTMLparser.h"
#include "globals.h"
#include "xpath.h"

#include "plugin.h"
#include "core.h"
#include "resource.h"
#include "settings.h"
#include "plugin_version.h"

#ifdef _WIN32
static HMODULE hLibcurl = NULL;
static HMODULE hLibxml2 = NULL;
typedef CURLcode (*pfnCurlGlobalInit_t)(long);
typedef CURL* (*pfnCurlEasyInit_t)(void);
typedef CURLcode (*pfnCurlEasySetopt_t)(CURL*, CURLoption, ...);
typedef CURLcode (*pfnCurlEasyPerform_t)(CURL*);
typedef const char* (*pfnCurlEasyStrerror_t)(CURLcode);
typedef void (*pfnCurlEasyCleanup_t)(CURL*);
typedef void (*pfnCurlGlobalCleanup_t)(void);
static pfnCurlGlobalInit_t pfn_curl_global_init;
static pfnCurlEasyInit_t pfn_curl_easy_init;
static pfnCurlEasySetopt_t pfn_curl_easy_setopt;
static pfnCurlEasyPerform_t pfn_curl_easy_perform;
static pfnCurlEasyStrerror_t pfn_curl_easy_strerror;
static pfnCurlEasyCleanup_t pfn_curl_easy_cleanup;
static pfnCurlGlobalCleanup_t pfn_curl_global_cleanup;
typedef htmlDocPtr (*pfnHtmlReadMemory_t)(const char*, int, const char*, const char*, int);
typedef xmlXPathContextPtr (*pfnXmlXPathNewContext_t)(xmlDocPtr);
typedef xmlXPathObjectPtr (*pfnXmlXPathEvalExpression_t)(const xmlChar*, xmlXPathContextPtr);
typedef void (*pfnXmlXPathFreeObject_t)(xmlXPathObjectPtr);
typedef xmlChar* (*pfnXmlNodeListGetString_t)(xmlDocPtr, xmlNodePtr, int);
typedef void (*pfnXmlFree_t)(void*);
typedef void (*pfnXmlFreeDoc_t)(xmlDocPtr);
typedef void (*pfnXmlXPathFreeContext_t)(xmlXPathContextPtr);
static pfnHtmlReadMemory_t pfn_htmlReadMemory;
static pfnXmlXPathNewContext_t pfn_xmlXPathNewContext;
static pfnXmlXPathEvalExpression_t pfn_xmlXPathEvalExpression;
static pfnXmlXPathFreeObject_t pfn_xmlXPathFreeObject;
static pfnXmlNodeListGetString_t pfn_xmlNodeListGetString;
static pfnXmlFree_t pfn_xmlFree;
static pfnXmlFreeDoc_t pfn_xmlFreeDoc;
static pfnXmlXPathFreeContext_t pfn_xmlXPathFreeContext;
#endif

/*#ifdef _WIN32
#pragma comment(lib, "libcurl")
#pragma comment(lib, "iconv")
#pragma comment(lib, "libxml2")
#pragma comment(lib, "zlib1")
#endif*/

static struct TS3Functions ts3Functions;

#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); (dest)[destSize-1] = '\0'; }
#endif

#define PLUGIN_API_VERSION 26

#define PATH_BUFSIZE 512
#define COMMAND_BUFSIZE 128
#define INFODATA_BUFSIZE 128
#define SERVERINFO_BUFSIZE 256
#define CHANNELINFO_BUFSIZE 512
#define RETURNCODE_BUFSIZE 128

static char* pluginID = NULL;
static char g_pluginPath[MAX_PATH] = "";

static int sentSelfMessage = 0;

/* Track the last URL we sent formatted output for, to suppress the server echo
 * of the original [URL]...[/URL] message firing onTextMessageEvent a second time. */
static char lastSentURL[2048] = "";
static DWORD lastSentURLTick = 0;

/* Track the last mixed message (use case 2) to suppress its server echo. */
static char lastSentMessage[2048] = "";
static DWORD lastSentMessageTick = 0;

#ifdef _WIN32
/* Helper function to convert wchar_T to Utf-8 encoded strings on Windows */
static int wcharToUtf8(const wchar_t* str, char** result) {
	int outlen = WideCharToMultiByte(CP_UTF8, 0, str, -1, 0, 0, 0, 0);
	*result = (char*)malloc(outlen);
	if(WideCharToMultiByte(CP_UTF8, 0, str, -1, *result, outlen, 0, 0) == 0) {
		*result = NULL;
		return -1;
	}
	return 0;
}
#endif

/*********************************** Required functions ************************************/
/*
 * If any of these required functions is not implemented, TS3 will refuse to load the plugin
 */

/* Unique name identifying this plugin */
const char* ts3plugin_name() {
#ifdef _WIN32
	/* TeamSpeak expects UTF-8 encoded characters. Following demonstrates a possibility how to convert UTF-16 wchar_t into UTF-8. */
	static char* result = NULL;  /* Static variable so it's allocated only once */
	if(!result) {
		const wchar_t* name = PLUGIN_NAME_W;
		if(wcharToUtf8(name, &result) == -1) {  /* Convert name into UTF-8 encoded result */
			result = PLUGIN_NAME;  /* Conversion failed, fallback here */
		}
	}
	return result;
#else
	return PLUGIN_NAME;
#endif
}

/* Plugin version */
const char* ts3plugin_version() {
    return PLUGIN_VERSION_STR;
}

/* Plugin API version. Must be the same as the clients API major version, else the plugin fails to load. */
int ts3plugin_apiVersion() {
	return PLUGIN_API_VERSION;
}

/* Plugin author */
const char* ts3plugin_author() {
    return PLUGIN_AUTHOR;
}

/* Plugin description */
const char* ts3plugin_description() {
    return PLUGIN_DESCRIPTION;
}

/* Set TeamSpeak 3 callback functions */
void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
    ts3Functions = funcs;
}

/*
 * Custom code called right after loading the plugin. Returns 0 on success, 1 on failure.
 * If the function returns 1 on failure, the plugin will be unloaded again.
 */
int ts3plugin_init() {
    char appPath[PATH_BUFSIZE];
    char resourcesPath[PATH_BUFSIZE];
    char configPath[PATH_BUFSIZE];
	char pluginPath[PATH_BUFSIZE];
	//TCHAR buffer[MAX_PATH];
	//char* path = (char *) malloc(sizeof(char)*100);
    //GetModuleFileName( NULL, buffer, MAX_PATH );
	//wcharToUtf8(buffer, &path);

    /* Your plugin init code here */
    printf("PLUGIN: TS3 Website Preview initializing...\n");

    /* Example on how to query application, resources and configuration paths from client */
    /* Note: Console client returns empty string for app and resources path */
    ts3Functions.getAppPath(appPath, PATH_BUFSIZE);
    ts3Functions.getResourcesPath(resourcesPath, PATH_BUFSIZE);
    ts3Functions.getConfigPath(configPath, PATH_BUFSIZE);
	ts3Functions.getPluginPath(pluginPath, PATH_BUFSIZE, pluginID);
	strncpy_s(g_pluginPath, sizeof(g_pluginPath), pluginPath, _TRUNCATE);
	Settings_Load(g_pluginPath);

	//ts3Functions.logMessage("Current working directory.: ", LogLevel_INFO, "Plugin", 0);
	//ts3Functions.logMessage((const char *) path, LogLevel_INFO, "Plugin", 0);

	//printf("PLUGIN: App path: %s\nResources path: %s\nConfig path: %s\nPlugin path: %s\n", appPath, resourcesPath, configPath, pluginPath);
	ts3Functions.logMessage("App path: ", LogLevel_INFO, "Plugin", 0);
	ts3Functions.logMessage(appPath, LogLevel_INFO, "Plugin", 0);
	ts3Functions.logMessage("Resources path: ", LogLevel_INFO, "Plugin", 0);
	ts3Functions.logMessage(resourcesPath, LogLevel_INFO, "Plugin", 0);
	ts3Functions.logMessage("Config path: ", LogLevel_INFO, "Plugin", 0);
	ts3Functions.logMessage(configPath, LogLevel_INFO, "Plugin", 0);
	ts3Functions.logMessage("Plugin path: ", LogLevel_INFO, "Plugin", 0);
	ts3Functions.logMessage(pluginPath, LogLevel_INFO, "Plugin", 0);

#ifdef _WIN32
	{
		wchar_t dllDir[MAX_PATH];
		wchar_t dllPath[MAX_PATH];
		HMODULE hm = NULL;
		if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		                       (LPCWSTR)(void*)ts3plugin_init, &hm) &&
		    GetModuleFileNameW(hm, dllDir, MAX_PATH)) {
			wchar_t* lastSlash = wcsrchr(dllDir, L'\\');
			if (lastSlash) {
				lastSlash[1] = L'\0';
				wcscat_s(dllDir, MAX_PATH, L"ts3websitepreview\\");
				{
					char narrowDir[MAX_PATH];
					char logMsg[MAX_PATH + 32];
					WideCharToMultiByte(CP_UTF8, 0, dllDir, -1, narrowDir, MAX_PATH, NULL, NULL);
					snprintf(logMsg, sizeof(logMsg), "Loading deps from: %s", narrowDir);
					ts3Functions.logMessage(logMsg, LogLevel_INFO, "Plugin", 0);
				}

				/* Load libcurl.dll — LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR lets its own deps find each other */
				wcscpy_s(dllPath, MAX_PATH, dllDir);
				wcscat_s(dllPath, MAX_PATH, L"libcurl.dll");
				hLibcurl = LoadLibraryExW(dllPath, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
				if (!hLibcurl) {
					char errMsg[128];
					snprintf(errMsg, sizeof(errMsg), "Could not load libcurl.dll (error %lu)", GetLastError());
					ts3Functions.logMessage(errMsg, LogLevel_ERROR, "Plugin", 0);
					return 1;
				}
				pfn_curl_global_init   = (pfnCurlGlobalInit_t)  GetProcAddress(hLibcurl, "curl_global_init");
				pfn_curl_easy_init     = (pfnCurlEasyInit_t)    GetProcAddress(hLibcurl, "curl_easy_init");
				pfn_curl_easy_setopt   = (pfnCurlEasySetopt_t)  GetProcAddress(hLibcurl, "curl_easy_setopt");
				pfn_curl_easy_perform  = (pfnCurlEasyPerform_t) GetProcAddress(hLibcurl, "curl_easy_perform");
				pfn_curl_easy_strerror = (pfnCurlEasyStrerror_t)GetProcAddress(hLibcurl, "curl_easy_strerror");
				pfn_curl_easy_cleanup  = (pfnCurlEasyCleanup_t) GetProcAddress(hLibcurl, "curl_easy_cleanup");
				pfn_curl_global_cleanup= (pfnCurlGlobalCleanup_t)GetProcAddress(hLibcurl, "curl_global_cleanup");

				/* Load libxml2.dll — LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR finds libiconv.dll alongside it */
				wcscpy_s(dllPath, MAX_PATH, dllDir);
				wcscat_s(dllPath, MAX_PATH, L"libxml2.dll");
				hLibxml2 = LoadLibraryExW(dllPath, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
				if (!hLibxml2) {
					char errMsg[128];
					snprintf(errMsg, sizeof(errMsg), "Could not load libxml2.dll (error %lu)", GetLastError());
					ts3Functions.logMessage(errMsg, LogLevel_ERROR, "Plugin", 0);
					FreeLibrary(hLibcurl); hLibcurl = NULL;
					return 1;
				}
				pfn_htmlReadMemory         = (pfnHtmlReadMemory_t)       GetProcAddress(hLibxml2, "htmlReadMemory");
				pfn_xmlXPathNewContext     = (pfnXmlXPathNewContext_t)    GetProcAddress(hLibxml2, "xmlXPathNewContext");
				pfn_xmlXPathEvalExpression = (pfnXmlXPathEvalExpression_t)GetProcAddress(hLibxml2, "xmlXPathEvalExpression");
				pfn_xmlXPathFreeObject     = (pfnXmlXPathFreeObject_t)    GetProcAddress(hLibxml2, "xmlXPathFreeObject");
				pfn_xmlNodeListGetString   = (pfnXmlNodeListGetString_t)  GetProcAddress(hLibxml2, "xmlNodeListGetString");
				pfn_xmlFreeDoc             = (pfnXmlFreeDoc_t)            GetProcAddress(hLibxml2, "xmlFreeDoc");
				/* xmlFree is a data symbol (function pointer variable) — must dereference to get the free function */
				{
					pfnXmlFree_t* pVar = (pfnXmlFree_t*)GetProcAddress(hLibxml2, "xmlFree");
					pfn_xmlFree = pVar ? *pVar : NULL;
				}
				pfn_xmlXPathFreeContext = (pfnXmlXPathFreeContext_t)GetProcAddress(hLibxml2, "xmlXPathFreeContext");
				ts3Functions.logMessage("libcurl and libxml2 loaded OK", LogLevel_INFO, "Plugin", 0);
			}
		}
	}
#endif

    return 0;  /* 0 = success, 1 = failure, -2 = failure but client will not show a "failed to load" warning */
	/* -2 is a very special case and should only be used if a plugin displays a dialog (e.g. overlay) asking the user to disable
	 * the plugin again, avoiding the show another dialog by the client telling the user the plugin failed to load.
	 * For normal case, if a plugin really failed to load because of an error, the correct return value is 1. */
}

/* Custom code called right before the plugin is unloaded */
void ts3plugin_shutdown() {
    /* Your plugin cleanup code here */
    printf("PLUGIN: shutdown\n");

	/*
	 * Note:
	 * If your plugin implements a settings dialog, it must be closed and deleted here, else the
	 * TeamSpeak client will most likely crash (DLL removed but dialog from DLL code still open).
	 */

	/* Free pluginID if we registered it */
	if(pluginID) {
		free(pluginID);
		pluginID = NULL;
	}

#ifdef _WIN32
	if (hLibxml2) { FreeLibrary(hLibxml2); hLibxml2 = NULL; }
	if (hLibcurl)  { FreeLibrary(hLibcurl);  hLibcurl  = NULL; }
#endif
}

/****************************** Optional functions ********************************/

int ts3plugin_offersConfigure() {
    return 1;
}

#ifdef _WIN32
static INT_PTR CALLBACK SettingsDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;
    switch (msg) {
        case WM_INITDIALOG:
            CheckDlgButton(hwnd, IDC_CHECK_DESCRIPTION,
                g_settings.show_description  ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwnd, IDC_CHECK_TITLE_INLINE,
                g_settings.show_title_inline ? BST_CHECKED : BST_UNCHECKED);
            return TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                g_settings.show_description =
                    (IsDlgButtonChecked(hwnd, IDC_CHECK_DESCRIPTION)  == BST_CHECKED) ? 1 : 0;
                g_settings.show_title_inline =
                    (IsDlgButtonChecked(hwnd, IDC_CHECK_TITLE_INLINE) == BST_CHECKED) ? 1 : 0;
                Settings_Save(g_pluginPath);
                EndDialog(hwnd, IDOK);
            } else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hwnd, IDCANCEL);
            }
            return TRUE;
    }
    return FALSE;
}
#endif

void ts3plugin_configure(void* handle, void* qParentWidget) {
    (void)handle; (void)qParentWidget;
#ifdef _WIN32
    HMODULE hDll = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)(void*)ts3plugin_configure, &hDll);
    DialogBox(hDll, MAKEINTRESOURCE(IDD_SETTINGS), NULL, SettingsDlgProc);
#endif
}

/* Client changed current server connection handler */
void ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID) {
    printf("PLUGIN: currentServerConnectionChanged %llu (%llu)\n", (long long unsigned int)serverConnectionHandlerID, (long long unsigned int)ts3Functions.getCurrentServerConnectionHandlerID());
}

/*
 * Implement the following three functions when the plugin should display a line in the server/channel/client info.
 * If any of ts3plugin_infoTitle, ts3plugin_infoData or ts3plugin_freeMemory is missing, the info text will not be displayed.
 */

/* Static title shown in the left column in the info frame */
const char* ts3plugin_infoTitle() {
	return "This is the website preview plugin.";
}

/*
 * Dynamic content shown in the right column in the info frame. Memory for the data string needs to be allocated in this
 * function. The client will call ts3plugin_freeMemory once done with the string to release the allocated memory again.
 * Check the parameter "type" if you want to implement this feature only for specific item types. Set the parameter
 * "data" to NULL to have the client ignore the info data.
 */
void ts3plugin_infoData(uint64 serverConnectionHandlerID, uint64 id, enum PluginItemType type, char** data) {
	char* name;

	/* For demonstration purpose, display the name of the currently selected server, channel or client. */
	switch(type) {
		case PLUGIN_SERVER:
			if(ts3Functions.getServerVariableAsString(serverConnectionHandlerID, VIRTUALSERVER_NAME, &name) != ERROR_ok) {
				printf("Error getting virtual server name\n");
				return;
			}
			break;
		case PLUGIN_CHANNEL:
			if(ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, id, CHANNEL_NAME, &name) != ERROR_ok) {
				printf("Error getting channel name\n");
				return;
			}
			break;
		case PLUGIN_CLIENT:
			if(ts3Functions.getClientVariableAsString(serverConnectionHandlerID, (anyID)id, CLIENT_NICKNAME, &name) != ERROR_ok) {
				printf("Error getting client nickname\n");
				return;
			}
			break;
		default:
			printf("Invalid item type: %d\n", type);
			data = NULL;  /* Ignore */
			return;
	}

	*data = (char*)malloc(INFODATA_BUFSIZE * sizeof(char));  /* Must be allocated in the plugin! */
	snprintf(*data, INFODATA_BUFSIZE, "The nickname is \"%s\"", name);
	ts3Functions.freeMemory(name);
}

/* Required to release the memory for parameter "data" allocated in ts3plugin_infoData and ts3plugin_initMenus */
void ts3plugin_freeMemory(void* data) {
	free(data);
}

/************************** TeamSpeak callbacks ***************************/
/*
 * Following functions are optional, feel free to remove unused callbacks.
 * See the clientlib documentation for details on each function.
 */

/* Clientlib */

void GetHTML(const char* url, struct MemoryStruct *chunk, CURLcode *curlCode, const char *curlMessage) {
	CURL *curl;

	pfn_curl_global_init(CURL_GLOBAL_ALL);
	curl = pfn_curl_easy_init();
	if (curl) {
		pfn_curl_easy_setopt(curl, CURLOPT_URL, url);
		pfn_curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		pfn_curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
		pfn_curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) chunk);
		pfn_curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl/1.0");
		pfn_curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		pfn_curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		*curlCode = pfn_curl_easy_perform(curl);

		if (*curlCode != CURLE_OK) {
			//curl_easy_cleanup(curl);
			//free(chunk.memory);
			//return NULL;
			curlMessage = pfn_curl_easy_strerror(*curlCode);
		}

		pfn_curl_easy_cleanup(curl);
		//free(chunk.memory);
		pfn_curl_global_cleanup();

	} else {
		*curlCode = CURLE_LIBRARY_NOT_FOUND;
		curlMessage = "Could not open cURL handle";
	}
}

static char* GetOGProperty(htmlDocPtr doc, xmlXPathContextPtr context, const char* property) {
	char xpath[128];
	xmlXPathObjectPtr result;
	char* value = NULL;

	snprintf(xpath, sizeof(xpath), "//meta[@property='%s']/@content", property);
	result = pfn_xmlXPathEvalExpression((const xmlChar*)xpath, context);
	if (result && !xmlXPathNodeSetIsEmpty(result->nodesetval)) {
		/* XPath selected the @content attribute node; its text child holds the value */
		value = (char*)pfn_xmlNodeListGetString(doc, result->nodesetval->nodeTab[0]->children, 1);
	}
	if (result) pfn_xmlXPathFreeObject(result);
	return value;
}

int ts3plugin_onTextMessageEvent(
	uint64 serverConnectionHandlerID, 
	anyID targetMode, 
	anyID toID, 
	anyID fromID, 
	const char* fromName, 
	const char* fromUniqueIdentifier, 
	const char* message, 
	int ffIgnored
) {
	anyID myID;
	uint64 channelID;

	printf("PLUGIN: onTextMessageEvent %llu %d %d %s %s %d\n", (long long unsigned int)serverConnectionHandlerID, targetMode, fromID, fromName, message, ffIgnored);

	if(ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok) {
		ts3Functions.logMessage("Error querying own client id", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
		return 0;
	}

	if (ts3Functions.getChannelOfClient(serverConnectionHandlerID, myID, &channelID) != ERROR_ok) {
		ts3Functions.logMessage("Error querying current channel", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
		return 0;
	}

	/* Friend/Foe manager has ignored the message, so ignore here as well. */
	if (ffIgnored) {
		//sentSelfMessage = 1;
		return 1; /* Client will ignore the message anyways, so return value here doesn't matter */
	}

	if (fromID == myID) {
		/*
		 * Is 0 / false by default.
		 * Set to 1 / true BEFORE sending newly formatted message
		 * Set to 0 / false AFTER newly formatted message is sent
		 */
		if (!sentSelfMessage) {
			CURLcode curlCode;
			const char *curlMessage = "";
			struct MemoryStruct chunk;

			htmlDocPtr doc;
			xmlXPathContextPtr context;
			xmlXPathObjectPtr result;
			char* og_title = NULL;
			char* og_desc  = NULL;
			char* og_image = NULL;
			char* html_title = NULL;
			const char* title;

			char newMessage[4096];
			char errorMessage[128];

			const char* url = GetURLFromMessage(message);

			if (url != NULL) {
			chunk.memory = (char *) malloc(1);  /* will be grown as needed by the realloc above */
			chunk.size = 0;    /* no data at this point */

			/* Suppress the server echo of a URL we already processed within the last 30 s.
			 * TS3 echoes the original [URL]...[/URL] message back from the server after we
			 * have already handled it, which would otherwise trigger a duplicate fetch. */
			{
				DWORD now = GetTickCount();
				if (strcmp(url, lastSentURL) == 0 && (now - lastSentURLTick) < 30000U) {
					free((void*)url);
					free(chunk.memory);
					return 1;
				}
			}

			ts3Functions.logMessage("Opening URL: ", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
			ts3Functions.logMessage(url, LogLevel_INFO, "Plugin", serverConnectionHandlerID);

			GetHTML(url, &chunk, &curlCode, curlMessage);

			if (curlCode != 0) {
				ts3Functions.logMessage("cURL Error: ", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
				ts3Functions.logMessage(curlMessage, LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
			}

#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
			sprintf_s(errorMessage, 128, "Reading HTML file that is the following bytes long: %d", chunk.size);
#else
			sprintf(errorMessage, "Reading HTML file that is the following bytes long: %d", chunk.size);
#endif
			ts3Functions.logMessage(errorMessage, LogLevel_INFO, "Plugin", serverConnectionHandlerID);

			doc = pfn_htmlReadMemory(chunk.memory, chunk.size, url, NULL, HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
			if (!doc) {
				ts3Functions.logMessage("Could not read HTML document from memory", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
				free((void*)url);
				free(chunk.memory);
				return 0;
			}

			context = pfn_xmlXPathNewContext(doc);

			og_title = GetOGProperty(doc, context, "og:title");
			og_desc  = GetOGProperty(doc, context, "og:description");
			og_image = GetOGProperty(doc, context, "og:image");

			/* Fall back to <title> element if no og:title */
			if (!og_title) {
				result = pfn_xmlXPathEvalExpression("/html/head/title", context);
				if (result && !xmlXPathNodeSetIsEmpty(result->nodesetval)) {
					html_title = (char*)pfn_xmlNodeListGetString(doc,
						result->nodesetval->nodeTab[result->nodesetval->nodeNr - 1]->xmlChildrenNode, 1);
				}
				if (result) pfn_xmlXPathFreeObject(result);
			}

			if (pfn_xmlXPathFreeContext) pfn_xmlXPathFreeContext(context);

			title = og_title ? og_title : (html_title ? html_title : "(untitled)");

			BuildPreviewMessage(title, url, g_settings.show_description ? og_desc : NULL, og_image, newMessage, sizeof(newMessage));

			if (pfn_xmlFree) {
				if (og_title)   pfn_xmlFree(og_title);
				if (og_desc)    pfn_xmlFree(og_desc);
				if (og_image)   pfn_xmlFree(og_image);
				if (html_title) pfn_xmlFree(html_title);
			}
			pfn_xmlFreeDoc(doc);
			free(chunk.memory);
			
			strncpy_s(lastSentURL, sizeof(lastSentURL), url, _TRUNCATE);
			free((void*)url);
			lastSentURLTick = GetTickCount();
			sentSelfMessage = 1;
			if (ts3Functions.requestSendChannelTextMsg(serverConnectionHandlerID, newMessage, channelID, NULL) != ERROR_ok) {
				ts3Functions.logMessage("Error requesting send text message", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
			}
			return 1;
			} /* url != NULL (use case 1) */

			/* Use case 2: URL(s) embedded in a typed message. */
			if (g_settings.show_title_inline) {
				char* inlineURLs[MAX_URLS_PER_MESSAGE];
				char* inlineTitles[MAX_URLS_PER_MESSAGE];
				int urlCount, i, anyTitle;
				struct MemoryStruct chunk2;
				CURLcode curlCode2;
				const char *curlMessage2;
				htmlDocPtr doc2;
				xmlXPathContextPtr context2;
				xmlXPathObjectPtr result2;
				char* og_title2;
				char* html_title2;

				/* Suppress server echo of the original message we already processed. */
				{
					DWORD now = GetTickCount();
					if (lastSentMessage[0] != '\0' && strcmp(message, lastSentMessage) == 0
							&& (now - lastSentMessageTick) < 30000U) {
						return 1;
					}
				}

				memset(inlineURLs,   0, sizeof(inlineURLs));
				memset(inlineTitles, 0, sizeof(inlineTitles));
				urlCount = FindURLsInMessage(message, inlineURLs, MAX_URLS_PER_MESSAGE);
				anyTitle = 0;

				for (i = 0; i < urlCount; i++) {
					curlMessage2 = "";
					og_title2    = NULL;
					html_title2  = NULL;

					chunk2.memory = (char*)malloc(1);
					chunk2.size   = 0;

					GetHTML(inlineURLs[i], &chunk2, &curlCode2, curlMessage2);

					if (curlCode2 != 0) {
						free(chunk2.memory);
						continue;
					}

					doc2 = pfn_htmlReadMemory(chunk2.memory, chunk2.size, inlineURLs[i], NULL,
					                          HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
					free(chunk2.memory);
					if (!doc2) continue;

					context2  = pfn_xmlXPathNewContext(doc2);
					og_title2 = GetOGProperty(doc2, context2, "og:title");

					if (!og_title2) {
						result2 = pfn_xmlXPathEvalExpression("/html/head/title", context2);
						if (result2 && !xmlXPathNodeSetIsEmpty(result2->nodesetval)) {
							html_title2 = (char*)pfn_xmlNodeListGetString(doc2,
								result2->nodesetval->nodeTab[result2->nodesetval->nodeNr - 1]->xmlChildrenNode, 1);
						}
						if (result2) pfn_xmlXPathFreeObject(result2);
					}

					if (pfn_xmlXPathFreeContext) pfn_xmlXPathFreeContext(context2);
					pfn_xmlFreeDoc(doc2);

					inlineTitles[i] = og_title2 ? og_title2 : html_title2;
					if (inlineTitles[i]) anyTitle = 1;
				}

				if (urlCount > 0 && anyTitle) {
					BuildMessageWithInlineTitles(message, (const char**)inlineURLs,
					                             (const char**)inlineTitles, urlCount,
					                             newMessage, sizeof(newMessage));

					for (i = 0; i < urlCount; i++) {
						if (inlineURLs[i])                  free(inlineURLs[i]);
						if (inlineTitles[i] && pfn_xmlFree) pfn_xmlFree(inlineTitles[i]);
					}

					if (strcmp(newMessage, message) != 0) {
						strncpy_s(lastSentMessage, sizeof(lastSentMessage), message, _TRUNCATE);
						lastSentMessageTick = GetTickCount();
						sentSelfMessage = 1;
						if (ts3Functions.requestSendChannelTextMsg(serverConnectionHandlerID, newMessage, channelID, NULL) != ERROR_ok) {
							ts3Functions.logMessage("Error requesting send text message", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
						}
						return 1;
					}
				} else {
					for (i = 0; i < urlCount; i++) {
						if (inlineURLs[i])                  free(inlineURLs[i]);
						if (inlineTitles[i] && pfn_xmlFree) pfn_xmlFree(inlineTitles[i]);
					}
				}
			}

			return 0;
		} else {
			/* First echo of our formatted message — display it, then clear the flag. */
			sentSelfMessage = 0;
			return 0;
		}
	} else {
		return 0;
	}
}
