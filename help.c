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
"To leave aoeui now, perhaps because you arrived here by accident, hold down\n"
"the Control key, hit the Space bar, and then the backslash (\\) key.  This\n"
"emergency escape sequence is deliberately hard to type so that you won't\n"
"accidentally terminate the editor and lose work.\n"
"\n"
"Documentation for aoeui is now a standard manual page and should be available\n"
"with the shell command \"man aoeui\".\n"
"\n"
"Bugs, complaints, suggestions, and greetings can be sent to the forum linked\n"
"to aoeui's home page http://aoeui.sourceforge.net.  Please let me know how\n"
"aoeui is working for you.\n"
"\n"
"THANKS to my lovely wife for putting up with her Dvorak-obsessed husband\n"
"for a few evenings recently as aoeui has been polished and uploaded,\n"
"and to all you brave souls curious enough to play with it!\n";

struct view *view_help(void)
{
	struct view *view = text_create("* Help *", TEXT_EDITOR);
	view->text->flags &= ~TEXT_RDONLY;
	view_insert(view, help, 0, sizeof help - 1);
	locus_set(view, CURSOR, 0);
	return view;
}
