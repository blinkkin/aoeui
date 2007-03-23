#include "all.h"
#include <fcntl.h>
#include <sys/stat.h>

static void newlines(struct text *text)
{
	struct buffer *buffer = text->buffer;
	char *raw;
	unsigned offset;
	unsigned bytes = buffer_raw(buffer, &raw, 0, ~0);
	int ch, last = 0;
	unsigned lfs = 0, crs = 0, crlfs = 0;

	text->newlines = 0; /* no conversion */
	if ('\n' != '\x0a')
		return;
	for (offset = 0; offset < bytes; last = ch, offset++) {
		ch = raw[offset];
		if (ch == '\n') {
			lfs++;
			if (last == '\r')
				crlfs++;
		} else if (ch == '\r')
			crs++;
		else if (ch < ' ' && ch != '\t')
			return;
	}

	if (!crs)
		return;
	if (!lfs) {
		text->newlines = 1; /* old Mac, CR is newline */
		for (offset = 0; offset < bytes; offset++)
			if (raw[offset] == '\r')
				raw[offset] = '\n';
		return;
	}
	if (lfs != crs || lfs != crlfs)
		return;

	text->newlines = 2; /* DOS CR-LF is newline */
	for (last = offset = 0; offset < bytes; offset++)
		if ((raw[last] = raw[offset]) != '\r')
			last++;
	buffer_delete(buffer, last, bytes - last);
}

static void undo_newlines(struct text *text)
{
	struct buffer *buffer = text->buffer;
	char *raw;
	unsigned bytes = buffer_raw(buffer, &raw, 0, ~0);
	unsigned offset;
	unsigned lines;

	if (!text->newlines)
		return;
	if (text->newlines == 1) {
		for (offset = 0; offset < bytes; offset++)
			if (raw[offset] == '\n')
				raw[offset] = '\r';
		return;
	}
	for (lines = offset = 0; offset < bytes; offset++)
		lines += raw[offset] == '\n';
	if (!lines)
		return;
	buffer_insert(buffer, NULL, bytes, lines);
	buffer_raw(buffer, &raw, 0, offset = bytes + lines);
	while (bytes--)
		if ((raw[--offset] = raw[bytes]) == '\n')
			raw[--offset] = '\r';
}

static int text_read(struct text *text)
{
	char *raw;
	int got, total = 0;
	unsigned max;
#define CHUNK 1024

	do {
		buffer_insert(text->buffer, NULL, total, CHUNK);
		max = buffer_raw(text->buffer, &raw, total, CHUNK);
		errno = 0;
		got = read(text->fd, raw, max);
		if (got < 0) {
			message("error reading %s", text->path);
			buffer_delete(text->buffer, 0, total + CHUNK);
			return -1;
		}
		buffer_delete(text->buffer, total + got, CHUNK - got);
		total += got;
	} while (got);

	return total;
}

static char *fix_path(const char *path)
{
	char *fpath;
	int freepath = 0;
	int pathlen;

	if (!path)
		return NULL;
	while (isspace(*path))
		path++;
	while (*path == '.' && path[1] == '/')
		path += 2;
	if (!(pathlen = strlen(path)))
		return NULL;
	if (isspace(path[pathlen-1])) {
		char *apath;
		while (pathlen && isspace(path[--pathlen]))
			;
		if (!pathlen)
			return NULL;
		apath = allocate(NULL, pathlen+1);
		memcpy(apath, path, pathlen);
		apath[pathlen] = '\0';
		path = apath;
		freepath = 1;
	}
	if (*path != '/') {
		char *cwd = getcwd(NULL, 0);
		char *apath = allocate(NULL, strlen(cwd) + pathlen + 2);
		sprintf(apath, "%s/%s", cwd, path);
		if (freepath)
			allocate(path, 0);
		path = apath;
		allocate(cwd, 0);
		freepath = 1;
	}
	fpath = strdup(path);
	if (freepath)
		allocate(path, 0);
	return fpath;
}

struct view *view_open(const char *path0)
{
	struct view *view;
	struct text *text;
	struct stat statbuf;
	char *path = fix_path(path0);

	if (!path)
		return NULL;

	for (text = text_list; text; text = text->next)
		if (text->path && !strcmp(text->path, path)) {
			for (view = text->views; view; view = view->next)
				if (!view->window)
					goto done;
			view = view_create(text);
			goto done;
		}

	view = text_create(path, 0);
	text = view->text;

	errno = 0;
	if (stat(path, &statbuf)) {
		if (errno != ENOENT) {
			message("can't stat %s", path);
			goto fail;
		}
		errno = 0;
		text->fd = creat(path, S_IRUSR|S_IWUSR);
		if (text->fd < 0) {
			message("can't create %s", path);
			goto fail;
		}
		text->mtime = 0;
		text->flags |= TEXT_CREATED;
	} else {
		if (!S_ISREG(statbuf.st_mode)) {
			message("%s is not a regular file", path);
			goto fail;
		}
		text->mtime = statbuf.st_mtime;
		text->fd = open(path, O_RDWR);
		if (text->fd < 0) {
			errno = 0;
			text->flags |= TEXT_RDONLY;
			text->fd = open(path, O_RDONLY);
			if (text->fd >= 0)
				message("%s opened read-only", path);
			else {
				message("can't open %s", path);
				goto fail;
			}
		}
		if (text_read(text) < 0)
			goto fail;
		newlines(text);
		view->bytes = buffer_bytes(text->buffer);
		locus_set(view, CURSOR, 0);
		text_forget_undo(text);
	}
	goto done;

fail:	view_close(view);
	view = NULL;

done:	allocate(path, 0);
	return view;
}

int text_rename(struct text *text, const char *path0)
{
	char *path = fix_path(path0);
	struct text *b;
	struct view *view;
	int fd;

	if (!path)
		return 0;
	for (b = text; b; b = b->next)
		if (b->path && !strcmp(b->path, path))
			return 0;

	errno = 0;
	if ((fd = open(path, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR)) < 0) {
		message("can't create %s", path);
		allocate(path, 0);
		return 0;
	}

	/* Do not truncate or overwrite yet. */
	close(text->fd);
	text->fd = fd;
	text->flags &= ~TEXT_CREATED;
	text->flags |= TEXT_DIRTY | TEXT_SAVED_ORIGINAL;
	allocate(text->path, 0);
	text->path = path;
	for (view = text->views; view; view = view->next)
		view_name(view);
	return 1;
}

void text_dirty(struct text *text)
{
	char *save_path, *raw;
	int fd;
	unsigned length;

	if (text->path &&
	    (text->flags & (TEXT_DIRTY | TEXT_RDONLY)) == TEXT_RDONLY)
		message("%s is a read-only file, changes will not be saved.",
			text->path);

	text->flags |= TEXT_DIRTY;

	if (text->flags &
	    (TEXT_SAVED_ORIGINAL | TEXT_CREATED | TEXT_EDITOR))
		return;
	if (!text->path)
		return;

	save_path = allocate(NULL, strlen(text->path)+2);
	sprintf(save_path, "%s~", text->path);
	errno = 0;
	fd = creat(save_path, S_IRUSR|S_IWUSR);
	if (fd < 0)
		message("can't save copy of original file to %s",
			save_path);
	else {
		length = buffer_raw(text->buffer, &raw, 0, ~0);
		write(fd, raw, length);
		close(fd);
	}
	allocate(save_path, 0);
	text->flags |= TEXT_SAVED_ORIGINAL;
}

void text_preserve(struct text *text)
{
	char *raw;
	unsigned bytes;
	struct stat statbuf;

	if (!(text->flags & TEXT_DIRTY) || text->fd < 0)
		return;
	undo_newlines(text);
	bytes = buffer_raw(text->buffer, &raw, 0, ~0);
	lseek(text->fd, 0, SEEK_SET);
	write(text->fd, raw, bytes);
	ftruncate(text->fd, bytes);
	if (text->newlines)
		newlines(text);
	text->flags &= ~(TEXT_DIRTY | TEXT_CREATED);
	if (!fstat(text->fd, &statbuf))
		text->mtime = statbuf.st_mtime;
}

void buffers_preserve(void)
{
	struct text *text;

	for (text = text_list; text; text = text->next)
		text_preserve(text);
}

void buffers_uncreate(void)
{
	struct text *text;

	for (text = text_list; text; text = text->next)
		if (text->flags & TEXT_CREATED)
			unlink(text->path);
}
