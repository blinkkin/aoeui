/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#ifndef MODE_H
#define MODE_H

struct view;
typedef void (*command)(struct view *, Unicode_t);

/* All modes start with this header. */
struct mode {
	command command;
	rgba_t selection_bgrgba;
};

extern Boolean_t is_asdfg;
struct mode *mode_default(void);
void mode_search(struct view *, Boolean_t regex);

#endif
