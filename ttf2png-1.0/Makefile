CFLAGS := -Wall -Wextra -Werror -std=c89 -pedantic -O2
FREETYPE_FLAGS := $(shell freetype-config --cflags --libs)
LIBPNG_FLAGS := $(shell pkg-config --cflags --libs libpng12)

ttf2png: ttf2png.c
	$(CC) $(CFLAGS) $^ -o $@ $(FREETYPE_FLAGS) $(LIBPNG_FLAGS)

.PHONY: clean
clean:
	$(RM) ttf2png

VER = 1.0

.PHONY: tarball
tarball: ttf2png-$(VER).tar.gz
ttf2png-$(VER).tar.gz: ttf2png.c Readme Makefile gpl.txt
	rm -rf ttf2png-$(VER)
	mkdir ttf2png-$(VER)
	cp -a $^ ttf2png-$(VER)
	tar -czf $@ ttf2png-$(VER)
	rm -rf ttf2png-$(VER)
