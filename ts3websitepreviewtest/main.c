#ifdef _WIN32
#pragma warning (disable : 4100)  /* Disable Unreferenced parameter warning */
#include <windows.h>
#include <delayimp.h>
#include <tchar.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "curl/curl.h"
#include "libxml/HTMLparser.h"
#include "libxml/globals.h"
#include "libxml/xpath.h"

#ifdef _WIN32
#pragma comment(lib, "libcurl")
#pragma comment(lib, "iconv")
#pragma comment(lib, "libxml2")
#pragma comment(lib, "zlib1")
#endif

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
	printf(urlStart);

	if (urlStart == NULL) {
		printf("URL Start is null .");
		return NULL;
	}

	/*
	 * If the pointer to message is not the same as urlStart,
	 * message does not begin with [URL]
	 */
	if (message != urlStart) {
		printf("Message does not equal URL Start, return null.");
		printf(message);
		printf(urlStart);
		return NULL;
	}

	urlStart += 5;

	urlEnd = strstr(urlStart, "[/URL]");
	if (urlEnd == NULL) {
		urlEnd = strstr(urlStart, "[/url]");
	}

	if (urlEnd == NULL) {
		printf("URL End is null .");
		return NULL;
	}

	/*
	 * If [/URL] is not followed by null, there is a message after the URL
	 */
	if (urlEnd[6] != '\0') {
		printf("URL End not the end of the message .");
		printf(urlEnd[6]);
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

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (curl) {
		printf("Running curl...");
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) chunk);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl/1.0");
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		*curlCode = curl_easy_perform(curl);

		if (*curlCode != CURLE_OK) {
			//curl_easy_cleanup(curl);
			//free(chunk.memory);
			//return NULL;
			curlMessage = curl_easy_strerror(*curlCode);
		}

		curl_easy_cleanup(curl);
		//free(chunk.memory);
		curl_global_cleanup();

	} else {
		*curlCode = CURLE_LIBRARY_NOT_FOUND;
		curlMessage = "Could not open cURL handle";
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
    char message[100] = "[URL]https://www.youtube.com/watch?v=wCRStRWMdWM#t=39s[/URL]\0";

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
	const char* url;

    //printf("Enter your message: ");
    //fgets(message, 100, stdin);
    printf("Your Message is: %s\n", message);

	system("pause");

	url = GetURLFromMessage(message);
			
	chunk.memory = (char *) malloc(1);  /* will be grown as needed by the realloc above */ 
	chunk.size = 0;    /* no data at this point */

	if (url == NULL) {
		printf("URL is null, exiting...\n");
		system("pause");
		free(chunk.memory);
		return 0;
	}

	printf("URL: ");
	printf(url);
	printf("\n");

	//ts3Functions.logMessage("Opening URL: ", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
	//ts3Functions.logMessage(url, LogLevel_INFO, "Plugin", serverConnectionHandlerID);

	GetHTML(url, &chunk, &curlCode, curlMessage);
	
	printf("curlMessage: ");
	printf(curlMessage);
	printf("\n");

	//if (curlCode != 0) {
	//	ts3Functions.logMessage("cURL Error: ", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
	//	ts3Functions.logMessage(curlMessage, LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
	//}

#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
	sprintf_s(errorMessage, 128, "Reading HTML file that is the following bytes long: %d", chunk.size);
#else
	sprintf(errorMessage, "Reading HTML file that is the following bytes long: %d", chunk.size);
#endif
	//ts3Functions.logMessage(errorMessage, LogLevel_INFO, "Plugin", serverConnectionHandlerID);

	doc = htmlReadMemory(chunk.memory, chunk.size, url, NULL, HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
	if (!doc) {
		printf("Could not read HTML document from memory\n");

	system("pause");
		//ts3Functions.logMessage("Could not read HTML document from memory", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
		free(chunk.memory);
		return 0;
	}

	context = xmlXPathNewContext(doc);
	result = xmlXPathEvalExpression("/html/head/title", context);

	if (xmlXPathNodeSetIsEmpty(result->nodesetval)) {
		printf("Could not read HTML node set from memory\n");
		system("pause");
		//ts3Functions.logMessage("Could not read HTML node set from memory", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
		xmlXPathFreeObject(result);
		free(chunk.memory);
		return 0;
	}

	for (i=0; i < result->nodesetval->nodeNr; i++) {
		keyword = (char *) xmlNodeListGetString(doc, result->nodesetval->nodeTab[i]->xmlChildrenNode, 1);
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
			
	//xmlFree(keyword);
	xmlXPathFreeObject(result);
	xmlFreeDoc(doc);
	free(chunk.memory);

	printf("New message: ");
	printf(newMessage);
	printf("\n");

	printf("End\n");
	system("pause");

    return 1;
}