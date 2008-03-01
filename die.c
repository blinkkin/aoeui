#include "all.h"

void die(const char *msg, ...)
{
	int err = errno;
	va_list ap;
	struct text *text;

	tcsetattr(1, TCSANOW, &original_termios);
	fputs("\aaoeui editor fatal error: ", stderr);
	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);
	if (err)
		fprintf(stderr, ": %s", strerror(err));
	fprintf(stderr, "\ncheck working files with # at the ends of their "
		"names for current unsaved data\n");
	for (text = text_list; text; text = text->next)
		buffer_snap(text->buffer);
	exit(EXIT_FAILURE);
}

void message(const char *msg, ...)
{
	int err = errno;
	va_list ap;
	struct view *view = view_find("* Scratch *");

	if (!view)
		view = text_create("* Scratch *", TEXT_EDITOR);
	view->text->flags &= ~TEXT_RDONLY;
	view_insert(view, "\n", view->bytes, 1);
	va_start(ap, msg);
	view_vprintf(view, msg, ap);
	va_end(ap);
	view_insert(view, "\n", view->bytes, 1);
	if (err)
		view_printf(view, "(System error code: %s)\n", strerror(err));
	view->text->flags |= TEXT_RDONLY;
	locus_set(view, CURSOR, view->bytes);
	window_below(NULL, view, 2 + !!err);
}
