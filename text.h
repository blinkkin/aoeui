#ifndef TEXT_H
#define TEXT_H

/* Texts and views */

struct text {
	struct text *next;
	struct view *views;		/* list of views into this text */
	char *clean;			/* unmodified content, mmap'ed */
	size_t clean_bytes;
	fd_t fd;
	struct buffer *buffer;		/* modified content */
	struct undo *undo;		/* undo/redo state */
	char *path;
	unsigned dirties;		/* number of modifications */
	unsigned preserved;		/* "dirties" at last save */
	time_t mtime;
	unsigned tabstop;
	unsigned flags;
#define TEXT_SAVED_ORIGINAL 0x001
#define TEXT_RDONLY 0x002
#define TEXT_EDITOR 0x004
#define TEXT_CREATED 0x008
#define TEXT_SCRATCH 0x010
#define TEXT_FOLDED 0x020
#define TEXT_NO_UTF8 0x040
#define TEXT_CRNL 0x080
};

struct view {
	struct view *next;
	struct text *text;
	struct window *window;
	char *name;
	position_t start;		/* offset in text */
	size_t bytes;
	unsigned loci;
	position_t *locus;
	locus_t shell_out_locus;
	struct mode *mode;
	fd_t shell_std_in;
};

extern struct text *text_list;
extern unsigned default_tab_stop;
extern Boolean_t no_tabs;
extern enum utf8_mode { UTF8_NO, UTF8_YES, UTF8_AUTO } utf8_mode;

/* text.c */
struct view *view_find(const char *name);
void view_name(struct view *);
struct view *view_create(struct text *);
struct view *text_create(const char *name, unsigned flags);
struct view *text_new(void);
void view_close(struct view *);
struct view *view_selection(struct view *, position_t, size_t);
void text_adjust_loci(struct text *, position_t, int delta);
size_t view_get(struct view *, void *, position_t, size_t);
size_t view_raw(struct view *, char **, position_t, size_t);
size_t view_delete(struct view *, position_t, size_t);
size_t view_insert(struct view *, const void *, position_t, ssize_t);

/* Use only for raw bytes.  See util.h for general folded and Unicode
 * character access with view_char[_prior]().
 */
INLINE Unicode_t view_byte(struct view *view, position_t offset)
{
	if (offset >= view->bytes)
		return UNICODE_BAD;
	offset += view->start;
	if (view->text->buffer)
		return buffer_byte(view->text->buffer, offset);
	if (view->text->clean)
		return (Byte_t) view->text->clean[offset];
	return UNICODE_BAD;
}

/* file.c */
struct view *view_open(const char *path);
Boolean_t text_rename(struct text *, const char *path);
void text_dirty(struct text *);
Boolean_t text_is_dirty(struct text *);
void text_preserve(struct text *);
void texts_preserve(void);
void texts_uncreate(void);

/* undo.c */
size_t text_delete(struct text *, position_t, size_t);
size_t text_insert(struct text *, const void *, position_t, size_t);
sposition_t text_undo(struct text *);
sposition_t text_redo(struct text *);
void text_forget_undo(struct text *);

/* bookmark.c */
void bookmark_set(unsigned, struct view *, position_t cursor, position_t mark);
Boolean_t bookmark_get(struct view **, position_t *cursor, position_t *mark,
		       unsigned id);
void bookmark_unset(unsigned);
void bookmark_unset_view(struct view *);

void demultiplex_view(struct view *);		/* child.c */
struct view *view_help(void);			/* help.c */
char *tab_complete(const char *, Boolean_t);	/* tab.c */
Boolean_t tab_completion_command(struct view *);
void insert_tab(struct view *);
void align(struct view *);

/* fold.c */
void view_fold(struct view *, position_t, position_t);
sposition_t view_unfold(struct view *, position_t);
void view_unfold_selection(struct view *);
void view_fold_indented(struct view *, unsigned);
void view_unfold_all(struct view *);
void text_unfold_all(struct text *);

/* see also util.h */

#endif
