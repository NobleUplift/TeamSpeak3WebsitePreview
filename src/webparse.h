#pragma once

#include <QByteArray>
#include <QString>

// HTML <title> / Open Graph extraction using the Gumbo HTML5 parser
// (google/gumbo-parser, vendored as a submodule and linked statically). Gumbo is
// a lenient, spec-compliant parser and decodes HTML entities itself.
namespace webparse {

// Text of the first <title>…</title> element (entity-decoded, whitespace-collapsed).
// Empty QString if there is no title.
QString extractTitle(const QByteArray& html);

// Value of the <meta property="…" content="…"> tag matching `property`
// (e.g. "og:title"), entity-decoded. Empty QString if not present.
QString extractOgProperty(const QByteArray& html, const QString& property);

}  // namespace webparse
