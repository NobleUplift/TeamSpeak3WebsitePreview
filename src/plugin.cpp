/*
 * TeamSpeak 3 Website Preview — plugin entry points (C++/Qt).
 *
 * Fetches the title (and optionally description) of URLs sent in chat and
 * resends the message with the title prepended. Uses Qt for everything that
 * used to require platform-specific code: QNetworkAccessManager for the fetch,
 * the Gumbo HTML5 parser (webparse) for title/OGP extraction, and a QDialog +
 * QSettings for configuration — so there are no #ifdef blocks.
 */

#include <QByteArray>
#include <QDateTime>
#include <QEventLoop>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>
#include <QString>
#include <QUrl>
#include <QWidget>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "teamspeak/public_errors.h"
#include "teamspeak/public_errors_rare.h"
#include "teamspeak/public_definitions.h"
#include "teamspeak/public_rare_definitions.h"
#include "ts3_functions.h"
#include "plugin_definitions.h"

#include "plugin.h"
#include "core.h"
#include "config.h"
#include "webparse.h"
#include "plugin_version.h"

#define PLUGIN_API_VERSION 26
#define PATH_BUFSIZE 512
#define INFODATA_BUFSIZE 128

static struct TS3Functions ts3Functions;
static char* pluginID = nullptr;

static QString g_iniPath;
static PluginSettings g_settings;

static int sentSelfMessage = 0;

/* Suppress the server echo of messages we just processed (30 s window). */
static QString lastSentURL;
static qint64  lastSentURLTick = 0;
static QString lastSentMessage;
static qint64  lastSentMessageTick = 0;

static qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

/* Blocking HTTP GET via Qt: spins a local event loop until the reply finishes,
 * so the surrounding synchronous message-handling logic is unchanged. */
static QByteArray httpGet(const QString& url) {
    QNetworkAccessManager nam;
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Mozilla/5.0 (compatible; TS3WebsitePreview)"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = nam.get(req);
    /* Mirror the previous CURLOPT_SSL_VERIFYPEER=0 behaviour: accept any cert. */
    QObject::connect(reply, &QNetworkReply::sslErrors, reply,
                     [reply](const QList<QSslError>&) { reply->ignoreSslErrors(); });

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QByteArray body = reply->readAll();
    reply->deleteLater();
    return body;
}

/* Best title for a page: og:title, else <title>, else "(untitled)". */
static QString pageTitle(const QByteArray& html) {
    QString t = webparse::extractOgProperty(html, QStringLiteral("og:title"));
    if (t.isEmpty()) t = webparse::extractTitle(html);
    return t;
}

/*********************************** Required functions ************************************/

extern "C" const char* ts3plugin_name()        { return PLUGIN_NAME; }
extern "C" const char* ts3plugin_version()      { return PLUGIN_VERSION_STR; }
extern "C" int         ts3plugin_apiVersion()   { return PLUGIN_API_VERSION; }
extern "C" const char* ts3plugin_author()       { return PLUGIN_AUTHOR; }
extern "C" const char* ts3plugin_description()  { return PLUGIN_DESCRIPTION; }

extern "C" void ts3plugin_setFunctionPointers(const struct TS3Functions funcs) {
    ts3Functions = funcs;
}

extern "C" int ts3plugin_init() {
    char pluginPath[PATH_BUFSIZE];
    ts3Functions.getPluginPath(pluginPath, PATH_BUFSIZE, pluginID);
    g_iniPath = QString::fromUtf8(pluginPath) + QStringLiteral("ts3websitepreview.ini");
    settingsLoad(g_iniPath, g_settings);
    return 0;  /* 0 = success */
}

extern "C" void ts3plugin_shutdown() {
    if (pluginID) {
        free(pluginID);
        pluginID = nullptr;
    }
}

/****************************** Optional functions ********************************/

extern "C" int ts3plugin_offersConfigure() {
    /* Qt dialog shown on the client's Qt thread — works on every platform. */
    return PLUGIN_OFFERS_CONFIGURE_QT_THREAD;
}

extern "C" void ts3plugin_configure(void* /*handle*/, void* qParentWidget) {
    auto* dlg = new ConfigDialog(g_iniPath, &g_settings, static_cast<QWidget*>(qParentWidget));
    dlg->show();
    dlg->raise();
}

extern "C" void ts3plugin_registerPluginID(const char* id) {
    const size_t sz = strlen(id) + 1;
    pluginID = static_cast<char*>(malloc(sz));
    if (pluginID) memcpy(pluginID, id, sz);
}

extern "C" const char* ts3plugin_infoTitle() {
    return "This is the website preview plugin.";
}

extern "C" void ts3plugin_infoData(uint64 serverConnectionHandlerID, uint64 id,
                                   enum PluginItemType type, char** data) {
    char* name;
    switch (type) {
        case PLUGIN_SERVER:
            if (ts3Functions.getServerVariableAsString(serverConnectionHandlerID, VIRTUALSERVER_NAME, &name) != ERROR_ok)
                return;
            break;
        case PLUGIN_CHANNEL:
            if (ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, id, CHANNEL_NAME, &name) != ERROR_ok)
                return;
            break;
        case PLUGIN_CLIENT:
            if (ts3Functions.getClientVariableAsString(serverConnectionHandlerID, (anyID)id, CLIENT_NICKNAME, &name) != ERROR_ok)
                return;
            break;
        default:
            data = NULL;
            return;
    }
    *data = static_cast<char*>(malloc(INFODATA_BUFSIZE));
    snprintf(*data, INFODATA_BUFSIZE, "The nickname is \"%s\"", name);
    ts3Functions.freeMemory(name);
}

extern "C" void ts3plugin_freeMemory(void* data) {
    free(data);
}

/************************** TeamSpeak callbacks ***************************/

extern "C" int ts3plugin_onTextMessageEvent(
    uint64 serverConnectionHandlerID, anyID targetMode, anyID toID, anyID fromID,
    const char* fromName, const char* fromUniqueIdentifier, const char* message, int ffIgnored) {
    (void)targetMode; (void)toID; (void)fromName; (void)fromUniqueIdentifier;

    anyID myID;
    uint64 channelID;

    if (ts3Functions.getClientID(serverConnectionHandlerID, &myID) != ERROR_ok)
        return 0;
    if (ts3Functions.getChannelOfClient(serverConnectionHandlerID, myID, &channelID) != ERROR_ok)
        return 0;

    if (ffIgnored)
        return 1;  /* Friend/Foe manager already ignores it */

    if (fromID != myID)
        return 0;  /* only act on our own outbound messages */

    if (sentSelfMessage) {
        /* First echo of our formatted message — display it, then clear the flag. */
        sentSelfMessage = 0;
        return 0;
    }

    char newMessage[4096];

    /* ---- Use case 1: the whole message is exactly [URL]...[/URL] ---- */
    const char* url = GetURLFromMessage(message);
    if (url != NULL) {
        /* Suppress the ~5 s-later server echo of a URL we just handled. */
        if (lastSentURL == QString::fromUtf8(url) && (nowMs() - lastSentURLTick) < 30000) {
            free((void*)url);
            return 1;
        }

        const QByteArray html = httpGet(QString::fromUtf8(url));
        const QString title = pageTitle(html);
        const QString ogDesc = webparse::extractOgProperty(html, QStringLiteral("og:description"));

        const QString finalTitle = title.isEmpty() ? QStringLiteral("(untitled)") : title;
        const QByteArray titleB = finalTitle.toUtf8();
        const QByteArray descB  = ogDesc.toUtf8();
        BuildPreviewMessage(titleB.constData(), url,
                            (g_settings.show_description && !ogDesc.isEmpty()) ? descB.constData() : NULL,
                            NULL, newMessage, sizeof(newMessage));

        lastSentURL = QString::fromUtf8(url);
        lastSentURLTick = nowMs();
        free((void*)url);
        sentSelfMessage = 1;
        if (ts3Functions.requestSendChannelTextMsg(serverConnectionHandlerID, newMessage, channelID, NULL) != ERROR_ok)
            ts3Functions.logMessage("Error requesting send text message", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
        return 1;
    }

    /* ---- Use case 2: URL(s) embedded in a typed message ---- */
    if (g_settings.show_title_inline) {
        if (!lastSentMessage.isEmpty() && lastSentMessage == QString::fromUtf8(message)
                && (nowMs() - lastSentMessageTick) < 30000) {
            return 1;
        }

        char* inlineURLs[MAX_URLS_PER_MESSAGE];
        const char* inlineTitles[MAX_URLS_PER_MESSAGE];
        QByteArray titleBytes[MAX_URLS_PER_MESSAGE];
        memset(inlineURLs, 0, sizeof(inlineURLs));
        memset(inlineTitles, 0, sizeof(inlineTitles));

        const int urlCount = FindURLsInMessage(message, inlineURLs, MAX_URLS_PER_MESSAGE);
        int anyTitle = 0;
        for (int i = 0; i < urlCount; i++) {
            const QByteArray html = httpGet(QString::fromUtf8(inlineURLs[i]));
            const QString t = pageTitle(html);
            if (!t.isEmpty()) {
                titleBytes[i] = t.toUtf8();
                inlineTitles[i] = titleBytes[i].constData();
                anyTitle = 1;
            }
        }

        int handled = 0;
        if (urlCount > 0 && anyTitle) {
            BuildMessageWithInlineTitles(message, (const char**)inlineURLs, inlineTitles,
                                         urlCount, newMessage, sizeof(newMessage));
            if (strcmp(newMessage, message) != 0) {
                lastSentMessage = QString::fromUtf8(message);
                lastSentMessageTick = nowMs();
                sentSelfMessage = 1;
                if (ts3Functions.requestSendChannelTextMsg(serverConnectionHandlerID, newMessage, channelID, NULL) != ERROR_ok)
                    ts3Functions.logMessage("Error requesting send text message", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
                handled = 1;
            }
        }

        for (int i = 0; i < urlCount; i++)
            free(inlineURLs[i]);

        if (handled)
            return 1;
    }

    return 0;
}
