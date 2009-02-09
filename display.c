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
#if 0 /* broken on Apple Terminal, dammit */
# define CTL_ERASETOEND	CSI "J"
# define CTL_ERASELINE	CSI "K"
#endif
#define CTL_ERASECOLS	CSI "%dX"
#define CTL_DELCOLS	CSI "%uP"
#define CTL_DELLINES	CSI "%uM"
#define CTL_INSCOLS	CSI "%u@"
#define CTL_INSLINES	CSI "%uL"
#define CTL_RGB		OSC "4;%d;rgb:%02x/%02x/%02x" ST
#define CTL_CURSORPOS	CSI "6n"
#define FG_COLOR	30
#define BG_COLOR	40
#define XTERM_TITLE	OSC "0;%s" ST
/* was: #define XTERM_TITLE	OSC "2;%s" ESC "\\" */
#define XTERM_ALTSCREEN	CSI "?47h"
#define XTERM_REGSCREEN	CSI "?47l"
#define XTERM_BCKISDEL	CSI "?67l"
#define XTERM_LOCATOR	CSI "1'z" CSI "1'{" CSI "4'{"


struct cell {
	Unicode_t unicode;
	rgba_t fgrgba, bgrgba;
};

struct display {
	unsigned rows, columns;
	unsigned cursor_row, cursor_column;
	Boolean_t size_changed;
	rgba_t fgrgba, bgrgba;
	unsigned at_row, at_column;
	struct cell *image;
	struct display *next;
	Byte_t inbuf[INBUF_SIZE];
	char outbuf[OUTBUF_SIZE];
	size_t inbuf_bytes, outbuf_bytes;
	Boolean_t is_xterm;
	rgba_t color[MAX_COLORS];
	unsigned colors;
	char *title;
};

static struct display *display_list;
static void (*old_sigwinch)(int, siginfo_t *, void *);

static void emit(const char *str, size_t bytes)
{
	size_t wrote;
	ssize_t chunk;

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
	unsigned j;
	for (j = 0; j < 32; j += 8) {
		Byte_t c1 = rgba1 >> j, c2 = rgba2 >> j;
		mean |= c1 + c2 >> 1 << j;
	}
	return mean;
}

static unsigned colormap(struct display *display, rgba_t rgba)
{
	static rgba_t basic[] = {
		0, 0x7f000000, 0x007f0000, 0x7f7f0000,
		0x00007f00, 0x7f007f00, 0x007f7f00, 0x7f7f7f00,
	};
	static rgba_t bright[] = {
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

static void set_color(struct display *display, rgba_t rgba, unsigned magic)
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

void display_put(struct display *display, unsigned row, unsigned column,
		 Unicode_t unicode, rgba_t fgrgba, rgba_t bgrgba)
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
		out(display, buf, unicode_utf8(buf, unicode));
		display->at_column++;
		cell->unicode = unicode;
		cell->bgrgba = bgrgba;
	}
	cell->fgrgba = fgrgba;
}

static void fill(struct display *display, unsigned row, unsigned column,
		 unsigned columns, Unicode_t code, rgba_t fgrgba,
		 rgba_t bgrgba)
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
		   rgba_t fgrgba, rgba_t bgrgba)
{
	int r, c;

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
			    cell->fgrgba != fgrgba ||
			    cell++->bgrgba != bgrgba)
				goto doit;
	}
	return;

doit:	foreground_color(display, fgrgba);
	background_color(display, bgrgba);

#ifdef CTL_ERASETOEND
	if (!column && columns == display->columns && row + rows == display->rows) {
		moveto(display, row, column);
		outs(display, CTL_ERASETOEND);
	} else
#endif
#ifdef CTL_ERASELINE
	if (column + columns == display->columns) {
		for (r = 0; r < rows; r++) {
			moveto(display, row + r, column);
			outs(display, CTL_ERASELINE);
		}
	} else
#endif
	for (r = 0; r < rows; r++) {
		moveto(display, row + r, column);
		outf(display, CTL_ERASECOLS, columns);
	}

	for (; rows--; row++)
		fill(display, row, column, columns, ' ', fgrgba, bgrgba);
}

void display_insert_spaces(struct display *display, unsigned row,
			   unsigned column, unsigned spaces, unsigned columns,
			   rgba_t fgrgba, rgba_t bgrgba)
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
			  rgba_t fgrgba, rgba_t bgrgba)
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
	moveto(display, row, column + columns - chars);
	outf(display, CTL_INSCOLS, chars);
	cell = &display->image[row*display->columns + column];
	memmove(cell, cell + chars, (columns - chars) * sizeof *cell);
	fill(display, row, column + columns - chars, chars,
	     ' ', fgrgba, bgrgba);
}

static Boolean_t validate(struct display *display, unsigned row,
			  unsigned column, unsigned *rows, unsigned *columns,
			  unsigned *lines)
{
	if (row >= display->rows || column >= display->columns)
		return FALSE;
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
			       rgba_t fgrgba, rgba_t bgrgba)
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
			       rgba_t fgrgba, rgba_t bgrgba)
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
			rgba_t fgrgba, rgba_t bgrgba,
			void (*mover)(struct display *, unsigned, unsigned,
				      unsigned, rgba_t, rgba_t))
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
			  rgba_t fgrgba, rgba_t bgrgba)
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
			  rgba_t fgrgba, rgba_t bgrgba)
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
	size_t bytes = cells * sizeof(struct cell);
	struct cell *new = allocate(bytes);
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
			*cell = *(cell - display->columns);
			cell++->unicode = ' ';
		}
	}

	RELEASE(old);
	return new;
}

static void set_geometry(struct display *display,
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
	display->size_changed = TRUE;
	display_cursor(display, display->cursor_row, display->cursor_column);
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
		moveto(display, 666, 666);
		outs(display, CTL_CURSORPOS);
	}
	set_geometry(display, rows, columns);
}

void display_get_geometry(struct display *display,
			  unsigned *rows, unsigned *columns)
{
	*rows = display->rows;
	*columns = display->columns;
	display->size_changed = FALSE;
}

void display_reset(struct display *display)
{
	RELEASE(display->image);
	display->cursor_row = display->cursor_column = 0;
	display->at_row = display->at_column = 0;
	if (display->is_xterm)
		outs(display, XTERM_ALTSCREEN);
	else
		outs(display, CTL_RESET);
	outs(display, CTL_UTF8
		      CTL_RESETMODES
		      CTL_RESETCOLORS
		      CTL_ERASEALL);
	if (display->is_xterm)
		outs(display, XTERM_BCKISDEL);
	else
		outs(display, CTL_NUMLOCK CTL_CLEARLEDS CTL_NUMLOCKLED);
	display->colors = 0;
	display->fgrgba = DEFAULT_FGRGBA;
	display->bgrgba = DEFAULT_BGRGBA;
	geometry(display);
	display_sync(display);
}

static void sigwinch(int signo, siginfo_t *info, void *data)
{
	struct display *display;
	for (display = display_list; display; display = display->next)
		geometry(display);
	if (old_sigwinch)
		old_sigwinch(signo, info, data);
}

#if !(defined __linux__ || defined __APPLE__)
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
	const char *term;

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
	display->is_xterm = (term = getenv("TERM")) &&
			    !strncmp(term, "xterm", 5);
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
	set_color(display, DEFAULT_FGRGBA, BG_COLOR);
	set_color(display, DEFAULT_BGRGBA, FG_COLOR);
	if (display->is_xterm)
		outs(display, XTERM_REGSCREEN);
	else
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

void display_title(struct display *display, const char *title)
{
	if (display->is_xterm) {
		if (title && !*title)
			title = NULL;
		if (!title && !display->title)
			return;
		if (!title)
			RELEASE(display->title);
		else if (display->title && !strcmp(title, display->title))
			return;
		if (title)
			display->title = strdup(title);
		else
			title = "";
		outf(display, XTERM_TITLE, title ? title : "");
	}
}

void display_cursor(struct display *display, unsigned row, unsigned column)
{
	if (row >= display->rows)
		row = display->rows - 1;
	if (column >= display->columns)
		column = display->columns - 1;
	moveto(display, display->cursor_row = row,
		display->cursor_column = column);
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

#define SET_GEOMETRY FUNCTION_F(99)

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
				key = SET_GEOMETRY;
			break;
		case '~':
			if (!val)
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
	if (key == SET_GEOMETRY) {
		set_geometry(display, val[0], val[1]);
		goto again;
	}
	return key;
}
