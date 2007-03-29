struct view;

unsigned find_line_start(struct view *, unsigned offset);
unsigned find_line_end(struct view *, unsigned offset);
unsigned find_word_start(struct view *, unsigned offset);
unsigned find_word_end(struct view *, unsigned offset);
unsigned find_sentence_start(struct view *, unsigned offset);
unsigned find_sentence_end(struct view *, unsigned offset);
int view_unicode_prior(struct view *, unsigned offset, unsigned *prev);
void view_erase(struct view *);
int view_vprintf(struct view *, const char *, va_list);
int view_printf(struct view *, const char *, ...);
unsigned view_get_selection(struct view *, unsigned *offset, int *append);
char *view_extract_selection(struct view *);
unsigned view_delete_selection(struct view *);
struct view *view_next(struct view *);
int view_corresponding_bracket(struct view *, unsigned offset);

int view_unicode_slow(struct view *, unsigned offset, unsigned *length);
INLINE int view_unicode(struct view *view, unsigned offset,
			unsigned *length)
{
	int ch = view_byte(view, offset);
	if (ch < 0x80) {
		*length = ch >= 0;
		return ch;
	}
	return view_unicode_slow(view, offset, length);
}

char *tab_complete(const char *);
