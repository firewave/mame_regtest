#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifndef _MSC_VER
#include <dirent.h>
#else
#include <direct.h>
#include <io.h>
#if _MSC_VER >= 1400
#undef access
#define access _access
#undef rmdir
#define rmdir _rmdir
#undef mkdir
#define mkdir _mkdir
#endif
#define F_OK 00
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

void append_string(char** str, const char* str_to_append)
{
	size_t applen = strlen(str_to_append);
	if( *str == NULL ) {
		*str = (char*)malloc(applen+1);
		memcpy(*str, str_to_append, applen);
		(*str)[applen] = '\0';
	}
	else {
		size_t length = strlen(*str);
		*str = (char*)realloc(*str, length+applen+1);
		memcpy(*str+length, str_to_append, applen);
		(*str)[length+applen] = '\0';
	}
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

#ifndef _MSC_VER
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
#else
	(void)user_data;
	(void)callback;
	(void)recursive;
	// TODO
#endif
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
