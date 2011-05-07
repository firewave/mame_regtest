#ifndef COMMON_H_
#define COMMON_H_

#ifdef LOG_ALLOC

void* mrt_malloc(size_t size);
char* mrt_strdup(const char* str);
void* mrt_realloc(void* ptr, size_t size);
void mrt_free(void *ptr);

#define malloc(size) mrt_malloc(size)
#define strdup(str) mrt_strdup(str)
#define realloc(ptr, size) mrt_realloc(ptr, size)
#define free(ptr) mrt_free(ptr)

xmlDocPtr mrt_xmlNewDoc(const xmlChar* version);
xmlDocPtr mrt_xmlReadFile(const char* URL, const char* encoding, int options);
void mrt_xmlFreeDoc(xmlDocPtr ptr);

#define xmlNewDoc mrt_xmlNewDoc
#define xmlReadFile mrt_xmlReadFile
#define xmlFreeDoc mrt_xmlFreeDoc

xmlChar* mrt_xmlGetProp(xmlNodePtr node, const xmlChar* name);
xmlChar* mrt_xmlStrdup(const xmlChar* cur);
xmlChar* mrt_xmlNodeGetContent(xmlNodePtr cur);
void mrt_xmlFree(void* ptr);

#define xmlGetProp mrt_xmlGetProp
#define xmlStrdup mrt_xmlStrdup
#define xmlNodeGetContent mrt_xmlNodeGetContent
#define xmlFree mrt_xmlFree

void print_leaked_pointers();

#endif

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
void append_string_n(char** str, const char* str_to_append, size_t applen);
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

/* you have to free the result */
char* get_directory(const char* filepath);

void replace_string(const char* input, char** output, const char* old_str, const char* new_str);

int mrt_system(const char* command, char** stdout_str, char** stderr_str);

#endif
