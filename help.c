/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

static const char *help[2] = {

	"Welcome to aoeui, pmk's editor optimized for fellow users of...\n"
	"\n"
	"       the	 ESC `  1 2 3 4 5  6 7 8 9 0  [ ] BACK\n"
	"    Dvorak	   TAB  ' , . P Y  F G C R L  / = \\\n"
	"Simplified	  CTRL  A O E U I  D H T N S  - ENTER\n"
	"  Keyboard	 SHIFT  ; Q J K X  B M W V Z  SHIFT\n"
	"    layout	  LOCK  ALT	Space	 ALT  CTRL\n"
	"\n"
	"To leave now, perhaps because you arrived here by accident, hold\n"
	"down the Control key and hit both the Space bar and then the\n"
	"backslash (\\) key.  This emergency escape sequence is deliberately\n"
	"hard to type so that you won't accidentally terminate the editor\n"
	"and lose work.\n"
	"\n"
	"Documentation for the editor should be available with the shell\n"
	"command \"man aoeui\".\n"
	"\n"
	"Bugs, complaints, suggestions, and greetings can be sent to the\n"
	"forum linked to aoeui's home page http://aoeui.sourceforge.net.\n"
	"Please let me know how aoeui is working for you.\n",

	"Welcome to asdfg, the QWERTY editor of pmk's aoeui editor.\n"
	"\n"
	"To leave now, perhaps because you arrived here by accident, hold\n"
	"down the Control key and hit both the Space bar and then the\n"
	"backslash (\\) key.  This emergency escape sequence is deliberately\n"
	"hard to type so that you won't accidentally terminate the editor\n"
	"and lose work.\n"
	"\n"
	"Documentation for the editor should be available with the shell\n"
	"command \"man asdfg\".\n"
	"\n"
	"Bugs, complaints, suggestions, and greetings can be sent to the\n"
	"forum linked to the editor's home page http://aoeui.sourceforge.net.\n"
	"Please let me know how asdfg is working for you.\n"
};

struct view *view_help(void)
{
	struct view *view = text_create("* Help *", TEXT_EDITOR);
	view->text->flags &= ~TEXT_RDONLY;
	view_insert(view, help[is_asdfg], 0, strlen(help[is_asdfg]));
	locus_set(view, CURSOR, 0);
	return view;
}
