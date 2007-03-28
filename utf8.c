#include "utf8.h"

int utf8_out(char *out, unsigned unicode)
{
	char *p = out;

	if (!(unicode >> 7))
		*p++ = unicode;
	else {
		int n;
		for (n = 1; n < 5; n++)
			if (!(unicode >> 6 + 5*n))
				break;
		*p++ = 0xfc << 5-n | unicode >> 6*n;
		while (n--)
			*p++ = 0x80 | unicode >> 6*n & 0x3f;
	}

	return p - out;
}

unsigned utf8_length(const char *in, unsigned max)
{
	int n;
	const unsigned char *p = (const unsigned char *) in;

	if ((*p & 0xc0) != 0xc0)
		return 1;
	if (max > 6)
		max = 6;
	for (n = 1; n < max; n++) {
		if (p[n] & 0xc0 != 0x80)
			return 1;
		if (!((unsigned char)(*p ^ 0xfc << 5-n) >> 5-n))
			return n+1;
	}
	return 1;
}

unsigned utf8_length_backwards(const char *in, unsigned max)
{
	int n;
	const unsigned char *p = (const unsigned char *) in;

	if ((*p & 0xc0) != 0x80)
		return 1;
	if (max > 6)
		max = 6;
	for (n = 1; n < max; n++)
		if ((p[-n] & 0xc0) != 0x80)
			break;
	if (!((unsigned char)(p[-n] ^ 0xfc << 5-n) >> 5-n))
		return n+1;
	return 1;
}

unsigned utf8_unicode(const char *in, unsigned length)
{
	const unsigned char *p = (const unsigned char *) in;
	unsigned unicode;

	if (length <= 1 || length > 6)
		return *p;
	unicode = *p & (1 << 7-length)-1;
	while (--length)
		unicode <<= 6, unicode |= *++p & 0x3f;
	return unicode;
}
