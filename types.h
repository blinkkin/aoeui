/* Copyright 2007, 2008 Peter Klausler.  See COPYING for license. */
#ifndef TYPES_H
#define TYPES_H

typedef unsigned Unicode_t;
typedef unsigned char Byte_t;
typedef enum Boolean_t { FALSE = 0!=0, TRUE = 0==0 } Boolean_t;
typedef size_t position_t;
typedef ssize_t sposition_t;
typedef int fd_t;

#endif
