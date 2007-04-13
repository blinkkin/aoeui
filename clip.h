struct view;
void clip_init(unsigned reg);
unsigned clip(unsigned reg, struct view *, unsigned offset,
	      unsigned bytes, int append);
unsigned clip_paste(struct view *, unsigned offset, unsigned reg);
