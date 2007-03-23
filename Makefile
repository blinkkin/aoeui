OPT = -O2
LIBS =
#OPT = -g -O0
#OPT = -pg -O2
#LIBS = -pg
CFLAGS = $(OPT) -Wall -Werror \
-Wno-parentheses \
-Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes \
-Wmissing-prototypes -Wmissing-declarations

SRCS = main.c mem.c die.c display.c text.c file.c locus.c buffer.c undo.c \
	utf8.c window.c util.c clip.c mode.c search.c child.c
HDRS = all.h buffer.h mode.h text.h locus.h utf8.h display.h window.h \
	util.h clip.h
RELS = $(SRCS:.c=.o)
TARGET = aoeui
BIN_DIR = $(DESTDIR)/usr/local/bin
SHARE_DIR = $(DESTDIR)/usr/local/share/$(TARGET)
MAN_DIR = $(DESTDIR)/usr/local/share/man/man1

default: $(TARGET)
$(TARGET): $(RELS)
	$(CC) $(CFLAGS) -o $@ $(RELS) $(LIBS)
$(RELS): $(HDRS)
$(TARGET).1.gz: $(TARGET).1
	gzip -c $(TARGET).1 >$@

install: $(TARGET) $(TARGET).1.gz
	cp $(TARGET) $(BIN_DIR)
	cp *.txt $(SHARE_DIR)
	cp $(TARGET).1.gz $(MAN_DIR)
clean:
	rm -f *.o core
clobber: clean
	rm -f $(TARGET) $(TARGET).1.gz
spotless: clobber
	rm -f *~
backup: spotless
	(cd ..; tar czf backup/$(TARGET).`date '+%F'`.tgz $(TARGET))