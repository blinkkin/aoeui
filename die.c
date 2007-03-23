#include "all.h"

void die(const char *msg, ...)
{
	int err = errno;
	va_list ap;

	window_beep(NULL);
	windows_end();

	fputs("editor error: ", stderr);
	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);
	if (err)
		fprintf(stderr, ": %s", strerror(err));
	fputc('\n', stderr);

	buffers_preserve();
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
	window_raise(view);
}
