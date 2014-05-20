/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file string.h Types and function related to low-level strings.
 *
 * @note Be aware of "dangerous" string functions; string functions that
 * have behaviour that could easily cause buffer overruns and such:
 * - strncpy: does not '\0' terminate when input string is longer than
 *   the size of the output string. Use strecpy instead.
 * - [v]snprintf: returns the length of the string as it would be written
 *   when the output is large enough, so it can be more than the size of
 *   the buffer and than can underflow size_t (uint-ish) which makes all
 *   subsequent snprintf alikes write outside of the buffer. Use
 *   [v]seprintf instead; it will return the number of bytes actually
 *   added so no [v]seprintf will cause outside of bounds writes.
 * - [v]sprintf: does not bounds checking: use [v]seprintf instead.
 */

#ifndef STRING_H
#define STRING_H

#include <stdarg.h>

#include "core/bitmath_func.hpp"
#include "core/enum_type.hpp"

/** A non-breaking space. */
#define NBSP "\xC2\xA0"

/** A left-to-right marker, marks the next character as left-to-right. */
#define LRM "\xE2\x80\x8E"

/**
 * Valid filter types for IsValidChar.
 */
enum CharSetFilter {
	CS_ALPHANUMERAL,      ///< Both numeric and alphabetic and spaces and stuff
	CS_NUMERAL,           ///< Only numeric ones
	CS_NUMERAL_SPACE,     ///< Only numbers and spaces
	CS_ALPHA,             ///< Only alphabetic values
	CS_HEXADECIMAL,       ///< Only hexadecimal characters
};

/** Type for wide characters, i.e. non-UTF8 encoded unicode characters. */
typedef uint32 WChar;

/** Max. length of UTF-8 encoded unicode character. */
static const uint MAX_CHAR_LENGTH = 4;

/* The following are directional formatting codes used to get the LTR and RTL strings right:
 * http://www.unicode.org/unicode/reports/tr9/#Directional_Formatting_Codes */
static const WChar CHAR_TD_LRM = 0x200E; ///< The next character acts like a left-to-right character.
static const WChar CHAR_TD_RLM = 0x200F; ///< The next character acts like a right-to-left character.
static const WChar CHAR_TD_LRE = 0x202A; ///< The following text is embedded left-to-right.
static const WChar CHAR_TD_RLE = 0x202B; ///< The following text is embedded right-to-left.
static const WChar CHAR_TD_LRO = 0x202D; ///< Force the following characters to be treated as left-to-right characters.
static const WChar CHAR_TD_RLO = 0x202E; ///< Force the following characters to be treated as right-to-left characters.
static const WChar CHAR_TD_PDF = 0x202C; ///< Restore the text-direction state to before the last LRE, RLE, LRO or RLO.

/** Settings for the string validation. */
enum StringValidationSettings {
	SVS_NONE                       = 0,      ///< Allow nothing and replace nothing.
	SVS_REPLACE_WITH_QUESTION_MARK = 1 << 0, ///< Replace the unknown/bad bits with question marks.
	SVS_ALLOW_NEWLINE              = 1 << 1, ///< Allow newlines.
	SVS_ALLOW_CONTROL_CODE         = 1 << 2, ///< Allow the special control codes.
};
DECLARE_ENUM_AS_BIT_SET(StringValidationSettings)

void ttd_strlcat(char *dst, const char *src, size_t size);
void ttd_strlcpy(char *dst, const char *src, size_t size);

char *strecat(char *dst, const char *src, const char *last);
char *strecpy(char *dst, const char *src, const char *last);

int CDECL seprintf(char *str, const char *last, const char *format, ...) WARN_FORMAT(3, 4);

char *CDECL str_fmt(const char *str, ...) WARN_FORMAT(1, 2);

void str_validate(char *str, const char *last, StringValidationSettings settings = SVS_REPLACE_WITH_QUESTION_MARK);
void ValidateString(const char *str);

void str_fix_scc_encoded(char *str, const char *last);
void str_strip_colours(char *str);
bool strtolower(char *str);

bool StrValid(const char *str, const char *last);

/**
 * Check if a string buffer is empty.
 *
 * @param s The pointer to the first element of the buffer
 * @return true if the buffer starts with the terminating null-character or
 *         if the given pointer points to NULL else return false
 */
static inline bool StrEmpty(const char *s)
{
	return s == NULL || s[0] == '\0';
}

/**
 * Get the length of a string, within a limited buffer.
 *
 * @param str The pointer to the first element of the buffer
 * @param maxlen The maximum size of the buffer
 * @return The length of the string
 */
static inline size_t ttd_strnlen(const char *str, size_t maxlen)
{
	const char *t;
	for (t = str; (size_t)(t - str) < maxlen && *t != '\0'; t++) {}
	return t - str;
}

char *md5sumToString(char *buf, const char *last, const uint8 md5sum[16]);

/** Convert the md5sum to a hexadecimal string representation, pointer version. */
template <uint N>
static inline void md5sumToString (char (*buf) [N], const uint8 md5sum [16])
{
	assert_tcompile (N > 2 * 16);
	md5sumToString (&(*buf)[0], &(*buf)[N - 1], md5sum);
}

/** Convert the md5sum to a hexadecimal string representation, reference version. */
template <uint N>
static inline void md5sumToString (char (&buf) [N], const uint8 md5sum [16])
{
	md5sumToString (&buf, md5sum);
}

bool IsValidChar(WChar key, CharSetFilter afilter);

size_t Utf8Decode(WChar *c, const char *s);
size_t Utf8Encode(char *buf, WChar c);
size_t Utf8TrimString(char *s, size_t maxlen);


static inline WChar Utf8Consume(const char **s)
{
	WChar c;
	*s += Utf8Decode(&c, *s);
	return c;
}

/**
 * Return the length of a UTF-8 encoded character.
 * @param c Unicode character.
 * @return Length of UTF-8 encoding for character.
 */
static inline int8 Utf8CharLen(WChar c)
{
	if (c < 0x80)       return 1;
	if (c < 0x800)      return 2;
	if (c < 0x10000)    return 3;
	if (c < 0x110000)   return 4;

	/* Invalid valid, we encode as a '?' */
	return 1;
}


/**
 * Return the length of an UTF-8 encoded value based on a single char. This
 * char should be the first byte of the UTF-8 encoding. If not, or encoding
 * is invalid, return value is 0
 * @param c char to query length of
 * @return requested size
 */
static inline int8 Utf8EncodedCharLen(char c)
{
	if (GB(c, 3, 5) == 0x1E) return 4;
	if (GB(c, 4, 4) == 0x0E) return 3;
	if (GB(c, 5, 3) == 0x06) return 2;
	if (GB(c, 7, 1) == 0x00) return 1;

	/* Invalid UTF8 start encoding */
	return 0;
}


/* Check if the given character is part of a UTF8 sequence */
static inline bool IsUtf8Part(char c)
{
	return GB(c, 6, 2) == 2;
}

/**
 * Retrieve the previous UNICODE character in an UTF-8 encoded string.
 * @param s char pointer pointing to (the first char of) the next character
 * @return a pointer in 's' to the previous UNICODE character's first byte
 * @note The function should not be used to determine the length of the previous
 * encoded char because it might be an invalid/corrupt start-sequence
 */
static inline char *Utf8PrevChar(char *s)
{
	char *ret = s;
	while (IsUtf8Part(*--ret)) {}
	return ret;
}

static inline const char *Utf8PrevChar(const char *s)
{
	const char *ret = s;
	while (IsUtf8Part(*--ret)) {}
	return ret;
}

size_t Utf8StringLength(const char *s);

/**
 * Is the given character a lead surrogate code point?
 * @param c The character to test.
 * @return True if the character is a lead surrogate code point.
 */
static inline bool Utf16IsLeadSurrogate(uint c)
{
	return c >= 0xD800 && c <= 0xDBFF;
}

/**
 * Is the given character a lead surrogate code point?
 * @param c The character to test.
 * @return True if the character is a lead surrogate code point.
 */
static inline bool Utf16IsTrailSurrogate(uint c)
{
	return c >= 0xDC00 && c <= 0xDFFF;
}

/**
 * Convert an UTF-16 surrogate pair to the corresponding Unicode character.
 * @param lead Lead surrogate code point.
 * @param trail Trail surrogate code point.
 * @return Decoded Unicode character.
 */
static inline WChar Utf16DecodeSurrogate(uint lead, uint trail)
{
	return 0x10000 + (((lead - 0xD800) << 10) | (trail - 0xDC00));
}

/**
 * Decode an UTF-16 character.
 * @param c Pointer to one or two UTF-16 code points.
 * @return Decoded Unicode character.
 */
static inline WChar Utf16DecodeChar(const uint16 *c)
{
	if (Utf16IsLeadSurrogate(c[0])) {
		return Utf16DecodeSurrogate(c[0], c[1]);
	} else {
		return *c;
	}
}

/**
 * Is the given character a text direction character.
 * @param c The character to test.
 * @return true iff the character is used to influence
 *         the text direction.
 */
static inline bool IsTextDirectionChar(WChar c)
{
	switch (c) {
		case CHAR_TD_LRM:
		case CHAR_TD_RLM:
		case CHAR_TD_LRE:
		case CHAR_TD_RLE:
		case CHAR_TD_LRO:
		case CHAR_TD_RLO:
		case CHAR_TD_PDF:
			return true;

		default:
			return false;
	}
}

static inline bool IsPrintable(WChar c)
{
	if (c < 0x20)   return false;
	if (c < 0xE000) return true;
	if (c < 0xE200) return false;
	return true;
}

/**
 * Check whether UNICODE character is whitespace or not, i.e. whether
 * this is a potential line-break character.
 * @param c UNICODE character to check
 * @return a boolean value whether 'c' is a whitespace character or not
 * @see http://www.fileformat.info/info/unicode/category/Zs/list.htm
 */
static inline bool IsWhitespace(WChar c)
{
	return c == 0x0020 /* SPACE */ || c == 0x3000; /* IDEOGRAPHIC SPACE */
}

/* Needed for NetBSD version (so feature) testing */
#if defined(__NetBSD__) || defined(__FreeBSD__)
#include <sys/param.h>
#endif

/* strndup is a GNU extension */
#if defined(_GNU_SOURCE) || (defined(__NetBSD_Version__) && 400000000 <= __NetBSD_Version__) || (defined(__FreeBSD_version) && 701101 <= __FreeBSD_version) || (defined(__DARWIN_C_LEVEL) && __DARWIN_C_LEVEL >= 200809L)
#	undef DEFINE_STRNDUP
#else
#	define DEFINE_STRNDUP
char *strndup(const char *s, size_t len);
#endif /* strndup is available */

/* strcasestr is available for _GNU_SOURCE, BSD and some Apple */
#if defined(_GNU_SOURCE) || (defined(__BSD_VISIBLE) && __BSD_VISIBLE) || (defined(__APPLE__) && (!defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE))) || defined(_NETBSD_SOURCE)
#	undef DEFINE_STRCASESTR
#else
#	define DEFINE_STRCASESTR
char *strcasestr(const char *haystack, const char *needle);
#endif /* strcasestr is available */

int strnatcmp(const char *s1, const char *s2, bool ignore_garbage_at_front = false);


/* buffer-aware string functions */

/** Copy a string, pointer version. */
template <uint N>
static inline void bstrcpy (char (*dest) [N], const char *src)
{
	snprintf (&(*dest)[0], N, "%s", src);
}

/** Copy a string, reference version. */
template <uint N>
static inline void bstrcpy (char (&dest) [N], const char *src)
{
	bstrcpy (&dest, src);
}

/** Format a string from a va_list, pointer version. */
template <uint N>
static inline void bstrvfmt (char (*dest) [N], const char *fmt, va_list args)
{
	vsnprintf (&(*dest)[0], N, fmt, args);
}

/** Format a string from a va_list, reference version. */
template <uint N>
static inline void bstrvfmt (char (&dest) [N], const char *fmt, va_list args)
{
	bstrvfmt (&dest, fmt, args);
}

/* The following one must be a macro because there is no variadic template
 * support in MSVC. */

/** Get the pointer and size to use for a static buffer, pointer version. */
template <uint N>
static inline void bstrptr (char (*dest) [N], char **buffer, uint *size)
{
	*buffer = &(*dest)[0];
	*size = N;
}

/** Get the pointer and size to use for a static buffer, reference version. */
template <uint N>
static inline void bstrptr (char (&dest) [N], char **buffer, uint *size)
{
	*buffer = &(dest)[0];
	*size = N;
}

/** Format a string. */
#define bstrfmt(dest, ...) do {                                 \
	char *bstrfmt__buffer;                                  \
	uint  bstrfmt__size;                                    \
	bstrptr (dest, &bstrfmt__buffer, &bstrfmt__size);       \
	snprintf (bstrfmt__buffer, bstrfmt__size, __VA_ARGS__); \
	} while(0)

#endif /* STRING_H */
