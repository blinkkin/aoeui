struct display;

struct display *display_init(void);
void display_reset(struct display *);
void display_end(struct display *);
void display_get_geometry(struct display *, unsigned *rows, unsigned *columns);
void display_title(struct display *, const char *);
void display_cursor(struct display *, unsigned row, unsigned column);

/* note: setting any alpha in RGBA means "default" */
void display_put(struct display *, unsigned row, unsigned column,
		 unsigned unicode, unsigned fgRGBA, unsigned bgRGBA);

void display_erase(struct display *, unsigned row, unsigned column,
		   unsigned rows, unsigned columns,
		   unsigned fgRGBA, unsigned bgRGBA);
void display_insert_spaces(struct display *, unsigned row, unsigned column,
			   unsigned spaces, unsigned columns,
			   unsigned fgRGBA, unsigned bgRGBA);
void display_delete_chars(struct display *, unsigned row, unsigned column,
			  unsigned chars, unsigned columns,
			  unsigned fgRGBA, unsigned bgRGBA);
void display_insert_lines(struct display *, unsigned row, unsigned column,
			  unsigned lines, unsigned rows, unsigned columns,
			  unsigned fgRGBA, unsigned bgRGBA);
void display_delete_lines(struct display *, unsigned row, unsigned column,
			  unsigned lines, unsigned rows, unsigned columns,
			  unsigned fgRGBA, unsigned bgRGBA);
void display_beep(struct display *);
void display_sync(struct display *);

int display_getch(struct display *, int block);
#define DISPLAY_EOF	( -1)
#define DISPLAY_WINCH	( -2)	/* size changed; MUST call _get_geometry()! */
#define DISPLAY_ERR	( -3)
#define DISPLAY_NONE	( -4)
#define DISPLAY_FKEY(x)	(0x10000 + (x))
#define DISPLAY_IS_FKEY(x) ((x) >= DISPLAY_FKEY(0))
#define DISPLAY_UP	DISPLAY_FKEY(1)
#define DISPLAY_DOWN	DISPLAY_FKEY(2)
#define DISPLAY_RIGHT	DISPLAY_FKEY(3)
#define DISPLAY_LEFT	DISPLAY_FKEY(4)
#define DISPLAY_PGUP	DISPLAY_FKEY(5)
#define DISPLAY_PGDOWN	DISPLAY_FKEY(6)
#define DISPLAY_HOME	DISPLAY_FKEY(7)
#define DISPLAY_END	DISPLAY_FKEY(8)
#define DISPLAY_INSERT	DISPLAY_FKEY(9)
#define DISPLAY_DELETE	DISPLAY_FKEY(10)
#define DISPLAY_F1	DISPLAY_FKEY(21)
#define DISPLAY_F2	DISPLAY_FKEY(22)
#define DISPLAY_F3	DISPLAY_FKEY(23)
#define DISPLAY_F4	DISPLAY_FKEY(24)
#define DISPLAY_F5	DISPLAY_FKEY(25)
#define DISPLAY_F6	DISPLAY_FKEY(26)
#define DISPLAY_F7	DISPLAY_FKEY(27)
#define DISPLAY_F8	DISPLAY_FKEY(28)
#define DISPLAY_F9	DISPLAY_FKEY(29)
#define DISPLAY_F10	DISPLAY_FKEY(30)
#define DISPLAY_F11	DISPLAY_FKEY(31)
#define DISPLAY_F12	DISPLAY_FKEY(32)
