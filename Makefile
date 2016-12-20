CC=gcc
CFLAGS=-lImlib2 -lX11 -O2 -s

all: xlunch icons.conf

install: xlunch icons.conf
	mkdir -p $(DESTDIR)/etc/xlunch/
	cp icons.conf $(DESTDIR)/etc/xlunch/
	cp xlunch $(DESTDIR)/usr/bin/

test: xlunch
	./xlunch -g extra/wp.jpg -f "extra/OpenSans-Regular.ttf/10" -c extra/sample_config.cfg

xlunch:
	$(CC) xlunch.c -o xlunch $(CFLAGS)

icons.conf:
	sh extra/genconf > icons.conf

clean:
	rm -f xlunch
	rm -f icons.conf
