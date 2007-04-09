int utf8_out(char *, unsigned);
unsigned utf8_length(const char *, unsigned max);
unsigned utf8_length_backwards(const char *, unsigned max);
unsigned utf8_unicode(const char *, unsigned length);

/* Huge Unicode code values are used to bracket folded sections. */
#define FOLD_START 0x40000000
#define FOLD_END   0x60000000
#define FOLDED_BYTES(ch) ((ch) & 0x1fffffff)
