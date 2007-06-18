struct view;
typedef void (*command)(struct view *, unsigned);

/* All modes start with this header. */
struct mode {
	command command;
};

extern int is_asdfg;
struct mode *mode_default(void);
void mode_search(struct view *, int regex);
void mode_child(struct view *);
void mode_shell_pipe(struct view *);
void shell_command(struct view *);
