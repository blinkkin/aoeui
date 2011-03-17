#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include "types.h"
#include "utf8.h"
#include "display.h"

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
	dfill(rows-1, 1, 0, columns, ' ', RED_RGBA, WHITE_RGBA);
	dprint(rows-1, 0, RED_RGBA, WHITE_RGBA, "Hit Q to quit, or any other key to continue...");
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
	dprint(0, 0, BLACK_RGBA, WHITE_RGBA, "geometry: %d rows, %d columns", rows, columns);
	dprint(1, 0, DEFAULT_FGRGBA, DEFAULT_BGRGBA, "default foreground and background");
	dprint(2, 0, WHITE_RGBA, BLACK_RGBA, "white foreground on black background");
	dprint(3, 0, BLACK_RGBA, WHITE_RGBA, "black foreground on white background");
	dprint(4, 0, RED_RGBA, BLUE_RGBA, "red foreground on blue background");
	dprint(5, 0, BLUE_RGBA, RED_RGBA, "blue foreground on red background");
	dpause();

	dfill(0, rows, 0, columns, '.', RED_RGBA, GREEN_RGBA);
	dprint(0, 0, BLUE_RGBA, RED_RGBA, "filled with red dots on green");
	dpause();

	display_erase(D, 0, 0, rows, columns);
	dprint(0, 0, BLUE_RGBA, RED_RGBA, "after ERASEALL display_erase()");
	dpause();

	dfill(0, rows, 0, columns, '.', RED_RGBA, GREEN_RGBA);
	display_erase(D, rows/2, 0, rows - rows/2, columns);
	dprint(0, 0, BLUE_RGBA, RED_RGBA, "after ERASETOEND display_erase()");
	dpause();

	dfill(0, rows, 0, columns, '.', RED_RGBA, GREEN_RGBA);
	display_erase(D, 1, columns/2, 1, columns);
	dprint(0, 0, BLUE_RGBA, RED_RGBA, "after ERASELINE display_erase()");
	dpause();

	dfill(0, rows, 0, columns, '.', RED_RGBA, GREEN_RGBA);
	display_erase(D, 1, 0, 1, columns/2);
	dprint(0, 0, BLUE_RGBA, RED_RGBA, "after ERASECOLS display_erase()");
	dpause();

	dfill(0, rows, 0, columns, '.', RED_RGBA, GREEN_RGBA);
	display_insert_lines(D, 2, 0, rows/2, rows-2, columns);
	dprint(0, 0, BLUE_RGBA, RED_RGBA, "after display_insert_lines()");
	dpause();

	dfill(0, rows, 0, columns, '.', RED_RGBA, GREEN_RGBA);
	dfill(rows-1, 1, 0, columns, '*', RED_RGBA, GREEN_RGBA);
	display_delete_lines(D, 2, 0, rows/2, rows-2, columns);
	dprint(0, 0, BLUE_RGBA, RED_RGBA, "after display_delete_lines()");
	dpause();

	dfill(0, rows, 0, columns, '.', RED_RGBA, GREEN_RGBA);
	display_insert_spaces(D, 2, 0, columns/2, columns);
	display_insert_spaces(D, 3, columns/2, columns - (columns/2), columns);
	dprint(0, 0, BLUE_RGBA, RED_RGBA, "after display_insert_spaces()");
	dpause();

	dfill(0, rows, 0, columns, '.', RED_RGBA, GREEN_RGBA);
	display_delete_chars(D, 2, 0, columns/2, columns);
	display_delete_chars(D, 3, columns/2, columns - (columns/2), columns);
	dprint(0, 0, BLUE_RGBA, RED_RGBA, "after display_delete_chars()");
	dpause();

	display_end(D);
	printf("display-test done\n");
	return EXIT_SUCCESS;
}
