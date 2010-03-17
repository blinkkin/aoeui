/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

enum utf8_mode utf8_mode = UTF8_AUTO;
const char *make_writable;
Boolean_t no_save_originals;
Boolean_t read_only;

const char *path_format(const char *path)
{
	char *cwdbuf, *cwd;
	const char *slash;

	if (!path || *path != '/')
		return path;
	cwdbuf = allocate(1024);
	cwd = getcwd(cwdbuf, 1024);
	while ((slash = strchr(path, '/')) &&
	       !strncmp(cwd, path, slash - path)) {
		cwd += slash++ - path;
		if (*cwd && *cwd++ != '/')
			break;
		path = slash;
	}
	RELEASE(cwdbuf);
	return path;
}

static ssize_t old_fashioned_read(struct text *text)
{
	char *raw;
	ssize_t got, total = 0;
	size_t max;
#define CHUNK 1024

	do {
		buffer_insert(text->buffer, NULL, total, CHUNK);
		max = buffer_raw(text->buffer, &raw, total, CHUNK);
		errno = 0;
		got = read(text->fd, raw, max);
		if (got < 0) {
			message("%s: can't read",
				path_format(text->path));
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
	const char *freepath = NULL, *home;
	size_t pathlen;

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
		apath = allocate(pathlen+1);
		memcpy(apath, path, pathlen);
		apath[pathlen] = '\0';
		freepath = path = apath;
	}
	if (!strncmp(path, "~/", 2) && (home = getenv("HOME"))) {
		char *apath = allocate(strlen(home) + pathlen);
		sprintf(apath, "%s%s", home, path + 1);
		RELEASE(freepath);
		freepath = path = apath;
	} else if (*path != '/') {
		char *cwdbuf = allocate(1024);
		char *cwd = getcwd(cwdbuf, 1024);
		char *apath = allocate(strlen(cwd) + pathlen + 2);
		sprintf(apath, "%s/%s", cwd, path);
		RELEASE(freepath);
		RELEASE(cwdbuf);
		freepath = path = apath;
	}
	fpath = strdup(path);
	RELEASE(freepath);
	return fpath;
}

static void clean_mmap(struct text *text, size_t bytes, int flags)
{
	void *p;
	size_t pagesize = getpagesize();
	unsigned pages = (bytes + pagesize - 1) / pagesize;

	if (text->clean)
		munmap(text->clean, text->clean_bytes);
	text->clean_bytes = bytes;
	text->clean = NULL;
	if (!pages)
		return;
	p = mmap(0, pages * pagesize, flags, MAP_SHARED, text->fd, 0);
	if (p != MAP_FAILED)
		text->clean = p;
}

static void grab_mtime(struct text *text)
{
	struct stat statbuf;

	if (text->fd >= 0 && !fstat(text->fd, &statbuf))
		text->mtime = statbuf.st_mtime;
	else
		text->mtime = 0;
}

static void scan(struct view *view)
{
	char *raw, scratch[8];
	position_t at;
	size_t bytes = view_raw(view, &raw, 0, getpagesize());
	size_t chop = bytes < view->bytes ? 8 : 0;
	size_t chlen, check;
	Unicode_t ch, lastch = 0;
	int crnl = 0, nl = 0;

	if (utf8_mode == UTF8_NO)
		view->text->flags |= TEXT_NO_UTF8;
	else if (utf8_mode == UTF8_AUTO)
		for (at = 0; at + chop < bytes; at += chlen) {
			chlen = utf8_length(raw + at, bytes - at);
			ch = utf8_unicode(raw + at, chlen);
			check = unicode_utf8(scratch, ch);
			if (chlen != check) {
				view->text->flags |= TEXT_NO_UTF8;
				break;
			}
		}

	for (at = 0; at + chop < bytes; lastch = ch)
		if ((ch = view_unicode(view, at, &at)) == '\n') {
			nl++;
			crnl += lastch == '\r';
		}
	if (nl && crnl == nl)
		view->text->flags |= TEXT_CRNL;
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
			message("%s: can't stat", path_format(path));
			goto fail;
		}
		if (read_only) {
			message("%s: can't create in read-only mode",
				path_format(path));
			goto fail;
		}
		errno = 0;
		text->fd = open(path, O_CREAT|O_TRUNC|O_RDWR,
				S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		if (text->fd < 0) {
			message("%s: can't create", path_format(path));
			goto fail;
		}
		text->flags |= TEXT_CREATED;
	} else {
		if (!S_ISREG(statbuf.st_mode)) {
			message("%s: not a regular file", path_format(path));
			goto fail;
		}
		if (!read_only)
			text->fd = open(path, O_RDWR);
		if (text->fd < 0) {
			errno = 0;
			text->flags |= TEXT_RDONLY;
			text->fd = open(path, O_RDONLY);
			if (text->fd < 0) {
				message("%s: can't open", path_format(path));
				goto fail;
			}
		}
		clean_mmap(text, statbuf.st_size, PROT_READ);
		if (!text->clean) {
			text->buffer = buffer_create(path);
			if (old_fashioned_read(text) < 0)
				goto fail;
			grab_mtime(text);
		}
		view->bytes = text->buffer ? buffer_bytes(text->buffer) :
					     text->clean_bytes;
		scan(view);
		text_forget_undo(text);
	}
	goto done;

fail:	view_close(view);
	view = NULL;

done:	RELEASE(path);
	return view;
}

static fd_t try_dir(char *path, const char *dir, const struct tm *gmt)
{
	struct stat statbuf;

	errno = 0;
	if (stat(dir, &statbuf)) {
		if (errno != ENOENT)
			return -1;
		if (mkdir(dir, S_IRUSR|S_IWUSR|S_IXUSR))
			return -1;
		if (stat(dir, &statbuf))
			return -1;
	}
	if (!S_ISDIR(statbuf.st_mode))
		return -1;
	sprintf(path, "%s/%02d-%02d-%02d.%02d%02d%02d", dir,
		gmt->tm_year+1900, gmt->tm_mon+1, gmt->tm_mday,
		gmt->tm_hour, gmt->tm_min, gmt->tm_sec);
	return open(path, O_CREAT|O_TRUNC|O_RDWR, S_IRUSR|S_IWUSR);
}

struct view *text_new(void)
{
	char dir[128], path[128];
	const char *me, *home;
	time_t now = time(NULL);
	struct tm *gmt = gmtime(&now);
	fd_t fd = -1;
	struct view *view;

	if ((home = getenv("HOME"))) {
		sprintf(dir, "%s/.aoeui", home);
		fd = try_dir(path, dir, gmt);
	}
	if (fd < 0 && (me = getenv("LOGNAME"))) {
		sprintf(dir, "/tmp/aoeui-%s", me);
		fd = try_dir(path, dir, gmt);
	}
#ifndef __APPLE__
	if (fd < 0 && (me = cuserid(NULL))) {
		sprintf(dir, "/tmp/aoeui-%s", me);
		fd = try_dir(path, dir, gmt);
	}
#endif
	if (fd < 0)
		fd = try_dir(path, "/tmp/aoeui", gmt);
	if (fd < 0)
		fd = try_dir(path, "./aoeui", gmt);

	if (fd < 0)
		view = text_create("* New *", TEXT_EDITOR);
	else {
		view = text_create(path, TEXT_CREATED | TEXT_SCRATCH);
		view->text->fd = fd;
	}
	return view;
}

Boolean_t text_rename(struct text *text, const char *path0)
{
	char *path = fix_path(path0);
	struct text *b;
	struct view *view;
	fd_t fd;

	if (!path)
		return FALSE;
	for (b = text; b; b = b->next)
		if (b->path && !strcmp(b->path, path))
			return FALSE;

	errno = 0;
	if ((fd = open(path, O_CREAT|O_RDWR,
			S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
		message("%s: can't create", path_format(path));
		RELEASE(path);
		return FALSE;
	}

	if (text->flags & TEXT_CREATED) {
		unlink(text->path);
		text->flags &= ~(TEXT_CREATED | TEXT_SCRATCH);
	}

	/* Do not truncate or overwrite yet. */
	text->flags |= TEXT_SAVED_ORIGINAL;
	text->flags &= ~TEXT_RDONLY;
	text_dirty(text);
	close(text->fd);
	if (text->clean) {
		munmap(text->clean, text->clean_bytes);
		text->clean = NULL;
	}
	text->fd = fd;
	grab_mtime(text);
	RELEASE(text->path);
	text->path = path;
	keyword_init(text);
	for (view = text->views; view; view = view->next)
		view_name(view);
	return TRUE;
}

void text_dirty(struct text *text)
{
	if (text->path && !text->dirties && text->flags & TEXT_RDONLY)
		message("%s: read-only, %s",
			path_format(text->path),
			make_writable ? "will be made writable"
				      : "changes won't be saved here");
	text->dirties++;
	if (!text->buffer) {
		text->buffer = buffer_create(text->fd >= 0 ? text->path : NULL);
		if (text->clean)
			buffer_insert(text->buffer, text->clean, 0,
				      text->clean_bytes);
		grab_mtime(text);
	}
}

static void save_original(struct text *text)
{
	char *save_path;
	fd_t fd;

	if (no_save_originals ||
	    !text->clean ||
	    !text->path ||
	    text->flags & (TEXT_SAVED_ORIGINAL |
			   TEXT_RDONLY |
			   TEXT_CREATED |
			   TEXT_EDITOR))
		return;

	save_path = allocate(strlen(text->path)+2);
	sprintf(save_path, "%s~", text->path);
	errno = 0;
	fd = creat(save_path, S_IRUSR|S_IWUSR);
	if (fd < 0)
		message("%s: can't save original text", path_format(save_path));
	else {
		write(fd, text->clean, text->clean_bytes);
		close(fd);
	}
	RELEASE(save_path);
	text->flags |= TEXT_SAVED_ORIGINAL;
}

Boolean_t text_is_dirty(struct text *text)
{
	return	text->preserved != text->dirties &&
		text->fd >= 0 &&
		text->buffer;
}

void text_preserve(struct text *text)
{
	char *raw;
	size_t bytes;
	struct stat statbuf;

	if (text->preserved == text->dirties ||
	    text->fd < 0 ||
	    !text->buffer)
		return;
	text->preserved = text->dirties;
	if (read_only)
		return;
	text_unfold_all(text);
	if (text->clean) {
		save_original(text);
		bytes = buffer_raw(text->buffer, &raw, 0, ~0);
		if (bytes == text->clean_bytes &&
		    !memcmp(text->clean, raw, bytes))
			return;
		munmap(text->clean, text->clean_bytes);
		text->clean = NULL;
	}
	if (text->mtime &&
	    text->path &&
	    !fstat(text->fd, &statbuf) &&
	    text->mtime < statbuf.st_mtime)
		message("%s: modified since read into the "
			"editor, changes may have been overwritten.",
			path_format(text->path));
	text->preserved = text->dirties;
	if (text->flags & TEXT_RDONLY && text->path && make_writable) {
		char cmd[128];
		int newfd;
		snprintf(cmd, sizeof cmd, make_writable, text->path);
		background_command(cmd);
		newfd = open(text->path, O_RDWR);
		if (newfd >= 0) {
			close(text->fd);
			text->fd = newfd;
			text->flags &= ~TEXT_RDONLY;
		}
	}
	if (text->flags & TEXT_RDONLY && text->path) {
		int newfd;
		char *new_path = allocate(strlen(text->path) + 2);
		sprintf(new_path, "%s@", text->path);
		errno = 0;
		newfd = open(new_path, O_CREAT|O_TRUNC|O_RDWR,
			     S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		if (newfd < 0) {
			message("%s: can't create", path_format(new_path));
			RELEASE(new_path);
			return;
		}
		message("%s: read-only, new version saved to %s@",
			text->path, text->path);
		text->flags &= ~TEXT_RDONLY;
		close(text->fd);
		RELEASE(text->path);
		text->fd = newfd;
		text->path = new_path;
	}
	text->flags &= ~TEXT_CREATED;
	bytes = buffer_raw(text->buffer, &raw, 0, ~0);
	ftruncate(text->fd, bytes);
	clean_mmap(text, bytes, PROT_READ|PROT_WRITE);
	if (text->clean) {
		memcpy(text->clean, raw, bytes);
		msync(text->clean, bytes, MS_SYNC);
	} else {
		ssize_t wrote;
		lseek(text->fd, 0, SEEK_SET);
		errno = 0;
		wrote = write(text->fd, raw, bytes);
		if (wrote != bytes)
			message("%s: write failed", path_format(text->path));
	}
	grab_mtime(text);
}

void texts_preserve(void)
{
	struct text *text;
	for (text = text_list; text; text = text->next)
		text_preserve(text);
}

void texts_uncreate(void)
{
	struct text *text;
	for (text = text_list; text; text = text->next)
		if (text->flags & TEXT_CREATED)
			unlink(text->path);
}
