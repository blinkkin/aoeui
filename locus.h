#ifndef LOCUS_H
#define LOCUS_H

/* A locus is a fixed point in a view, an offset that get adjusted
 * when insertions and deletions occur before its position.
 */

typedef unsigned locus_t;

/* loci in all views */
#define CURSOR 0
#define MARK 1
#define NO_LOCUS (~0u)
#define DEFAULT_LOCI (MARK+1)

#define UNSET (~0)

struct view;

locus_t locus_create(struct view *, position_t);
void locus_destroy(struct view *, locus_t);
position_t locus_get(struct view *, locus_t);
position_t locus_set(struct view *, locus_t, position_t);
void loci_adjust(struct view *, position_t, int delta);

#endif
