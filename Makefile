CC = gcc
LDFLAGS = -lImlib2 -lX11
CFLAGS ?= -O2 -s
#CFLAGS ?= -ggdb # For debugging

all: xlunch entries.dsv

install: xlunch entries.dsv
	mkdir -p $(DESTDIR)/etc/xlunch/
	mkdir -p $(DESTDIR)/etc/xlunch/svgicons/
	mkdir -p $(DESTDIR)/usr/bin/
	cat entries.dsv | sed "s/;svgicons\//;$(DESTDIR)\/etc\/xlunch\/svgicons\//g" > $(DESTDIR)/etc/xlunch/entries.dsv
	cp -r svgicons/ $(DESTDIR)/etc/xlunch/ 2>/dev/null || :
	cp extra/ghost.png $(DESTDIR)/usr/share/icons/hicolor/48x48/apps/xlunch_ghost.png
	cp xlunch $(DESTDIR)/usr/bin/

remove:
	rm -r $(DESTDIR)/etc/xlunch
	rm -r $(DESTDIR)/usr/bin/xlunch
	rm $(DESTDIR)/usr/share/icons/hicolor/48x48/apps/xlunch_ghost.png

test: xlunch
	./xlunch -g extra/wp.jpg -f "extra/OpenSans-Regular.ttf/10" -i extra/sample_entries.dsv -b 140 --outputonly

xlunch: xlunch.c
	$(CC) xlunch.c -o xlunch $(LDFLAGS) $(CFLAGS)

entries.dsv:
	sh extra/genentries > entries.dsv

clean:
	rm -f xlunch
	rm -f entries.dsv
