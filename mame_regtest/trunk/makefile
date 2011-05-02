CC = gcc
BIN_EXT = .exe

#LIBXML2_INC = -I/usr/include/libxml2
LIBXML2_INC = -I./libxml2/include
ZLIB_INC = -I./zlib/include

#LIBS = -lxml2 -lz
LIBS = ./libxml2/libxml2.lib ./zlib/lib/zdll.lib

WARNINGS = -Wall -Wextra -Wformat=2 -Wshadow -Wcast-qual -Wwrite-strings
#WARNINGS += -Wunreachable-code

#CFLAGS = -g $(WARNINGS)
CFLAGS = -O3 -s $(WARNINGS)
#CFLAGS += -DLOG_ALLOC

all: mame_regtest$(BIN_EXT) create_image_xml$(BIN_EXT) create_report$(BIN_EXT)

clean:
	rm -f *.o
	rm -f mame_regtest$(BIN_EXT)
	rm -f create_image_xml$(BIN_EXT)
	rm -f create_report$(BIN_EXT)
	
create_image_xml$(BIN_EXT): create_image_xml.c common.c
	$(CC) $(LIBXML2_INC) $(ZLIB_INC) $(CFLAGS) $? $(LIBS) -o $@

create_report$(BIN_EXT): create_report.c common.c config.c
	$(CC) $(LIBXML2_INC) $(ZLIB_INC) $(CFLAGS) $? $(LIBS) -o $@

mame_regtest$(BIN_EXT): mame_regtest.c common.c config.c
	$(CC) $(LIBXML2_INC) $(ZLIB_INC) $(CFLAGS) $? $(LIBS) -o $@
