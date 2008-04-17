#include "all.h"

void depart(int status)
{
	struct text *text;
	Boolean_t msg = FALSE;
	char *raw;

	for (text = text_list; text; text = text->next) {
		if (!text->path || !text->buffer || !text->buffer->path)
			continue;
		text_unfold_all(text);
		if (text->clean &&
		    buffer_raw(text->buffer, &raw, 0, ~0) ==
			text->clean_bytes &&
		    !memcmp(text->clean, raw, text->clean_bytes)) {
			unlink(text->buffer->path);
			continue;
		}
		if (!msg) {
			fprintf(stderr, "\ncheck working files for "
				"current unsaved data\n");
			msg = TRUE;
		}
		fprintf(stderr, "\t%s\n", text->buffer->path);
		buffer_snap(text->buffer);
	}
	exit(status);
}

void die(const char *msg, ...)
{
	int err = errno;
	va_list ap;

	tcsetattr(1, TCSANOW, &original_termios);
	fputs("\aaoeui editor fatal error: ", stderr);
	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);
	if (err)
		fprintf(stderr, ": %s", strerror(err));
	fputc('\n', stderr);
	depart(EXIT_FAILURE);
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
	view->text->flags |= TEXT_RDONLY;
	view_insert(view, " ", view->bytes, 1);
	locus_set(view, CURSOR, start);
	window_below(NULL, view, 3 + !!err);
}
