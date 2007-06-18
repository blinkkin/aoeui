#include "all.h"
#include <sys/wait.h>

struct termios original_termios;

static void sighandler(int signo)
{
	if (signo == SIGCHLD) {
		while (waitpid(-1, NULL, WNOHANG) > 0)
			;
	} else {
		errno = 0;
		die("fatal signal %d", signo);
	}
}

static void signals(void)
{
	static int sig[] = { SIGHUP, SIGINT, SIGQUIT, SIGFPE, SIGSEGV,
			     SIGTERM, SIGCHLD,
#ifdef SIGPWR
			     SIGPWR,
#endif
			     0 };
	int j;

	for (j = 0; sig[j]; j++)
		signal(sig[j], sighandler);
}

int main(int argc, char *const *argv)
{
	int ch, value;
	struct view *view;

	errno = 0;
	if (tcgetattr(1, &original_termios))
		die("not running in a terminal");
	signals();

	is_asdfg = argc && argv[0] && strstr(argv[0], "asdfg");

	while ((ch = getopt(argc, argv, "dqt:")) >= 0)
		switch (ch) {
		case 'd':
			is_asdfg = 0;
			break;
		case 'q':
			is_asdfg = 1;
			break;
		case 't':
			value = atoi(optarg);
			if (value >= 1 && value <= 20)
				default_tab_stop = value;
			else
				message("bad tab stop setting: %s", optarg);
			break;
		default:
			die("unknown flag -%c", ch);
		}

	for (; optind < argc; optind++)
		view_open(argv[optind]);

	/* Main loop */
	while ((view = window_current_view()) &&
	       (ch = view_getch(view)) >= 0)
		view->mode->command(view, ch);

	return EXIT_SUCCESS;
}
