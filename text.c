#include "all.h"
#include <sys/mman.h>

/*
 *	A text is a container for the content of a file or
 *	an internal scratch buffer.  It has a buffer, which
 *	contains the actual bytes of the text, an "undo" history
 *	buffer, and one or more views, which each present part or
 *	all of the text to the user for editing.  The positions of
 *	the cursor, mark, and other "loci" are all local to a view.
 */

struct text *text_list;

struct view *view_find(const char *name)
{
	struct view *view;
	struct text *text;

	if (!name)
		name = "";
	for (text = text_list; text; text = text->next)
		for (view = text->views; view; view = view->next)
			if (view->name && !strcmp(view->name, name))
				return view;
	return NULL;
}

void view_name(struct view *view)
{
	int j, len;
	const char *name = view->text->path;
	char *new, *p;

	if (!name)
		name = "";
	len = strlen(name);
	new = allocate(NULL, len + 8);
	if (view->text->flags & TEXT_EDITOR) {
		memcpy(new, name, len+1);
		p = new + len;
	} else {
		for (j = 0, p = new; j < len; j++) {
			unsigned ch = name[j];
			if (ch >= 'a' && ch <= 'z' ||
			    ch >= 'A' && ch <= 'Z' ||
			    ch >= '0' && ch <= '9' ||
			    ch == '_' || ch == '-' || ch == '+' ||
			    ch == '.' || ch == ',' ||
			    ch >= 0x80)
				*p++ = ch;
			else if (ch == '/' && j < len-1)
				p = new;
		}
		*p = '\0';
	}

	if (view_find(new))
		for (j = 2; ; j++) {
			sprintf(p, "<%d>", j);
			if (!view_find(new))
				break;
		}

	allocate(view->name, 0);
	view->name = new;
}

struct view *view_create(struct text *text)
{
	struct view *view = allocate(NULL, sizeof *view);
	memset(view, 0, sizeof *view);
	view->loci = DEFAULT_LOCI;
	view->locus = allocate(NULL, view->loci * sizeof *view->locus);
	memset(view->locus, UNSET, view->loci * sizeof *view->locus);
	view->locus[CURSOR] = 0;
	view->text = text;
	view->next = text->views;
	text->views = view;
	view->bytes = text->buffer ? buffer_bytes(text->buffer) :
			text->clean ? text->clean_bytes :0;
	view->mode = mode_default();
	view_name(view);
	return view;
}

struct view *text_create(const char *path, unsigned flags)
{
	struct text *text = allocate(NULL, sizeof *text), *prev, *bp;
	memset(text, 0, sizeof *text);
	text->tabstop = 8;
	text->fd = -1;
	text->flags = flags;
	text->path = strdup(path);
	for (prev = NULL, bp = text_list; bp; prev = bp, bp = bp->next)
		;
	if (prev)
		prev->next = text;
	else
		text_list = text;
	return view_create(text);
}

static void text_close(struct text *text)
{
	struct text *prev = NULL, *bp;

	for (bp = text_list; bp; prev = bp, bp = bp->next)
		if (bp == text) {
			if (prev)
				prev->next = text->next;
			else
				text_list = text->next;
			break;
		}

	if (text->clean)
		munmap(text->clean, text->clean_bytes);
	buffer_destroy(text->buffer);
	text_forget_undo(text);
	if (text->fd >= 0)
		close(text->fd);
	allocate(text->path, 0);
	allocate(text, 0);
}

void view_close(struct view *view)
{
	struct view *vp, *prev = NULL;
	struct text *text = view->text;

	if (!view)
		return;

	window_unmap(view);
	if (text)
		for (vp = text->views; vp; prev = vp, vp = vp->next)
			if (vp == view) {
				if (prev)
					prev->next = vp->next;
				else if (!(text->views = vp->next))
					text_close(text);
				break;
			}

	allocate(view->name, 0);
	allocate(view->last_search, 0);
	allocate(view->macro, 0);
	allocate(view, 0);
}

struct view *view_selection(struct view *current, unsigned offset, unsigned bytes)
{
	struct view *view;

	if (!current)
		return NULL;
	view = view_create(current->text);

	if (offset > current->bytes)
		offset = current->bytes;
	if (offset + bytes > current->bytes)
		bytes = current->bytes - offset;
	view->start = current->start + offset;
	view->bytes = bytes;
	locus_set(view, CURSOR, 0);
	return view;
}

void text_adjust_loci(struct text *text, unsigned offset, int delta)
{
	struct view *view;

	if (!delta)
		return;

	if (delta < 0) {
		unsigned limit = offset - delta;
		for (view = text->views; view; view = view->next)
			if (limit < view->start)
				view->start += delta;
			else if (offset < view->start) {
				unsigned loss = limit - view->start;
				if (loss > view->bytes)
					loss = view->bytes;
				view->start = offset;
				view->bytes -= loss;
				loci_adjust(view, 0, -loss);
			} else if (offset < view->start + view->bytes) {
				unsigned loss = view->start + view->bytes -
						offset;
				if (loss > -delta)
					loss = -delta;
				view->bytes -= loss;
				loci_adjust(view, offset - view->start, -loss);
			}
	} else
		for (view = text->views; view; view = view->next)
			if (offset < view->start)
				view->start += delta;
			else if (offset <= view->start + view->bytes) {
				view->bytes += delta;
				loci_adjust(view, offset - view->start, delta);
			}
}

unsigned view_get(struct view *view, void *out, unsigned offset, unsigned bytes)
{
	struct text *text = view->text;
	offset += view->start;
	if (text->buffer)
		return buffer_get(text->buffer, out, offset, bytes);
	if (!text->clean || offset >= text->clean_bytes)
		return 0;
	if (offset + bytes > text->clean_bytes)
		bytes = text->clean_bytes - offset;
	memcpy(out, text->clean + offset, bytes);
	return bytes;
}

unsigned view_raw(struct view *view, char **out, unsigned offset,
		  unsigned bytes)
{
	struct text *text = view->text;
	offset += view->start;
	if (text->buffer)
		return buffer_raw(text->buffer, out, offset, bytes);
	if (!text->clean || offset >= text->clean_bytes)
		return 0;
	if (offset + bytes > text->clean_bytes)
		bytes = text->clean_bytes - offset;
	*out = text->clean + offset;
	return bytes;
}

unsigned view_delete(struct view *view, unsigned offset, unsigned bytes)
{
	return text_delete(view->text, view->start + offset, bytes);
}

unsigned view_insert(struct view *view, const void *in,
		     unsigned offset, int bytes)
{
	if (bytes < 0)
		bytes = in ? strlen(in) : 0;
	return text_insert(view->text, in, view->start + offset, bytes);
}

int view_getch(struct view *view)
{
	int ch;

	if (view->macro && view->macro_at < view->macro_bytes)
		return view->macro[view->macro_at++];
	ch = window_getch();
	if (ch >= 0 &&
	    view->macro &&
	    view->macro_at == view->macro_bytes+1) {
		/* record */
		if (view->macro_bytes == view->macro_alloc)
			view->macro = allocate(view->macro, view->macro_alloc += 64);
		view->macro[view->macro_bytes++] = ch;
		view->macro_at++;
	}
	return ch;
}
