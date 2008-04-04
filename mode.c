#include "all.h"

/*
 *	This is the default command mode.
 */

Boolean_t is_asdfg;

static struct macro *default_macro, *function_key[FUNCTION_FKEYS+1];

struct mode_default {
	command command;
	Boolean_t variant, is_hex;
	int value;
};

static void command_handler(struct view *, Unicode_t);


static position_t cut(struct view *view, Boolean_t delete)
{
	struct mode_default *mode = (struct mode_default *) view->mode;
	position_t offset;
	Boolean_t append;
	size_t bytes = view_get_selection(view, &offset, &append);
	int copies = mode->value ? mode->value : 1;

	if (!mode->variant || mode->value)
		clip_init(0);
	while (copies--)
		clip(0, view, offset, bytes, append);
	if (delete)
		view_delete(view, offset, bytes);
	locus_set(view, MARK, UNSET);
	return offset;
}

static void paste(struct view *view)
{
	struct mode_default *mode = (struct mode_default *) view->mode;
	position_t cursor = locus_get(view, CURSOR);
	clip_paste(view, cursor, mode->value);
	locus_set(view, MARK, /*old*/ cursor);
}

static void forward_lines(struct view *view)
{
	struct mode_default *mode = (struct mode_default *) view->mode;
	unsigned count = mode->variant ? mode->value : 1;
	position_t cursor = locus_get(view, CURSOR);

	if (!count)
		cursor = find_paragraph_end(view, cursor);
	else
		while (count-- && cursor < view->bytes)
			cursor = find_line_end(view, cursor+1);
	locus_set(view, CURSOR, cursor);
}

static void backward_lines(struct view *view)
{
	struct mode_default *mode = (struct mode_default *) view->mode;
	unsigned count = mode->variant ? mode->value : 1;
	position_t cursor = locus_get(view, CURSOR);

	if (!count)
		cursor = find_paragraph_start(view, cursor);
	else
		while (count-- && cursor)
			cursor = find_line_start(view, cursor-1);
	locus_set(view, CURSOR, cursor);
}

static void forward_chars(struct view *view)
{
	struct mode_default *mode = (struct mode_default *) view->mode;
	unsigned count = mode->variant ? mode->value : 1;
	position_t cursor = locus_get(view, CURSOR), next;

	while (count-- && IS_UNICODE(view_char(view, cursor, &next)))
		cursor = next;
	locus_set(view, CURSOR, cursor);
}

static void backward_chars(struct view *view)
{
	struct mode_default *mode = (struct mode_default *) view->mode;
	unsigned count = mode->variant ? mode->value : 1;
	position_t cursor = locus_get(view, CURSOR);

	while (count-- && cursor)
		view_char_prior(view, cursor, &cursor);
	locus_set(view, CURSOR, cursor);
}

static Boolean_t funckey(struct view *view, int Fk)
{
	struct mode_default *mode = (struct mode_default *) view->mode;

	if (Fk > FUNCTION_FKEYS)
		return FALSE;
	if (mode->variant && !mode->value) {
		macro_end_recording(CONTROL('@'));
		macro_free(function_key[Fk]);
		function_key[Fk] = macro_record();
		return TRUE;
	}
	return macro_play(function_key[Fk], mode->value);
}

static void command_handler(struct view *view, Unicode_t ch0)
{
	struct mode_default *mode = (struct mode_default *) view->mode;
	Unicode_t ch = ch0;
	position_t cursor = locus_get(view, CURSOR);
	position_t mark = locus_get(view, MARK);
	position_t offset;
	char buf[8];
	Boolean_t ok = TRUE, literal_unicode = FALSE;
	struct view *new_view;
	char *select;

	/* Backspace always deletes the character before cursor. */
	if (ch == 0x7f /*BCK*/) {
delete:		if (IS_UNICODE(view_char_prior(view, cursor, &mark)))
			view_delete(view, mark, cursor-mark);
		else
			window_beep(view);
		goto done;
	}

	/* Decode function-key sequences */
	if (IS_FUNCTION_KEY(ch)) {
		switch (ch) {
		case FUNCTION_UP:
			backward_lines(view);
			break;
		case FUNCTION_DOWN:
			forward_lines(view);
			break;
		case FUNCTION_LEFT:
			backward_chars(view);
			break;
		case FUNCTION_RIGHT:
			forward_chars(view);
			break;
		case FUNCTION_PGUP:
			window_page_up(view);
			break;
		case FUNCTION_PGDOWN:
			window_page_down(view);
			break;
		case FUNCTION_HOME:
			locus_set(view, CURSOR, 0);
			break;
		case FUNCTION_END:
			locus_set(view, CURSOR, view->bytes);
			break;
		case FUNCTION_INSERT:
			paste(view);
			break;
		case FUNCTION_DELETE:
			goto delete;
		default:
			if (ch < FUNCTION_F(1) ||
			    !funckey(view, ch - FUNCTION_F(1) + 1))
				ok = FALSE;
		}
		goto done;
	}


	/*
	 *	Non-control characters are self-inserted, with a prior
	 *	automatic cut of the selection if one exists and the
	 *	cursor is at its beginning.  But if we're in a variant,
	 *	some characters may contribute to the value, or be
	 *	non-Control commands.
	 */

	if (ch >= ' ' /*0x20*/) {

		if (mode->variant) {
			if (mode->is_hex && isxdigit(ch)) {
				mode->value *= 16;
				if (isdigit(ch))
					mode->value += ch - '0';
				else
					mode->value += tolower(ch) - 'a' + 10;
				return;
			}
			if (isdigit(ch)) {
				mode->value *= 10;
				mode->value += ch - '0';
				return;
			}
			if (!mode->value && (ch == 'x' || ch == 'X')) {
				mode->is_hex = 1;
				return;
			}
			switch (ch) {
			case '=':
				bookmark_set(mode->value, view, cursor, mark);
				goto done;
			case '-':
				if (bookmark_get(&new_view, &cursor, &mark,
						 mode->value)) {
					locus_set(new_view, CURSOR, cursor);
					if (mark != UNSET)
						locus_set(new_view, MARK, mark);
					window_activate(new_view);
				} else
					ok = FALSE;
				goto done;
			case ';':
				window_after(view, text_new(), -1);
				goto done;
			case '\'':
				find_tag(view);
				goto done;
			case ',':
				if (mark == UNSET)
					view_fold_indented(view, mode->value);
				else {
					view_fold(view, cursor, mark);
					locus_set(view, MARK, UNSET);
				}
				goto done;
			case '.':
				if (mode->value)
					view_unfold_all(view);
				else if (mark != UNSET)
					view_unfold_selection(view);
				else {
					mark = view_unfold(view, cursor);
					if ((signed) mark < 0)
						view_unfold_all(view);
					else
						locus_set(view, MARK, mark);
				}
				goto done;
			}
		}

		/* self insertion or replacement */
self_insert:	if (mark != UNSET && mark > cursor) {
			cursor = cut(view, 1);
			mark = UNSET;
		}
		if (ch == '\n' && view->text->flags & TEXT_CRNL)
			view_insert(view, "\r\n", cursor, 2);
		else if (ch <= 0x100 &&
			 (!literal_unicode ||
			  view->text->flags & TEXT_NO_UTF8)) {
			buf[0] = ch;
			view_insert(view, buf, cursor, 1);
		} else
			view_insert(view, buf, cursor, unicode_utf8(buf, ch));
		if (mark == cursor)
			locus_set(view, MARK, /*old*/ cursor);
		else if (ch == '\n')
			shell_command(view);
		goto done;
	}


	/*
	 *	Control character commands
	 */

	ch += '@';
	if (is_asdfg && ch >= 'A' && ch <= 'Z') {
		static char asdfg_to_aoeui[26] = {
			'A', 'O', 'F', 'Y', 'X', 'W', 'H', 'T',
			'I', 'J', 'N', 'S', 'M', 'Z', 'R', 'L',
			'Q', 'E', 'P', 'G', 'V', 'B', 'K', 'D',
			'C', 'U'
		};
		ch = asdfg_to_aoeui[ch-'A'];
	}

	switch (ch) {
	case '@': /* (^Space) */
		if (mode->variant)
			break; /* unset variant */
		mode->variant = 1;
		return;
	case 'A': /* synonym */
	case '_': /* ^/, ^_: search */
		mode_search(view, mode->variant);
		break;
	case 'B': /* exchange clip buffer and selection, if any, else paste */
		if (mark != UNSET) {
			size_t outbytes = view_get_selection(view, &offset, NULL);
			unsigned reg = mode->value;
			size_t inbytes = clip_paste(view, offset + outbytes, reg);
			clip_init(reg);
			clip(reg, view, offset, outbytes, 0);
			view_delete(view, offset, outbytes);
			locus_set(view, CURSOR, offset);
			locus_set(view, MARK, offset + inbytes);
		} else
			paste(view);
		break;
	case 'C':
		forward_lines(view);
		break;
	case 'D': /* [select whitespace] / cut [pre/appending] */
		if (mark == UNSET && mode->variant) {
			locus_set(view, MARK, find_nonspace(view, cursor));
			while (IS_UNICODE(ch = view_char_prior(view, cursor, &offset)) &&
			       isspace(ch))
				cursor = offset;
			locus_set(view, CURSOR, cursor);
		} else
			cut(view, TRUE);
		break;
	case 'E':
		if (mark == UNSET)
			if (mode->variant)
				demultiplex_view(view);
			else if (view->shell_std_in < 0) {
				new_view = text_new();
				window_after(view, new_view, -1);
				mode_shell_pipe(new_view);
			} else
				window_beep(view);
		else
			mode_child(view);
		break;
	case 'F': /* copy [pre/appending] */
		cut(view, FALSE);
		break;
	case 'G':
		backward_lines(view);
		break;
	case 'H':
		backward_chars(view);
		break;
	case 'I': /* (TAB) tab / tab completion [align; set tab stop] */
		if (!mode->variant) {
			if (!tab_completion_command(view))
				insert_tab(view);
		} else if (mode->value)
			if (mode->value >= 1 && mode->value <= 20)
				view->text->tabstop =
					default_tab_stop = mode->value;
			else
				window_beep(view);
		else
			align(view);
		break;
	case 'J': /* line feed: new line with alignment */
		if (view->text->flags & TEXT_CRNL)
			view_insert(view, "\r", cursor++, 1);
		view_insert(view, "\n", cursor++, 1);
		align(view);
		break;
	case 'K': /* save all [single] */
		if (mode->variant)
			text_preserve(view->text);
		else
			texts_preserve();
		break;
	case 'L': /* forward screen [end of view] */
		if (mode->variant)
			locus_set(view, CURSOR, view->bytes);
		else
			window_page_down(view);
		break;
	case 'M': /* (ENT) new line [opened] */
		if (mode->variant) {
			if (view->text->flags & TEXT_CRNL)
				view_insert(view, "\r\n", cursor, 2);
			else
				view_insert(view, "\n", cursor, 1);
			locus_set(view, CURSOR, cursor);
		} else {
			ch = '\n';
			goto self_insert;
		}
		break;
	case 'N': /* backward word(s) [sentence] */
		if (mode->value)
			while (mode->value--)
				cursor = find_word_start(view, cursor);
		else if (mode->variant)
			cursor = find_sentence_start(view, cursor);
		else
			cursor = find_word_start(view, cursor);
		locus_set(view, CURSOR, cursor);
		break;
	case 'O': /* macro end/execute [start] */
		if (mode->variant && !mode->value) {
			macro_end_recording(CONTROL('@'));
			macro_free(default_macro);
			default_macro = macro_record();
		} else if (!macro_end_recording(ch0) &&
			   !macro_play(default_macro, mode->value))
			window_beep(view);
		break;
	case 'P': /* select other window [closing current] */
		if (mode->variant)
			window_destroy(view->window);
		else
			window_next(view);
		break;
	case 'Q': /* suspend [quit] */
		windows_end();
		if (mode->variant) {
			texts_preserve();
			while (text_list)
				view_close(text_list->views);
			exit(EXIT_SUCCESS);
		}
		fprintf(stderr, "The editor is suspended.  "
			"Type 'fg' to resume.\n");
		kill(getpid(), SIGSTOP);
		window_recenter(view);
		break;
	case 'R': /* backward screen [beginning of view] */
		if (mode->variant)
			locus_set(view, CURSOR, 0);
		else
			window_page_up(view);
		break;
	case 'S': /* forward word(s) [sentence] */
		if (mode->value)
			while (mode->value--)
				cursor = find_word_end(view, cursor);
		else if (mode->variant)
			cursor = find_sentence_end(view, cursor);
		else
			cursor = find_word_end(view, cursor);
		locus_set(view, CURSOR, cursor);
		break;
	case 'T':
		forward_chars(view);
		break;
	case 'U': /* undo [redo] */
		offset = (mode->variant ? text_redo : text_undo)(view->text);
		if ((offset -= view->start) <= view->bytes)
			locus_set(view, CURSOR, offset);
		locus_set(view, MARK, UNSET);
		break;
	case 'V': /* set/unset mark [exchange, or select line; force unset] */
		if (!mode->variant)
			locus_set(view, MARK, mark == UNSET ? cursor : UNSET);
		else if (mode->value)
			locus_set(view, MARK, UNSET);
		else if (mark == UNSET) {
			locus_set(view, MARK, find_line_end(view, cursor) + 1);
			locus_set(view, CURSOR, find_line_start(view, cursor));
		} else {
			locus_set(view, MARK, cursor);
			locus_set(view, CURSOR, mark);
		}
		break;
	case 'W': /* select other view [closing current] */
		if (mode->variant)
			view_close(view);
		else if (!window_replace(view, view_next(view)))
			window_beep(view);
		break;
	case 'X': /* get path / visit file [set path] */
		if (mark == UNSET) {
			view_insert(view, view->text->path, cursor, -1);
			locus_set(view, MARK, /*old*/ cursor);
		} else if ((select = view_extract_selection(view))) {
			if (mode->variant) {
				if ((ok = text_rename(view->text, select)))
					window_activate(view);
				new_view = NULL;
			} else
				ok = !!(new_view = view_open(select));
			RELEASE(select);
			if (ok)
				view_delete_selection(view);
			if (new_view)
				window_after(view, new_view, -1 /*auto*/);
		}
		break;
	case 'Y': /* split window [vertically] */
		if (mark != UNSET) {
			position_t offset = mark < cursor ? mark : cursor;
			size_t bytes = mark < cursor ? cursor - mark :
				       mark - cursor;
			new_view = view_selection(view, offset, bytes);
		} else
			new_view = view_next(view);
		if (new_view)
			window_after(view, new_view, mode->variant);
		else
			window_beep(view);
		break;
	case 'Z': /* recenter/goto */
		if (mode->value)
			locus_set(view, CURSOR,
				  find_line_number(view, mode->value));
		else if (mode->variant)
			window_raise(view);
		window_recenter(view);
		break;
	case '\\': /* quit */
		if (mode->variant) {
			windows_end();
			texts_uncreate();
			depart(EXIT_SUCCESS);
		}
		break;
	case ']': /* move to corresponding bracket */
		cursor = find_corresponding_bracket(view, cursor);
		if ((signed) cursor < 0)
			window_beep(view);
		else
			locus_set(view, CURSOR, cursor);
		break;
	case '^': /* literal [; unicode] */
		if (mode->value) {
			ch = mode->value;
			literal_unicode = TRUE;
			goto self_insert;
		}
		if (IS_UNICODE(ch = macro_getch())) {
			if (ch >= '@' && ch <= '_')
				ch = CONTROL(ch);
			else if (ch >= 'a' && ch <= 'z')
				ch = CONTROL(ch-'a'+'A');
			else if (ch == '?')
				ch = 0x7f;
			goto self_insert;
		}
		ok = FALSE;
		break;
	default:
		ok = FALSE;
		break;
	}

done:	mode->variant = mode->is_hex = FALSE;
	mode->value = 0;
	if (!ok)
		window_beep(view);
}

struct mode *mode_default(void)
{
	struct mode_default *dft = allocate0(sizeof *dft);
	dft->command = command_handler;
	return (struct mode *) dft;
}
