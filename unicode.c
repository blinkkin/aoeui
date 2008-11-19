/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "types.h"
#include "utf8.h"

int main(int argc, const char *argv[])
{
	Unicode_t ch = 0x203b;
	unsigned num = 1, at = 0;
	char buf[8];
	if (argc > 1)
		ch = strtoul(argv[1], NULL, 16);
	if (argc > 2)
		num = strtoul(argv[2], NULL, 0);
	while (num--) {
		if (!at)
			printf("0x%04x", ch);
		buf[unicode_utf8(buf, ch++)] = '\0';
		printf("\t%s", buf);
		if (++at == 8) {
			at = 0;
			putchar('\n');
		}
	}
	if (at)
		putchar('\n');
	return EXIT_SUCCESS;
}
