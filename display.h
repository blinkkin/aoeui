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
		   unsigned rows, unsigned columns, unsigned bgRGBA);
void display_beep(struct display *);
void display_sync(struct display *);

int display_getch(struct display *, int block);
#define DISPLAY_EOF   (-1)
#define DISPLAY_WINCH (-2)
#define DISPLAY_ERR   (-3)
#define DISPLAY_NONE  (-4)
