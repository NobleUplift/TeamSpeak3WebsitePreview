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
#include "teamspeak/clientlib_publicdefinitions.h"
#include "ts3_functions.h"

#include "curl.h"
#include "HTMLparser.h"
#include "globals.h"
#include "xpath.h"

#include "plugin.h"

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
static pfnHtmlReadMemory_t pfn_htmlReadMemory;
static pfnXmlXPathNewContext_t pfn_xmlXPathNewContext;
static pfnXmlXPathEvalExpression_t pfn_xmlXPathEvalExpression;
static pfnXmlXPathFreeObject_t pfn_xmlXPathFreeObject;
static pfnXmlNodeListGetString_t pfn_xmlNodeListGetString;
static pfnXmlFree_t pfn_xmlFree;
static pfnXmlFreeDoc_t pfn_xmlFreeDoc;
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

static int sentSelfMessage = 0;

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
		const wchar_t* name = L"Website Preview";
		if(wcharToUtf8(name, &result) == -1) {  /* Convert name into UTF-8 encoded result */
			result = "Website Preview";  /* Conversion failed, fallback here */
		}
	}
	return result;
#else
	return "Website Preview";
#endif
}

/* Plugin version */
const char* ts3plugin_version() {
    return "1.0";
}

/* Plugin API version. Must be the same as the clients API major version, else the plugin fails to load. */
int ts3plugin_apiVersion() {
	return PLUGIN_API_VERSION;
}

/* Plugin author */
const char* ts3plugin_author() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "NobleUplift";
}

/* Plugin description */
const char* ts3plugin_description() {
	/* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "This plugin parses URLs in channel chat and appends a preview of the webpage along with its title and description to the end of the chat message.";
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

				/* Load libcurl.dll — LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR lets its own deps find each other */
				wcscpy_s(dllPath, MAX_PATH, dllDir);
				wcscat_s(dllPath, MAX_PATH, L"libcurl.dll");
				hLibcurl = LoadLibraryExW(dllPath, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
				if (!hLibcurl) {
					ts3Functions.logMessage("Could not load libcurl.dll", LogLevel_ERROR, "Plugin", 0);
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
					ts3Functions.logMessage("Could not load libxml2.dll", LogLevel_ERROR, "Plugin", 0);
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

struct MemoryStruct {
  char *memory;
  size_t size;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
	mem->memory = (char *) realloc(mem->memory, mem->size + realsize + 1);
	if (mem->memory == NULL) {
		/* out of memory! */
		return 0;
	}
 
	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;
 
	return realsize;
}

const char* GetURLFromMessage(const char* message) {
	//char prefix[5];
	const char *urlStart;
	const char *urlEnd;
	char *url;
	size_t length;

	/*strncpy(prefix, message, 5);
	if (strcmp(prefix, "[URL]") == 0 || strcmp(prefix, "[url]") == 0) {
		return NULL;
	}*/
	
	urlStart = strstr(message, "[URL]"); // ((mailto\:|(news|(ht|f)tp(s?)|)\://|www.){1}\S+) http://regexlib.com/RETester.aspx?regexp_id=37
	if (urlStart == NULL) {
		urlStart = strstr(message, "[url]");
	}

	if (urlStart == NULL) {
		return NULL;
	}

	/*
	 * If the pointer to message is not the same as urlStart,
	 * message does not begin with [URL]
	 */
	if (message != urlStart) {
		return NULL;
	}

	urlStart += 5;

	urlEnd = strstr(urlStart, "[/URL]");
	if (urlEnd == NULL) {
		urlEnd = strstr(urlStart, "[/url]");
	}

	if (urlEnd == NULL) {
		return NULL;
	}

	/*
	 * If [/URL] is not followed by null, there is a message after the URL
	 */
	if (urlEnd[6] != '\0') {
		return NULL;
	}

	length = urlEnd - urlStart;

	url = (char *) malloc(length + 1);
	memcpy((void *) url, urlStart, length);
	url[length] = 0;

	return (const char *) url;
}

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
			char * keyword;

			int i;
			char newMessage[1024];
			char errorMessage[128];

			const char* url = GetURLFromMessage(message);
			
			chunk.memory = (char *) malloc(1);  /* will be grown as needed by the realloc above */ 
			chunk.size = 0;    /* no data at this point */

			if (url == NULL) {
				free(chunk.memory);
				return 0;
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
				free(chunk.memory);
				return 0;
			}

			context = pfn_xmlXPathNewContext(doc);
			result = pfn_xmlXPathEvalExpression("/html/head/title", context);

			if (xmlXPathNodeSetIsEmpty(result->nodesetval)) {
				ts3Functions.logMessage("Could not read HTML node set from memory", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
				pfn_xmlXPathFreeObject(result);
				free(chunk.memory);
				return 0;
			}

			for (i=0; i < result->nodesetval->nodeNr; i++) {
				keyword = (char *) pfn_xmlNodeListGetString(doc, result->nodesetval->nodeTab[i]->xmlChildrenNode, 1);
				continue;
				//printf("keyword: %s\n", keyword);
			}

#if defined(_WIN32) || defined(WIN32) || defined(WIN64) || defined(_WIN64)
			strcpy_s(newMessage, 1024, "\"");
			strcat_s(newMessage, 1024, (const char *) keyword);
			strcat_s(newMessage, 1024, "\" <[URL]");
			strcat_s(newMessage, 1024, url);
			strcat_s(newMessage, 1024, "[/URL]>");
#else
			strcpy(newMessage, "\"");
			strcat(newMessage, (const char *) keyword);
			strcat(newMessage, "\" <");
			strcat(newMessage, message);
			strcat(newMessage, ">");
#endif
			
			if (pfn_xmlFree) pfn_xmlFree(keyword);
			pfn_xmlXPathFreeObject(result);
			pfn_xmlFreeDoc(doc);
			free(chunk.memory);
			
			sentSelfMessage = 1;
			if (ts3Functions.requestSendChannelTextMsg(serverConnectionHandlerID, newMessage, channelID, NULL) != ERROR_ok) {
				ts3Functions.logMessage("Error requesting send text message", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
			}
			// Caused infininte loop, kept going to this block of code:
			//sentSelfMessage = 0;
			return 1;
		} else {
			sentSelfMessage = 0;
			return 0;
		}
	} else {
		// Would have caused bug where a message sent by another client at the same time would have inversed the flow of client messages
		//sentSelfMessage = 0;
		return 0;  /* 0 = handle normally, 1 = client will ignore the text message */
	}
}
