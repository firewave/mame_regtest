#ifdef LOG_ALLOC

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void* m_arr2[100000];
static int m_entries = 100000;

static void init_pointer_array()
{
	static int m_arr_init = 0;
	if( m_arr_init == 0 ) {
		printf("initializaing array\n");
		memset(m_arr2, 0x00, sizeof(m_arr2));
		m_arr_init = 1;
	}
}

void print_leaked_pointers()
{
	printf("leaked pointers:\n");
	int i = 0;
	for( ; i < m_entries; ++i )
	{
		if( m_arr2[i] != NULL )
			printf("%p\n", m_arr2[i]);
	}
}

static void add_pointer(void* ptr)
{
	if( ptr == NULL )
		return;

	int i = 0;
	for( ; i < m_entries; ++i )
	{
		if( m_arr2[i] == NULL )
		{
			m_arr2[i] = ptr;
			break;
		}
	}
	
	if( i == m_entries )
		printf("pointer array too small\n");
}

static void replace_pointer(void* old_ptr, void* new_ptr)
{
	if( old_ptr == NULL ) {
		add_pointer(new_ptr);
		return;
	}

	int i = 0;
	for( ; i < m_entries; ++i )
	{
		if( m_arr2[i] == old_ptr )
		{
			m_arr2[i] = new_ptr;
			break;
		}
	}
	
	if( i == m_entries )
		printf("pointer not found in array\n");
}

static void remove_pointer(void* ptr)
{
	replace_pointer(ptr, NULL);
}

static int m = 0;

void* mrt_malloc(size_t size)
{
	init_pointer_array();

	void* ptr = malloc(size);
	printf("%d - malloc - %d - %p\n", ++m, size, ptr);
	add_pointer(ptr);
	return ptr;
}

char* mrt_strdup(const char* str)
{
	init_pointer_array();

	char* ptr = strdup(str);
	printf("%d - strdup - %p\n", ++m, ptr);
	add_pointer(ptr);
	return ptr;
}

void* mrt_realloc(void* ptr, size_t size)
{
	init_pointer_array();

	void* ptr2 = realloc(ptr, size);
	printf("%d - realloc - %p - %d - %p\n", (ptr == NULL) ? ++m : -1, ptr, size, ptr2);
	replace_pointer(ptr, ptr2);
	return ptr2;
}

void mrt_free(void *ptr)
{
	if( ptr != NULL )
	{
		printf("%d - free - %p\n", --m, ptr);
		free(ptr);
		remove_pointer(ptr);
	}
}

/* libxml2 */
#include "libxml/parser.h"

xmlDocPtr mrt_xmlNewDoc(const xmlChar* version)
{
	init_pointer_array();
	
	xmlDocPtr ptr = xmlNewDoc(version);
	printf("%d - xmlNewDoc - %p\n", ++m, ptr);
	add_pointer(ptr);
	return ptr;
}

xmlDocPtr mrt_xmlReadFile(const char* URL, const char* encoding, int options)
{
	init_pointer_array();
	
	xmlDocPtr ptr = xmlReadFile(URL, encoding, options);
	printf("%d - xmlReadFile - %p\n", ++m, ptr);
	add_pointer(ptr);
	return ptr;
}

void mrt_xmlFreeDoc(xmlDocPtr ptr)
{
	if( ptr != NULL )
	{
		printf("%d - xmlFreeDoc - %p\n", --m, ptr);
		xmlFreeDoc(ptr);
		remove_pointer(ptr);
	}
}

xmlChar* mrt_xmlGetProp(xmlNodePtr node, const xmlChar* name)
{
	init_pointer_array();
	
	xmlChar* ptr = xmlGetProp(node, name);
	printf("%d - xmlGetProp - %p\n", (ptr != NULL) ? ++m : -1, ptr);
	add_pointer(ptr);
	return ptr;
}

xmlChar* mrt_xmlStrdup(const xmlChar* cur)
{
	init_pointer_array();
	
	xmlChar* ptr = xmlStrdup(cur);
	printf("%d - xmlStrdup - %p\n", ++m, ptr);
	add_pointer(ptr);
	return ptr;
}

xmlChar* mrt_xmlNodeGetContent(xmlNodePtr cur)
{
	init_pointer_array();
	
	xmlChar* ptr = xmlNodeGetContent(cur);
	printf("%d - xmlNodeGetContent - %p\n", ++m, ptr);
	add_pointer(ptr);
	return ptr;
}

void mrt_xmlFree(void* ptr)
{
	if( ptr != NULL )
	{
		printf("%d - xmlFree - %p\n", --m, ptr);
		xmlFree(ptr);
		remove_pointer(ptr);
	}
}

#include "common.h"

#else

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#endif

#ifdef __MINGW32__
#undef htonl
#include <windows.h>
#endif

#ifndef _MSC_VER
#include <dirent.h>
#else
#include "dirent.h"
#include <direct.h>
#include <io.h>
#if _MSC_VER >= 1400
#undef access
#define access _access
#undef rmdir
#define rmdir _rmdir
#undef mkdir
#define mkdir _mkdir
#undef strdup
#define strdup _strdup
#endif
#define F_OK 00
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
#include <fcntl.h>
#include <process.h>
#undef pipe
#define pipe _pipe
#undef dup
#define dup _dup
#undef dup2
#define dup2 _dup2
#undef close
#define close _close
#undef spawnvp
#define spawnvp _spawnvp
#undef read
#define read _read
#endif

#ifndef WIN32
#include <termios.h>
#include <unistd.h>
#endif

/* zlib */
#include "zlib.h"

#ifndef WIN32
int mrt_getch()
{
	struct termios oldt, newt;
	int ch;
	tcgetattr( STDIN_FILENO, &oldt );
	newt = oldt;
	newt.c_lflag &= ~( ICANON | ECHO );
	tcsetattr( STDIN_FILENO, TCSANOW, &newt );
	ch = getchar();
	tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
	return ch;
}
#endif

void append_string_n(char** str, const char* str_to_append, size_t applen)
{
	size_t length = 0;
	if( *str != NULL )
		length = strlen(*str);

	*str = (char*)realloc(*str, length+applen+1);
	memcpy(*str+length, str_to_append, applen);
	(*str)[length+applen] = '\0';
}

void append_string(char** str, const char* str_to_append)
{
	size_t applen = strlen(str_to_append);
	append_string_n(str, str_to_append, applen);
}

void append_quoted_string(char** str, const char* str_to_append)
{
	append_string(str, "\"");
	append_string(str, str_to_append);
	append_string(str, "\"");
}

int read_file(const char* file, char** content)
{
	FILE* fd = fopen(file, "rb");
	if( fd ) {
		char buf[1024];
		size_t bufsize = 0;
		size_t read_bytes;
		int count = 0;
		while( (read_bytes = fread(buf, 1, sizeof(buf), fd)) != 0 ) {
			bufsize = (count * sizeof(buf)) + read_bytes;
			*content = (char*)realloc(*content, bufsize);
			memcpy(*content + (count * sizeof(buf)), buf, read_bytes);
			buf[0] = '\0';
			++count;
		}

		/* terminate content */
		*content = (char*)realloc(*content, bufsize + 1);
		(*content)[bufsize] = '\0';

		fclose(fd);
		fd = NULL;

		return 0;
	}

	printf("read_file() - could not open file: %s\n", file);

	return -1;
}

int copy_file(const char* source, const char* dest)
{
	FILE* in_fd = fopen(source, "rb");
	if( !in_fd ) {
		printf("copy_file() - could not open file: %s\n", source);
		return -1;
	}

	FILE* out_fd = fopen(dest, "wb");
	if( !out_fd ) {
		printf("copy_file() - could not create file: %s\n", dest);
		
		fclose(in_fd);
		in_fd = NULL;
		
		return -1;
	}
	
	int result = 0;

	char buf[1024];
	size_t read_bytes;
	size_t written_bytes;
	while( (read_bytes = fread(buf, 1, sizeof(buf), in_fd)) != 0 ) {
		written_bytes = fwrite(buf, 1, read_bytes, out_fd);
		if( written_bytes != read_bytes ) {
			printf("copy_file() - could not write to: %s\n", dest);
			result = -1;
			break;
		}
	}

	fclose(out_fd);
	out_fd = NULL;

	fclose(in_fd);
	in_fd = NULL;
	
	if( result == -1 )
		remove(dest);
	
	return result;
}

void parse_directory(const char* dirname,
						int recursive,
						void (*callback)(struct parse_callback_data* pcd),
						void* user_data)
{
	if( access(dirname, F_OK) != 0 )
		return;

	DIR* d = opendir(dirname);
	if( d ) {
		struct parse_callback_data pcd;
		pcd.type = ENTRY_START;
		pcd.dirname = dirname;
		pcd.entry_name = NULL;
		pcd.fullname = NULL;
		pcd.s = NULL;
		pcd.user_data = user_data;
		(callback)(&pcd);

		struct dirent* de = readdir(d);
		while( de != NULL ) {
			if( (strcmp(de->d_name, ".") != 0) && (strcmp(de->d_name, "..") != 0) ) {
				char* tmp_de = NULL;
				append_string(&tmp_de, dirname);
				append_string(&tmp_de, FILESLASH);
				append_string(&tmp_de, de->d_name);

				struct stat s;
				if( stat(tmp_de, &s) == 0 ) {
					pcd.entry_name = de->d_name;
					pcd.fullname = tmp_de;
					pcd.s = &s;
					if( s.st_mode & S_IFDIR ) {
						pcd.type = ENTRY_DIR_START;
						(callback)(&pcd);
						if( recursive )
							parse_directory(tmp_de, recursive, callback, user_data);
						pcd.type = ENTRY_DIR_END;
						(callback)(&pcd);
					}
					else {
						pcd.type = ENTRY_FILE;
						(callback)(&pcd);
					}
				}
				else
					printf("parse_directory() - could not stat '%s'\n", tmp_de);

				free(tmp_de);
			}
			de = readdir(d);
		};

		closedir(d);

		pcd.type = ENTRY_END;		
		pcd.entry_name = NULL;
		pcd.fullname = NULL;
		pcd.s = NULL;
		(callback)(&pcd);
	}
	else
		printf("parse_directory() - could not open '%s'\n", dirname);
}

int is_absolute_path(const char* path)
{
	int len = strlen(path);

#ifdef WIN32
	if(len >= 3 && path[1] == ':' && path[2] == '\\')
		return 1;
#else
	if(len >= 1 && path[0] == '/')
		return 1;
#endif

	return 0;
}

int mrt_mkdir(const char* path)
{
#ifdef WIN32
	return mkdir(path);
#else
	return mkdir(path, 0777);
#endif
}

char* get_filename(const char* filepath)
{
	char* result = NULL;
	
	const char* pos = strrchr(filepath, FILESLASH[0]);
	if( pos )
		append_string(&result, pos+1);

	return result;	
}

static void clear_callback(struct parse_callback_data* pcd)
{
	if( pcd->type == ENTRY_FILE ) {
		remove(pcd->fullname);
	}
	else if( pcd->type == ENTRY_DIR_END ) {
		int delete_root = 1;
		parse_directory(pcd->fullname, 0, clear_callback, (void*)&delete_root);
	}
	else if( pcd->type == ENTRY_END ) {
		int* delete_root = (int*)pcd->user_data;
		if( *delete_root )
			rmdir(pcd->dirname);
	}
}

void clear_directory(const char* dirname, int delete_root)
{
	parse_directory(dirname, 0, clear_callback, (void*)&delete_root);

	if( delete_root )
		rmdir(dirname);
}

void calc_crc32(const char* file, unsigned int* crc)
{
	char buffer[1024];
	FILE* fd = fopen(file, "rb");
	if( !fd )
		return;

	size_t bytesread;
	*crc = crc32(0L, Z_NULL, 0);
	while( (bytesread = fread(buffer, 1, 1024, fd)) != 0 ) {
		*crc = crc32(*crc, (const Bytef*)buffer, bytesread);
	}

	fclose(fd);
	fd = NULL;
}

char** split_string(const char* str, const char* delims)
{
	char** strings = NULL;
	int i = 0;

	char* cpy = strdup(str);
	
	char* pch = strtok(cpy, delims);
	while(pch != NULL)
	{
		strings = (char**)realloc(strings, sizeof(char*) * (i+2));
		strings[i] = strdup(pch);
		pch = strtok(NULL, delims);
		i++;
	}

	strings[i] = NULL;

	free(cpy);

	return strings;
}

void free_array(char** arr)
{
	int i;
	for(i=0;;++i)
	{
		if(arr[i] == NULL)
			break;
		free(arr[i]);
	}
	free(arr);
}

char* get_directory(const char* filepath)
{
	char* result = NULL;
	
	const char* pos = strrchr(filepath, FILESLASH[0]);
	if( pos ) {
		int len = pos - filepath;
		result = (char*)malloc(len);
		strncpy(result, filepath, len);
	}

	return result;	
}

void replace_string(const char* input, char** output, const char* old_str, const char* new_str)
{	
	char* str_copy = strdup(input);
	char* pos = strstr(str_copy, old_str);
	while( pos != NULL )
	{
		pos = pos + strlen(old_str);
		char* pos1 = strstr(pos, old_str);
		if( pos1 != NULL )
			*pos1 = 0;
		
		append_string(output, new_str);
		append_string(output, pos);
		
		pos = pos1;
	}
	
	free(str_copy);
	str_copy = NULL;
}

int mrt_system(const char* command, char** stdout_str, char** stderr_str)
{
	int exitcode = -1;
	HANDLE hProcess;
	char szBuffer[512];

	int out;
	int out_pipe[2];
	int err;
	int err_pipe[2];
	
	if( stdout_str ) {
		if( pipe(out_pipe, 512, _O_NOINHERIT | O_BINARY ) == -1 )
			return exitcode;

		out = dup(_fileno(stdout));

		if( dup2(out_pipe[1], _fileno(stdout)) != 0 )
			return exitcode;

		close(out_pipe[1]);
	}

	if( stderr_str ) { 
		if( pipe(err_pipe, 512, _O_NOINHERIT | O_BINARY ) == -1 )
			return exitcode;

		err = dup(_fileno(stderr));

		if( dup2(err_pipe[1], _fileno(stderr)) != 0 )
			return exitcode;

		close(err_pipe[1]);
	}

	char** argv = split_string(command, " ");

	hProcess = (HANDLE)spawnvp(P_NOWAIT, argv[0], (const char* const*)argv);
	
	if( stderr_str ) {
		if( dup2(err, _fileno(stderr)) != 0 )
			return exitcode;

		close(err);
	}

	if( stdout_str ) {
		if( dup2(out, _fileno(stdout)) != 0 )
			return exitcode;

		close(out);
	}

	if(hProcess)
	{
		int nExitCode = STILL_ACTIVE;
		while( nExitCode == STILL_ACTIVE )
		{
			if( stdout_str ) {
				int nOutRead = read(out_pipe[0], szBuffer, 512);
				if( nOutRead )
					append_string_n(stdout_str, szBuffer, nOutRead);
			}

			if( stderr_str ) {
				int nErrRead = read(err_pipe[0], szBuffer, 512);
				if( nErrRead )
					append_string_n(stderr_str, szBuffer, nErrRead);
			}

			if(!GetExitCodeProcess(hProcess,(unsigned long*)&nExitCode))
				return exitcode;
		}

		exitcode = nExitCode;
	}

	return exitcode;
}

