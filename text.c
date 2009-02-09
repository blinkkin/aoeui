/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

/*
 *	A text is a container for the content of a file or
 *	an internal scratch buffer.  It has a buffer, which
 *	contains the actual bytes of the text, an "undo" history
 *	buffer, and one or more views, which each present part or
 *	all of the text to the user for editing.  The positions of
 *	the cursor, mark, and other "loci" are all local to a view.
 */

struct text *text_list;
unsigned default_tab_stop = 8; /* the only correct value :-) */
Boolean_t default_no_tabs;

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
	new = allocate(len + 8);
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

	RELEASE(view->name);
	view->name = new;
}

struct view *view_create(struct text *text)
{
	struct view *view = allocate0(sizeof *view);
	view->loci = DEFAULT_LOCI;
	view->locus = allocate(view->loci * sizeof *view->locus);
	memset(view->locus, UNSET, view->loci * sizeof *view->locus);
	view->locus[CURSOR] = 0;
	view->text = text;
	view->next = text->views;
	text->views = view;
	view->bytes = text->buffer ? buffer_bytes(text->buffer) :
			text->clean ? text->clean_bytes :0;
	view->mode = mode_default();
	view->shell_std_in = -1;
	view->shell_pg = -1;
	view->shell_out_locus = NO_LOCUS;
	view_name(view);
	return view;
}

struct view *text_create(const char *path, unsigned flags)
{
	struct text *text = allocate0(sizeof *text), *prev, *bp;

	if (default_no_tabs)
		flags |= TEXT_NO_TABS;
	text->tabstop = default_tab_stop;
	text->fd = -1;
	text->flags = flags;
	text->path = strdup(path);
	keyword_init(text);

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
	if (text->flags & (TEXT_SCRATCH | TEXT_CREATED))
		unlink(text->path);
	RELEASE(text->path);
	RELEASE(text);
}

void view_close(struct view *view)
{
	struct view *vp, *prev = NULL;
	struct text *text;

	if (!view)
		return;
	text = view->text;
	window_destroy(view->window);
	demultiplex_view(view);
	bookmark_unset_view(view);

	if (text)
		for (vp = text->views; vp; prev = vp, vp = vp->next)
			if (vp == view) {
				if (prev)
					prev->next = vp->next;
				else if (!(text->views = vp->next))
					text_close(text);
				break;
			}

	RELEASE(view->name);
	RELEASE(view);
}

struct view *view_selection(struct view *current, position_t offset,
			    size_t bytes)
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

void text_adjust_loci(struct text *text, position_t offset, int delta)
{
	struct view *view;

	if (!delta)
		return;

	if (delta < 0) {
		position_t limit = offset - delta;
		for (view = text->views; view; view = view->next)
			if (limit < view->start)
				view->start += delta;
			else if (offset < view->start) {
				size_t loss = limit - view->start;
				if (loss > view->bytes)
					loss = view->bytes;
				view->start = offset;
				view->bytes -= loss;
				loci_adjust(view, 0, -loss);
			} else if (offset < view->start + view->bytes) {
				size_t loss = view->start + view->bytes -
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

static size_t text_get(struct text *text, void *out, position_t offset,
		       size_t bytes)
{
	if (text->buffer)
		return buffer_get(text->buffer, out, offset, bytes);
	if (!text->clean || offset >= text->clean_bytes)
		return 0;
	if (offset + bytes > text->clean_bytes)
		bytes = text->clean_bytes - offset;
	memcpy(out, text->clean + offset, bytes);
	return bytes;
}

size_t view_get(struct view *view, void *out, position_t offset, size_t bytes)
{
	if (offset >= view->bytes)
		return 0;
	if (offset + bytes > view->bytes)
		bytes = view->bytes - offset;
	return text_get(view->text, out, view->start + offset, bytes);
}

static size_t text_raw(struct text *text, char **out, position_t offset,
		       size_t bytes)
{
	if (text->buffer)
		return buffer_raw(text->buffer, out, offset, bytes);
	if (!text->clean) {
		*out = NULL;
		return 0;
	}
	if (offset + bytes > text->clean_bytes)
		bytes = text->clean_bytes - offset;
	*out = bytes ? text->clean + offset : NULL;
	return bytes;
}

size_t view_raw(struct view *view, char **out, position_t offset, size_t bytes)
{
	if (offset >= view->bytes) {
		*out = NULL;
		return 0;
	}
	if (offset + bytes > view->bytes)
		bytes = view->bytes - offset;
	return text_raw(view->text, out, view->start + offset, bytes);
}

size_t view_delete(struct view *view, position_t offset, size_t bytes)
{
	return text_delete(view->text, view->start + offset, bytes);
}

size_t view_insert(struct view *view, const void *in,
		   position_t offset, ssize_t bytes)
{
	if (bytes < 0)
		bytes = in ? strlen(in) : 0;
	return text_insert(view->text, in, view->start + offset, bytes);
}
