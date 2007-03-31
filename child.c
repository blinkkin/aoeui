#include "all.h"
#include <sys/time.h>

/*
 *	Handle the ^E command, which runs the shell command pipeline
 *	in the selection using the current clip buffer content as
 *	the standard input, replacing the selection with the
 *	standard output of the child process.
 *
 *	This all works asynchronously.  The standard output and
 *	error streams of the child are monitored with select()
 *	and whenever new child output is available, it's captured
 *	and inserted into the original view.
 */

static struct stream *streams;

typedef int (*activity)(struct stream *, char *received, int bytes);

struct stream {
	struct stream *next;
	int fd, retain;
	activity activity;
	struct view *view;
	int locus;
	const char *data;
	unsigned bytes, writ;
};

static int insertion_activity(struct stream *stream, char *received, int bytes)
{
	unsigned offset;
	if (bytes <= 0)
		return 0;
	offset = locus_get(stream->view, stream->locus);
	if (offset == UNSET)
		return 0;
	view_insert(stream->view, received, offset, bytes);
	locus_set(stream->view, stream->locus, offset + bytes);
	return 1;
}

static int error_activity(struct stream *stream, char *received, int bytes)
{
	if (bytes <= 0)
		return 0;
	received[bytes] = '\0';
	message("%s", received);
	return 1;
}

static int out_activity(struct stream *stream, char *x, int bytes)
{
	int chunk = stream->bytes - stream->writ;
	if (chunk <= 0)
		return 0;
	do {
		errno = 0;
		bytes = write(stream->fd, stream->data + stream->writ,
			      chunk);
	} while (bytes < 0 && (errno == EAGAIN || errno == EINTR));
	if (bytes <= 0)
		die("write of %d bytes failed", chunk);
	stream->writ += bytes;
	return 1;
}

static struct stream *stream_create(int fd)
{
	struct stream *stream = allocate(NULL, sizeof *stream);
	memset(stream, 0, sizeof *stream);
	stream->fd = fd;
	if (!streams)
		streams = stream;
	else {
		struct stream *prev = streams;
		while (prev->next)
			prev = prev->next;
		prev->next = stream;
	}
	return stream;
}

static void stream_destroy(struct stream *stream, struct stream *prev)
{
	if (!stream->retain)
		close(stream->fd);
	if (stream->view && stream->locus >= 0)
		locus_destroy(stream->view, stream->locus);
	if (prev)
		prev->next = stream->next;
	else
		streams = stream->next;
	allocate(stream->data, 0);
	allocate(stream, 0);
}

int multiplexor(int block)
{
	struct timeval tv, *tvp = NULL;
	int j, maxfd, bytes;
	struct stream *stream, *prev, *next;
	fd_set fds[3];
	char *rdbuff = NULL;

	for (j = 0; j < 3; j++)
		FD_ZERO(&fds[j]);
	FD_SET(0, &fds[0]);
	FD_SET(0, &fds[2]);
	maxfd = 0;
	for (stream = streams; stream; stream = stream->next) {
		if (FD_ISSET(stream->fd, &fds[2]))
			continue;
		FD_SET(stream->fd, &fds[!!stream->data]);
		FD_SET(stream->fd, &fds[2]);
		if (stream->fd > maxfd)
			maxfd = stream->fd;
	}
	if (block)
		tvp = NULL;
	else
		memset(tvp = &tv, 0, sizeof tv);

	errno = 0;
	if (select(maxfd + 1, &fds[0], &fds[1], &fds[2], tvp) < 0)
		if (errno == EAGAIN || errno == EINTR)
			return 0;
		else
			die("select() failed");

	for (prev = NULL, stream = streams; stream; stream = next) {
		next = stream->next;
		if (!FD_ISSET(stream->fd, &fds[!!stream->data]) &&
		    !FD_ISSET(stream->fd, &fds[2]))
			continue;
		if (stream->data)
			bytes = 0;
		else {
			if (!rdbuff)
				rdbuff = allocate(NULL, 512);
			errno = 0;
			bytes = read(stream->fd, rdbuff, 511);
		}
		if (stream->activity(stream, rdbuff, bytes))
			prev = stream;
		else
			stream_destroy(stream, prev);
	}

	allocate(rdbuff, 0);
	return FD_ISSET(0, &fds[0]) || FD_ISSET(0, &fds[2]);
};

void demultiplex_view(struct view *view)
{
	struct stream *stream, *prev = NULL, *next;
	for (stream = streams; stream; stream = next) {
		next = stream->next;
		if (stream->view == view)
			stream_destroy(stream, prev);
		else
			prev = stream;
	}
}

void multiplex_write(int fd, const char *data, unsigned bytes, int retain)
{
	struct stream *stream;

	if (!bytes) {
		if (!retain)
			close(fd);
		return;
	}

	stream = stream_create(fd);
	stream->retain = retain;
	stream->activity = out_activity;
	stream->data = data;
	stream->bytes = bytes;
}

void mode_child(struct view *view)
{
	char *command = view_extract_selection(view);
	char *wrbuff = NULL;
	unsigned cursor, to_write;
	int pipefd[3][2];
	int j, pid;
	struct stream *std_out, *std_err;

	if (!command) {
		window_beep(view);
		return;
	}

	view_delete_selection(view);
	if (command[0] == 'c' && command[1] == 'd' &&
	    (!command[2] || command[2] == ' ')) {
		const char *dir = command + 2;
		while (*dir == ' ')
			dir++;
		if (!*dir && !(dir = getenv("HOME")))
			window_beep(view);
		else {
			errno = 0;
			if (chdir(dir))
				message("%s failed", command);
		}
		return;
	}
	cursor = locus_get(view, CURSOR);
	to_write = clip_paste(view, cursor);
	if (to_write) {
		locus_set(view, MARK, cursor);
		wrbuff = view_extract_selection(view);
		view_delete_selection(view);
	}

	errno = 0;
	for (j = 0; j < 3; j++)
		if (pipe(pipefd[j])) {
			message("could not create pipes");
			return;
		}
	fflush(NULL);
	if ((pid = fork()) < 0) {
		message("could not fork");
		return;
	}
	if (!pid) {
		const char *shell = getenv("SHELL");
		if (!shell)
			shell = "/bin/sh";
		for (j = 0; j < 3; j++) {
			dup2(pipefd[j][!!j], j);
			close(pipefd[j][0]);
			close(pipefd[j][1]);
		}
		errno = 0;
		execl(shell, shell, "-c", command, NULL);
		fprintf(stderr, "could not execute %s: %s\n",
			shell, strerror(errno));
		exit(EXIT_FAILURE);
	}

	for (j = 0; j < 3; j++)
		close(pipefd[j][!!j]);

	multiplex_write(pipefd[0][1], wrbuff, to_write, 0 /*retain*/);
	std_out = stream_create(pipefd[1][0]);
	std_out->activity = insertion_activity;
	std_out->view = view;
	std_out->locus = locus_create(view, cursor);
	std_err = stream_create(pipefd[2][0]);
	std_err->activity = error_activity;
}
