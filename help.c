#include "all.h"

static const char help[] =
"Welcome to aoeui, pmk's editor optimized for fellow users of the...\n"
"\n"
"       the	 ESC `  1 2 3 4 5  6 7 8 9 0  [ ] BACK\n"
"    Dvorak	   TAB  ' , . P Y  F G C R L  / = \\\n"
"Simplified	  LOCK  A O E U I  D H T N S  - ENTER\n"
"  Keyboard	 SHIFT  ; Q J K X  B M W V Z  SHIFT\n"
"    layout	  CTRL  ALT	Space	 ALT  CTRL\n"
"\n"
"(or to asdfg, aoeui's QWERTY edition)\n"
"\n"
"To leave now, perhaps because you arrived here by accident, hold down\n"
"the Control key and hit both the Space bar and then the backslash (\\) key.\n"
"This emergency escape sequence is deliberately hard to type so that you\n"
"won't accidentally terminate the editor and lose work.\n"
"\n"
"Documentation for the editor should be available with the shell command\n"
"\"man aoeui\" or \"man asdfg\".\n"
"\n"
"Bugs, complaints, suggestions, and greetings can be sent to the forum linked\n"
"to aoeui's home page http://aoeui.sourceforge.net.  Please let me know how\n"
"aoeui is working for you.\n"
;

struct view *view_help(void)
{
	struct view *view = text_create("* Help *", TEXT_EDITOR);
	view->text->flags &= ~TEXT_RDONLY;
	view_insert(view, help, 0, sizeof help - 1);
	locus_set(view, CURSOR, 0);
	return view;
}
