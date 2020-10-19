CC = gcc
LDFLAGS = -lImlib2 -lX11
CFLAGS ?= -O2 -s
#CFLAGS ?= -ggdb -g3 # For debugging

all: xlunch entries.dsv

install: xlunch
	mkdir -p $(DESTDIR)/etc/xlunch/
	mkdir -p $(DESTDIR)/usr/share/xlunch/svgicons/
	mkdir -p $(DESTDIR)/usr/bin/
	mkdir -p $(DESTDIR)/usr/share/icons/hicolor/48x48/apps
	mkdir -p $(DESTDIR)/usr/share/applications
	cp extra/ghost.png $(DESTDIR)/usr/share/icons/hicolor/48x48/apps/xlunch_ghost.png
	cp docs/logo.png $(DESTDIR)/usr/share/icons/hicolor/48x48/apps/xlunch.png
	cp xlunch $(DESTDIR)/usr/bin/
	cp extra/genentries $(DESTDIR)/usr/bin
	cp extra/updateentries $(DESTDIR)/usr/bin
	cp extra/genentries.desktop.sh $(DESTDIR)/usr/bin
	cp extra/genentries.desktop $(DESTDIR)/usr/share/applications/
	cp default.conf $(DESTDIR)/etc/xlunch/default.conf
	bash extra/genentries --path $(DESTDIR)/usr/share/xlunch/svgicons/ > $(DESTDIR)/etc/xlunch/entries.dsv
	cp -r svgicons/ $(DESTDIR)/usr/share/xlunch/ 2>/dev/null || :

remove:
	rm -r $(DESTDIR)/etc/xlunch
	rm -r $(DESTDIR)/usr/share/xlunch
	rm $(DESTDIR)/usr/bin/xlunch
	rm $(DESTDIR)/usr/bin/genentries
	rm $(DESTDIR)/usr/bin/updateentries
	rm $(DESTDIR)/usr/bin/genentries.desktop.sh
	rm $(DESTDIR)/usr/share/icons/hicolor/48x48/apps/xlunch_ghost.png
	rm $(DESTDIR)/usr/share/icons/hicolor/48x48/apps/xlunch.png
	rm $(DESTDIR)/usr/share/applications/genentries.desktop

livetest: xlunch
	./extra/gentriesquick > /tmp/xlunch-tmp.dsv
	./xlunch -g extra/wp.jpg -f "extra/OpenSans-Regular.ttf/10" -i /tmp/xlunch-tmp.dsv -b 140 --iconpadding 20 --textpadding 10 --paddingswap --leastmargin 10 --scroll || true
	rm -f /tmp/xlunch-tmp.dsv

test: xlunch
	./xlunch -g extra/wp.jpg -f "extra/OpenSans-Regular.ttf/10" -i extra/sample_entries.dsv -b 140 --outputonly --iconpadding 20 --textpadding 10 --paddingswap --leastmargin 10 --highlight extra/highlight.png

xlunch: xlunch.c
	$(CC) xlunch.c -o xlunch $(LDFLAGS) $(CFLAGS)

entries.dsv:
	bash extra/genentries > entries.dsv

clean:
	rm -f xlunch
	rm -f entries.dsv

release:
	bash extra/makerelease
