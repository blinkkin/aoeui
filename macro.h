#ifndef MACRO_H
#define MACRO_H

struct macro *macro_record(void);
Boolean_t macro_end_recording(Unicode_t chop);
Boolean_t macro_play(struct macro *, int repeat);
void macros_abort(void);
void macro_free(struct macro *);
Unicode_t macro_getch(void);

#endif
