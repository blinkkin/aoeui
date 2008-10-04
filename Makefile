VERSION = 1.2
PACKAGE = aoeui-$(VERSION)
SRCS = main.c mem.c die.c display.c text.c file.c locus.c buffer.c \
	undo.c utf8.c window.c util.c clip.c mode.c search.c \
	child.c bookmark.c help.c find.c tags.c tab.c fold.c macro.c \
	keyword.c
HDRS = all.h buffer.h mode.h text.h locus.h utf8.h display.h \
	window.h util.h clip.h macro.h
RELS = $(SRCS:.c=.o)
INST_DIR = $(DESTDIR)/usr
CFLAGS = -Wall -Wno-parentheses \
-Wpointer-arith -Wcast-align -Wwrite-strings -Wstrict-prototypes \
-Wmissing-prototypes -Wmissing-declarations
# -Werror

# Linux and perhaps other systems need to uncomment this line:
# LIBS = -lutil
# BSD users may need -lcompat as well in LIBS

# Uncomment this line if you want to develop aoeui yourself with exuberant-ctags
# CTAGS = exuberant-ctags
CTAGS = ctags

default: optimized aoeui.1 asdfg.1

aoeui: $(RELS)
	$(CC) $(CFLAGS) -o $@ $(RELS) $(LIBS)
$(RELS): $(HDRS)
aoeui.1.gz: aoeui.1
	gzip -9 -c aoeui.1 >$@
asdfg.1.gz: asdfg.1
	gzip -9 -c asdfg.1 >$@
aoeui.1.html: aoeui.1
	man2html aoeui.1 >$@
asdfg.1.html: asdfg.1
	man2html asdfg.1 >$@
aoeui.1: aoeui.1.m4
	m4 aoeui.1.m4 >$@
asdfg.1: aoeui.1.m4
	m4 -D ASDFG aoeui.1.m4 >$@

optimized: clean
	$(MAKE) CFLAGS="-O3 $(CFLAGS)" aoeui
debug: clean
	$(MAKE) CFLAGS="-g -O0 $(CFLAGS)" aoeui
profile: clean
	$(MAKE) CFLAGS="-pg $(CFLAGS)" LIBS="$(LIBS) -pg" aoeui

TAGS: $(SRCS) $(HDRS)
	$(CTAGS) -x $(SRCS) $(HDRS) >$@

install: aoeui aoeui.1.gz asdfg.1.gz
	install -d $(INST_DIR)/bin
	install -d $(INST_DIR)/share/aoeui
	install -d $(INST_DIR)/share/man/man1
	install aoeui $(INST_DIR)/bin
	ln -nf $(INST_DIR)/bin/aoeui $(INST_DIR)/bin/asdfg
	install *.txt $(INST_DIR)/share/aoeui
	install *.1.gz $(INST_DIR)/share/man/man1
clean:
	rm -f *.o core gmon.out screenlog.*
clobber: clean
	rm -f aoeui TAGS aoeui.1 asdfg.1 aoeui.1.gz asdfg.1.gz \
		aoeui.1.html asdfg.1.html unicode
spotless: clobber
	rm -f *~ *.tgz
release: spotless
	rm -rf .tar
	mkdir .tar
	find . | egrep -v '/2.0/' | egrep -v '/\.' | egrep -v '[~#]' | cpio -o | (cd .tar; cpio -id)
	mv .tar $(PACKAGE)
	ls -CF $(PACKAGE)
	tar czf $(PACKAGE).tgz $(PACKAGE)
	rm -rf $(PACKAGE)
