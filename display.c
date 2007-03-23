#include "all.h"
#include "display.h"
#include <termios.h>
#include <sys/ioctl.h>

/*
 *	Bandwidth-optimized terminal display management.
 *	Maintain an image of the display surface and
 *	update it when repainting differs from the image.
 *
 *	reference: man 4 console_codes
 */

#define BUFSZ 1024
#define ESC   "\x1b"

struct cell {
	unsigned unicode, fgrgba, bgrgba;
};

struct display {
	unsigned rows, columns;
	unsigned cursor_row, cursor_column;
	unsigned size_changed;
	unsigned fgrgba, bgrgba;
	unsigned at_row, at_column;
	struct cell *image;
	struct termios original;
	struct display *prev, *next;
	unsigned buffered_bytes;
	char *buffer;
	int is_xterm;
};

static struct display *display_list;
static void (*old_sigwinch)(int, siginfo_t *, void *);


static void emit(struct display *display, const char *str, unsigned bytes)
{
	unsigned wrote;
	int chunk;

	for (wrote = 0; wrote < bytes; wrote += chunk) {
		errno = 0;
		chunk = write(1, str + wrote, bytes - wrote);
		if (chunk < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			die("write of %d bytes failed", bytes);
		}
	}
}

static void flush(struct display *display)
{
	emit(display, display->buffer, display->buffered_bytes);
	display->buffered_bytes = 0;
}

static void out(struct display *display, const char *str, unsigned bytes)
{
	if (display->buffered_bytes + bytes > BUFSZ)
		flush(display);
	if (bytes > BUFSZ)
		emit(display, str, bytes);
	else {
		memcpy(display->buffer + display->buffered_bytes, str, bytes);
		display->buffered_bytes += bytes;
	}
}

static void outs(struct display *display, const char *str)
{
	out(display, str, strlen(str));
}

static void outf(struct display *display, const char *msg, ...)
{
	char buf[128];
	va_list ap;
	va_start(ap, msg);
	vsnprintf(buf, sizeof buf, msg, ap);
	va_end(ap);
	outs(display, buf);
}

static void moveto(struct display *display, unsigned row, unsigned column)
{
	outf(display, ESC "[%d;%df", row+1, column+1);
}

static void cursor(struct display *display)
{
	moveto(display, display->at_row = display->cursor_row,
	       display->at_column = display->cursor_column);
}

static void reset_modes(struct display *display)
{
	outs(display, ESC "[0;39;49m"); /* reset modes and colors */
	display->fgrgba = 1;
	display->bgrgba = ~0;
}

void display_sync(struct display *display)
{
	cursor(display);
	flush(display);
}

static unsigned colormap(unsigned rgba)
{
	unsigned bgr1;

	if (rgba & 0xff)
		return 9; /* any alpha implies "default color" */
	bgr1 = !!(rgba >> 24);
	bgr1 |= !!(rgba >> 16 & 0xff) << 1;
	bgr1 |= !!(rgba >>  8 & 0xff) << 2;
	return bgr1;
}

void display_put(struct display *display, unsigned row, unsigned column,
		 unsigned unicode, unsigned fgrgba, unsigned bgrgba)
{
	char buf[8];
	struct cell *cell;

	if (row >= display->rows || column >= display->columns)
		return;
	if (unicode < ' ')
		unicode = ' ';

	cell = &display->image[row*display->columns + column];
	if (cell->unicode == unicode &&
	    (cell->fgrgba == fgrgba || unicode == ' ') &&
	    cell->bgrgba == bgrgba)
		return;

	if (row != display->at_row ||
	    column != display->at_column ||
	    bgrgba != display->bgrgba ||
	    fgrgba != display->fgrgba && unicode != ' ') {
		if (row != display->at_row || column != display->at_column)
			moveto(display, display->at_row = row,
			       display->at_column = column);
		outf(display, ESC "[%d;%dm", 40 + colormap(bgrgba),
		     30 + colormap(fgrgba));
		display->bgrgba = bgrgba;
		display->fgrgba = fgrgba;
	}


	out(display, buf, utf8_out(buf, unicode));
	display->at_column++;

	cell->unicode = unicode;
	cell->fgrgba = fgrgba;
	cell->bgrgba = bgrgba;
}

void display_erase(struct display *display, unsigned row, unsigned column,
		   unsigned rows, unsigned columns, unsigned bgrgba)
{
	unsigned r, c;

	if (row >= display->rows || column >= display->columns)
		return;
	if (row + rows > display->rows)
		rows = display->rows - row;
	if (column + columns > display->columns)
		columns = display->columns - column;
	if (!columns)
		return;

	if (bgrgba != display->bgrgba) {
		outf(display, ESC "[%dm", 40 + colormap(bgrgba));
		display->bgrgba = bgrgba;
	}

	if (column + columns == display->columns)
		if (!column && rows == display->rows - row) {
			moveto(display, row, column);
			outs(display, ESC "[J");
		} else
			for (r = 0; r < rows; r++) {
				moveto(display, row + r, column);
				outs(display, ESC "[K");
			}
	else
		for (r = 0; r < rows; r++) {
			moveto(display, row + r, column);
			outf(display, ESC "[%dX", columns);
		}

	for (; rows--; row++) {
		struct cell *cell = &display->image[row*display->columns +
						    column];
		for (c = 0; c < columns; c++) {
			cell->unicode = ' ';
			cell++->bgrgba = bgrgba;
		}
	}

	moveto(display, display->at_row, display->at_column);
}

static struct cell *resize(struct display *display, struct cell *old,
			   unsigned old_rows, unsigned old_columns,
			   unsigned new_rows, unsigned new_columns)
{
	unsigned cells = new_rows * new_columns;
	unsigned bytes = cells * sizeof(struct cell);
	struct cell *new = allocate(NULL, bytes);
	unsigned min_rows = new_rows < old_rows ? new_rows : old_rows;
	unsigned min_columns = new_columns < old_columns ?
				new_columns : old_columns;
	unsigned j, k;

	if (!old) {
		memset(new, 0, bytes);
		for (j = 0; j < cells; j++) {
			new[j].unicode = ' ';
			new[j].bgrgba = display->bgrgba;
			new[j].fgrgba = display->fgrgba;
		}
		return new;
	}

	for (j = 0; j < min_rows; j++) {
		struct cell *cell = &new[j*new_columns];
		for (k = 0; k < min_columns; k++)
			*cell++ = old[j*old_columns+k];
		for (; k < new_columns; k++) {
			*cell = cell[-1];
			cell++->unicode = ' ';
		}
	}
	for (; j < new_rows; j++) {
		struct cell *cell = &new[j*new_columns];
		for (k = 0; k < new_columns; k++) {
			*cell = cell[-display->columns];
			cell++->unicode = ' ';
		}
	}

	allocate(old, 0);
	return new;
}

static void display_geometry(struct display *display)
{
	int rows, columns;
	struct winsize ws;

	if (!ioctl(1, TIOCGWINSZ, &ws)) {
		rows = ws.ws_row;
		columns = ws.ws_col;
	} else {
		rows = 24;
		columns = 80;
	}

	if (display->image &&
	    display->rows == rows &&
	    display->columns == columns)
		return;

	display->image = resize(display, display->image,
				display->rows, display->columns,
				rows, columns);
	display->rows = rows;
	display->columns = columns;
	display->size_changed = 1;
	display_cursor(display, display->cursor_row, display->cursor_column);
}

void display_get_geometry(struct display *display,
			  unsigned *rows, unsigned *columns)
{
	*rows = display->rows;
	*columns = display->columns;
	display->size_changed = 0;
}

void display_reset(struct display *display)
{
	allocate(display->image, 0);
	display->image = NULL;
	display->cursor_row = display->cursor_column = 0;
	display->at_row = display->at_column = 0;
	outs(display, ESC "c");		/* reset */
	outs(display, ESC "%G");	/* UTF-8 */
	outs(display, ESC "[2J");	/* erase all */
	reset_modes(display);
	display_geometry(display);
	display_sync(display);
}

static void sigwinch(int signo, siginfo_t *info, void *data)
{
	struct display *display;
	for (display = display_list; display; display = display->next)
		display_geometry(display);
	if (old_sigwinch)
		old_sigwinch(signo, info, data);
}

struct display *display_init(void)
{
	struct display *display = allocate(NULL, sizeof *display);
	struct termios termios;
	const char *term;
	const char *step;

	memset(display, 0, sizeof *display);

	errno = 0;
	step = "tcgetattr";
	if (tcgetattr(1, &display->original))
		goto error;

	cfmakeraw(&termios);
	errno = 0;
	step = "tcsetattr";
	if (tcsetattr(1, TCSADRAIN, &termios))
		goto recover;

	if ((display->next = display_list))
		display->next->prev = display;
	else {
		struct sigaction sigact, old_sigact;
		memset(&sigact, 0, sizeof sigact);
		sigact.sa_sigaction = sigwinch;
		sigact.sa_flags = SA_SIGINFO;
		sigaction(SIGWINCH, &sigact, &old_sigact);
		if ((void (*)(int)) old_sigact.sa_sigaction != SIG_DFL &&
		    (void (*)(int)) old_sigact.sa_sigaction != SIG_IGN)
			old_sigwinch = old_sigact.sa_sigaction;
	}

	display_list = display;
	display->buffer = allocate(NULL, BUFSZ);
	display->buffered_bytes = 0;

	display->is_xterm = (term = getenv("TERM")) && !strcmp(term, "xterm");

	display_reset(display);
	display_sync(display);
	return display;

recover:
	tcsetattr(1, TCSADRAIN, &display->original);
error:
	fprintf(stderr, "The display could not be initialized with %s, "
		"so aoeui can't run.  Sorry!\n"
		"The error code is: %s\n", step, strerror(errno));
	exit(EXIT_FAILURE);
	return NULL;
}

void display_end(struct display *display)
{
	if (!display)
		return;

	display_title(display, NULL);
	outs(display, ESC "c");
	outs(display, ESC "[0m");
	flush(display);

	tcsetattr(1, TCSADRAIN, &display->original);

	allocate(display->image, 0);
	allocate(display->buffer, 0);

	if (display->prev)
		display->prev->next = display->next;
	else
		display_list = display->next;
	if (display->next)
		display->next->prev = display->prev;

	allocate(display, 0);
}

void display_title(struct display *display, const char *title)
{
	if (display->is_xterm) {
		outf(display, ESC "]2;%s" ESC "\\", title ? title : "");
		display_sync(display);
	}
}

void display_cursor(struct display *display, unsigned row, unsigned column)
{
	display->cursor_row =
		row >= display->rows ? display->rows-1 : row;
	display->cursor_column =
		column >= display->columns ? display->columns-1 : column;
	cursor(display);
}

void display_beep(struct display *display)
{
	emit(display, "\a", 1);
	display_sync(display);
}

int display_getch(struct display *display)
{
	unsigned char ch;
	int n;

	display_sync(display);
	do {
		if (display->size_changed)
			return DISPLAY_WINCH;
		errno = 0;
		n = read(0, &ch, 1);
	} while (n < 0 && (errno == EAGAIN || errno == EINTR));

	if (!n)
		return DISPLAY_EOF;
	if (n < 0)
		return DISPLAY_ERR;
	return ch;
}

int display_any_input(struct display *display, unsigned millisecs)
{
	int n;
	struct timeval timeval;

	do {
		fd_set fds;
		if (display->size_changed)
			return 1;
		FD_ZERO(&fds);
		FD_SET(0, &fds);
		timeval.tv_sec = millisecs / 1000;
		timeval.tv_usec = millisecs % 1000 * 1000;
		errno = 0;
		n = select(1, &fds, NULL, NULL, &timeval);
	} while (n < 0 && (errno == EAGAIN || errno == EINTR));

	return !!n;
}
