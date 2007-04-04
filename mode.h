struct view;
typedef void (*command)(struct view *, unsigned);
struct mode {
	command command;
};

struct mode *mode_default(void);
void mode_search(struct view *);
void mode_child(struct view *);
void mode_shell_pipe(struct view *);
void shell_command(struct view *);
