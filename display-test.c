#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include "types.h"
#include "utf8.h"
#include "display.h"
#define RED 0xff000000
#define GREEN 0x00ff0000
#define BLUE 0x0000ff00
#define WHITE 0xffffff00
#define BLACK 0

void die(const char *, ...);
Boolean_t multiplexor(Boolean_t);

static struct display *D;
static int rows, columns;

static void dfill(int row, int rows, int col, int cols,
		  int ch, rgba_t fgrgba, rgba_t bgrgba)
{
	int j, k;
	for (j = 0; j < rows; j++)
		for (k = 0; k < cols; k++)
			display_put(D, row+j, col+k, ch, fgrgba, bgrgba);
}

static void dprint(int row, int col, rgba_t fgrgba, rgba_t bgrgba,
		   const char *format, ...)
{
	char buffer[256], *p;
	va_list ap;
	va_start(ap, format);
	vsnprintf(buffer, sizeof buffer, format, ap);
	va_end(ap);
	for (p = buffer; *p; p++)
		display_put(D, row, col++, *p, fgrgba, bgrgba);
}

static void dpause(void)
{
	int ch;
	dfill(rows-1, 1, 0, columns, ' ', RED, WHITE);
	dprint(rows-1, 0, RED, WHITE, "Hit Q to quit, or any other key to continue...");
	while ((ch = display_getch(D, 1)) == ERROR_CHANGED)
		display_get_geometry(D, &rows, &columns);
	if (ch == 'q' || ch == 'Q') {
		display_end(D);
		exit(EXIT_SUCCESS);
	}
}

void die(const char *msg, ...)
{
	va_list ap;
	display_end(D);
	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

Boolean_t multiplexor(Boolean_t block)
{
	return TRUE;
}

int main(void)
{
	if (tcgetattr(1, &original_termios))
		die("not running in a terminal");
	D = display_init();
	display_get_geometry(D, &rows, &columns);
	display_title(D, "display-test");
	dprint(0, 0, BLACK, WHITE, "geometry: %d rows, %d columns", rows, columns);
	dprint(1, 0, DEFAULT_FGRGBA, DEFAULT_BGRGBA, "default foreground and background");
	dprint(2, 0, WHITE, BLACK, "white foreground on black background");
	dprint(3, 0, BLACK, WHITE, "black foreground on white background");
	dprint(4, 0, RED, BLUE, "red foreground on blue background");
	dprint(5, 0, BLUE, RED, "blue foreground on red background");
	dpause();

	dfill(0, rows, 0, columns, '.', RED, GREEN);
	dprint(0, 0, BLUE, RED, "filled with red dots on green");
	dpause();

	display_erase(D, 0, 0, rows, columns);
	dprint(0, 0, BLUE, RED, "after ERASEALL display_erase()");
	dpause();

	dfill(0, rows, 0, columns, '.', RED, GREEN);
	display_erase(D, rows/2, 0, rows - rows/2, columns);
	dprint(0, 0, BLUE, RED, "after ERASETOEND display_erase()");
	dpause();

	dfill(0, rows, 0, columns, '.', RED, GREEN);
	display_erase(D, 1, columns/2, 1, columns);
	dprint(0, 0, BLUE, RED, "after ERASELINE display_erase()");
	dpause();

	dfill(0, rows, 0, columns, '.', RED, GREEN);
	display_erase(D, 1, 0, 1, columns/2);
	dprint(0, 0, BLUE, RED, "after ERASECOLS display_erase()");
	dpause();

	dfill(0, rows, 0, columns, '.', RED, GREEN);
	display_insert_lines(D, 2, 0, rows/2, rows-2, columns);
	dprint(0, 0, BLUE, RED, "after display_insert_lines()");
	dpause();

	dfill(0, rows, 0, columns, '.', RED, GREEN);
	dfill(rows-1, 1, 0, columns, '*', RED, GREEN);
	display_delete_lines(D, 2, 0, rows/2, rows-2, columns);
	dprint(0, 0, BLUE, RED, "after display_delete_lines()");
	dpause();

	dfill(0, rows, 0, columns, '.', RED, GREEN);
	display_insert_spaces(D, 2, 0, columns/2, columns);
	display_insert_spaces(D, 3, columns/2, columns - (columns/2), columns);
	dprint(0, 0, BLUE, RED, "after display_insert_spaces()");
	dpause();

	dfill(0, rows, 0, columns, '.', RED, GREEN);
	display_delete_chars(D, 2, 0, columns/2, columns);
	display_delete_chars(D, 3, columns/2, columns - (columns/2), columns);
	dprint(0, 0, BLUE, RED, "after display_delete_chars()");
	dpause();

	display_end(D);
	printf("display-test done\n");
	return EXIT_SUCCESS;
}
