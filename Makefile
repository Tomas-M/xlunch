CC = gcc
LDFLAGS = -lImlib2 -lX11
CFLAGS ?= -O2 -s

all: xlunch icons.conf

install: xlunch icons.conf
	mkdir -p $(DESTDIR)/etc/xlunch/
	mkdir -p $(DESTDIR)/usr/bin/
	cp icons.conf $(DESTDIR)/etc/xlunch/
	cp ghost.png $(DESTDIR)/etc/xlunch/
	cp xlunch $(DESTDIR)/usr/bin/

test: xlunch
	./xlunch -g extra/wp.jpg -f "extra/OpenSans-Regular.ttf/10" -c extra/sample_config.cfg -b 140

xlunch: xlunch.c
	$(CC) xlunch.c -o xlunch $(LDFLAGS) $(CFLAGS)

icons.conf:
	sh extra/genconf > icons.conf

clean:
	rm -f xlunch
	rm -f icons.conf
