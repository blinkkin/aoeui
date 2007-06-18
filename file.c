#include "all.h"
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

static int old_fashioned_read(struct text *text)
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
		char *cwdbuf = allocate(NULL, 1024);
		char *cwd = getcwd(cwdbuf, 1024);
		char *apath = allocate(NULL, strlen(cwd) + pathlen + 2);
		sprintf(apath, "%s/%s", cwd, path);
		if (freepath)
			allocate(path, 0);
		allocate(cwdbuf, 0);
		path = apath;
		freepath = 1;
	}
	fpath = strdup(path);
	if (freepath)
		allocate(path, 0);
	return fpath;
}

static void clean_mmap(struct text *text, unsigned bytes, int flags)
{
	void *p;
	unsigned pagesize = getpagesize();
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
		text->fd = open(path, O_CREAT|O_TRUNC|O_RDWR,
				S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
		if (text->fd < 0) {
			message("can't create %s", path);
			goto fail;
		}
		text->flags |= TEXT_CREATED;
	} else {
		if (!S_ISREG(statbuf.st_mode)) {
			message("%s is not a regular file", path);
			goto fail;
		}
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
		clean_mmap(text, statbuf.st_size, PROT_READ);
		if (!text->clean) {
			text->buffer = buffer_create(path);
			if (old_fashioned_read(text) < 0)
				goto fail;
			grab_mtime(text);
		}
		view->bytes = text->buffer ? buffer_bytes(text->buffer) :
					     text->clean_bytes;
		text_forget_undo(text);
	}
	goto done;

fail:	view_close(view);
	view = NULL;

done:	allocate(path, 0);
	return view;
}

static int try_dir(char *path, const char *dir, const struct tm *gmt)
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
	int fd = -1;
	struct view *view;

	if ((home = getenv("HOME"))) {
		sprintf(dir, "%s/.aoeui", home);
		fd = try_dir(path, dir, gmt);
	}
	if (fd < 0 && (me = getenv("LOGNAME"))) {
		sprintf(dir, "/tmp/aoeui-%s", me);
		fd = try_dir(path, dir, gmt);
	}
	if (fd < 0 && (me = cuserid(NULL))) {
		sprintf(dir, "/tmp/aoeui-%s", me);
		fd = try_dir(path, dir, gmt);
	}
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
	if ((fd = open(path, O_CREAT|O_RDWR,
			S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
		message("can't create %s", path);
		allocate(path, 0);
		return 0;
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
	allocate(text->path, 0);
	text->path = path;
	for (view = text->views; view; view = view->next)
		view_name(view);
	return 1;
}

void text_dirty(struct text *text)
{
	if (text->path && !text->dirties && text->flags & TEXT_RDONLY)
		message("%s is a read-only file, changes will not be saved.",
			text->path);
	text->dirties++;
	if (!text->buffer) {
		text->buffer = buffer_create(text->fd >= 0 ? text->path : NULL);
		if (text->clean)
			buffer_insert(text->buffer, text->clean, 0, text->clean_bytes);
		grab_mtime(text);
	}
}

static void save_original(struct text *text)
{
	char *save_path;
	int fd;

	if (!text->clean ||
	    !text->path ||
	    text->flags & (TEXT_SAVED_ORIGINAL |
			   TEXT_CREATED |
			   TEXT_EDITOR))
		return;

	save_path = allocate(NULL, strlen(text->path)+2);
	sprintf(save_path, "%s~", text->path);
	errno = 0;
	fd = creat(save_path, S_IRUSR|S_IWUSR);
	if (fd < 0)
		message("can't save copy of original file to %s",
			save_path);
	else {
		write(fd, text->clean, text->clean_bytes);
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

	if (text->preserved == text->dirties || text->fd < 0 || !text->buffer)
		return;

	text->preserved = text->dirties;

	text_unfold_all(text);

	if (text->clean) {
		bytes = buffer_raw(text->buffer, &raw, 0, ~0);
		if (bytes == text->clean_bytes &&
		    !memcmp(text->clean, raw, bytes))
			return;
		munmap(text->clean, text->clean_bytes);
		text->clean = NULL;
	}

	save_original(text);

	if (text->mtime &&
	    text->path &&
	    !fstat(text->fd, &statbuf) &&
	    text->mtime < statbuf.st_mtime)
		message("%s has been modified since it was read into the "
			"editor, and those changes may have now been lost.",
			text->path);

	bytes = buffer_raw(text->buffer, &raw, 0, ~0);
	ftruncate(text->fd, bytes);
	clean_mmap(text, bytes, PROT_READ|PROT_WRITE);
	if (text->clean) {
		memcpy(text->clean, raw, bytes);
		msync(text->clean, bytes, MS_SYNC);
	} else {
		lseek(text->fd, 0, SEEK_SET);
		write(text->fd, raw, bytes);
	}

	text->preserved = text->dirties;
	text->flags &= ~TEXT_CREATED;
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
