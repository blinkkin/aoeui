/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

/*
 *	A locus is a position in a text.  Insertions and
 *	deletions in the text prior to a locus cause
 *	automatic adjustments to the byte offset of a locus.
 */

#define DELETED (UNSET-1)

locus_t locus_create(struct view *view, position_t offset)
{
	locus_t locus;

	for (locus = 0; locus < view->loci; locus++)
		if (view->locus[locus] == DELETED)
			break;
	if (locus == view->loci) {
		view->locus = reallocate(view->locus,
					 (locus+1) * sizeof *view->locus);
		view->loci++;
	}
	locus_set(view, locus, offset);
	return locus;
}

void locus_destroy(struct view *view, locus_t locus)
{
	if (locus < view->loci)
		view->locus[locus] = DELETED;
}

position_t locus_get(struct view *view, locus_t locus)
{
	position_t offset;

	if (!view || locus >= view->loci)
		return UNSET;
	offset = view->locus[locus];
	if (offset == UNSET)
		return UNSET;
	if ((int) offset < 0)
		offset = 0;
	else if (offset > view->bytes)
		offset = view->bytes;
	return offset;
}

position_t locus_set(struct view *view, locus_t locus, position_t offset)
{
	if (offset != UNSET && offset > view->bytes)
		offset = view->bytes;
	if (locus < view->loci)
		view->locus[locus] = offset;
	return offset;
}

void loci_adjust(struct view *view, position_t offset, int delta)
{
	int j;

	if (delta < 0) {
		position_t limit = offset - delta;
		for (j = 0; j < view->loci; j++) {
			position_t locus = view->locus[j];
			if (locus == DELETED || locus == UNSET)
				continue;
			if (limit <= locus)
				locus += delta;
			else if (offset < locus)
				locus = j == CURSOR ? offset : UNSET;
			view->locus[j] = locus;
		}
	} else
		for (j = 0; j < view->loci; j++) {
			position_t locus = view->locus[j];
			if (locus == DELETED || locus == UNSET)
				continue;
			if (offset <= locus)
				view->locus[j] = locus + delta;
		}
}
