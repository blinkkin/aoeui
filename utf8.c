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

unsigned char utf8_bytes[0x100] = {
	/* 00-7f are themselves */
/*00*/	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/*10*/	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/*20*/	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/*30*/	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/*40*/	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/*50*/	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/*60*/	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/*70*/	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* 80-bf are later bytes, out-of-sync if first */
/*80*/	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/*90*/	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/*a0*/	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/*b0*/	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	/* c0-df are first byte of two-byte sequences (5+6=11 bits) */
	/* c0-c1 are noncanonical */
/*c0*/	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
/*d0*/	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	/* e0-ef are first byte of three-byte (4+6+6=16 bits) */
	/* e0 80-9f are noncanonical */
/*e0*/	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	/* f0-f7 are first byte of four-byte (3+6+6+6=21 bits) */
	/* f0 80-8f are noncanonical */
/*f0*/	4, 4, 4, 4, 4, 4, 4, 4,
	/* f8-fb are first byte of five-byte (2+6+6+6+6+6=26 bits) */
	/* f8 80-87 are noncanonical */
/*f8*/	5, 5, 5, 5,
	/* fc-fd are first byte of six-byte (1+6+6+6+6+6+6=31 bits) */
	/* fc 80-83 are noncanonical */
/*fc*/	6, 6,
	/* fe and ff are not part of valid UTF-8 so they stand alone */
/*fe*/	1, 1
};

unsigned utf8_length(const char *in, unsigned max)
{
	const unsigned char *p = (const unsigned char *) in;
	int n = utf8_bytes[*p];

	if (max > n)
		max = n;
	if (max < n)
		return 1;
	for (n = 1; n < max; n++)
		if ((p[n] & 0xc0) != 0x80)
			return 1;
	return max;
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
	if (utf8_bytes[p[-n]] == n+1)
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
