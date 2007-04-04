struct window {
	struct view *view;
	unsigned start; /* locus index */
	unsigned row, column;
	unsigned rows, columns;
	unsigned cursor_row, cursor_column;
	unsigned last_dirties, last_bgrgba, last_cursor, last_mark;
	struct window *next;
};

struct window *window_raise(struct view *);
struct window *window_activate(struct view *);
struct window *window_after(struct view *, struct view *, int vertical);
struct window *window_replace(struct view *, struct view *);
void window_unmap(struct view *);
void window_next(struct view *);
struct window *window_recenter(struct view *);
void window_page_up(struct view *);
void window_page_down(struct view *);
void window_beep(struct view *);
int window_getch(void);
struct view *window_current_view(void);
void windows_reset(void);
void windows_end(void);
