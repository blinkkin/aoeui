/* Utilities */

struct view;

/* find.c */
unsigned find_line_start(struct view *, unsigned offset);
unsigned find_line_end(struct view *, unsigned offset);
unsigned find_space(struct view *, unsigned offset);
unsigned find_nonspace(struct view *, unsigned offset);
unsigned find_word_start(struct view *, unsigned offset);
unsigned find_word_end(struct view *, unsigned offset);
unsigned find_id_start(struct view *, unsigned offset);
unsigned find_id_end(struct view *, unsigned offset);
unsigned find_sentence_start(struct view *, unsigned offset);
unsigned find_sentence_end(struct view *, unsigned offset);
int find_corresponding_bracket(struct view *, unsigned offset);
unsigned find_line_number(struct view *, unsigned line);

void find_tag(struct view *);	/* tags.c */

int view_vprintf(struct view *, const char *, va_list);
int view_printf(struct view *, const char *, ...);
unsigned view_get_selection(struct view *, unsigned *offset, int *append);
char *view_extract(struct view *, unsigned offset, unsigned bytes);
char *view_extract_selection(struct view *);
unsigned view_delete_selection(struct view *);
struct view *view_next(struct view *);

char *tab_complete(const char *);

int view_char(struct view *view, unsigned offset, unsigned *length);
int view_char_prior(struct view *view, unsigned offset, unsigned *prev);
void view_fold(struct view *, unsigned, unsigned);
int view_unfold(struct view *, unsigned);
void view_fold_indented(struct view *, unsigned);
void view_unfold_all(struct view *);

INLINE int is_wordch(int ch)
{
	return ch > 0x100 && ch < FOLD_START || isalnum(ch);
}

INLINE int is_idch(int ch)
{
	return ch == '_' || is_wordch(ch);
}
