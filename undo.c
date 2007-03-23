#include "all.h"

struct edit {
	unsigned offset;
	int bytes; /* negative means "inserted" */
};

struct undo {
	struct buffer *edits, *deleted;
	unsigned redo, saved;
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
		text->undo = allocate(NULL, sizeof *text->undo);
		memset(text->undo, 0, sizeof *text->undo);
		text->undo->edits = buffer_create();
		text->undo->deleted = buffer_create();
	}
	buffer_delete(text->undo->edits, text->undo->redo,
		      buffer_bytes(text->undo->edits) - text->undo->redo);
	buffer_delete(text->undo->deleted, text->undo->saved,
		      buffer_bytes(text->undo->deleted) - text->undo->saved);
}

unsigned text_delete(struct text *text, unsigned offset, unsigned bytes)
{
	char *old;
	struct edit edit, *last;

	if (!bytes)
		return 0;
	edit.offset = offset;
	edit.bytes = buffer_raw(text->buffer, &old, offset, bytes);

	if ((last = last_edit(text)) &&
	    last->bytes >= 0 && last->offset == offset)
		last->bytes += bytes;
	else {
		resume_editing(text);
		buffer_insert(text->undo->edits, &edit,
			      text->undo->redo, sizeof edit);
		text->undo->redo += sizeof edit;
	}
	buffer_move(text->undo->deleted, text->undo->saved,
		    text->buffer, offset, edit.bytes);
	text->undo->saved += edit.bytes;
	text_adjust_loci(text, offset, -edit.bytes);
	text_dirty(text);
	return edit.bytes;
}

unsigned text_insert(struct text *text, const void *in,
		     unsigned offset, unsigned bytes)
{
	struct edit edit, *last;

	if (!bytes)
		return 0;
	bytes = buffer_insert(text->buffer, in, offset, bytes);
	if ((last = last_edit(text)) &&
	    last->bytes < 0 && last->offset - last->bytes == offset)
		last->bytes -= bytes;
	else {
		resume_editing(text);
		edit.offset = offset;
		edit.bytes = -bytes;
		buffer_insert(text->undo->edits, &edit, text->undo->redo,
			      sizeof edit);
		text->undo->redo += sizeof edit;
	}
	text_dirty(text);
	text_adjust_loci(text, offset, bytes);
	return bytes;
}

int text_undo(struct text *text)
{
	char *raw;
	struct edit *edit;

	if (!text->undo || !text->undo->redo)
		return -1;
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
	text_dirty(text);
	return edit->offset;
}

int text_redo(struct text *text)
{
	char *raw;
	struct edit *edit;

	if (!text->undo ||
	    text->undo->redo == buffer_bytes(text->undo->edits))
		return -1;
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
	text_dirty(text);
	return edit->offset;
}

void text_forget_undo(struct text *text)
{
	if (text->undo) {
		buffer_destroy(text->undo->edits);
		buffer_destroy(text->undo->deleted);
		allocate(text->undo, 0);
		text->undo = NULL;
	}
}
