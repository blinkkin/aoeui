
/* loci in all views */
#define CURSOR 0
#define MARK 1
#define DEFAULT_LOCI (MARK+1)

#define UNSET (~0)

struct view;

unsigned locus_create(struct view *, unsigned offset);
    void locus_destroy(struct view *, unsigned locus);
unsigned locus_get(struct view *, unsigned locus);
unsigned locus_set(struct view *, unsigned locus, unsigned offset);
    void loci_adjust(struct view *, unsigned offset, int delta);
