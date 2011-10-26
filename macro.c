/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

struct macro {
	position_t start, at;
	size_t bytes;
	int repeat;
	struct macro *next, *suspended;
};

static struct buffer *macbuf;
static struct macro *macros;
static struct macro *recording, *playing;

struct macro *macro_record(void)
{
	struct macro *new;
	if (!macbuf)
		macbuf = buffer_create(NULL);
	new = allocate0(sizeof *new);
	new->next = macros;
	new->start = buffer_bytes(macbuf);
	return macros = recording = new;
}

Boolean_t macro_end_recording(Unicode_t chop)
{
	char *raw;
	size_t n;

	if (!recording)
		return FALSE;
	n = buffer_raw(macbuf, &raw, recording->start,
		       recording->bytes);
	while (n) {
		size_t lastlen = utf8_length_backwards(raw+n-1, n);
		n -= lastlen;
		if (utf8_unicode(raw+n, lastlen) == chop)
			break;
	}
	buffer_delete(macbuf, recording->start + n, recording->bytes - n);
	recording->bytes = n;
	recording->at = recording->bytes; /* not playing */
	recording = NULL;
	return TRUE;
}

static Boolean_t macro_is_playing(struct macro *macro)
{
	return macro && macro->at < macro->bytes;
}

Boolean_t macro_play(struct macro *macro, int repeat)
{
	if (!macro ||
	    macro_is_playing(macro) ||
	    !macro->bytes)
		return FALSE;
	macro->suspended = playing;
	macro->at = 0;
	macro->repeat = repeat;
	playing = macro;
	return TRUE;
}

void macros_abort(void)
{
	for (; playing; playing = playing->suspended)
		playing->at = playing->bytes;
}

void macro_free(struct macro *macro)
{
	struct macro *previous = NULL, *mac, *next;
	if (!macro)
		return;
	if (recording)
		recording = NULL;
	else if (macro_is_playing(macro))
		macros_abort();
	for (mac = macros; mac != macro; previous = mac, mac = next) {
		next = mac->next;
		if (mac == macro)
			if (previous)
				previous->next = next;
			else
				macros = next;
		else if (mac->start > macro->start)
			mac->start -= macro->bytes;
	}
	buffer_delete(macbuf, macro->start, macro->bytes);
	RELEASE(macro);
}

Unicode_t macro_getch(void)
{
	Unicode_t ch;

	if (playing) {
		char *p;
		size_t n = buffer_raw(macbuf, &p, playing->start + playing->at,
				      playing->bytes - playing->at);
		ch = utf8_unicode(p, n = utf8_length(p, n));
		if ((playing->at += n) == playing->bytes)
			if (playing->repeat-- > 1)
				playing->at = 0;
			else
				playing = playing->suspended;
	} else {
		ch = window_getch();
		if (!IS_ERROR_CODE(ch) && recording) {
			char buf[8];
			size_t n = unicode_utf8(buf, ch);
			buffer_insert(macbuf, buf,
				      recording->start + recording->bytes,
				      n);
			recording->bytes += n;
		}
	}
	return ch;
}
