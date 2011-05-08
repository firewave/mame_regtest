CC = gcc
BIN_EXT = .exe

INCLUDES = -I./iconv/include -I./libxml2/include -I./zlib/include

LIBS = ./libxml2/lib/libxml2.lib ./zlib/lib/zdll.lib

WARNINGS = -Wall -Wextra -Wformat=2 -Wshadow -Wcast-qual -Wwrite-strings
#WARNINGS += -Wunreachable-code

#CFLAGS = -g $(WARNINGS) $(INCLUDES)
CFLAGS = -O3 -s $(WARNINGS) $(INCLUDES)
#CFLAGS += -DLOG_ALLOC

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
