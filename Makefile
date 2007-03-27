TARGET = aoeui
VERSION = 1.0_alpha2
SRCS = main.c mem.c die.c display.c text.c file.c locus.c buffer.c undo.c \
	utf8.c window.c util.c clip.c mode.c search.c child.c
HDRS = all.h buffer.h mode.h text.h locus.h utf8.h display.h window.h \
	util.h clip.h
RELS = $(SRCS:.c=.o)
INST_DIR = $(DESTDIR)/usr
CFLAGS += -Wall -Werror -Wno-parentheses \
-Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes \
-Wmissing-prototypes -Wmissing-declarations

default: $(TARGET)
$(TARGET): $(RELS)
	$(CC) $(CFLAGS) -o $@ $(RELS) $(LIBS)
$(RELS): $(HDRS)
$(TARGET).1.gz: $(TARGET).1
	gzip -c $(TARGET).1 >$@

debug: clean
	$(MAKE) CFLAGS="-g -O0"
profile: clean
	$(MAKE) CFLAGS="-pg" LIBS="-pg"

install: $(TARGET) $(TARGET).1.gz
	install -d $(INST_DIR)/bin
	install -d $(INST_DIR)/share/$(TARGET)
	install -d $(INST_DIR)/share/man/man1
	install $(TARGET) $(INST_DIR)/bin
	install *.txt $(INST_DIR)/share/$(TARGET)
	install *.1.gz $(INST_DIR)/share/man/man1
clean:
	rm -f *.o core
clobber: clean
	rm -f $(TARGET) $(TARGET).1.gz
spotless: clobber
	rm -f *~ *.tgz
release: spotless
	rm -rf .tar
	mkdir .tar
	find . | egrep -v '/\.' | egrep -v '[~#]' | cpio -o | (cd .tar; cpio -id)
	mv .tar $(TARGET)-$(VERSION)
	tar czf $(TARGET)-$(VERSION).tgz $(TARGET)-$(VERSION)
	rm -rf $(TARGET)-$(VERSION)
