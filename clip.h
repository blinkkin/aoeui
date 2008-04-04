struct view;
void clip_init(unsigned reg);
size_t clip(unsigned reg, struct view *, position_t, size_t, Boolean_t append);
size_t clip_paste(struct view *, position_t, unsigned reg);
