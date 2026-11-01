# Quake loader for the SF3500. Same toolchain as the core:
#   make TC=/path/to/buildroot/output/host        (device build)
#   make CC=<prefix>gcc STRIP=<prefix>strip       (any other MIPS toolchain)
#   make preview                                  (PC build with stub display, for screenshots)
TC ?= $(HOME)/handhelds/sf3000-work/buildroot/output/host
ifeq ($(origin CC),default)
CC := $(TC)/bin/mips-mti-linux-gnu-gcc
endif
STRIP ?= $(TC)/bin/mips-mti-linux-gnu-strip

quakeloader.mipsel: quakeloader.c bitfont.c hwdisp.c joykey.c
	$(CC) -O2 -Wall -mips32r2 -mtune=74kc -mhard-float -I. -o $@ $^ -ldl -lpthread -lm
	$(STRIP) $@

preview: quakeloader.c bitfont.c preview_host.c
	gcc -O1 -Wall -DQL_NO_MAIN -I. -o quakeloader_preview $^

clean:
	rm -f quakeloader.mipsel quakeloader_preview
