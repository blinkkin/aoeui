/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#ifndef MEM_H
#define MEM_H

void *reallocate(const void *, size_t);
#define allocate(sz) (reallocate(NULL, (sz)))
void *allocate0(size_t);
#define RELEASE(p) (reallocate((p), 0), (p) = NULL)

#endif
