/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

struct edit {
	position_t offset;
	ssize_t bytes; /* negative means "inserted" */
};

struct undo {
	struct buffer *edits, *deleted;
	position_t redo, saved;
};

static struct edit *last_edit(struct text *text)
{
	char *raw = NULL;

	if (text->undo &&
	    text->undo->redo &&
	    text->undo->redo == buffer_bytes(text->undo->edits))
		buffer_raw(text->undo->edits, &raw,
			   text->undo->redo - sizeof(struct edit),
			   sizeof(struct edit));
	return (struct edit *) raw;
}

static void resume_editing(struct text *text)
{
	if (!text->undo) {
		text->undo = allocate0(sizeof *text->undo);
		text->undo->edits = buffer_create(NULL);
		text->undo->deleted = buffer_create(NULL);
	}
	buffer_delete(text->undo->edits, text->undo->redo,
		      buffer_bytes(text->undo->edits) - text->undo->redo);
	buffer_delete(text->undo->deleted, text->undo->saved,
		      buffer_bytes(text->undo->deleted) - text->undo->saved);
}

static Boolean_t in_view(struct view *view, position_t *offset, size_t *bytes)
{
	if (*offset < view->start) {
		if (*offset + *bytes <= view->start)
			return FALSE;
		*bytes -= view->start - *offset;
		*offset = FALSE;
	} else {
		*offset -= view->start;
		if (*offset >= view->bytes)
			return FALSE;
		if (*offset + *bytes > view->bytes)
			*bytes = view->bytes - *offset;
	}
	return TRUE;
}

static void view_hint_deleting(struct view *view, position_t offset,
			       size_t bytes)
{
	if (!view->window || !in_view(view, &offset, &bytes))
		return;
	window_hint_deleting(view->window, offset, bytes);
}

static void view_hint_inserted(struct view *view, position_t offset,
			       size_t bytes)
{
	if (!view->window || !in_view(view, &offset, &bytes))
		return;
	window_hint_inserted(view->window, offset, bytes);
}

size_t text_delete(struct text *text, position_t offset, size_t bytes)
{
	char *old;
	struct edit edit, *last;
	struct view *view;

	if (!bytes)
		return 0;
	text_dirty(text);
	edit.offset = offset;
	edit.bytes = buffer_raw(text->buffer, &old, offset, bytes);
	bytes = edit.bytes;

	if ((last = last_edit(text)) &&
	    last->bytes >= 0 &&
	    last->offset == offset)
		last->bytes += bytes;
	else {
		resume_editing(text);
		buffer_insert(text->undo->edits, &edit,
			      text->undo->redo, sizeof edit);
		text->undo->redo += sizeof edit;
	}
	for (view = text->views; view; view = view->next)
		view_hint_deleting(view, offset, bytes);
	buffer_move(text->undo->deleted, text->undo->saved,
		    text->buffer, offset, bytes);
	text->undo->saved += bytes;
	text_adjust_loci(text, offset, -bytes);
	return bytes;
}

size_t text_insert(struct text *text, const void *in,
		   position_t offset, size_t bytes)
{
	struct edit edit, *last;
	struct view *view;

	if (!bytes)
		return 0;
	text_dirty(text);
	bytes = buffer_insert(text->buffer, in, offset, bytes);
	if ((last = last_edit(text)) &&
	    last->bytes < 0 &&
	    last->offset - last->bytes == offset)
		last->bytes -= bytes;
	else {
		resume_editing(text);
		edit.offset = offset;
		edit.bytes = -bytes;
		buffer_insert(text->undo->edits, &edit, text->undo->redo,
			      sizeof edit);
		text->undo->redo += sizeof edit;
	}
	text_adjust_loci(text, offset, bytes);
	for (view = text->views; view; view = view->next)
		view_hint_inserted(view, offset, bytes);
	return bytes;
}

sposition_t text_undo(struct text *text)
{
	char *raw;
	struct edit *edit;

	if (!text->undo || !text->undo->redo)
		return -1;
	text_dirty(text);
	buffer_raw(text->undo->edits, &raw, text->undo->redo -= sizeof *edit,
		   sizeof *edit);
	edit = (struct edit *) raw;
	if (edit->bytes >= 0)
		buffer_move(text->buffer, edit->offset, text->undo->deleted,
			    text->undo->saved -= edit->bytes, edit->bytes);
	else
		buffer_move(text->undo->deleted, text->undo->saved,
			    text->buffer, edit->offset, -edit->bytes);
	text_adjust_loci(text, edit->offset, edit->bytes);
	return edit->offset;
}

sposition_t text_redo(struct text *text)
{
	char *raw;
	struct edit *edit;

	if (!text->undo ||
	    text->undo->redo == buffer_bytes(text->undo->edits))
		return -1;
	text_dirty(text);
	buffer_raw(text->undo->edits, &raw, text->undo->redo, sizeof *edit);
	edit = (struct edit *) raw;
	text->undo->redo += sizeof *edit;
	if (edit->bytes >= 0) {
		buffer_move(text->undo->deleted, text->undo->saved,
			    text->buffer, edit->offset, edit->bytes);
		text->undo->saved += edit->bytes;
	} else
		buffer_move(text->buffer, edit->offset, text->undo->deleted,
			    text->undo->saved, -edit->bytes);
	text_adjust_loci(text, edit->offset, -edit->bytes);
	return edit->offset;
}

void text_forget_undo(struct text *text)
{
	if (text->undo) {
		buffer_destroy(text->undo->edits);
		buffer_destroy(text->undo->deleted);
		RELEASE(text->undo);
	}
}
