#include "all.h"

/*
 *	Handle the ^E command, which runs the shell command pipeline
 *	in the selection using the current buffer content as
 *	the standard input, replacing the selection with the
 *	standard output of the child process.
 *
 *	This all works asynchronously.  The standard output and
 *	error streams of the child are monitored with select()
 *	and whenever new child output is available, it's captured
 *	and inserted into the original view.
 */

static struct stream *streams;

typedef Boolean_t (*activity)(struct stream *, char *received, ssize_t bytes);

struct stream {
	struct stream *next;
	fd_t fd;
	Boolean_t retain;
	activity activity;
	struct view *view;
	locus_t locus;
	const char *data;
	size_t bytes, writ;
};

static Boolean_t insertion_activity(struct stream *stream,
				    char *received, ssize_t bytes)
{
	position_t offset;
	if (bytes <= 0)
		return FALSE;
	offset = locus_get(stream->view, stream->locus);
	if (offset == UNSET)
		return FALSE;
	view_insert(stream->view, received, offset, bytes);
	locus_set(stream->view, stream->locus, offset + bytes);
	return TRUE;
}

static Boolean_t shell_output_activity(struct stream *stream, char *received,
				       ssize_t bytes)
{
	position_t offset;

	if (bytes <= 0)
		return FALSE;
	offset = locus_get(stream->view, stream->view->shell_out_locus);
	if (offset == UNSET)
		offset = stream->view->bytes;
	view_insert(stream->view, received, offset, bytes);
	locus_set(stream->view, stream->view->shell_out_locus, offset + bytes);
	return TRUE;
}

static Boolean_t error_activity(struct stream *stream, char *received,
				ssize_t bytes)
{
	if (bytes <= 0)
		return FALSE;
	received[bytes] = '\0';
	message("%s", received);
	return TRUE;
}

static Boolean_t out_activity(struct stream *stream, char *x, ssize_t bytes)
{
	ssize_t chunk = stream->bytes - stream->writ;

	if (chunk <= 0)
		return 0;
	do {
		errno = 0;
		bytes = write(stream->fd, stream->data + stream->writ,
			      chunk);
	} while (bytes < 0 && (errno == EAGAIN || errno == EINTR));

	if (bytes < 0 && errno == EPIPE) {
		message("write failed (child terminated?)");
		return FALSE;
	}
	if (bytes <= 0)
		die("write of %d bytes failed", chunk);
	stream->writ += bytes;
	return bytes > 0;
}

static struct stream *stream_create(fd_t fd)
{
	struct stream *stream = allocate0(sizeof *stream);
	stream->fd = fd;
	stream->locus = NO_LOCUS;
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
	if (stream->view)
		locus_destroy(stream->view, stream->locus);
	if (prev)
		prev->next = stream->next;
	else
		streams = stream->next;
	RELEASE(stream->data);
	RELEASE(stream);
}

Boolean_t multiplexor(Boolean_t block)
{
	struct timeval tv, *tvp = NULL;
	int j;
	fd_t maxfd = 0;
	ssize_t bytes;
	struct stream *stream, *prev, *next;
	fd_set fds[3];
	char *rdbuff = NULL;

	for (j = 0; j < 3; j++)
		FD_ZERO(&fds[j]);
	FD_SET(0, &fds[0]);
	FD_SET(0, &fds[2]);
	for (stream = streams; stream; stream = stream->next) {
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
		return errno != EAGAIN && errno != EINTR;

	for (prev = NULL, stream = streams; stream; stream = next) {
		next = stream->next;
		if (!FD_ISSET(stream->fd, &fds[!!stream->data]) &&
		    !FD_ISSET(stream->fd, &fds[2])) {
			prev = stream;
			continue;
		}
		if (stream->data)
			bytes = 0;
		else {
			if (!rdbuff)
				rdbuff = allocate(1024);
			errno = 0;
			bytes = read(stream->fd, rdbuff, 1023);
		}
		if (stream->activity(stream, rdbuff, bytes))
			prev = stream;
		else
			stream_destroy(stream, prev);
	}

	RELEASE(rdbuff);
	return FD_ISSET(0, &fds[0]) || FD_ISSET(0, &fds[2]);
};

static void child_close(struct view *view)
{
	if (!view)
		return;
	if (view->shell_std_in >= 0) {
		close(view->shell_std_in);
		view->shell_std_in = -1;
	}
	locus_destroy(view, view->shell_out_locus);
	view->shell_out_locus = NO_LOCUS;
}

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
	child_close(view);
}

void multiplex_write(fd_t fd, const char *data, ssize_t bytes, Boolean_t retain)
{
	struct stream *stream;

	if (bytes < 0)
		bytes = data ? strlen(data) : 0;
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

static void newline(fd_t fd)
{
	multiplex_write(fd, strdup("\n"), 1, TRUE /*retain*/);
}

int child(fd_t *stdfd, unsigned stdfds, const char *argv[])
{
	fd_t pipefd[3][2];
	int j, k;
	pid_t pid;

	if (stdfds > 3)
		stdfds = 3;
	errno = 0;
	for (j = 0; j < stdfds; j++)
		if (pipe(pipefd[j])) {
			message("could not create pipes");
			return 0;
		}
	for (; j < 3; j++)
		for (k = 0; k < 2; k++)
			pipefd[j][k] = dup(pipefd[j-1][k]);
	fflush(NULL);
	errno = 0;
	if ((pid = fork()) < 0) {
		message("could not fork");
		return 0;
	}
	if (!pid) {
		for (j = 0; j < 3; j++) {
			errno = 0;
			if (dup2(pipefd[j][!!j], j) != j) {
				fprintf(stderr, "dup2(%d,%d) "
					"failed: %s\n",
					pipefd[j][!!j], j,
					strerror(errno));
				exit(EXIT_FAILURE);
			}
			for (k = 0; k < 2; k++)
				close(pipefd[j][k]);
		}
		setenv("PS1", geteuid() ? "# " : "$ ", 1);
		unsetenv("LS_COLORS");
		unsetenv("TERM");
		errno = 0;
		execvp(argv[0], (char *const *) argv);
		fprintf(stderr, "could not execute %s: %s\n",
			argv[0], strerror(errno));
		exit(EXIT_FAILURE);
	}

	for (j = 0; j < 3; j++) {
		stdfd[j] = pipefd[j][!j];
		close(pipefd[j][!!j]);
	}
	return stdfds;
}

void mode_child(struct view *view)
{
	char *command = view_extract_selection(view);
	char *wrbuff = NULL;
	position_t cursor;
	size_t to_write;
	fd_t stdfd[3];
	const char *argv[4];
	struct stream *std_out, *std_err;
	const char *shell = getenv("SHELL");

	if (!command) {
		window_beep(view);
		return;
	}

	if (view->shell_std_in >= 0) {
		locus_set(view, MARK, UNSET);
		multiplex_write(view->shell_std_in, command, strlen(command),
				TRUE /*retain*/);
		newline(view->shell_std_in);
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
	to_write = clip_paste(view, cursor, 0);
	if (to_write) {
		locus_set(view, MARK, cursor);
		wrbuff = view_extract_selection(view);
		view_delete_selection(view);
	}

	argv[0] = shell ? shell : "/bin/sh";
	argv[1] = "-c";
	argv[2] = command;
	argv[3] = NULL;
	if (!child(stdfd, 3, argv))
		return;

	multiplex_write(stdfd[0], wrbuff, to_write, FALSE /*don't retain*/);
	std_out = stream_create(stdfd[1]);
	std_out->activity = insertion_activity;
	std_out->view = view;
	std_out->locus = locus_create(view, cursor);
	std_err = stream_create(stdfd[2]);
	std_err->activity = error_activity;
}

void mode_shell_pipe(struct view *view)
{
	fd_t stdfd[3];
	const char *argv[4];
	struct stream *output;
	const char *shell = getenv("SHELL");
	int ai = 0;

	argv[ai++] = shell ? shell : "/bin/sh";
	argv[ai++] = "--noediting";
#if 0
	argv[ai++] = "-l";
#endif
#ifndef __APPLE__
	argv[ai++] = "-i";
#endif
	argv[ai++] = NULL;
	if (!child(stdfd, 2, argv))
		return;
	child_close(view);
	view->shell_std_in = stdfd[0];
	view->shell_out_locus = locus_create(view, locus_get(view, CURSOR));
	output = stream_create(stdfd[1]);
	output->activity = shell_output_activity;
	output->view = view;
	close(stdfd[2]);
}

void shell_command(struct view *view)
{
	unsigned offset, cursor, linestart;
	char *command;

	if (view->shell_std_in < 0)
		return;
	cursor = locus_get(view, CURSOR);
	linestart = cursor ? find_line_start(view, cursor-1) : 0;
	offset = locus_get(view, view->shell_out_locus);
	if (offset < linestart || offset >= cursor)
		offset = linestart;
	command = view_extract(view, offset, cursor - offset);
	if (command)
		multiplex_write(view->shell_std_in, command,
				-1, TRUE /*retain*/);
	locus_set(view, view->shell_out_locus, view->bytes);
	locus_set(view, CURSOR, view->bytes);
}
