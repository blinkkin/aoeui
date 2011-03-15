/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#ifndef DISPLAY_H
#define DISPLAY_H

extern struct termios original_termios;

struct display;

typedef unsigned rgba_t;
#define DEFAULT_FGRGBA 0xff
#define DEFAULT_BGRGBA (~0)

struct display *display_init(void);
void display_reset(struct display *);
void display_end(struct display *);
void display_get_geometry(struct display *, int *rows, int *columns);
void display_title(struct display *, const char *);
void display_cursor(struct display *, int row, int column);
void display_put(struct display *, int row, int column,
		 Unicode_t unicode, rgba_t fgRGBA, rgba_t bgRGBA);
void display_beep(struct display *);
void display_sync(struct display *);

/* display_getch() implies a display_sync().
 *
 * Once ERROR_CHANGED is returned after a window size change,
 * it will continue to be returned until display_get_geometry() is called
 */
Unicode_t display_getch(struct display *, Boolean_t block);

/* hints */
void display_erase(struct display *, int row, int column,
		   int rows, int columns);
void display_insert_spaces(struct display *, int row, int column,
			   int spaces, int columns);
void display_delete_chars(struct display *, int row, int column,
			  int chars, int columns);
void display_insert_lines(struct display *, int row, int column,
			  int lines, int rows, int columns);
void display_delete_lines(struct display *, int row, int column,
			  int lines, int rows, int columns);

#endif
