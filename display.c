#include "all.h"
#include "display.h"
#include <sys/ioctl.h>

/*
 *	Bandwidth-optimized terminal display management.
 *	Maintain an image of the display surface and
 *	update it when repainting differs from the image.
 *
 *	reference: man 4 console_codes
 */

#define BUFSZ 1024
#define ESC "\x1b"
#define CSI ESC "["
#define OSC ESC "]"
#define ST  "\x07"
#define MAX_COLORS 8

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
	struct display *next;
	unsigned buffered_bytes;
	char *buffer;
	int is_xterm;
	unsigned color[MAX_COLORS], colors;
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
	outf(display, CSI "%d;%df", row+1, column+1);
}

static void cursor(struct display *display)
{
	moveto(display, display->at_row = display->cursor_row,
	       display->at_column = display->cursor_column);
}

void display_sync(struct display *display)
{
	cursor(display);
	flush(display);
}

static unsigned linux_colormap(unsigned rgba)
{
	unsigned bgr1;
	if (rgba & 0xff)
		return 9; /* any alpha implies "default color" */
	bgr1 = !!(rgba >> 24);
	bgr1 |= !!(rgba >> 16 & 0xff) << 1;
	bgr1 |= !!(rgba >>  8 & 0xff) << 2;
	return bgr1;
}

static unsigned color_delta(unsigned rgba1, unsigned rgba2)
{
	unsigned delta = 1, j;
	if (rgba1 >> 8 == rgba2 >> 8)
		return 0;
	for (j = 8; j < 32; j += 8) {
		unsigned char c1 = rgba1 >> j, c2 = rgba2 >> j;
		int cd = c1 - c2;
		delta *= (cd < 0 ? -cd : cd) + 1;
	}
	return delta;
}

static unsigned color_mean(unsigned rgba1, unsigned rgba2)
{
	unsigned mean = 0, j;
	for (j = 0; j < 32; j += 8) {
		unsigned char c1 = rgba1 >> j, c2 = rgba2 >> j;
		mean |= c1 + c2 >> 1 << j;
	}
	return mean;
}

static unsigned colormap(struct display *display, unsigned rgba)
{
	unsigned idx, best, bestdelta, delta;
	static unsigned basic[] = {
		0, 0x7f000000, 0x007f0000, 0x7f7f0000,
		0x00007f00, 0x7f007f00, 0x007f7f00, 0x7f7f7f00,
	};
	static unsigned bright[] = {
		0, 0xff000000, 0x00ff0000, 0xffff0000,
		0x0000ff00, 0xff00ff00, 0x00ffff00, 0xffffff00,
	};

	if (rgba & 0xff)
		return 9; /* any alpha? use default */
	for (idx = 0; idx < 8; idx++)
		if (rgba == basic[idx])
			return idx;
	for (idx = 0; idx < 8; idx++)
		if (rgba == bright[idx])
			return idx + 10;
	for (idx = 0; idx < display->colors; idx++)
		if (display->color[idx] == rgba)
			return idx+18;
	if (idx < MAX_COLORS)
		display->color[best = display->colors++] = rgba;
	else {
		best = 0;
		bestdelta = ~0;
		for (idx = 0; idx < display->colors; idx++) {
			delta = color_delta(display->color[idx], rgba);
			if (delta < bestdelta) {
				best = idx;
				bestdelta = delta;
			}
		}
		display->color[best] = color_mean(display->color[best], rgba);
	}
	best += 18;
	outf(display, OSC "4;%d;rgb:%02x/%02x/%02x" ST, best,
	     rgba >> 24, rgba >> 16 & 0xff, rgba >> 8 & 0xff);
	return best;
}

static void background_color(struct display *display, unsigned rgba)
{
	if (rgba == display->bgrgba)
		return;
	if (display->is_xterm) {
		unsigned c = colormap(display, rgba);
		if (c >= 18)
			outf(display, CSI "48;5;%dm", c);
		else if (c >= 10)
			outf(display, CSI "%dm", c - 10 + 100);
		else
			outf(display, CSI "%dm", c + 40);
	} else
		outf(display, CSI "%dm", 40 + linux_colormap(rgba));
	display->bgrgba = rgba;
}

static void foreground_color(struct display *display, unsigned rgba)
{
	if (rgba == display->fgrgba)
		return;
	if (display->is_xterm) {
		unsigned c = colormap(display, rgba);
		if (c >= 18)
			outf(display, CSI "38;5;%dm", c);
		else if (c >= 10)
			outf(display, CSI "%dm", c - 10 + 90);
		else
			outf(display, CSI "%dm", c + 30);
	} else
		outf(display, CSI "%dm", 30 + linux_colormap(rgba));
	display->fgrgba = rgba;
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
		background_color(display, bgrgba);
		if (unicode != ' ')
			foreground_color(display, fgrgba);
	}

	out(display, buf, utf8_out(buf, unicode));
	display->at_column++;

	cell->unicode = unicode;
	cell->fgrgba = display->fgrgba;
	cell->bgrgba = display->bgrgba;
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

	background_color(display, bgrgba);


	if (column + columns == display->columns)
		if (!column && rows == display->rows - row) {
			moveto(display, row, column);
			outs(display, CSI "J");
		} else
			for (r = 0; r < rows; r++) {
				moveto(display, row + r, column);
				outs(display, CSI "K");
			}
	else
		for (r = 0; r < rows; r++) {
			moveto(display, row + r, column);
			outf(display, CSI "%dX", columns);
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
	if (display->is_xterm) {
		outs(display, CSI "?47h"); /* alt screen */
		outs(display, CSI "?67h"); /* BCK is ^? */
	} else
		outs(display, ESC "c");		/* reset */
	outs(display, ESC "%G");	/* UTF-8 */
	outs(display, CSI "0;39;49m"); /* reset modes and colors */
	display->colors = 0;
	display->fgrgba = 1;
	display->bgrgba = ~0;
	outs(display, CSI "2J");	/* erase all */
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

#ifndef __linux__
void cfmakeraw(struct termios *termios)
{
	termios->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|IXON);
	termios->c_oflag &= ~OPOST;
	termios->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	termios->c_cflag &= ~(CSIZE|PARENB);
	termios->c_cflag |= CS8;
}
#endif

struct display *display_init(void)
{
	struct display *display = allocate(NULL, sizeof *display);
	struct termios termios = original_termios;
	const char *term;

	memset(display, 0, sizeof *display);

	cfmakeraw(&termios);
	tcsetattr(1, TCSADRAIN, &termios);

	if (!(display->next = display_list)) {
		struct sigaction sigact, old_sigact;
		memset(&sigact, 0, sizeof sigact);
		sigact.sa_sigaction = sigwinch;
		sigact.sa_flags = SA_SIGINFO;
		sigaction(SIGWINCH, &sigact, &old_sigact);
		if ((void (*)(int)) old_sigact.sa_sigaction != SIG_DFL &&
		    (void (*)(int)) old_sigact.sa_sigaction != SIG_IGN &&
		    old_sigact.sa_sigaction != sigwinch)
			old_sigwinch = old_sigact.sa_sigaction;
	}

	display_list = display;
	display->buffer = allocate(NULL, BUFSZ);
	display->buffered_bytes = 0;
	display->is_xterm = (term = getenv("TERM")) && !strcmp(term, "xterm");
	display_reset(display);
	display_sync(display);
	return display;
}

void display_end(struct display *display)
{
	struct display *d, *prev = NULL;

	if (!display)
		return;

	display_title(display, NULL);
	if (display->is_xterm)
		outs(display, CSI "?47l"); /* normal screen */
	else {
		outs(display, ESC "c");		/* reset */
		outs(display, CSI "0m");	/* reset modes */
	}
	flush(display);

	tcsetattr(1, TCSADRAIN, &original_termios);

	allocate(display->image, 0);
	allocate(display->buffer, 0);

	for (d = display_list; d; prev = d, d = d->next)
		if (d == display) {
			if (prev)
				prev->next = display->next;
			else
				display_list = display->next;
			break;
		}

	allocate(display, 0);
}

void display_title(struct display *display, const char *title)
{
	if (display->is_xterm) {
		outf(display, OSC "2;%s" ESC "\\", title ? title : "");
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

int display_getch(struct display *display, int block)
{
	unsigned char ch;
	int n;

	if (!display)
		return DISPLAY_EOF;
	display_sync(display);
	if (display->size_changed)
		return DISPLAY_WINCH;
	if (!multiplexor(block))
		return DISPLAY_NONE;
	do {
		errno = 0;
		n = read(0, &ch, 1);
	} while (n < 0 && (errno == EAGAIN || errno == EINTR));
	return !n ? DISPLAY_EOF : n < 0 ? DISPLAY_ERR : ch;
}
