struct view;
void clip_init(void);
unsigned clip(struct view *, unsigned offset, unsigned bytes, int append);
unsigned clip_paste(struct view *, unsigned offset);
