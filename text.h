/* Texts and views */

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
	int shell_std_in, shell_out_locus;
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
	unsigned dirties, preserved;
	int fd;
	time_t mtime;
	unsigned tabstop;
};

/* text->flags */
#define TEXT_SAVED_ORIGINAL 1
#define TEXT_RDONLY 2
#define TEXT_EDITOR 4
#define TEXT_CREATED 8
#define TEXT_SCRATCH 16
#define TEXT_FOLDED 32

extern struct text *text_list;
extern int default_tab_stop;

/* text.c */
struct view *view_find(const char *name);
void view_name(struct view *);
struct view *view_create(struct text *);
struct view *text_create(const char *name, unsigned flags);
struct view *text_new(void);
void view_close(struct view *);
struct view *view_selection(struct view *, unsigned offset, unsigned bytes);
void text_adjust_loci(struct text *, unsigned offset, int delta);
unsigned view_get(struct view *, void *, unsigned offset, unsigned bytes);
unsigned view_raw(struct view *, char **, unsigned offset, unsigned bytes);
unsigned view_delete(struct view *, unsigned offset, unsigned bytes);
unsigned view_insert(struct view *, const void *, unsigned offset, int bytes);
int view_getch(struct view *);

/* Use only for raw bytes.  See util.h for general folded and Unicode
 * character access with view_char[_prior]().
 */
INLINE int view_byte(struct view *view, unsigned offset)
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

/* file.c */
struct view *view_open(const char *path);
int text_rename(struct text *, const char *path);
void text_dirty(struct text *);
void text_preserve(struct text *);
void texts_preserve(void);
void texts_uncreate(void);

/* undo.c */
unsigned text_delete(struct text *, unsigned offset, unsigned bytes);
unsigned text_insert(struct text *, const void *,
		       unsigned offset, unsigned bytes);
int text_undo(struct text *);
int text_redo(struct text *);
void text_forget_undo(struct text *);

/* bookmark.c */
void bookmark_set(unsigned, struct view *, unsigned cursor, unsigned mark);
int bookmark_get(struct view **, unsigned *cursor, unsigned *mark,
		 unsigned id);
void bookmark_unset(unsigned);
void bookmark_unset_view(struct view *);

void demultiplex_view(struct view *);		/* child.c */
struct view *view_help(void);			/* help.c */
char *tab_complete(const char *, int path);	/* tab.c */
int tab_completion_command(struct view *);
void align(struct view *);

/* fold.c */
void view_fold(struct view *, unsigned, unsigned);
int view_unfold(struct view *, unsigned);
void view_unfold_selection(struct view *);
void view_fold_indented(struct view *, unsigned);
void view_unfold_all(struct view *);
void text_unfold_all(struct text *);

/* see also util.h */
