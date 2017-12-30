BIN_EXT = .exe

INCLUDES = -I./iconv/include -I./libxml2/include -I./zlib/include
#INCLUDES = -I/usr/include/libxml2

LIBS = ./libxml2/lib/libxml2.lib ./zlib/lib/zdll.lib
#LIBS = -lz -lxml2 -lpthread

ifneq (,$(findstring clang,$(CC)))
WARNINGS = -Weverything -Wno-padded -Wno-disabled-macro-expansion -Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-documentation-unknown-command -Wno-documentation -Wno-unused-variable -Wno-reserved-id-macro -Wno-cast-qual -Wno-double-promotion
else
WARNINGS = -Wall -Wextra -Wformat=2 -Wshadow -Wcast-qual -Wwrite-strings -Wunreachable-code -Wpedantic -Wno-shadow -Wno-unused-variable -Wno-cast-qual -Wno-array-bounds
endif

CFLAGS = $(WARNINGS) $(INCLUDES) -g -Werror

ifndef DEBUG
CFLAGS += -O3
endif

ifdef LOG_ALLOC
CFLAGS += -DLOG_ALLOC
endif

all: mame_regtest$(BIN_EXT) create_image_xml$(BIN_EXT) create_report$(BIN_EXT)

clean:
	$(RM) -f *.o
	$(RM) -f mame_regtest$(BIN_EXT)
	$(RM) -f create_image_xml$(BIN_EXT)
	$(RM) -f create_report$(BIN_EXT)
	
create_image_xml$(BIN_EXT): create_image_xml.c common.c
	$(CC) $(CFLAGS) $? $(LIBS) -o $@

create_report$(BIN_EXT): create_report.c common.c config.c
	$(CC) $(CFLAGS) $? $(LIBS) -o $@

mame_regtest$(BIN_EXT): mame_regtest.c common.c config.c
	$(CC) $(CFLAGS) $? $(LIBS) -o $@
