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

struct in_stream {
	int fd;
	struct view *view; /* if none, message() it */
	unsigned locus;
	struct in_stream *next;
};

struct out_stream {
	int fd;
	char *data;
	unsigned bytes, writ;
	struct out_stream *next;
};

static struct in_stream *ins;
static struct out_stream *outs;

int multiplexor(int block)
{
	struct timeval tv, *tvp = NULL;
	int j, maxfd, bytes;
	struct in_stream *in, *prev_in, *next_in;
	struct out_stream *out, *prev_out, *next_out;
	fd_set fds[3];
	char *rdbuff = NULL;

	for (j = 0; j < 3; j++)
		FD_ZERO(&fds[j]);
	FD_SET(0, &fds[0]);
	FD_SET(0, &fds[2]);
	maxfd = 0;
	for (in = ins; in; in = in->next) {
		FD_SET(in->fd, &fds[0]);
		FD_SET(in->fd, &fds[2]);
		if (in->fd > maxfd)
			maxfd = in->fd;
	}
	for (out = outs; out; out = out->next) {
		FD_SET(out->fd, &fds[1]);
		FD_SET(out->fd, &fds[2]);
		if (out->fd > maxfd)
			maxfd = out->fd;
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

	for (in = ins, prev_in = NULL; in; in = next_in) {
		unsigned offset = locus_get(in->view, in->locus);
		next_in = in->next;
		if (!FD_ISSET(in->fd, &fds[0]) &&
		    !FD_ISSET(in->fd, &fds[2]))
			continue;
		if (!rdbuff)
			rdbuff = allocate(NULL, 512);
		if (in->view && offset == UNSET ||
		    (bytes = read(in->fd, rdbuff, 511)) <= 0) {
			close(in->fd);
			if (in->view)
				locus_destroy(in->view, in->locus);
			if (prev_in)
				prev_in->next = next_in;
			else
				ins = next_in;
			allocate(in, 0);
			continue;
		}
		prev_in = in;
		if (in->view) {
			view_insert(in->view, rdbuff, offset, bytes);
			locus_set(in->view, in->locus, offset + bytes);
		} else {
			rdbuff[bytes] = '\0';
			message("%s", rdbuff);
		}
	}

	for (out = outs, prev_out = NULL; out; out = next_out) {
		next_out = out->next;
		if (!FD_ISSET(out->fd, &fds[1]) &&
		    !FD_ISSET(out->fd, &fds[2]))
			continue;
		errno = 0;
		bytes = write(out->fd, out->data + out->writ,
				out->bytes - out->writ);
		if (bytes < 0 ||
		    (out->writ += bytes) == out->bytes) {
			close(out->fd);
			if (prev_out)
				prev_out->next = next_out;
			else
				outs = next_out;
			allocate(out->data, 0);
			allocate(out, 0);
		} else {
			prev_out = out;
			out->writ += bytes;
		}
	}

	if (rdbuff)
		allocate(rdbuff, 0);

	return FD_ISSET(0, &fds[0]) || FD_ISSET(0, &fds[2]);
};

void demultiplex_view(struct view *view)
{
	struct in_stream *in, *prev_in = NULL, *next_in;

	for (in = ins; in; in = next_in) {
		next_in = in->next;
		if (in->view == view) {
			close(in->fd);
			locus_destroy(view, in->locus);
			if (prev_in)
				prev_in->next = next_in;
			else
				ins = next_in;
			allocate(in, 0);
		}
	}
}

void mode_child(struct view *view)
{
	char *command = view_extract_selection(view);
	const char *shell = getenv("SHELL");
	char *wrbuff = NULL;
	unsigned cursor, to_write;
	int pipefd[3][2];
	int j, pid;
	struct in_stream *std_out, *std_err;
	struct out_stream *std_in;

	if (!command) {
		window_beep(view);
		return;
	}

	view_delete_selection(view);
	cursor = locus_get(view, CURSOR);
	to_write = clip_paste(view, cursor);
	if (to_write) {
		locus_set(view, MARK, cursor);
		wrbuff = view_extract_selection(view);
		view_delete_selection(view);
	} else if (!strncmp(command, "cd ", 3)) {
		errno = 0;
		if (chdir(command + 3))
			message("%s failed", command);
		return;
	}

	errno = 0;
	for (j = 0; j < 3; j++)
		if (pipe(pipefd[j])) {
			message("could not create pipes");
			return;
		}
	if ((pid = fork()) < 0) {
		message("could not fork");
		return;
	}
	if (!pid) {
		for (j = 0; j < 3; j++) {
			dup2(pipefd[j][!!j], j);
			close(pipefd[j][0]);
			close(pipefd[j][1]);
		}
		if (!shell)
			shell = "/bin/sh";
		execl(shell, shell, "-c", command, NULL);
		exit(EXIT_FAILURE);
	}

	for (j = 0; j < 3; j++)
		close(pipefd[j][!!j]);

	if (to_write) {
		std_in = allocate(NULL, sizeof *std_in);
		std_in->fd = pipefd[0][1];
		std_in->data = wrbuff;
		std_in->bytes = to_write;
		std_in->writ = 0;
		std_in->next = outs;
		outs = std_in;
	} else
		close(pipefd[0][1]);

	std_out = allocate(NULL, sizeof *std_out);
	std_out->fd = pipefd[1][0];
	std_out->view = view;
	std_out->locus = locus_create(view, cursor);
	std_out->next = ins;
	ins = std_out;

	std_err = allocate(NULL, sizeof *std_err);
	std_err->fd = pipefd[2][0];
	std_err->view = NULL;
	std_err->next = ins;
	ins = std_err;
}
