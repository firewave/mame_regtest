CC = gcc
BIN_EXT = .exe

INCLUDES = -I./iconv/include -I./libxml2/include -I./zlib/include
#INCLUDES = -I/usr/include/libxml2

LIBS = ./libxml2/lib/libxml2.lib ./zlib/lib/zdll.lib
#LIBS = -lz -lxml2 -lpthread

ifneq (,$(findstring clang,$(CC)))
WARNINGS = -Weverything -Wno-padded -Wno-disabled-macro-expansion -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-documentation-unknown-command -Wno-documentation -Wno-unused-variable
else
WARNINGS = -Wall -Wextra -Wformat=2 -Wshadow -Wcast-qual -Wwrite-strings -Wunreachable-code -Wno-shadow
endif

CFLAGS = $(WARNINGS) $(INCLUDES)

ifdef DEBUG
CFLAGS += -g
else
CFLAGS += -O3 -s
endif

ifdef LOG_ALLOC
CFLAGS += -DLOG_ALLOC
endif

all: mame_regtest$(BIN_EXT) create_image_xml$(BIN_EXT) create_report$(BIN_EXT)

clean:
	rm -f *.o
	rm -f mame_regtest$(BIN_EXT)
	rm -f create_image_xml$(BIN_EXT)
	rm -f create_report$(BIN_EXT)
	
create_image_xml$(BIN_EXT): create_image_xml.c common.c
	$(CC) $(CFLAGS) $? $(LIBS) -o $@

create_report$(BIN_EXT): create_report.c common.c config.c
	$(CC) $(CFLAGS) $? $(LIBS) -o $@

mame_regtest$(BIN_EXT): mame_regtest.c common.c config.c
	$(CC) $(CFLAGS) $? $(LIBS) -o $@
