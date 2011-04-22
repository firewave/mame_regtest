#ifndef COMMON_H_
#define COMMON_H_

#include <sys/stat.h>

/* getch */
#ifdef WIN32
#include <conio.h>
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#undef getch
#define getch _getch
#endif
#define mrt_getch getch
#else
int mrt_getch();
#endif

#ifdef WIN32
#define FILESLASH "\\"
#else
#define FILESLASH "/"
#endif

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

/* you have to free the pointer you append the string to */
void append_string(char** str, const char* str_to_append);
void append_quoted_string(char** str, const char* str_to_append);

/* you have to free the content */
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

int mrt_mkdir(const char* path);

/* you have to free the result */
char* get_filename(const char* filepath);

void clear_directory(const char* dirname, int delete_root);

void calc_crc32(const char* file, unsigned int* crc);

/* you have to free the result with free_array() */
char** split_string(const char* str, const char* delims);

void free_array(char** arr);

#endif
