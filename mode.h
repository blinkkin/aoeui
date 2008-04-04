#ifndef MODE_H
#define MODE_H

struct view;
typedef void (*command)(struct view *, Unicode_t);

/* All modes start with this header. */
struct mode {
	command command;
};

extern Boolean_t is_asdfg;
struct mode *mode_default(void);
void mode_search(struct view *, Boolean_t regex);
void mode_child(struct view *);
void mode_shell_pipe(struct view *);
void shell_command(struct view *);

#endif
