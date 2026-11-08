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

#ifndef TEXTENCODINGXATTR_H
#define TEXTENCODINGXATTR_H

#include <QString>

// Reads and writes the macOS "com.apple.TextEncoding" extended attribute so
// Notepad Next agrees with CotEditor / TextEdit on a file's on-disk encoding.
//
// The attribute value is "<IANA name>;<CFStringEncoding>", e.g. "gb18030;1586".
// Cocoa decodes primarily from the IANA name, so the number is optional; we
// only emit one for the encodings whose CFStringEncoding we know for certain.
namespace TextEncodingXattr {

// Map an IANA charset name (e.g. "gb18030", "iso-8859-1") to an EncodingSpecs
// menu spec, or return an empty string when the encoding is not offered.
QString ianaToSpec(const QString &iana);

// Read com.apple.TextEncoding from a file and return the EncodingSpecs spec, or
// an empty string when the attribute is absent or unrecognised.
QString readSpec(const QString &path);

// Write com.apple.TextEncoding for the given spec. Returns true on success.
// A no-op on non-macOS builds.
bool writeSpec(const QString &path, const QString &spec);

} // namespace TextEncodingXattr

#endif // TEXTENCODINGXATTR_H
