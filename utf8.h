/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#ifndef UTF8_H
#define UTF8_H

/*
 * UTF-8 encoding is used to represent actual Unicode characters
 * as well as the artificial values used as the delimiters of folded
 * blocks.  One of these extended characters can be held in a Unicode_t
 * and passes IS_UNICODE().  True Unicode code points also satisfy
 * IS_CODEPOINT().
 */

#define UNICODE_BAD (1u << 31)
#define IS_UNICODE(u) ((u) < UNICODE_BAD)
#define IS_CODEPOINT(u) ((u) < 0x10000)

/* Some non-Unicode code points are used to represent function keys
 * and input errors.
 */
#define FUNCTION_KEY(x) (UNICODE_BAD + 1 + (x))
#define IS_FUNCTION_KEY(x) ((x) - FUNCTION_KEY(0) < 256)
#define FUNCTION_UP	FUNCTION_KEY(1)
#define FUNCTION_DOWN	FUNCTION_KEY(2)
#define FUNCTION_RIGHT	FUNCTION_KEY(3)
#define FUNCTION_LEFT	FUNCTION_KEY(4)
#define FUNCTION_PGUP	FUNCTION_KEY(5)
#define FUNCTION_PGDOWN	FUNCTION_KEY(6)
#define FUNCTION_HOME	FUNCTION_KEY(7)
#define FUNCTION_END	FUNCTION_KEY(8)
#define FUNCTION_INSERT	FUNCTION_KEY(9)
#define FUNCTION_DELETE	FUNCTION_KEY(10)
#define FUNCTION_F(k)	FUNCTION_KEY(20+(k))
#define FUNCTION_FKEYS	12

#define ERROR_CODE(e)	(FUNCTION_KEY(256) + (e))
#define IS_ERROR_CODE(e) ((e) >= ERROR_CODE(0))
#define ERROR_EOF	ERROR_CODE(1)
#define ERROR_CHANGED	ERROR_CODE(2)	/* must call _get_geometry()! */
#define ERROR_INPUT	ERROR_CODE(3)
#define ERROR_EMPTY	ERROR_CODE(4)	/* no input */

size_t unicode_utf8(char *, Unicode_t);
size_t utf8_length(const char *, size_t max);
size_t utf8_length_backwards(const char *, size_t max);
Unicode_t utf8_unicode(const char *, size_t length);

#define CONTROL(x) ((x)-'@')

/* Huge code point values are used to bracket folded sections. */
#define FOLD_START 0x40000000
#define FOLD_END   0x60000000
#define IS_FOLDED(ch) ((ch) - FOLD_START < FOLD_END-FOLD_START)
#define FOLDED_BYTES(ch) ((ch) & 0x1fffffff)

extern Byte_t utf8_bytes[0x100];

#endif
