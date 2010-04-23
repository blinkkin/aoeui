/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#ifndef CHILD_H
#define CHILD_H

void mode_child(struct view *);
void mode_shell_pipe(struct view *);
void shell_command(struct view *, Unicode_t);
void background_command(const char *command);

Boolean_t multiplexor(Boolean_t block);
void multiplex_write(fd_t fd, const char *, ssize_t bytes, Boolean_t retain);

#endif
