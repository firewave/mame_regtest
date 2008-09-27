#ifndef COMMON_H_
#define COMMON_H_

#ifdef WIN32
#include <conio.h>
#define mrt_getch getch
#define FILESLASH "\\"
#else
#define FILESLASH "/"
#endif
#include <sys/stat.h>

#if !defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN)
/* #define BIG_ENDIAN */
#define LITTLE_ENDIAN
#endif

#if defined(BIG_ENDIAN) && !defined(LITTLE_ENDIAN)

#define htonl(A)  (A)

#elif defined(LITTLE_ENDIAN) && !defined(BIG_ENDIAN)

#define htonl(A)	((((unsigned int)(A) & 0xff000000) >> 24) | \
					(((unsigned int)(A) & 0x00ff0000) >> 8) | \
					(((unsigned int)(A) & 0x0000ff00) << 8) | \
					(((unsigned int)(A) & 0x000000ff) << 24))

#endif

int mrt_getch();
void append_string(char** str, const char* str_to_append);
void append_quoted_string(char** str, const char* str_to_append);
int read_file(const char* file, char** content);
int copy_file(const char* source, const char* dest);

enum
{
	ENTRY_START = 0,
	ENTRY_FILE,
	ENTRY_DIR_START,
	ENTRY_DIR_END,
	ENTRY_END
};

struct parse_callback_data
{
	int type;
	const char* dirname;
	const char* entry_name;
	const char* fullname;
	struct stat* s;
	void* user_data;
};

void parse_directory(const char* dirname,
						int recursive, 
						void (*callback)(struct parse_callback_data* pcd),
						void* user_data);

int is_absolute_path(const char* path);

#endif
