#ifndef ENCODING_H
#define ENCODING_H

#include <QString>

namespace FeatherPad {

const QString detectCharset (const QByteArray& byteArray);

}

#endif // ENCODING_H
