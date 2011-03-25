/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

static const char *help[2] = {
#include "aoeui.help"
,
#include "asdfg.help"
};

struct view *view_help(void)
{
	struct view *view = text_create("* Help *", TEXT_EDITOR);
	view_insert(view, help[is_asdfg], 0, strlen(help[is_asdfg]));
	locus_set(view, CURSOR, 0);
	view->text->flags |= TEXT_RDONLY;
	return view;
}
