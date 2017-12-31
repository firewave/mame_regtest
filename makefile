ifeq ($(OS),Windows_NT)
BIN_EXT = .exe
else
BIN_EXT =
endif

ifeq ($(OS),Windows_NT)
INCLUDES = -I./libxml2/include/libxml2 -I./zlib/include
else
INCLUDES = -I/usr/include/libxml2
endif

ifeq ($(OS),Windows_NT)
LIBS = -L./libxml2/lib -L./zlib/lib
endif
LIBS += -lz -lxml2
ifneq ($(OS),Windows_NT)
LIBS += -lpthread
endif

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

ifdef SANITIZE
CFLAGS += -fsanitize=$(SANITIZE) -fno-omit-frame-pointer
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
