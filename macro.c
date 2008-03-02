#include "all.h"

struct macro {
	int start, bytes, at, repeat;
	struct macro *next, *suspended;
};

struct macro *function_key[FUNCTION_KEYS+1];

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

int macro_end_recording(unsigned chop)
{
	char *raw;
	unsigned n;

	if (!recording)
		return 0;
	n = buffer_raw(macbuf, &raw, recording->start,
		       recording->bytes);
	while (n) {
		unsigned lastlen = utf8_length_backwards(raw+n-1, n);
		n -= lastlen;
		if (utf8_unicode(raw+n, lastlen) == chop)
			break;
	}
	buffer_delete(macbuf, recording->start + n, recording->bytes - n);
	recording->bytes = n;
	recording->at = recording->bytes; /* not playing */
	recording = NULL;
	return 1;
}

static int macro_is_playing(struct macro *macro)
{
	return macro && macro->at < macro->bytes;
}

int macro_play(struct macro *macro, int repeat)
{
	if (!macro ||
	    macro_is_playing(macro) ||
	    !macro->bytes)
		return 0;
	macro->suspended = playing;
	macro->at = 0;
	macro->repeat = repeat;
	playing = macro;
	return 1;
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
	allocate(macro, 0);
}

int macro_getch(void)
{
	int ch;

	if (playing) {
		char *p;
		unsigned n = buffer_raw(macbuf, &p, playing->start + playing->at,
					playing->bytes - playing->at);
		ch = utf8_unicode(p, n = utf8_length(p, n));
		if ((playing->at += n) == playing->bytes)
			if (playing->repeat-- > 0)
				playing->at = 0;
			else
				playing = playing->suspended;
	} else {
		ch = window_getch();
		if (ch >= 0 && recording) {
			char buf[8];
			int n = utf8_out(buf, ch);
			buffer_insert(macbuf, buf,
				      recording->start + recording->bytes,
				      n);
			recording->bytes += n;
		}
	}
	return ch;
}
