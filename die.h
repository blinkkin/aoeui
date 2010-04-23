/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#ifndef DIE_H
#define DIE_H

void die(const char *, ...);
void message(const char *, ...);
void status(const char *, ...);
void status_hide(void);

#endif
