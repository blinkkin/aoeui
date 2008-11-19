/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

static struct bookmark {
	unsigned id;
	struct view *view;
	locus_t locus[2];
	struct bookmark *next;
} *bookmarks;

void bookmark_set(unsigned id, struct view *view,
		  position_t cursor, position_t mark)
{
	struct bookmark *bm = allocate0(sizeof *bm);

	bookmark_unset(id);
	bm->id = id;
	bm->view = view;
	bm->locus[0] = locus_create(view, cursor);
	bm->locus[1] = locus_create(view, mark);
	bm->next = bookmarks;
	bookmarks = bm;
}

Boolean_t bookmark_get(struct view **view, position_t *cursor, position_t *mark,
		       unsigned id)
{
	struct bookmark *bm;
	for (bm = bookmarks; bm; bm = bm->next)
		if (bm->id == id) {
			*view = bm->view;
			*cursor = locus_get(bm->view, bm->locus[0]);
			if (*cursor == UNSET)
				*cursor = 0;
			*mark = locus_get(bm->view, bm->locus[1]);
			return TRUE;
		}
	*view = NULL;
	*cursor = *mark = 0;
	return FALSE;
}

void bookmark_unset(unsigned id)
{
	struct bookmark *bm, *prev = NULL;

	for (bm = bookmarks; bm; bm = bm->next)
		if (bm->id == id) {
			locus_destroy(bm->view, bm->locus[0]);
			locus_destroy(bm->view, bm->locus[1]);
			if (prev)
				prev->next = bm->next;
			else
				bookmarks = bm->next;
			return;
		}
}

void bookmark_unset_view(struct view *view)
{
	struct bookmark *bm, *prev = NULL, *next;

	for (bm = bookmarks; bm; bm = next) {
		next = bm->next;
		if (bm->view != view) {
			prev = bm;
			continue;
		}
		locus_destroy(view, bm->locus[0]);
		locus_destroy(view, bm->locus[1]);
		if (prev)
			prev->next = next;
		else
			bookmarks = next;
	}
}
