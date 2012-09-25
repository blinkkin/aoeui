/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

/*
 *	Bandwidth-optimized terminal display management.
 *	Maintain an image of the display surface and
 *	update it when repainting differs from the image.
 *	Assume lowest-common-denominator terminal emulation.
 *
 *	reference: man 4 console_codes
 */

#define OUTBUF_SIZE	1024
#define INBUF_SIZE	64
#define MAX_COLORS	8

#define ESCCHAR '\x1b'
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
#define CTL_CURSORRGB	OSC "12;rgb:%02x/%02x/%02x" ST
#define CTL_CURSORPOS	CSI "6n"
#define FG_COLOR	30
#define BG_COLOR	40
#define XTERM_TITLE	OSC "0;%s" ST
#define XTERM_ALTSCREEN	CSI "?47h"
#define XTERM_REGSCREEN	CSI "?47l"
#define XTERM_BCKISDEL	CSI "?67l"
#define XTERM_LOCATOR	CSI "1'z" CSI "1'{" CSI "4'{"

#define BAD_RGBA 0x01010100

struct cell {
	Unicode_t unicode;
	rgba_t fgrgba, bgrgba;
};

enum cursor_position_knowledge {
	NEEDED, SOUGHT, KNOWN, INVALID
};

struct display {
	int rows, columns;
	int cursor_row, cursor_column;
	rgba_t cursor_rgba;
	Boolean_t size_changed;
	rgba_t fgrgba, bgrgba;
	int at_row, at_column;
	enum cursor_position_knowledge get_initial_cursor_position;
	int initial_row, initial_column;
	struct cell *image;
	struct display *next;
	Byte_t inbuf[INBUF_SIZE];
	char outbuf[OUTBUF_SIZE];
	size_t inbuf_bytes, outbuf_bytes;
	Boolean_t is_xterm, is_linux, is_apple;
	rgba_t color[MAX_COLORS];
	unsigned colors;
	char *title;
};

struct termios original_termios;
static struct display *display_list;
static void (*old_sigwinch)(int, siginfo_t *, void *);
static FILE *debug_file;

static void emit(const char *str, size_t bytes)
{
	size_t wrote;
	ssize_t chunk;

	if (debug_file && bytes) {
		fprintf(debug_file, "emit %d:", (int) bytes);
		fwrite(str, bytes, 1, debug_file);
		fputc('\n', debug_file);
	}

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
	emit(display->outbuf, display->outbuf_bytes);
	display->outbuf_bytes = 0;
}

static void out(struct display *display, const char *str, size_t bytes)
{
	if (display->outbuf_bytes + bytes > sizeof display->outbuf)
		flush(display);
	if (bytes > sizeof display->outbuf)
		emit(str, bytes);
	else {
		memcpy(display->outbuf + display->outbuf_bytes, str, bytes);
		display->outbuf_bytes += bytes;
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

static void force_moveto(struct display *display, int row, int column)
{
	outf(display, CTL_GOTO, (display->at_row = row) + 1,
	     (display->at_column = column) + 1);
}

static void moveto(struct display *display, int row, int column)
{
	if (row == display->at_row &&
	    column >= display->at_column &&
	    column < display->at_column + 8) {
		int j, cols = column - display->at_column;
		struct cell *cell = &display->image[row*display->columns +
						    display->at_column];
		for (j = 0; j < cols; j++)
			if (cell[j].unicode != ' ' ||
			    cell[j].bgrgba != display->bgrgba)
				break;
		if (j == cols) {
			while (j-- > 0)
				outs(display, " ");
			display->at_column = column;
			return;
		}
	} else if (row == display->at_row + 1) {
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
	force_moveto(display, row, column);
}

void display_sync(struct display *display)
{
	moveto(display, display->cursor_row, display->cursor_column);
	flush(display);
}

static unsigned linux_colormap(rgba_t rgba)
{
	unsigned bgr1;
	if (rgba & 0xff)
		return 9; /* any alpha implies "default color" */
	bgr1 = !!(rgba >> 24);
	bgr1 |= !!(rgba >> 16 & 0xff) << 1;
	bgr1 |= !!(rgba >>  8 & 0xff) << 2;
	return bgr1;
}

static unsigned color_delta(rgba_t rgba1, rgba_t rgba2)
{
	unsigned delta = 1, j;
	if (rgba1 >> 8 == rgba2 >> 8)
		return 0;
	for (j = 8; j < 32; j += 8) {
		Byte_t c1 = rgba1 >> j, c2 = rgba2 >> j;
		int cd = c1 - c2;
		delta *= (cd < 0 ? -cd : cd) + 1;
	}
	return delta;
}

static rgba_t color_mean(rgba_t rgba1, rgba_t rgba2)
{
	rgba_t mean = 0;
	int j;
	for (j = 0; j < 32; j += 8) {
		Byte_t c1 = rgba1 >> j, c2 = rgba2 >> j;
		mean |= c1 + c2 >> 1 << j;
	}
	return mean;
}

static unsigned colormap(struct display *display, rgba_t rgba)
{
	static rgba_t basic[] = {
		BLACK_RGBA, PALE_RGBA(RED_RGBA), PALE_RGBA(GREEN_RGBA),
		PALE_RGBA(YELLOW_RGBA), PALE_RGBA(BLUE_RGBA),
		PALE_RGBA(MAGENTA_RGBA), PALE_RGBA(CYAN_RGBA),
		PALE_RGBA(WHITE_RGBA),
	};
	static rgba_t bright[] = {
		BLACK_RGBA, RED_RGBA, GREEN_RGBA, YELLOW_RGBA,
		BLUE_RGBA, MAGENTA_RGBA, CYAN_RGBA, WHITE_RGBA,
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

static void set_color(struct display *display, rgba_t rgba, unsigned magic)
{
	if (display->is_linux)
		outf(display, CSI "%dm", linux_colormap(rgba) + magic);
	else {
		unsigned c = colormap(display, rgba);
		if (c >= 18)
			outf(display, CSI "%d;5;%dm", magic+8, c);
		else if (c >= 10)
			outf(display, CSI "%dm", c - 10 + magic + 60);
		else
			outf(display, CSI "%dm", c + magic);
	}
}

static void background_color(struct display *display, rgba_t rgba)
{
	if (rgba != display->bgrgba) {
		set_color(display, rgba, BG_COLOR);
		display->bgrgba = rgba;
	}
}

static void foreground_color(struct display *display, rgba_t rgba)
{
	if (rgba != display->fgrgba) {
		set_color(display, rgba, FG_COLOR);
		display->fgrgba = rgba;
	}
}

static void default_colors(struct display *display) {
	foreground_color(display, DEFAULT_FGRGBA);
	background_color(display, DEFAULT_BGRGBA);
}

void display_put(struct display *display, int row, int column,
		 Unicode_t unicode, rgba_t fgrgba, rgba_t bgrgba)
{
	char buf[8];
	struct cell *cell;
	if (row < 0 || row >= display->rows ||
	    column < 0 || column >= display->columns)
		return;
	if (unicode < ' ')
		unicode = ' ';
	cell = &display->image[row*display->columns + column];
	if (cell->unicode == unicode &&
	    (cell->fgrgba == fgrgba || unicode == ' ') &&
	    cell->bgrgba == bgrgba)
		return;
	moveto(display, row, column);
	background_color(display, bgrgba);
	if (unicode != ' ')
		foreground_color(display, fgrgba);
	out(display, buf, unicode_utf8(buf, unicode));
	display->at_column++;
	/* display->image may have moved due to resize */
	cell = &display->image[row*display->columns + column];
	cell->unicode = unicode;
	cell->bgrgba = bgrgba;
	cell->fgrgba = fgrgba;
}

static Boolean_t color_fill(struct display *display,
			    int row, int rows, int column, int columns,
			    rgba_t fgrgba, rgba_t bgrgba)
{
	int r, c;
	Boolean_t any = FALSE;
	for (r = 0; r < rows; r++) {
		struct cell *cell = &display->image[(row+r) * display->columns +
						    column];
		for (c = 0; c < columns; c++, cell++)
			if (cell->fgrgba != fgrgba ||
			    cell->bgrgba != bgrgba) {
				cell->fgrgba = fgrgba;
				cell->bgrgba = bgrgba;
				any = TRUE;
			}
	}
	return any;
}

/* After an erase, insert, or delete command, update the state.
 * These commands affect an Apple Terminal differently than other
 * terminal emulators with respect to the background colors;
 * Apple fills with the default background, while a regular Xterm
 * and the Linux console fill with the most recent color selections.
 * We avoid the discrepancy by setting the colors to the default values
 * prior to executing erase, insert, and delete commands in the functions
 * that complete their work by calling this routine.
 * Returns TRUE if any state was modified.
 */
static Boolean_t space_fill(struct display *display, int row, int rows,
			    int column, int columns)
{
	int r, c;
	Boolean_t any = FALSE;
	rgba_t fg = display->fgrgba, bg = display->bgrgba;

	if (display->is_apple) {
		fg = DEFAULT_FGRGBA;
		bg = DEFAULT_BGRGBA;
	}

	for (r = 0; r < rows; r++) {
		struct cell *cell = &display->image[(row+r) * display->columns +
						    column];
		for (c = 0; c < columns; c++, cell++)
			if (cell->unicode != ' ') {
				cell->unicode = ' ';
				any = TRUE;
			}
	}

	if (color_fill(display, row, rows, column, columns, fg, bg))
		any = TRUE;
	return any;
}

void display_erase(struct display *display, int row, int column,
		   int rows, int columns)
{
	int r;

	if (row < 0 || row >= display->rows ||
	    column < 0 || column >= display->columns)
		return;
	if (row + rows > display->rows)
		rows = display->rows - row;
	if (column + columns > display->columns)
		columns = display->columns - column;
	if (rows <= 0 || columns <= 0)
		return;
	default_colors(display);
	if (!space_fill(display, row, rows, column, columns))
		return;
	if (!column &&
	    columns == display->columns &&
	    row + rows == display->rows) {
		if (!row)
			outs(display, CTL_ERASEALL);
		else {
			moveto(display, row, column);
			outs(display, CTL_ERASETOEND);
		}
	} else if (column + columns == display->columns) {
		for (r = 0; r < rows; r++) {
			moveto(display, row + r, column);
			outs(display, CTL_ERASELINE);
		}
	} else {
		for (r = 0; r < rows; r++) {
			moveto(display, row + r, column);
			outf(display, CTL_ERASECOLS, columns);
		}
	}
}

void display_insert_spaces(struct display *display, int row, int column,
			   int spaces, int columns)
{
	struct cell *cell;

	if (row < 0 || row >= display->rows ||
	    column < 0 || column >= display->columns)
		return;
	if (column + columns > display->columns)
		columns = display->columns - column;
	if (spaces > columns)
		spaces = columns;
	if (spaces <= 0)
		return;
	default_colors(display);
	if (column + columns != display->columns) {
		moveto(display, row, column + columns - spaces);
		outf(display, CTL_DELCOLS, spaces);
	}
	moveto(display, row, column);
	outf(display, CTL_INSCOLS, spaces);
	cell = &display->image[row*display->columns + column];
	memmove(cell + spaces, cell, (columns - spaces) * sizeof *cell);
	space_fill(display, row, 1, column, spaces);
}

void display_delete_chars(struct display *display, int row, int column,
			  int chars, int columns)
{
	struct cell *cell;

	if (row < 0 || row >= display->rows ||
	    column < 0 || column >= display->columns)
		return;
	if (column + columns > display->columns)
		columns = display->columns - column;
	if (chars > columns)
		chars = columns;
	if (chars <= 0)
		return;
	default_colors(display);
	moveto(display, row, column);
	outf(display, CTL_DELCOLS, chars);
	moveto(display, row, column + columns - chars);
	outf(display, CTL_INSCOLS, chars);
	cell = &display->image[row*display->columns + column];
	memmove(cell, cell + chars, (columns - chars) * sizeof *cell);
	space_fill(display, row, 1, column + columns - chars, chars);
}

static Boolean_t validate(struct display *display, int row, int column,
			  int *rows, int *columns, int *lines)
{
	if (row < 0 || row >= display->rows ||
	    column < 0 || column >= display->columns)
		return FALSE;
	if (row + *rows > display->rows)
		*rows = display->rows - row;
	if (*lines > *rows)
		*lines = *rows;
	if (column + *columns > display->columns)
		*columns = display->columns - column;
	return *lines > 0 && *columns > 0;
}

void display_insert_lines(struct display *display, int row, int column,
			  int lines, int rows, int columns)
{
	if (column ||
	    columns != display->columns ||
	    !validate(display, row, column, &rows, &columns, &lines))
		return;
	default_colors(display);
	if (row + rows != display->rows) {
		moveto(display, row + rows - lines, 0);
		outf(display, CTL_DELLINES, lines);
	}
	moveto(display, row, 0);
	outf(display, CTL_INSLINES, lines);
	memmove(&display->image[(row + lines) * display->columns],
		&display->image[row * display->columns],
		(rows - lines) * display->columns * sizeof *display->image);
	space_fill(display, row, lines, 0, display->columns);
}

void display_delete_lines(struct display *display, int row, int column,
			  int lines, int rows, int columns)
{
	struct cell *cell;  /* assigned later for safety from resizing */

	if (column ||
	    columns != display->columns ||
	    !validate(display, row, column, &rows, &columns, &lines))
		return;
	default_colors(display);
	moveto(display, row, 0);
	outf(display, CTL_DELLINES, lines);
	if (row + rows != display->rows) {
		moveto(display, row + rows - lines, 0);
		outf(display, CTL_INSLINES, lines);
	}
	cell = &display->image[row * display->columns];
	memmove(cell, cell + lines * display->columns,
		(rows - lines) * display->columns * sizeof *cell);
	space_fill(display, row + rows - lines, lines, 0, display->columns);
}

static struct cell *resize(struct display *display, struct cell *old,
			   int old_rows, int old_columns,
			   int new_rows, int new_columns)
{
	int cells = new_rows * new_columns;
	size_t bytes = cells * sizeof *old;
	struct cell *new = allocate(bytes);
	int min_rows = new_rows < old_rows ? new_rows : old_rows;
	int min_columns = new_columns < old_columns ? new_columns : old_columns;
	int j, k;

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
			*cell = *(cell - display->columns);
			cell++->unicode = ' ';
		}
	}

	RELEASE(old);
	return new;
}

static void set_geometry(struct display *display, int rows, int columns)
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
	display->size_changed = TRUE;
	display_erase(display, 0, 0, rows, columns);
	display_cursor(display, display->cursor_row, display->cursor_column);
	if (display->get_initial_cursor_position == KNOWN)
		display->get_initial_cursor_position = INVALID;
}

static void geometry(struct display *display)
{
	int rows = 0, columns = 0;
	struct winsize ws;
	const char *p;
	unsigned n;

#ifdef TIOCGWINSZ
	if (!ioctl(1, TIOCGWINSZ, &ws)) {
		rows = ws.ws_row;
		columns = ws.ws_col;
	}
#endif
	if (!rows &&
	    (p = getenv("ROWS")) &&
	    (n = atoi(p)) &&
	    n <= 100)
		rows = n;
	if (!columns &&
	    (p = getenv("COLUMNS")) &&
	    (n = atoi(p)) &&
	    n <= 200)
		columns = n;
	if (!rows || !columns) {
		if (!rows)
			rows = 24;
		if (!columns)
			columns = 80;
		force_moveto(display, 666, 666);
		outs(display, CTL_CURSORPOS);
	}
	set_geometry(display, rows, columns);
}

void display_get_geometry(struct display *display, int *rows, int *columns)
{
	*rows = display->rows;
	*columns = display->columns;
	display->size_changed = FALSE;
}

void display_reset(struct display *display)
{
	RELEASE(display->image);
	if (display->is_xterm) {
		if (display->get_initial_cursor_position == NEEDED) {
			display->get_initial_cursor_position = SOUGHT;
			outs(display, CTL_CURSORPOS);
		}
		outs(display, XTERM_ALTSCREEN);
	} else
		outs(display, CTL_RESET);
	outs(display, CTL_UTF8
		      CTL_RESETMODES
		      CTL_RESETCOLORS);
	if (display->is_xterm) {
		outs(display, XTERM_BCKISDEL);
		/* reset character attributes (blink, etc.) */
		outs(display, CSI "0;24;25;27;28;39;49m");
	}
	if (display->is_linux)
		outs(display, CTL_NUMLOCK CTL_CLEARLEDS CTL_NUMLOCKLED);
	display->colors = 0;
	display->fgrgba = BAD_RGBA;
	display->bgrgba = BAD_RGBA;
	geometry(display);
	force_moveto(display, 0, 0);
	display->cursor_row = display->cursor_column = 0;
	display->cursor_rgba = BAD_RGBA;
	flush(display);
}

static void sigwinch(int signo, siginfo_t *info, void *data)
{
	struct display *display;
	for (display = display_list; display; display = display->next)
		geometry(display);
	if (old_sigwinch)
		old_sigwinch(signo, info, data);
}

#if !(defined __linux__ || defined BSD || defined __APPLE__)
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
	struct display *display = allocate0(sizeof *display);
	struct termios termios = original_termios;
	const char *term, *path;

	if ((path = getenv("DISPLAY_DEBUG_PATH")) &&
	    !(debug_file = fopen(path, "w")))
		die("could not open display debug file %s for writing", path);

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
	display->inbuf_bytes = display->outbuf_bytes = 0;
	if (!(term = getenv("TERM"))) {
	} else if ((display->is_xterm = !strncmp(term, "xterm", 5))) {
		if ((term = getenv("COLORTERM")) &&
		    !strncmp(term, "gnome-", 6)) {
			/* Gnome terminal? Ignore $TERM_PROGRAM */
		} else if ((term = getenv("TERM_PROGRAM")))
			display->is_apple = !strcmp(term, "Apple_Terminal");
	} else {
		display->is_linux = !strcmp(term, "linux") ||
				    !strcmp(term, "network");
	}
	display_reset(display);
	return display;
}

void display_end(struct display *display)
{
	struct display *d, *prev = NULL;

	if (!display)
		return;

	display_title(display, NULL);
	default_colors(display);
	if (display->is_xterm) {
		outs(display, CTL_ERASEALL
			      CTL_RESETMODES
			      CTL_RESETCOLORS
			      XTERM_REGSCREEN);
		if (display->get_initial_cursor_position == KNOWN)
			force_moveto(display, display->initial_row,
				     display->initial_column);
		else
			force_moveto(display, display->rows-1, 0);
	} else
		outs(display, CTL_RESET CTL_RESETMODES);
	flush(display);

	tcsetattr(1, TCSADRAIN, &original_termios);

	RELEASE(display->image);

	for (d = display_list; d; prev = d, d = d->next)
		if (d == display) {
			if (prev)
				prev->next = display->next;
			else
				display_list = display->next;
			break;
		}

	RELEASE(display);
}

Boolean_t display_title(struct display *display, const char *title)
{
	if (!display->is_xterm)
		return FALSE;
	if (title && !*title)
		title = NULL;
	if (!title && !display->title)
		return TRUE;
	if (!title)
		RELEASE(display->title);
	else if (display->title && !strcmp(title, display->title))
		return TRUE;
	if (title)
		display->title = strdup(title);
	else
		title = "";
	outf(display, XTERM_TITLE, title ? title : "");
	return TRUE;
}

void display_cursor(struct display *display, int row, int column)
{
	if (row >= display->rows)
		row = display->rows - 1;
	if (row < 0)
		row = 0;
	if (column >= display->columns)
		column = display->columns - 1;
	if (column < 0)
		column = 0;
	display->cursor_row = row;
	display->cursor_column = column;
}

Boolean_t display_cursor_color(struct display *display, rgba_t rgba)
{
	if (!display->is_xterm ||
	    display->is_apple ||
	    rgba & 0xff /* no alpha */)
		return FALSE;
	if (rgba != display->cursor_rgba) {
		outf(display, CTL_CURSORRGB,
		     rgba >> 24, rgba >> 16 & 0xff, rgba >> 8 & 0xff);
		display->cursor_rgba = rgba;
	}
	return TRUE;
}

void display_beep(struct display *display)
{
	emit("\a", 1);
	display_sync(display);
}

Unicode_t display_getch(struct display *display, Boolean_t block)
{
	Byte_t *p;
	Unicode_t key;
	unsigned used, vals, val[16];

#define GOT_CURSORPOS FUNCTION_F(99)

	if (!display)
		return ERROR_EOF;

again:	if (display->size_changed)
		return ERROR_CHANGED;
	display_sync(display);
	if (display->inbuf_bytes >= sizeof display->inbuf - 1)
		;
	else if (!multiplexor(block)) {
		if (!display->inbuf_bytes)
			return ERROR_EMPTY;
	} else {
		int n;
		do {
			errno = 0;
			n = read(0, display->inbuf + display->inbuf_bytes,
				 sizeof display->inbuf - 1 -
					display->inbuf_bytes);
		} while (n < 0 && (errno == EAGAIN || errno == EINTR));
		if (debug_file) {
			fprintf(debug_file, "read %d:", n);
			if (n > 0)
				fwrite(display->inbuf + display->inbuf_bytes,
				       n, 1, debug_file);
			fputc('\n', debug_file);
		}
		if (!n)
			return ERROR_EOF;
		if (n < 0)
			return ERROR_INPUT;
		display->inbuf[display->inbuf_bytes += n] = '\0';
	}

	p = display->inbuf;
	if (*p != ESCCHAR) {
		if (utf8_bytes[*p] > display->inbuf_bytes) {
			if (block)
				goto again;
			return ERROR_EMPTY;
		}
		used = utf8_length((char *) p, display->inbuf_bytes);
		key = utf8_unicode((char *) p, used);
		p += used - 1;
		goto done;
	}

	/* Translate Escape characters */

	key = vals = 0;
	switch (p[1]) {

	case '[':
		for (p += 2; isdigit(*p); p++) {
			val[vals] = 0;
			do {
				val[vals] *= 10;
				val[vals] += *p++ - '0';
			} while (isdigit(*p));
			vals++;
			if (*p != ';')
				break;
			if (vals == 16)
				vals--;
		}
		switch (*p) {
		case 'R': /* cursor position report from southeast corner */
			if (vals >= 2)
				key = GOT_CURSORPOS;
			break;
		case '~':
			if (!val[0])
				break;
			switch (val[0]) {
			case  2: key = FUNCTION_INSERT;	break;
			case  3: key = FUNCTION_DELETE;	break;
			case  5: key = FUNCTION_PGUP;	break;
			case  6: key = FUNCTION_PGDOWN;	break;
			case 15: key = FUNCTION_F(5);	break;
			case 17: key = FUNCTION_F(6);	break;
			case 18: key = FUNCTION_F(7);	break;
			case 19: key = FUNCTION_F(8);	break;
			case 20: key = FUNCTION_F(9);	break;
			case 21: key = FUNCTION_F(10);	break;
	/*pmk?*/	case 22: key = FUNCTION_F(11);	break;
			case 24: key = FUNCTION_F(12);	break;
			}
			break;
		case 'A': key = FUNCTION_UP;	break;
		case 'B': key = FUNCTION_DOWN;	break;
		case 'C': key = FUNCTION_RIGHT;	break;
		case 'D': key = FUNCTION_LEFT;	break;
		case 'F': key = FUNCTION_END;	break;
		case 'H': key = FUNCTION_HOME;	break;
		case '\0': /* truncated escape sequence; get more */
			if (!block)
				return ERROR_EMPTY;
			goto again;
		}
		break;

	case 'O':
		switch (*(p += 2)) {
		case 'H': key = FUNCTION_HOME;	break;
		case 'F': key = FUNCTION_END;	break;
		case 'P': key = FUNCTION_F(1);	break;
		case 'Q': key = FUNCTION_F(2);	break;
		case 'R': key = FUNCTION_F(3);	break;
		case 'S': key = FUNCTION_F(4);	break;
		}
		break;

	/* Translate many Escape'd characters into Control */
	case '@':
	case ' ':
		key = CONTROL('@');
		p++;
		break;
	case '\\':
	case ']':
	case '^':
	case '_':
		key = CONTROL(*++p);
		break;
	case '/':
		key = CONTROL('_');
		p++;
		break;
	case '?':
		key = '\x7f';
		p++;
		break;
	case ESCCHAR:
		memmove(display->inbuf, p+2, display->inbuf_bytes -= 2);
		goto again;
	case '\0': /* truncated escape sequence; get more */
		if (!block)
			return ERROR_EMPTY;
		goto again;
	default:
		if (p[1] < ' ')
			key = *++p;  /* ignore Esc before control character */
		else if (p[1] >= 'a' && p[1] <= 'z')
			key = CONTROL(*++p-'a'+'A');
		else if (p[1] >= 'A' && p[1] <= 'Z')
			key = CONTROL(*++p);
	}

done:	if (!key)
		key = *(p = display->inbuf);
	used = ++p - display->inbuf;
	memmove(display->inbuf, p, display->inbuf_bytes -= used);
	if (key == GOT_CURSORPOS) {
		if (display->get_initial_cursor_position == SOUGHT &&
		    val[0] > 0 && val[1] > 0) {
			display->get_initial_cursor_position = KNOWN;
			display->initial_row = val[0] - 1;
			display->initial_column = val[1] - 1;
		} else
			set_geometry(display, val[0], val[1]);
		goto again;
	}
	return key;
}
