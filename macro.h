#ifndef MACRO_H
#define MACRO_H

struct macro *macro_record(void);
int macro_end_recording(unsigned chop);
int macro_play(struct macro *);
void macro_free(struct macro *);
int macro_getch(void);

#define FUNCTION_KEYS 12
extern struct macro *function_key[FUNCTION_KEYS+1];

#endif
