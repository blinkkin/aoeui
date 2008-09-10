#include "all.h"

/* TAGS file searching */

static char *extract_id(struct view *view)
{
	Unicode_t ch;
	position_t at = locus_get(view, CURSOR), next;
	char *id;

	if (is_idch((ch = view_char(view, at, &next))) ||
	    ch == ':' && view_char(view, next, NULL) == ':')
		locus_set(view, CURSOR, at = find_id_end(view, at));
	locus_set(view, MARK, find_id_start(view, at));
	id = view_extract_selection(view);
	locus_set(view, MARK, UNSET);
	return id;
}

static struct view *find_TAGS(struct view *view, struct view *tags)
{
	const char *currpath = (tags ? tags : view)->text->path;
	char *path = allocate(strlen(currpath) + 8);
	char *slash;

	strcpy(path, currpath);
	if (tags) {
		view_close(tags);
		slash = strrchr(path, '/');
		if (slash)
			*slash = '\0';
	}
	for (; (slash = strrchr(path, '/')); *slash = '\0') {
		strcpy(slash+1, "TAGS");
		if (!access(path, R_OK)) {
			tags = view_open(path);
			if (tags && !(tags->text->flags & TEXT_CREATED)) {
				RELEASE(path);
				return tags;
			}
			view_close(tags);
		}
	}
	RELEASE(path);
	return NULL;
}

static sposition_t find_id_in_TAGS(struct view *tags, const char *id)
{
	position_t first = 0;
	position_t last = tags->bytes;
	position_t at, wordstart, wordend;
	sposition_t result = -1;
	char *this;
	int cmp;

	while (first < last) {
		at = find_line_start(tags, first + last >> 1);
		if (at < first)
			at = find_line_end(tags, at) + 1;
		if (at >= last)
			break;
		wordstart = find_nonspace(tags, at);
		wordend = find_space(tags, wordstart);
		this = view_extract(tags, wordstart, wordend - wordstart);
		if (!this)
			break;
		cmp = strcmp(id, this);
		RELEASE(this);
		if (!cmp)
			result = wordend;
		if (cmp <= 0)
			last = at;
		else
			first = find_line_end(tags, at) + 1;
	}
	return result;	/* failure */
}

static sposition_t find_next_id_in_TAGS(struct view *tags, const char *id,
					position_t prevend)
{
	position_t wordstart = find_nonspace(tags,
					     find_line_end(tags, prevend) + 1);
	position_t wordend = find_space(tags, wordstart);
	char *this = view_extract(tags, wordstart, wordend - wordstart);
	int cmp;

	if (!this)
		return -1;
	cmp = strcmp(id, this);
	RELEASE(this);
	return cmp ? -1 : wordend;
}

static struct view *show_tag(struct view *tags, sposition_t wordend,
			     const char *id)
{
	position_t wordstart, linestart;
	char *this, *path, *slash;
	int line;
	struct view *view;
	sposition_t at;

	if (wordend < 0)
		return NULL;
	wordstart = find_nonspace(tags, wordend); /* line number */
	wordend = find_space(tags, wordstart);
	this = view_extract(tags, wordstart, wordend - wordstart);
	if (!isdigit(*this)) {
		/* exuberant-ctags puts a classifier before the line number */
		RELEASE(this);
		wordstart = find_nonspace(tags, wordend); /* line number */
		wordend = find_space(tags, wordstart);
		this = view_extract(tags, wordstart, wordend - wordstart);
		if (!isdigit(*this)) {
			RELEASE(this);
			return NULL;
		}
	}
	line = atoi(this);
	RELEASE(this);
	wordstart = find_nonspace(tags, wordend); /* file name */
	wordend = find_space(tags, wordstart);
	this = view_extract(tags, wordstart, wordend - wordstart);

	if (*this == '/')
		path = this;
	else {
		path = allocate(strlen(tags->text->path) + strlen(this) + 8);
		strcpy(path, tags->text->path);
		if (!(slash = strrchr(path, '/')))
			*(slash = path + strlen(path)) = '/';
		strcpy(slash + 1, this);
		RELEASE(this);
	}

	view = view_open(path);
	if (!view)
		message("Could not open %s from TAGS", path);
	RELEASE(path);
	if (!view || view->text->flags & TEXT_CREATED) {
		view_close(view);
		return FALSE;
	}
	linestart = find_line_number(view, line);
	at = find_string(view, id, linestart);
	if (at >= 0) {
		locus_set(view, CURSOR, at);
		locus_set(view, MARK, at + strlen(id));
	} else {
		locus_set(view, CURSOR, linestart);
		locus_set(view, MARK, UNSET);
	}
	return view;
}

static Boolean_t show_tags(struct view *tags, struct view *view,
			   const char *id)
{
	sposition_t wordend = find_id_in_TAGS(tags, id);
	struct view *new_view = show_tag(tags, wordend, id);
	struct view *top_view;

	if (!new_view)
		return FALSE;

	window_below(view, top_view = new_view, 4);
	while ((wordend = find_next_id_in_TAGS(tags, id, wordend)) >= 0)
		if ((new_view = show_tag(tags, wordend, id)))
			window_below(view, top_view = new_view, 4);
	window_activate(top_view);
	return TRUE;
}

void find_tag(struct view *view)
{
	struct view *tags = NULL;
	char *id;

	if (locus_get(view, MARK) != UNSET) {
		id = view_extract_selection(view);
		view_delete_selection(view);
	} else
		id = extract_id(view);
	if (!id) {
		window_beep(view);
		return;
	}

	if (!(tags = find_TAGS(view, NULL))) {
		message("No readable TAGS file found.");
		RELEASE(id);
		return;
	}

	do {
		if (show_tags(tags, view, id)) {
			view_close(tags);
			RELEASE(id);
			return;
		}
	} while ((tags = find_TAGS(view, tags)));

	errno = 0;
	message("couldn't find tag %s", id);
	RELEASE(id);
}
