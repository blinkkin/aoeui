/* Windows */

struct window *window_raise(struct view *);
struct window *window_activate(struct view *);
struct window *window_after(struct view *, struct view *, int vertical);
struct window *window_below(struct view *, struct view *, unsigned rows);
struct window *window_replace(struct view *, struct view *);
void window_destroy(struct window *);
void window_next(struct view *);
void window_hint_deleting(struct window *, unsigned offset, unsigned bytes);
void window_hint_inserted(struct window *, unsigned offset, unsigned bytes);
struct window *window_recenter(struct view *);
void window_page_up(struct view *);
void window_page_down(struct view *);
void window_beep(struct view *);
int window_getch(void);
struct view *window_current_view(void);
void windows_reset(void);
void windows_force_geometry(unsigned rows, unsigned columns);
void windows_end(void);
