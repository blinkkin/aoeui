/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#ifndef DISPLAY_H
#define DISPLAY_H

struct display;

typedef unsigned rgba_t;
#define DEFAULT_FGRGBA 0xff
#define DEFAULT_BGRGBA (~0)

struct display *display_init(void);
void display_reset(struct display *);
void display_end(struct display *);
void display_get_geometry(struct display *, unsigned *rows, unsigned *columns);
void display_title(struct display *, const char *);
void display_cursor(struct display *, unsigned row, unsigned column);
void display_put(struct display *, unsigned row, unsigned column,
		 Unicode_t unicode, rgba_t fgRGBA, rgba_t bgRGBA);
void display_erase(struct display *, unsigned row, unsigned column,
		   unsigned rows, unsigned columns,
		   rgba_t fgRGBA, rgba_t bgRGBA);
void display_beep(struct display *);
void display_sync(struct display *);

/* display_getch() implies a display_sync().
 *
 * Once ERROR_CHANGED is returned after a window size change,
 * it will continue to be returned until display_get_geometry() is called
 */
Unicode_t display_getch(struct display *, Boolean_t block);

/* hints */
void display_insert_spaces(struct display *, unsigned row, unsigned column,
			   unsigned spaces, unsigned columns,
			   rgba_t fgRGBA, rgba_t bgRGBA);
void display_delete_chars(struct display *, unsigned row, unsigned column,
			  unsigned chars, unsigned columns,
			  rgba_t fgRGBA, rgba_t bgRGBA);
void display_insert_lines(struct display *, unsigned row, unsigned column,
			  unsigned lines, unsigned rows, unsigned columns,
			  rgba_t fgRGBA, rgba_t bgRGBA);
void display_delete_lines(struct display *, unsigned row, unsigned column,
			  unsigned lines, unsigned rows, unsigned columns,
			  rgba_t fgRGBA, rgba_t bgRGBA);

#endif
