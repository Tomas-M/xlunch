CC = gcc
LDFLAGS = -lImlib2 -lX11
#CFLAGS ?= -O2 -s
CFLAGS ?= -ggdb

all: xlunch icons.conf

install: xlunch icons.conf
	mkdir -p $(DESTDIR)/etc/xlunch/
	mkdir -p $(DESTDIR)/etc/xlunch/svgicons/
	mkdir -p $(DESTDIR)/usr/bin/
	#cp icons.conf $(DESTDIR)/etc/xlunch/
	cat icons.conf | sed "s/;svgicons\//;$(DESTDIR)\/etc\/xlunch\/svgicons\//g" > $(DESTDIR)/etc/xlunch/icons.conf
	cp -r svgicons/ $(DESTDIR)/etc/xlunch/ 2>/dev/null || :
	cp extra/ghost.png $(DESTDIR)/usr/share/icons/hicolor/48x48/apps/xlunch_ghost.png
	cp xlunch $(DESTDIR)/usr/bin/

remove:
	rm -r $(DESTDIR)/etc/xlunch
	rm -r $(DESTDIR)/usr/bin/xlunch
	rm $(DESTDIR)/usr/share/icons/hicolor/48x48/apps/xlunch_ghost.png

test: xlunch
	./xlunch -g extra/wp.jpg -f "extra/OpenSans-Regular.ttf/10" -c extra/sample_config.cfg -b 140

xlunch: xlunch.c
	$(CC) xlunch.c -o xlunch $(LDFLAGS) $(CFLAGS)

icons.conf:
	sh extra/genconf > icons.conf

clean:
	rm -f xlunch
	rm -f icons.conf
