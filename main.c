#include "all.h"

static void sighandler(int signo)
{
	errno = 0;
	die("fatal signal %d", signo);
}

static void signals(void)
{
	static int sig[] = { SIGHUP, SIGINT, SIGQUIT, SIGFPE, SIGSEGV,
			     SIGTERM,
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
	int ch;
	struct view *view;

	signals();

	while ((ch = getopt(argc, argv, "")) >= 0)
		switch (ch) {
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
