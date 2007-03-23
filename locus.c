#include "all.h"

/*
 *	A locus is a position in a text.  Insertions and
 *	deletions in the text prior to a locus cause
 *	automatic adjustments to the byte offset of a locus.
 */

#define DELETED (UNSET-1)

unsigned locus_create(struct view *view, unsigned offset)
{
	unsigned locus;

	for (locus = 0; locus < view->loci; locus++)
		if (view->locus[locus] == DELETED)
			break;
	if (locus == view->loci) {
		view->locus = allocate(view->locus,
				       (locus+1) * sizeof *view->locus);
		view->loci++;
	}
	locus_set(view, locus, offset);
	return locus;
}

void locus_destroy(struct view *view, unsigned locus)
{
	if (locus < view->loci)
		view->locus[locus] = DELETED;
}

unsigned locus_offset(struct view *view, unsigned locus, int delta)
{
	unsigned offset;

	if (locus >= view->loci)
		return view->bytes;
	offset = view->locus[locus];
	if (offset == UNSET)
		return UNSET;
	offset += delta;
	if ((int) offset < 0)
		offset = 0;
	else if (offset > view->bytes)
		offset = view->bytes;
	return offset;
}

unsigned locus_get(struct view *view, unsigned locus)
{
	return locus_offset(view, locus, 0);
}

unsigned locus_set(struct view *view, unsigned locus, unsigned offset)
{
	if (offset != UNSET && offset > view->bytes)
		offset = view->bytes;
	return view->locus[locus] = offset;
}

void loci_adjust(struct view *view, unsigned offset, int delta)
{
	unsigned j;

	if (delta < 0) {
		unsigned limit = offset - delta;
		for (j = 0; j < view->loci; j++) {
			unsigned locus = view->locus[j];
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
			unsigned locus = view->locus[j];
			if (locus == DELETED || locus == UNSET)
				continue;
			if (offset <= locus)
				view->locus[j] = locus + delta;
		}
}
