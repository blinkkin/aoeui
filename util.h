/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#ifndef UTIL_H
#define UTIL_H

/* Utilities */

struct view;

/* find.c */
position_t find_line_start(struct view *, position_t);
position_t find_line_end(struct view *, position_t);
position_t find_paragraph_start(struct view *, position_t);
position_t find_paragraph_end(struct view *, position_t);
position_t find_line_up(struct view *, position_t);
position_t find_line_down(struct view *, position_t);
position_t find_space(struct view *, position_t);
position_t find_space_prior(struct view *, position_t);
position_t find_nonspace(struct view *, position_t);
position_t find_nonspace_prior(struct view *, position_t);
position_t find_word_start(struct view *, position_t);
position_t find_word_end(struct view *, position_t);
position_t find_id_start(struct view *, position_t);
position_t find_id_end(struct view *, position_t);
position_t find_sentence_start(struct view *, position_t);
position_t find_sentence_end(struct view *, position_t);
sposition_t find_corresponding_bracket(struct view *, position_t);
position_t find_line_number(struct view *, unsigned line);
position_t find_row_bytes(struct view *, position_t,
			unsigned column, unsigned columns);
unsigned find_column(unsigned *row, struct view *, position_t linestart,
		     position_t offset, unsigned start_column);
sposition_t find_string(struct view *, const char *, position_t);

const char *path_format(const char *);	/* file.c */

void find_tag(struct view *);	/* tags.c */

ssize_t view_vprintf(struct view *, const char *, va_list);
ssize_t view_printf(struct view *, const char *, ...);
size_t view_get_selection(struct view *, position_t *offset, Boolean_t *append);
char *view_extract(struct view *, position_t, unsigned bytes);
char *view_extract_selection(struct view *);
size_t view_delete_selection(struct view *);
struct view *view_next(struct view *);

Unicode_t view_unicode(struct view *, position_t, size_t *);
Unicode_t view_unicode_prior(struct view *, position_t, position_t *prev);
Unicode_t view_char(struct view *, position_t, size_t *);
Unicode_t view_char_prior(struct view *, position_t, position_t *prev);
Boolean_t is_open_bracket(const char *, Unicode_t);
Boolean_t is_close_bracket(const char *, Unicode_t);

void keyword_init(struct text *);			/* keyword.c */
Boolean_t is_keyword(struct view *, position_t);

INLINE Boolean_t is_wordch(Unicode_t ch)
{
	return ch > 0x100 && ch < FOLD_START || IS_CODEPOINT(ch) && isalnum(ch);
}

INLINE Boolean_t is_idch(Unicode_t ch)
{
	return ch == '_' || is_wordch(ch);
}

INLINE unsigned char_columns(Unicode_t ch, unsigned column, unsigned tabstop)
{
	if (ch == '\t')
		return tabstop - column % tabstop;
	if (ch < ' ' || ch == 0x7f || IS_FOLDED(ch))
		return 2; /* ^X or folded <> */
	return 1;
}

#endif
