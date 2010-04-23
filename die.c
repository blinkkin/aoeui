/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

/* Error messaging */

void die(const char *msg, ...)
{
	int err = errno;
	va_list ap;

	tcsetattr(1, TCSANOW, &original_termios);
	fputs("\afatal editor error: ", stderr);
	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);
	if (err)
		fprintf(stderr, ": %s", strerror(err));
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

void message(const char *msg, ...)
{
	int err = errno;
	va_list ap;
	struct view *view = view_find("* ATTENTION *");
	position_t start;

	if (!view)
		view = text_create("* ATTENTION *", TEXT_EDITOR);
	view->text->flags &= ~TEXT_RDONLY;
	view_insert(view, "\n", view->bytes, 1);
	start = view->bytes;
	va_start(ap, msg);
	view_vprintf(view, msg, ap);
	va_end(ap);
	if (err)
		view_printf(view, "\n(System error code: %s)", strerror(err));
	view_insert(view, " ", view->bytes, 1);
	view->text->flags |= TEXT_RDONLY;
	locus_set(view, CURSOR, start);
	window_below(NULL, view, 3 + !!err);
}

static struct view *status_view;

void status(const char *msg, ...)
{
	va_list ap;

	if (!status_view)
		status_view = text_create("* STATUS *", TEXT_EDITOR);
	status_view->text->flags &= ~TEXT_RDONLY;
	view_delete(status_view, 0, status_view->bytes);
	va_start(ap, msg);
	view_vprintf(status_view, msg, ap);
	va_end(ap);
	view_insert(status_view, " ", status_view->bytes, 1);
	locus_set(status_view, CURSOR, status_view->bytes-1);
	status_view->text->flags |= TEXT_RDONLY;
	window_below(NULL, status_view, 3);
}

void status_hide(void)
{
	if (status_view)
		window_destroy(status_view->window);
}
