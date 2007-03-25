struct view {
	struct view *next;
	struct text *text;
	struct window *window;
	char *name;
	unsigned start, bytes;
	unsigned loci, *locus;
	struct mode *mode;
	char *last_search;
	char *macro;
	unsigned macro_bytes, macro_alloc;
	int macro_at;
};

struct text {
	struct view *views;
	char *clean;
	unsigned long clean_bytes;
	struct buffer *buffer;
	struct undo *undo;
	struct text *next;
	char *path;
	unsigned flags;
	int fd;
	time_t mtime;
	unsigned tabstop;
	unsigned newlines; /* 0: UNIX, 1: old Mac, 2: DOS */
};

/* flags */
#define TEXT_DIRTY 1
#define TEXT_SAVED_ORIGINAL 2
#define TEXT_RDONLY 4
#define TEXT_EDITOR 8
#define TEXT_CREATED 16

struct view *view_find(const char *name);
       void  view_name(struct view *);
struct view *view_create(struct text *);
struct view *text_create(const char *name, unsigned flags);
       void  view_close(struct view *);
struct view *view_selection(struct view *, unsigned offset, unsigned bytes);
void text_adjust_loci(struct text *, unsigned offset, int delta);
unsigned view_get(struct view *, void *, unsigned offset, unsigned bytes);
unsigned view_raw(struct view *, char **, unsigned offset, unsigned bytes);
unsigned view_delete(struct view *, unsigned offset, unsigned bytes);
unsigned view_insert(struct view *, const void *, unsigned offset, int bytes);
int view_getch(struct view *);

/* Use only for raw bytes.  See util.h for character access with
 * view_unicode() and view_unicode_prior().
 */
static INLINE int view_byte(struct view *view, unsigned offset)
{
	if (offset >= view->bytes)
		return -1;
	offset += view->start;
	if (view->text->buffer)
		return buffer_byte(view->text->buffer, offset);
	if (view->text->clean)
		return (unsigned char) view->text->clean[offset];
	return -1;
}

/* in file.c */
struct view *view_open(const char *path);
int text_rename(struct text *, const char *path);
void text_dirty(struct text *);
void text_preserve(struct text *);
void texts_preserve(void);
void texts_uncreate(void);

/* in undo.c */
unsigned text_delete(struct text *, unsigned offset, unsigned bytes);
unsigned text_insert(struct text *, const void *,
		       unsigned offset, unsigned bytes);
int text_undo(struct text *);
int text_redo(struct text *);
void text_forget_undo(struct text *);

/* in window.c */
unsigned view_screen_start(struct view *);

/* see also util.h */
