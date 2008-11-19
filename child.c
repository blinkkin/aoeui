/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"
#ifdef __APPLE__
# include <util.h>
#else
# include <pty.h>
#endif
#include <sys/ioctl.h>

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
	struct view *view = stream->view;
	locus_t locus = view->shell_out_locus;

	if (bytes <= 0)
		return FALSE;
	offset = locus_get(view, locus);
	if (offset == UNSET)
		offset = view->bytes;
	else
		offset++;
	locus_set(view, locus, offset);

	while (bytes--) {
		char ch = *received++;
		switch (ch) {
		case '\r':
#if 0
			offset = find_line_start(view, offset);
#endif
			break;
		case CONTROL('H'):
#if 0
			if (offset)
				view_delete(view, --offset, 1);
#endif
			break;
		case '\n':
			offset = locus_get(view, locus);
			/* fall-through */
		default:
			view_insert(view, &ch, offset++, 1);
		}
	}

	offset = locus_get(view, locus);
	locus_set(view, locus, offset - !!offset);
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

	if (bytes < 0 && (errno == EPIPE || errno == EIO)) {
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
	if (view->shell_std_in >= 0) {
		close(view->shell_std_in);
		view->shell_std_in = -1;
	}
	if (view->shell_pg >= 0) {
		killpg(view->shell_pg, SIGHUP);
		view->shell_pg = -1;
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

static void single_write(fd_t fd, Unicode_t ch)
{
	char buf[8];
	size_t len = unicode_utf8(buf, ch);
	char *single = allocate(len);

	memcpy(single, buf, len);
	multiplex_write(fd, single, len, TRUE /*retain*/);
}

static Boolean_t pipes(fd_t fd[3][2], unsigned stdfds)
{
	int j, k;
	static Boolean_t use_ptys = TRUE;

	/* pipe(2) creates two file descriptors: [0] read,   [1] write;
	 * this code transposes some to produce: [0] parent, [1] child
	 */

	if (stdfds == 2 && use_ptys)  {
		struct termios termios = original_termios;
		termios.c_oflag &= ~ONLCR;
		termios.c_lflag &= ~(ECHO|ECHOE| ECHOKE);
		termios.c_lflag |= ECHOK;
		errno = 0;
		if (!openpty(&fd[0][0], &fd[0][1], NULL, &termios, NULL)) {
			for (j = 1; j < 3; j++)
				for (k = 0; k < 2; k++)
					fd[j][k] = dup(fd[0][k]);
			return TRUE;
		}
		message("could not create pty");
		use_ptys = FALSE;
	}

	/* use pipes */
	errno = 0;
	for (j = 0; j < stdfds; j++) {
		if (pipe(fd[j])) {
			message("could not create pipes");
			return 0;
		}
	}
	j = fd[0][0], fd[0][0] = fd[0][1], fd[0][1] = j;
	return TRUE;
}

static pid_t child(fd_t stdfd[3][2], unsigned stdfds, const char *argv[])
{
	int j;
	pid_t pid;

	if (!pipes(stdfd, stdfds))
		return -1;
	fflush(NULL);
	errno = 0;
	if ((pid = fork()) < 0) {
		message("could not fork");
		return -1;
	}

	if (pid)
		return pid;	/* parent */

	/* child */
	for (j = 0; j < 3; j++) {
		close(stdfd[j][0]);
		errno = 0;
		if (dup2(stdfd[j][1], j) != j) {
			fprintf(stderr, "dup2(%d,%d) failed: %s\n",
				stdfd[j][1], j, strerror(errno));
			exit(EXIT_FAILURE);
		}
		close(stdfd[j][1]);
	}

	if (isatty(0)) {
		pid = setsid();		/* new session */
		ioctl(0, TIOCSCTTY);	/* set controlling terminal */
		ioctl(0, TIOCSPGRP, &pid); /* set process group */
	}
	setenv("TERM", "network", TRUE);
	unsetenv("LS_COLORS");

	errno = 0;
	execvp(argv[0], (char *const *) argv);

	fprintf(stderr, "could not execute %s: %s\n",
		argv[0], strerror(errno));
	exit(EXIT_FAILURE);
}

static const char *shell_name(void)
{
	const char *shell = getenv("SHELL");
	if (access(shell, X_OK))
		shell = "/bin/sh";
	return shell;
}

void mode_child(struct view *view)
{
	char *command = view_extract_selection(view);
	char *wrbuff = NULL;
	position_t cursor;
	size_t to_write;
	fd_t stdfd[3][2];
	const char *argv[4];
	struct stream *std_out, *std_err;
	int j;

	if (!command) {
		window_beep(view);
		return;
	}

	if (view->shell_std_in >= 0) {
		locus_set(view, MARK, UNSET);
		multiplex_write(view->shell_std_in, command, strlen(command),
				TRUE /*retain*/);
		single_write(view->shell_std_in, '\n');
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

	argv[0] = shell_name();
	argv[1] = "-c";
	argv[2] = command;
	argv[3] = NULL;
	if (child(stdfd, 3, argv) < 0)
		return;

	for (j = 0; j < 3; j++)
		close(stdfd[j][1]);

	multiplex_write(stdfd[0][0], wrbuff, to_write, FALSE /*don't retain*/);
	std_out = stream_create(stdfd[1][0]);
	std_out->activity = insertion_activity;
	std_out->view = view;
	std_out->locus = locus_create(view, cursor);
	std_err = stream_create(stdfd[2][0]);
	std_err->activity = error_activity;
}

void mode_shell_pipe(struct view *view)
{
	fd_t stdfd[3][2];
	const char *shell, *p, *argv[8];
	struct stream *output;
	int j, ai = 0;
	pid_t pg;

	argv[ai++] = shell = shell_name();
	if ((p = strrchr(shell, '/')) && !strcmp(p+1, "bash"))
		argv[ai++] = "--noediting";
	argv[ai++] = NULL;

	if ((pg = child(stdfd, 2, argv)) < 0)
		return;

	close(stdfd[2][0]);
	for (j = 0; j < 3; j++)
		close(stdfd[j][1]);

	child_close(view);
	view->shell_out_locus = locus_create(view, locus_get(view, CURSOR));
	view->shell_pg = pg;
	output = stream_create(stdfd[1][0]);
	output->activity = shell_output_activity;
	output->view = view;
	view->shell_std_in = stdfd[0][0];
}

void shell_command(struct view *view, Unicode_t ch)
{
	position_t offset, linestart, cursor;
	char *command;

	if (ch >= ' ' || ch == '\t')
		return;

	cursor = locus_get(view, CURSOR);
	linestart = cursor ? find_line_start(view, cursor-1) : 0;
	offset = locus_get(view, view->shell_out_locus) + 1;
	if (offset < linestart || offset >= cursor)
		offset = linestart;
	command = view_extract(view, offset, cursor - offset);
	if (command)
		multiplex_write(view->shell_std_in, command,
				-1, TRUE /*retain*/);
	locus_set(view, view->shell_out_locus,
		  view->bytes ? view->bytes-1 : UNSET);
	locus_set(view, CURSOR, view->bytes);
}
