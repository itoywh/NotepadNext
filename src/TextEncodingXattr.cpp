/*
 * This file is part of Notepad Next.
 * Copyright 2026 Notepad Next contributors
 *
 * Notepad Next is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Notepad Next is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Notepad Next.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "TextEncodingXattr.h"
#include "EncodingSpecs.h"

#ifdef Q_OS_MACOS
#include <sys/xattr.h>
#endif

#include <QTextCodec>
#include <QVarLengthArray>

namespace TextEncodingXattr {

// IANA charset name -> EncodingSpecs menu spec. Keys are lower-cased. Only the
// encodings Notepad Next actually offers are present; the rest fall through to
// detection. Mirrors EncodingSpecs::menu() but keyed by the IANA spelling that
// Cocoa writes.
#ifdef Q_OS_MACOS
static const struct { const char *iana; const char *spec; } kIanaToSpec[] = {
    { "utf-8",               "UTF-8" },
    { "utf-16le",            "UTF-16LE" },
    { "utf-16be",            "UTF-16BE" },
    { "utf-16",              "UTF-16LE" },  // Apple platforms are little-endian
    { "utf-32le",            "UTF-32LE" },
    { "utf-32be",            "UTF-32BE" },
    { "utf-32",              "UTF-32LE" },
    { "gb18030",             "GB18030" },
    { "gbk",                 "GBK" },
    { "gb2312",              "GB2312" },
    { "big5",                "Big5" },
    { "big5-hkscs",          "Big5-HKSCS" },
    { "big5-eten",           "Big5-ETen" },
    { "shift_jis",           "Shift-JIS" },
    { "shift-jis",           "Shift-JIS" },
    { "euc-jp",              "EUC-JP" },
    { "euc-kr",              "EUC-KR" },
    { "uhc",                 "CP949" },
    { "cp949",               "CP949" },
    { "windows-1250",        "windows-1250" },
    { "windows-1251",        "windows-1251" },
    { "windows-1252",        "windows-1252" },
    { "windows-1253",        "windows-1253" },
    { "windows-1254",        "windows-1254" },
    { "windows-1255",        "windows-1255" },
    { "windows-1256",        "windows-1256" },
    { "windows-1257",        "windows-1257" },
    { "windows-1258",        "windows-1258" },
    { "iso-8859-1",          "ISO 8859-1" },
    { "iso-8859-2",          "ISO 8859-2" },
    { "iso-8859-3",          "ISO 8859-3" },
    { "iso-8859-4",          "ISO 8859-4" },
    { "iso-8859-5",          "ISO 8859-5" },
    { "iso-8859-6",          "ISO 8859-6" },
    { "iso-8859-7",          "ISO 8859-7" },
    { "iso-8859-8",          "ISO 8859-8" },
    { "iso-8859-9",          "ISO 8859-9" },
    { "iso-8859-10",         "ISO 8859-10" },
    { "iso-8859-13",         "ISO 8859-13" },
    { "iso-8859-14",         "ISO 8859-14" },
    { "iso-8859-15",         "ISO 8859-15" },
    { "iso-8859-16",         "ISO 8859-16" },
    { "macintosh",           "MacRoman" },
    { "x-mac-roman",         "MacRoman" },
    { "ibm850",              "IBM 850" },
    { "ibm866",              "IBM 866" },
    { "ibm874",              "IBM 874" },
    { "koi8-r",              "KOI8-R" },
    { "koi8-u",              "KOI8-U" },
    { "tis-620",             "TIS-620" },
};
#endif

QString ianaToSpec(const QString &iana)
{
#ifdef Q_OS_MACOS
    if (iana.isEmpty())
        return QString();

    const QString key = iana.toLower();
    for (const auto &entry : kIanaToSpec) {
        if (key == QLatin1String(entry.iana))
            return QString::fromLatin1(entry.spec);
    }
    return QString();
#else
    Q_UNUSED(iana);
    return QString();
#endif
}

// EncodingSpecs spec -> IANA name + optional CFStringEncoding. The number is
// only emitted when we are certain of it; Cocoa falls back to the IANA name.
#ifdef Q_OS_MACOS
static const struct { const char *spec; const char *iana; qint64 cf; bool hasCf; } kSpecToIana[] = {
    { "UTF-8",        "utf-8",        134217984, true },  // kCFStringEncodingUTF8
    { "UTF-8 BOM",    "utf-8",        134217984, true },  // BOM state is not recorded
    { "UTF-16LE",     "utf-16le",     2147483905, true },  // kCFStringEncodingUTF16LE (0x80000101)
    { "UTF-16LE BOM", "utf-16le",     2147483905, true },
    { "UTF-16BE",     "utf-16be",     2147483906, true },  // kCFStringEncodingUTF16BE (0x80000102)
    { "UTF-16BE BOM", "utf-16be",     2147483906, true },
    { "UTF-32LE",     "utf-32le",     2147483907, true },  // kCFStringEncodingUTF32LE (0x80000103)
    { "UTF-32BE",     "utf-32be",     2147483908, true },  // kCFStringEncodingUTF32BE (0x80000104)
    { "GB18030",      "gb18030",      1586,      true },   // kCFStringEncodingGB_18030_2000
    { "windows-1252", "windows-1252", 1280,      true },   // kCFStringEncodingWindowsLatin1
};
#endif

QString readSpec(const QString &path)
{
#ifdef Q_OS_MACOS
    // com.apple.TextEncoding is small; size it generously.
    QVarLengthArray<char, 256> buffer(256);
    ssize_t size = getxattr(path.toUtf8().constData(), "com.apple.TextEncoding",
                            buffer.data(), buffer.size(), 0, 0);
    if (size < 0)
        return QString();

    if (size > buffer.size()) {
        buffer.resize(static_cast<int>(size));
        size = getxattr(path.toUtf8().constData(), "com.apple.TextEncoding",
                        buffer.data(), buffer.size(), 0, 0);
        if (size < 0)
            return QString();
    }

    const QByteArray value(buffer.data(), static_cast<int>(size));
    // Value is "<IANA name>;<CFStringEncoding>". Keep the IANA name only.
    const QByteArray iana = value.split(';').first().trimmed();

    const QString spec = ianaToSpec(QString::fromLatin1(iana));
    if (spec.isEmpty())
        return QString();

    // Reject specs Qt cannot actually open, just like detectCharset() does.
    if (!QTextCodec::codecForName(spec.toLatin1()) && !EncodingSpecs::isUnicodeEncoding(spec))
        return QString();

    return spec;
#else
    Q_UNUSED(path);
    return QString();
#endif
}

bool writeSpec(const QString &path, const QString &spec)
{
#ifdef Q_OS_MACOS
    QByteArray iana;
    QByteArray value;

    for (const auto &entry : kSpecToIana) {
        if (spec == QLatin1String(entry.spec)) {
            iana = QByteArray(entry.iana);
            if (entry.hasCf)
                value = iana + ";" + QByteArray::number(entry.cf);
            else
                value = iana;
            break;
        }
    }

    // Encode the menu spelling to its IANA name for the common cases we know.
    if (iana.isEmpty()) {
        if (spec == QLatin1String("GBK"))
            iana = "gbk";
        else if (spec == QLatin1String("GB2312"))
            iana = "gb2312";
        else if (spec == QLatin1String("Big5") || spec == QLatin1String("Big5-HKSCS") || spec == QLatin1String("Big5-ETen"))
            iana = "big5";
        else if (spec.startsWith(QLatin1String("Shift-JIS")))
            iana = "shift_jis";
        else if (spec.startsWith(QLatin1String("EUC-JP")))
            iana = "euc-jp";
        else if (spec.startsWith(QLatin1String("CP949")) || spec == QLatin1String("EUC-KR"))
            iana = "cp949";
        else if (spec.startsWith(QLatin1String("windows-125")))
            iana = spec.toLatin1();
        else if (spec.startsWith(QLatin1String("ISO 8859-")))
            iana = spec.toLatin1().replace(' ', '-');
        else if (spec == QLatin1String("MacRoman"))
            iana = "macintosh";
        else if (spec == QLatin1String("KOI8-R"))
            iana = "koi8-r";
        else if (spec == QLatin1String("KOI8-U"))
            iana = "koi8-u";
        else if (spec == QLatin1String("TIS-620"))
            iana = "tis-620";
        else if (spec == QLatin1String("IBM 850"))
            iana = "ibm850";
        else if (spec == QLatin1String("IBM 866"))
            iana = "ibm866";
        else if (spec == QLatin1String("IBM 874"))
            iana = "ibm874";
    }

    if (iana.isEmpty())
        return false;

    if (value.isEmpty())
        value = iana;

    const int rc = setxattr(path.toUtf8().constData(), "com.apple.TextEncoding",
                            value.constData(), value.size(), 0, 0);
    return rc == 0;
#else
    Q_UNUSED(path);
    Q_UNUSED(spec);
    return false;
#endif
}

} // namespace TextEncodingXattr
