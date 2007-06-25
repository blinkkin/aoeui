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
#define MAX_COLORS 8

#define ESC "\x1b"
#define CSI ESC "["
#define OSC ESC "]"
#define ST "\x07"

#define CTL_RESET	ESC "c"
#define CTL_NUMLOCK	ESC ">"
#define CTL_CLEARLEDS	ESC "[0q"
#define CTL_NUMLOCKLED	ESC "[2q"
#define CTL_UTF8	ESC "%G"
#define CTL_RESETMODES	CSI "0m"
#define CTL_RESETCOLORS	CSI "39;49m"
#define CTL_GOTO	CSI "%d;%df"
#define CTL_ERASEALL	CSI "2J"
#define CTL_ERASETOEND	CSI "J"
#define CTL_ERASELINE	CSI "K"
#define CTL_ERASECOLS	CSI "%dX"
#define CTL_DELCOLS	CSI "%uP"
#define CTL_DELLINES	CSI "%uM"
#define CTL_INSCOLS	CSI "%u@"
#define CTL_INSLINES	CSI "%uL"
#define CTL_RGB		OSC "4;%d;rgb:%02x/%02x/%02x" ST
#define CTL_CURSORPOS	CSI "6n"
#define FG_COLOR	30
#define BG_COLOR	40
#define XTERM_TITLE	OSC "2;%s" ESC "\\"
#define XTERM_ALTSCREEN	CSI "?47h"
#define XTERM_REGSCREEN	CSI "?47l"
#define XTERM_BCKISDEL	CSI "?67h"


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
	if (row == display->at_row && column == display->at_column)
		return;
	if (row == display->at_row + 1) {
		if (column == display->at_column) {
			outs(display, "\n");
			display->at_row++;
			return;
		}
		if (!column) {
			outs(display, "\n\r");
			display->at_row++;
			display->at_column = 0;
			return;
		}
	}
	outf(display, CTL_GOTO, (display->at_row = row) + 1,
	     (display->at_column = column) + 1);
}

void display_sync(struct display *display)
{
	moveto(display, display->cursor_row, display->cursor_column);
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
	static unsigned basic[] = {
		0, 0x7f000000, 0x007f0000, 0x7f7f0000,
		0x00007f00, 0x7f007f00, 0x007f7f00, 0x7f7f7f00,
	};
	static unsigned bright[] = {
		0, 0xff000000, 0x00ff0000, 0xffff0000,
		0x0000ff00, 0xff00ff00, 0x00ffff00, 0xffffff00,
	};

	unsigned idx, best, bestdelta, delta;

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
	outf(display, CTL_RGB, best,
	     rgba >> 24, rgba >> 16 & 0xff, rgba >> 8 & 0xff);
	return best;
}

static void set_color(struct display *display, unsigned rgba, unsigned magic)
{
	if (display->is_xterm) {
		unsigned c = colormap(display, rgba);
		if (c >= 18)
			outf(display, CSI "%d;5;%dm", magic+8, c);
		else if (c >= 10)
			outf(display, CSI "%dm", c - 10 + magic + 60);
		else
			outf(display, CSI "%dm", c + magic);
	} else
		outf(display, CSI "%dm", linux_colormap(rgba) + magic);
}

static void background_color(struct display *display, unsigned rgba)
{
	if (rgba != display->bgrgba) {
		set_color(display, rgba, BG_COLOR);
		display->bgrgba = rgba;
	}
}

static void foreground_color(struct display *display, unsigned rgba)
{
	if (rgba != display->fgrgba) {
		set_color(display, rgba, FG_COLOR);
		display->fgrgba = rgba;
	}
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
	if (cell->unicode != unicode ||
	    cell->fgrgba != fgrgba && unicode != ' ' ||
	    cell->bgrgba != bgrgba) {
		moveto(display, row, column);
		background_color(display, bgrgba);
		if (unicode != ' ')
			foreground_color(display, fgrgba);
		out(display, buf, utf8_out(buf, unicode));
		display->at_column++;
		cell->unicode = unicode;
		cell->bgrgba = bgrgba;
	}
	cell->fgrgba = fgrgba;
}

static void fill(struct display *display, unsigned row, unsigned column,
		 unsigned columns, unsigned code, unsigned fgrgba,
		 unsigned bgrgba)
{
	struct cell *cell = &display->image[row * display->columns +
					    column];
	while (columns--) {
		cell->unicode = code;
		cell->fgrgba = fgrgba;
		cell++->bgrgba = bgrgba;
	}
}

void display_erase(struct display *display, unsigned row, unsigned column,
		   unsigned rows, unsigned columns,
		   unsigned fgrgba, unsigned bgrgba)
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

	for (r = 0; r < rows; r++) {
		struct cell *cell = &display->image[(row+r)*display->columns +
						    column];
		for (c = 0; c < columns; c++)
			if (cell->unicode != ' ' ||
			    cell++->bgrgba != bgrgba)
				goto doit;
	}
	return;

doit:	background_color(display, bgrgba);
	if (column + columns == display->columns)
		if (!column && rows == display->rows - row) {
			moveto(display, row, column);
			outs(display, CTL_ERASETOEND);
		} else
			for (r = 0; r < rows; r++) {
				moveto(display, row + r, column);
				outs(display, CTL_ERASELINE);
			}
	else
		for (r = 0; r < rows; r++) {
			moveto(display, row + r, column);
			outf(display, CTL_ERASECOLS, columns);
		}
	for (; rows--; row++)
		fill(display, row, column, columns, ' ', fgrgba, bgrgba);
}

void display_insert_spaces(struct display *display, unsigned row,
			   unsigned column, unsigned spaces, unsigned columns,
			   unsigned fgrgba, unsigned bgrgba)
{
	struct cell *cell;

	if (row >= display->rows || column >= display->columns)
		return;
	if (column + columns > display->columns)
		columns = display->columns - column;
	if (spaces > columns)
		spaces = columns;
	if (!spaces)
		return;

	if (column + columns != display->columns) {
		moveto(display, row, column + columns - spaces);
		outf(display, CTL_DELCOLS, spaces);
	}
	moveto(display, row, column);
	background_color(display, bgrgba);
	outf(display, CTL_INSCOLS, spaces);
	cell = &display->image[row*display->columns + column];
	memmove(cell + spaces, cell, (columns - spaces) * sizeof *cell);
	fill(display, row, column, spaces, ' ', fgrgba, bgrgba);
}

void display_delete_chars(struct display *display, unsigned row,
			  unsigned column, unsigned chars, unsigned columns,
			  unsigned fgrgba, unsigned bgrgba)
{
	struct cell *cell;

	if (row >= display->rows || column >= display->columns)
		return;
	if (column + columns > display->columns)
		columns = display->columns - column;
	if (chars > columns)
		chars = columns;
	if (!chars)
		return;

	moveto(display, row, column);
	background_color(display, bgrgba);
	outf(display, CTL_DELCOLS, chars);
	if (column + columns != display->columns) {
		moveto(display, row, column + columns - chars);
		outf(display, CTL_INSCOLS, chars);
	}
	cell = &display->image[row*display->columns + column];
	memmove(cell, cell + chars, (columns - chars) * sizeof *cell);
	fill(display, row, column + columns - chars, chars,
	     ' ', fgrgba, bgrgba);
}

static int validate(struct display *display, unsigned row, unsigned column,
		    unsigned *rows, unsigned *columns, unsigned *lines)
{
	if (row >= display->rows || column >= display->columns)
		return 0;
	if (row + *rows > display->rows)
		*rows = display->rows - row;
	if (*lines > *rows)
		*lines = *rows;
	if (column + *columns > display->columns)
		*columns = display->columns - column;
	return *lines && *columns;
}

static void insert_whole_lines(struct display *display, unsigned row,
			       unsigned lines, unsigned rows,
			       unsigned fgrgba, unsigned bgrgba)
{
	struct cell *cell = &display->image[row * display->columns];

	if (row + rows != display->rows) {
		moveto(display, row + rows - lines, 0);
		outf(display, CTL_DELLINES, lines);
	}
	moveto(display, row, 0);
	background_color(display, bgrgba);
	outf(display, CTL_INSLINES, lines);
	memmove(&display->image[(row + lines) * display->columns],
		cell, (rows - lines) * display->columns * sizeof *cell);
	fill(display, row, 0, lines * display->columns, ' ', fgrgba, bgrgba);
}

static void delete_whole_lines(struct display *display, unsigned row,
			       unsigned lines, unsigned rows,
			       unsigned fgrgba, unsigned bgrgba)
{
	struct cell *cell = &display->image[row * display->columns];

	moveto(display, row, 0);
	background_color(display, bgrgba);
	outf(display, CTL_DELLINES, lines);
	if (row + rows != display->rows) {
		moveto(display, row + rows - lines, 0);
		background_color(display, bgrgba);
		outf(display, CTL_INSLINES, lines);
	}
	memmove(cell, cell + lines * display->columns,
		(rows - lines) * display->columns * sizeof *cell);
	fill(display, row + rows - lines, 0, lines * display->columns,
	     ' ', fgrgba, bgrgba);
}

static void shortpause(struct display *display, int millisec)
{
	struct timespec ts;
	flush(display);
	ts.tv_sec = 0;
	ts.tv_nsec = millisec * 1000 * 1000;
	nanosleep(&ts, NULL);
}

static void whole_lines(struct display *display, unsigned row,
			unsigned lines, unsigned rows,
			unsigned fgrgba, unsigned bgrgba,
			void (*mover)(struct display *, unsigned, unsigned,
				      unsigned, unsigned, unsigned))
{
	unsigned amount;
	for (; lines; lines -= amount) {
		amount = 1 + (lines > 2) + (lines > 4) + (lines > 8);
		mover(display, row, amount = 1 + (lines > 2),
		      rows, fgrgba, bgrgba);
		shortpause(display, 12 - 4 * amount);
	}
}

static void copy_line(struct display *display, unsigned to_row,
		      unsigned from_row, unsigned column, unsigned columns)
{
	struct cell *cell = &display->image[from_row * display->columns +
					    column];
	for (; columns--; cell++)
		display_put(display, to_row, column++,
			    cell->unicode, cell->fgrgba, cell->bgrgba);
}

void display_insert_lines(struct display *display, unsigned row,
			  unsigned column, unsigned lines,
			  unsigned rows, unsigned columns,
			  unsigned fgrgba, unsigned bgrgba)
{
	int j;

	if (!validate(display, row, column, &rows, &columns, &lines))
		return;
	if (!column && columns == display->columns)
		whole_lines(display, row, lines, rows, fgrgba, bgrgba,
			    insert_whole_lines);
	else {
		for (j = rows - lines; j-- > 0; )
			copy_line(display, row + lines + j, row + j,
				  column, columns);
		display_erase(display, row, column, lines, columns,
			      fgrgba, bgrgba);
	}
}

void display_delete_lines(struct display *display, unsigned row,
			  unsigned column, unsigned lines,
			  unsigned rows, unsigned columns,
			  unsigned fgrgba, unsigned bgrgba)
{
	int j;

	if (!validate(display, row, column, &rows, &columns, &lines))
		return;
	if (!column && columns == display->columns)
		whole_lines(display, row, lines, rows, fgrgba, bgrgba,
			    delete_whole_lines);
	else {
		for (j = 0; j < rows - lines; j++)
			copy_line(display, row + j, row + lines + j,
				  column, columns);
		display_erase(display, row + rows - lines, column,
			      lines, columns, fgrgba, bgrgba);
	}
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

void display_set_geometry(struct display *display,
			  unsigned rows, unsigned columns)
{
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

static void display_geometry(struct display *display)
{
	int rows = 0, columns = 0;
	struct winsize ws;

	if (!ioctl(1, TIOCGWINSZ, &ws)) {
		rows = ws.ws_row;
		columns = ws.ws_col;
	}
	if (!rows)
		rows = 24;
	if (!columns)
		columns = 80;
	moveto(display, 666, 666);
	outs(display, CTL_CURSORPOS);
	display_set_geometry(display, rows, columns);
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
		outs(display, XTERM_ALTSCREEN);
		outs(display, XTERM_BCKISDEL);
	} else
		outs(display, CTL_RESET);
	outs(display, CTL_UTF8
		      CTL_RESETMODES
		      CTL_RESETCOLORS
		      CTL_ERASEALL);
	if (!display->is_xterm)
		outs(display, CTL_NUMLOCK CTL_CLEARLEDS CTL_NUMLOCKLED);
	display->colors = 0;
	display->fgrgba = 1;
	display->bgrgba = ~0;
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
		outs(display, XTERM_REGSCREEN);
	else
		outs(display, CTL_RESET CTL_RESETMODES);
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
	if (display->is_xterm)
		outf(display, XTERM_TITLE, title ? title : "");
}

void display_cursor(struct display *display, unsigned row, unsigned column)
{
	if (row >= display->rows)
		row = display->rows - 1;
	if (column >= display->columns)
		column = display->columns - 1;
	moveto(display, display->cursor_row = row,
		display->cursor_column = column);
	foreground_color(display, display->image[row * display->columns +
						 column].fgrgba);
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
