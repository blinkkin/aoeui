#include "all.h"

struct macro {
	int start, bytes, at;
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
	new->suspended = recording;
	new->start = buffer_bytes(macbuf);
	return macros = recording = new;
}

static void adjust(unsigned past, int delta, struct macro *not)
{
	struct macro *mac;
	for (mac = macros; mac; mac = mac->next)
		if (mac != not && mac->start >= past)
			mac->start += delta;
}

int macro_end_recording(unsigned chop)
{
	if (!recording)
		return 0;
	if (chop > recording->bytes)
		chop = recording->bytes;
	adjust(recording->start + recording->bytes, -chop, recording);
	buffer_delete(macbuf, recording->start + recording->bytes - chop, chop);
	recording->bytes -= chop;
	recording->at = recording->bytes; /* not playing */
	recording = recording->suspended;
	return 1;
}

static int macro_is_playing(struct macro *macro)
{
	return macro && macro->at < macro->bytes;
}

int macro_play(struct macro *macro)
{
	if (!macro ||
	    recording ||
	    macro_is_playing(macro) ||
	    !macro->bytes)
		return 0;
	macro->suspended = playing;
	macro->at = 0;
	playing = macro;
	return 1;
}

void macro_free(struct macro *macro)
{
	struct macro *previous = NULL, *mac, *next;
	if (!macro)
		return;
	if (recording)
		recording = NULL;
	else if (macro_is_playing(macro))
		for (; playing; playing = playing->suspended)
			playing->at = playing->bytes;
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
	adjust(macro->start + macro->bytes, -macro->bytes, macro);
	buffer_delete(macbuf, macro->start, macro->bytes);
	allocate(macro, 0);
}

int macro_getch(void)
{
	int ch;

	if (playing) {
		ch = buffer_byte(macbuf, playing->start + playing->at++);
		if (playing->at == playing->bytes)
			playing = playing->suspended;
	} else {
		ch = window_getch();
		if (ch >= 0 && recording) {
			char rch = ch;
			adjust(recording->start + recording->bytes, 1,
			       recording);
			buffer_insert(macbuf, &rch,
				      recording->start + recording->bytes++,
				      1);
		}
	}
	return ch;
}
