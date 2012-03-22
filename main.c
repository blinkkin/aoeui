/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#include "all.h"

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
	static int sig[] = { SIGHUP, SIGINT, SIGQUIT,
			     SIGTERM, SIGCHLD,
#ifndef DEBUG
			     SIGFPE, SIGSEGV,
#endif
#ifdef SIGPWR
			     SIGPWR,
#endif
			     0 };
	static int sigig[] = {
#ifdef SIGTTIN
				SIGTTIN,
#endif
#ifdef SIGTTOUT
				SIGTTOUT,
#endif
#ifdef SIGPIPE
				SIGPIPE,
#endif
				0 };
	int j;

	for (j = 0; sig[j]; j++)
		signal(sig[j], sighandler);
	for (j = 0; sigig[j]; j++)
		signal(sigig[j], SIG_IGN);
}

static void save_all(void)
{
	struct text *text;
	Boolean_t msg = FALSE;
	char *raw;

	for (text = text_list; text; text = text->next) {
		if (!text->path || !text->buffer || !text->buffer->path)
			continue;
		text_unfold_all(text);
		if (text->clean &&
		    buffer_raw(text->buffer, &raw, 0, ~(size_t)0) ==
			text->clean_bytes &&
		    !memcmp(text->clean, raw, text->clean_bytes)) {
			unlink(text->buffer->path);
			continue;
		}
		if (!msg) {
			fprintf(stderr, "\ncheck working files for "
				"current unsaved data\n");
			msg = TRUE;
		}
		fprintf(stderr, "\t%s\n", text->buffer->path);
		buffer_snap(text->buffer);
	}
}

int main(int argc, char *const *argv)
{
	int ch, value;
	struct view *view;
	Unicode_t unicode;

	errno = 0;
	if (tcgetattr(1, &original_termios))
		die("not running in a terminal");
	signals();
	atexit(save_all);

	is_asdfg = argc && argv[0] && strstr(argv[0], "asdfg");
	if (!make_writable)
		make_writable = getenv("AOEUI_WRITABLE");

	while ((ch = getopt(argc, argv, "dkoqrsSt:uUw:")) >= 0)
		switch (ch) {
		case 'd':
			is_asdfg = FALSE;
			break;
		case 'k':
			no_keywords = TRUE;
			break;
		case 'o':
			no_save_originals = TRUE;
			break;
		case 'q':
			is_asdfg = TRUE;
			break;
		case 'r':
			read_only = TRUE;
			break;
		case 's':
			default_no_tabs = TRUE;
			break;
		case 'S':
			default_tabs = TRUE;
			break;
		case 't':
			value = atoi(optarg);
			if (value >= 1 && value <= 20)
				default_tab_stop = value;
			else
				message("bad tab stop setting: %s", optarg);
			break;
		case 'u':
			utf8_mode = UTF8_YES;
			break;
		case 'U':
			utf8_mode = UTF8_NO;
			break;
		case 'w':
			make_writable = optarg;
			break;
		default:
			die("unknown flag");
		}

	for (; optind < argc; optind++)
		view_open(argv[optind]);

	/* Main loop */
	while ((view = window_current_view()) &&
	       ((unicode = macro_getch()),
		!IS_ERROR_CODE(unicode)))
		view->mode->command(view, unicode);

	die("error in input");
	return EXIT_FAILURE;
}
