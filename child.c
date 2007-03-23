#include "all.h"

/*
 *	Handle the ^E command, which runs the shell command pipeline
 *	in the selection using the current clip buffer content as
 *	the standard input, replacing the selection with the
 *	standard output of the child process.
 */

void mode_child(struct view *view)
{
	char *command = view_extract_selection(view);
	const char *shell = getenv("SHELL");
	char *wrbuff, *rdbuff;
	unsigned cursor, to_write, writ = 0;;
	int pipefd[3][2];
	int j, pid;

	if (!command) {
		window_beep(view);
		return;
	}
	view_delete_selection(view);
	cursor = locus_get(view, CURSOR);
	to_write = clip_paste(view, cursor);
	locus_set(view, MARK, cursor);
	wrbuff = view_extract_selection(view);
	view_delete_selection(view);

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
	rdbuff = allocate(NULL, 512);

	for (;;) {
		fd_set rfds, wfds, xfds;
		do {
			FD_ZERO(&rfds);
			FD_ZERO(&wfds);
			FD_ZERO(&xfds);
			FD_SET(pipefd[1][0], &rfds);
			FD_SET(pipefd[1][0], &xfds);
			FD_SET(pipefd[2][0], &rfds);
			FD_SET(pipefd[2][0], &xfds);
			if (pipefd[2][0] > (j = pipefd[1][0]))
				j = pipefd[2][0];
			if (pipefd[0][1] >= 0) {
				FD_SET(pipefd[0][1], &wfds);
				FD_SET(pipefd[0][1], &xfds);
				if (pipefd[0][1] > j)
					j = pipefd[0][1];
			}
			errno = 0;
			j = select(j+1, &rfds, &wfds, &xfds, NULL);
		} while (j < 0 && (errno == EAGAIN || errno == EINTR));
		if (j <= 0)
			break;
		if (pipefd[0][1] >= 0 &&
		    (FD_ISSET(pipefd[0][1], &wfds) ||
		     FD_ISSET(pipefd[0][1], &xfds))) {
			if ((j = write(pipefd[0][1], wrbuff+writ,
					to_write-writ)) >= 0)
				writ += j;
			else
				writ = to_write;
			if (writ == to_write) {
				close(pipefd[0][1]);
				pipefd[0][1] = -1;
			}
		} else if (FD_ISSET(pipefd[1][0], &rfds) ||
			   FD_ISSET(pipefd[1][0], &xfds)) {
			if ((j = read(pipefd[1][0], rdbuff, 512)) > 0)
				view_insert(view, rdbuff, locus_get(view, CURSOR),
					    j);
			else
				break;
		} else {
			if ((FD_ISSET(pipefd[2][0], &rfds) ||
			     FD_ISSET(pipefd[2][0], &xfds)) &&
			    (j = read(pipefd[2][0], rdbuff, 512-1)) > 0) {
				rdbuff[j] = '\0';
				message("%s", rdbuff);
			}
			break;
		}
	}

	for (j = 0; j < 3; j++)
		close(pipefd[j][!j]);
	allocate(wrbuff, 0);
	allocate(rdbuff, 0);

	locus_set(view, MARK, /*old*/ cursor);
}
