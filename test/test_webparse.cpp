#include <QtTest>

#include "webparse.h"

// Covers the Gumbo-based title/OGP extraction in src/webparse (was test_parse.c).
class TestWebparse : public QObject {
    Q_OBJECT
private slots:
    void title_basic() {
        const QByteArray html = "<html><head><title>Hello World</title></head><body>x</body></html>";
        QCOMPARE(webparse::extractTitle(html), QStringLiteral("Hello World"));
    }
    void title_entities_decoded() {
        const QByteArray html = "<title>Tom &amp; Jerry &lt;3 &#39;quote&#39;</title>";
        QCOMPARE(webparse::extractTitle(html), QStringLiteral("Tom & Jerry <3 'quote'"));
    }
    void title_whitespace_collapsed() {
        const QByteArray html = "<title>\n  spaced\t out  \n</title>";
        QCOMPARE(webparse::extractTitle(html), QStringLiteral("spaced out"));
    }
    void title_missing() {
        QVERIFY(webparse::extractTitle("<html><body>no title</body></html>").isEmpty());
    }
    void title_case_insensitive() {
        QCOMPARE(webparse::extractTitle("<TITLE>Caps</TITLE>"), QStringLiteral("Caps"));
    }
    void og_title_and_description() {
        const QByteArray html =
            "<meta property=\"og:title\" content=\"OG Title\">"
            "<meta property=\"og:description\" content=\"A description.\">";
        QCOMPARE(webparse::extractOgProperty(html, "og:title"), QStringLiteral("OG Title"));
        QCOMPARE(webparse::extractOgProperty(html, "og:description"), QStringLiteral("A description."));
    }
    void og_attribute_order_and_quotes() {
        // content before property, single quotes, self-closing
        const QByteArray html = "<meta content='Reversed' property='og:title'/>";
        QCOMPARE(webparse::extractOgProperty(html, "og:title"), QStringLiteral("Reversed"));
    }
    void og_missing() {
        QVERIFY(webparse::extractOgProperty("<title>x</title>", "og:title").isEmpty());
    }
};

QTEST_MAIN(TestWebparse)
#include "test_webparse.moc"
