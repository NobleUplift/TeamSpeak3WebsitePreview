#ifdef _WIN32
#pragma warning (disable : 4100)  /* Disable Unreferenced parameter warning */
#include <windows.h>
//#include <delayimp.h>
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

/*#ifdef _WIN32
#pragma comment(lib, "libcurl")
#pragma comment(lib, "iconv")
#pragma comment(lib, "libxml2")
#pragma comment(lib, "zlib1")
#endif*/

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

static char* GetOGProperty(htmlDocPtr doc, xmlXPathContextPtr context, const char* property) {
	char xpath[128];
	xmlXPathObjectPtr result;
	char* value = NULL;

#if defined(_WIN32) || defined(WIN32) || defined(WIN64) || defined(_WIN64)
	sprintf_s(xpath, sizeof(xpath), "//meta[@property='%s']/@content", property);
#else
	snprintf(xpath, sizeof(xpath), "//meta[@property='%s']/@content", property);
#endif
	result = xmlXPathEvalExpression((const xmlChar*)xpath, context);
	if (result && !xmlXPathNodeSetIsEmpty(result->nodesetval)) {
		value = (char*)xmlNodeListGetString(doc, result->nodesetval->nodeTab[0]->children, 1);
	}
	if (result) xmlXPathFreeObject(result);
	return value;
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
	char* og_title = NULL;
	char* og_desc  = NULL;
	char* og_image = NULL;
	char* html_title = NULL;
	const char* title;

	char newMessage[4096];
	char errorMessage[128];
	const char* url;

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

	printf("URL: %s\n", url);

	GetHTML(url, &chunk, &curlCode, curlMessage);

	printf("curlMessage: %s\n", curlMessage);

#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
	sprintf_s(errorMessage, sizeof(errorMessage), "Reading HTML file that is the following bytes long: %d", (int)chunk.size);
#else
	snprintf(errorMessage, sizeof(errorMessage), "Reading HTML file that is the following bytes long: %d", (int)chunk.size);
#endif
	printf("%s\n", errorMessage);

	doc = htmlReadMemory(chunk.memory, chunk.size, url, NULL, HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
	if (!doc) {
		printf("Could not read HTML document from memory\n");
		system("pause");
		free(chunk.memory);
		return 0;
	}

	context = xmlXPathNewContext(doc);

	og_title = GetOGProperty(doc, context, "og:title");
	og_desc  = GetOGProperty(doc, context, "og:description");
	og_image = GetOGProperty(doc, context, "og:image");

	printf("og:title       = %s\n", og_title ? og_title : "(none)");
	printf("og:description = %s\n", og_desc  ? og_desc  : "(none)");
	printf("og:image       = %s\n", og_image ? og_image : "(none)");

	/* Fall back to <title> element if no og:title */
	if (!og_title) {
		result = xmlXPathEvalExpression("/html/head/title", context);
		if (result && !xmlXPathNodeSetIsEmpty(result->nodesetval)) {
			html_title = (char*)xmlNodeListGetString(doc,
				result->nodesetval->nodeTab[result->nodesetval->nodeNr - 1]->xmlChildrenNode, 1);
		}
		if (result) xmlXPathFreeObject(result);
	}

	xmlXPathFreeContext(context);

	title = og_title ? og_title : (html_title ? html_title : "(untitled)");
	printf("title (used)   = %s\n", title);

#if defined(_WIN32) || defined(WIN32) || defined(WIN64) || defined(_WIN64)
	sprintf_s(newMessage, sizeof(newMessage), "\"%s\" <[URL]%s[/URL]>", title, url);
	if (og_desc) {
		strncat_s(newMessage, sizeof(newMessage), "\n", _TRUNCATE);
		strncat_s(newMessage, sizeof(newMessage), og_desc, _TRUNCATE);
	}
	if (og_image) {
		strncat_s(newMessage, sizeof(newMessage), "\n<img src=\"", _TRUNCATE);
		strncat_s(newMessage, sizeof(newMessage), og_image, _TRUNCATE);
		strncat_s(newMessage, sizeof(newMessage), "\">", _TRUNCATE);
	}
#else
	snprintf(newMessage, sizeof(newMessage), "\"%s\" <[URL]%s[/URL]>", title, url);
	if (og_desc) {
		strncat(newMessage, "\n", sizeof(newMessage) - strlen(newMessage) - 1);
		strncat(newMessage, og_desc, sizeof(newMessage) - strlen(newMessage) - 1);
	}
	if (og_image) {
		strncat(newMessage, "\n<img src=\"", sizeof(newMessage) - strlen(newMessage) - 1);
		strncat(newMessage, og_image, sizeof(newMessage) - strlen(newMessage) - 1);
		strncat(newMessage, "\">", sizeof(newMessage) - strlen(newMessage) - 1);
	}
#endif

	xmlFree(og_title);
	xmlFree(og_desc);
	xmlFree(og_image);
	xmlFree(html_title);
	xmlFreeDoc(doc);
	free(chunk.memory);

	printf("\nNew message:\n%s\n", newMessage);

	printf("\nEnd\n");
	system("pause");

    return 1;
}