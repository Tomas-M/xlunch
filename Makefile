CC = gcc
LDFLAGS = -lImlib2 -lX11
CFLAGS ?= -O2 -s
#CFLAGS ?= -ggdb # For debugging

all: xlunch entries.dsv

install: xlunch
	mkdir -p $(DESTDIR)/etc/xlunch/
	mkdir -p $(DESTDIR)/etc/xlunch/svgicons/
	mkdir -p $(DESTDIR)/usr/bin/
	cp -r svgicons/ $(DESTDIR)/etc/xlunch/ 2>/dev/null || :
	cp extra/ghost.png $(DESTDIR)/usr/share/icons/hicolor/48x48/apps/xlunch_ghost.png
	cp docs/logo.png $(DESTDIR)/usr/share/icons/hicolor/48x48/apps/xlunch.png
	cp xlunch $(DESTDIR)/usr/bin/
	cp extra/genentries $(DESTDIR)/etc/xlunch/
	cp extra/genentries.desktop $(DESTDIR)/usr/share/applications/
	sh extra/genentries | sed "s/;svgicons\//;$(DESTDIR)\/etc\/xlunch\/svgicons\//g" > $(DESTDIR)/etc/xlunch/entries.dsv

remove:
	rm -r $(DESTDIR)/etc/xlunch
	rm -r $(DESTDIR)/usr/bin/xlunch
	rm $(DESTDIR)/usr/share/icons/hicolor/48x48/apps/xlunch_ghost.png
	rm $(DESTDIR)/usr/share/icons/hicolor/48x48/apps/xlunch.png
	rm $(DESTDIR)/usr/share/applications/genentries.desktop

test: xlunch
	./xlunch -g extra/wp.jpg -f "extra/OpenSans-Regular.ttf/10" -i extra/sample_entries.dsv -b 140 --outputonly

xlunch: xlunch.c
	$(CC) xlunch.c -o xlunch $(LDFLAGS) $(CFLAGS)

entries.dsv:
	sh extra/genentries > entries.dsv

clean:
	rm -f xlunch
	rm -f entries.dsv
