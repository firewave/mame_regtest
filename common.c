#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

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
#undef fileno
#define fileno _fileno
#endif

#ifndef WIN32
#include <termios.h>
#include <unistd.h>
#include <pthread.h>
#endif

/* zlib */
#include <zlib.h>

/* libxml2 */
#include <libxml/xpath.h>

#include "common.h"

#ifndef WIN32
int mrt_getch(void)
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
	/* TODO: check result */
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
	int res = -1;

	FILE* fd = fopen(file, "rb");
	if( fd ) {
		if( fseek(fd, 0, SEEK_END) == 0 )
		{
			long filesize = ftell(fd);
			if( filesize != -1 )
			{
				if( fseek(fd, 0, SEEK_SET) == 0)
				{
					*content = (char*)malloc((size_t)filesize + 1);
					/* TODO: check result */
					size_t num_read = fread(*content, 1, (size_t)filesize, fd);
					if( num_read == (size_t)filesize )
					{
						(*content)[filesize] = '\0';
						res = 0;
					}
				}
				else
					goto error;
			}
			else
				goto error;
		}
	
		if( res != 0 )
		{
			free(*content);
			*content = NULL;
		}

		fclose(fd);
	}
	else
		goto error;

	return res;

error:
	if( fd != NULL )
	{
		fclose(fd);
	}
	/* TODO: this will return the errno of fclose() */
	return errno;
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

		return -1;
	}
	
	int result = 0;

	char buf[1024];
	size_t read_bytes;
	while( (read_bytes = fread(buf, 1, sizeof(buf), in_fd)) != 0 ) {
        size_t written_bytes = fwrite(buf, 1, read_bytes, out_fd);
		if( written_bytes != read_bytes ) {
			printf("copy_file() - could not write to: %s\n", dest);
			result = -1;
			break;
		}
	}

	fclose(out_fd);
	fclose(in_fd);

	if( result == -1 )
		remove(dest);
	
	return result;
}

void parse_directory(const char* dirname,
						int recursive,
						void (*callback)(struct parse_callback_data* pcd),
						void* user_data)
{
	if( !dirname )
		return;

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
					printf("parse_directory() - could not stat '%s'\n", tmp_de); /* TODO: propagate error */

				free(tmp_de);
			}
			de = readdir(d);
		}

		closedir(d);

		pcd.type = ENTRY_END;		
		pcd.entry_name = NULL;
		pcd.fullname = NULL;
		pcd.s = NULL;
		(callback)(&pcd);
	}
	else
		printf("parse_directory() - could not open '%s'\n", dirname); /* TODO: propagate error */
}

int is_absolute_path(const char* path)
{
	const size_t len = strlen(path);

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

char* get_filename_base(const char* filepath)
{
	char* result = NULL;
	
	const char* pos1 = strrchr(filepath, FILESLASH[0]);
	if( pos1 == NULL )
		pos1 = filepath;
	else 
		pos1 = pos1 + 1;
	const char* pos2 = strrchr(filepath, '.');
	if( pos2 == NULL )
		pos2 = filepath + strlen(filepath);

    const ptrdiff_t len = pos2 - pos1;
	result = (char*)malloc((size_t)len+1);
	/* TODO: check result */
	strncpy(result, pos1, (size_t)len);
	result[len] = '\0';

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
			rmdir(pcd->dirname); /* TODO: check result */
	}
}

void clear_directory(const char* dirname, int delete_root)
{
	if( !dirname )
		return;

	parse_directory(dirname, 0, clear_callback, (void*)&delete_root);

	if( delete_root )
		rmdir(dirname); /* TODO: check result */
}

void calc_crc32(const char* file, unsigned long* crc)
{
	char buffer[1024];
	FILE* fd = fopen(file, "rb");
	if( !fd )
		return;

	size_t bytesread;
	*crc = crc32(0L, Z_NULL, 0);
	while( (bytesread = fread(buffer, 1, 1024, fd)) != 0 ) {
		*crc = crc32(*crc, (const Bytef*)buffer, (uInt)bytesread);
	}

	fclose(fd);
}

char** split_string(const char* str, const char* delims)
{
	char** strings = NULL;
	size_t i = 0;

	char* cpy = strdup(str);
	
	char* pch = strtok(cpy, delims);
	while(pch != NULL)
	{
		strings = (char**)realloc(strings, sizeof(char*) * (i+2));
		/* TODO: check result */
		strings[i] = strdup(pch);
		pch = strtok(NULL, delims);
		i++;
	}

	if( strings != NULL )
		strings[i] = NULL;

	free(cpy);

	return strings;
}

void free_array(char** arr)
{
	if( arr == NULL )
		return;
	int i;
	for(i=0; arr[i] != NULL;++i)
	{
		free(arr[i]);
	}
	free(arr);
}

char* get_directory(const char* filepath)
{
	char* result = NULL;
	
	const char* pos = strrchr(filepath, FILESLASH[0]);
	if( pos ) {
		const ptrdiff_t len = pos - filepath;
		result = (char*)malloc((size_t)len+1);
		/* TODO: check result */
		strncpy(result, filepath, (size_t)len);
		result[len] = '\0';
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
}

#if defined (_MSC_VER) || defined (__MINGW32__)
/* you need to free the result with free_array() */
static char** command_to_argv(const char* command)
{
	char** argv = NULL;
	int argc = 0;

	char** parts = split_string(command, " ");

	int in_quote = 0;
	char* temp = NULL;
	int i = 0;
	for( ; parts[i] != NULL; ++i)
	{
		if( in_quote )
		{
			size_t len = strlen(parts[i]);
			if( parts[i][len-1] == '"' )
			{
				append_string_n(&temp, parts[i], len-1);
				in_quote = 0;

				argv = (char**)realloc(argv, sizeof(char*) * (argc+2));
				/* TODO: check result */
				argv[argc] = strdup(temp);
				free(temp);
				temp = NULL;
				argc++;
			}
		}
		else {
			if( parts[i][0] == '"' )
			{
				in_quote = 1;
				size_t len = strlen(parts[i]);
				if( parts[i][len-1] == '"' )
				{
					argv = (char**)realloc(argv, sizeof(char*) * (argc+2));
					/* TODO: check result */
					append_string_n(&temp, parts[i]+1, len-2);
					argv[argc] = strdup(temp);
					free(temp);
					temp = NULL;
					argc++;
					in_quote = 0;
				}
				else
				{
					append_string_n(&temp, parts[i]+1, len-1);
				}
			}
			else
			{
				argv = (char**)realloc(argv, sizeof(char*) * (argc+2));
				/* TODO: check result */
				argv[argc] = strdup(parts[i]);
				argc++;
			}
		}
	}

	free_array(parts);

	argv[argc] = NULL;

	return argv;
}

int mrt_system(const char* command, char** stdout_str, char** stderr_str)
{
	int exitcode = -1;
	HANDLE hProcess;

	int out = -1;
	int out_pipe[2];
	int err = -1;
	int err_pipe[2];
	
	if( stdout_str )
		append_string(stdout_str, "");

	if( stderr_str )
		append_string(stderr_str, "");
		
	if( stdout_str ) {
		if( pipe(out_pipe, 512, _O_NOINHERIT | O_BINARY ) == -1 )
			return exitcode;

		out = dup(fileno(stdout));

		if( dup2(out_pipe[1], fileno(stdout)) != 0 )
			return exitcode;

		close(out_pipe[1]);
	}

	if( stderr_str ) { 
		if( pipe(err_pipe, 512, _O_NOINHERIT | O_BINARY ) == -1 )
			return exitcode;

		err = dup(fileno(stderr));

		if( dup2(err_pipe[1], fileno(stderr)) != 0 )
			return exitcode;

		close(err_pipe[1]);
	}

	char** argv = command_to_argv(command);

	hProcess = (HANDLE)spawnvp(P_NOWAIT, argv[0], (const char* const*)argv);
	
	if( stderr_str ) {
		if( dup2(err, fileno(stderr)) != 0 )
		{
			free_array(argv);
			return exitcode;
		}

		close(err);
	}

	if( stdout_str ) {
		if( dup2(out, fileno(stdout)) != 0 )
		{
			free_array(argv);
			return exitcode;
		}

		close(out);
	}

	if(hProcess != INVALID_HANDLE_VALUE)
	{
		char szBuffer[512];
		int nExitCode = STILL_ACTIVE;
		while( nExitCode == STILL_ACTIVE )
		{
			if(!GetExitCodeProcess(hProcess,(unsigned long*)&nExitCode))
			{
				free_array(argv);
				return exitcode;
			}
			
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
		}

		exitcode = nExitCode;
	}

	CloseHandle(hProcess);
	
	free_array(argv);

	if( stderr_str )
		close(err_pipe[0]);

	if( stdout_str )
		close(out_pipe[0]);

	return exitcode;
}
#endif

void libxml2_init(void)
{
	if(!xmlFree)
		xmlMemGet(&xmlFree, &xmlMalloc, &xmlRealloc, NULL);
}

#ifdef WIN32
static void WorkerFuntion(void* data)
#else
static void* WorkerFuntion(void* data)
#endif
{
	(void)data;
#ifdef WIN32
	_endthread();
#else
	pthread_exit(0);
#endif
}

void* create_thread(void)
{
#ifdef WIN32
	return (void*)_beginthread(WorkerFuntion, 0, NULL);
#else
	pthread_t thread;
	/* TODO: check result */
	/*int ret = */pthread_create(&thread, NULL, WorkerFuntion, NULL);
	return (void*)thread;
#endif
}

void wait_for_thread(void* thread)
{
#ifdef WIN32
	WaitForSingleObject((HANDLE)thread, INFINITE);
#else
	void* thread_ret = NULL;
	/* TODO: check result */
	/*int ret = */pthread_join((pthread_t)thread, &thread_ret);
#endif
}

void filter_unprintable(char* str, size_t len)
{
	size_t i = 0;
	for(; i < len; ++i)
	{
		int c = (unsigned char)str[i];
		if(isspace(c) || isprint(c))
			continue;
		str[i] = ' ';
	}
}

void append_to_array(struct mrt_array* arr, void* to_append)
{
	arr->ptr = (void**)realloc(arr->ptr, sizeof(void*) * (arr->size+2));
	/* TODO: check result */
	arr->ptr[arr->size] = to_append;
	arr->size = arr->size+1;
	arr->ptr[arr->size] = NULL;
}

/* result must be free'd with xmlXPathFreeNodeSet() */
xmlNodeSetPtr get_xpath_nodeset(xmlDocPtr doc, const xmlChar* xpath_expr)
{
	xmlNodeSetPtr nodeset = NULL;
	
	xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
	if( xpathCtx ) {
		xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpath_expr, xpathCtx);
		if( xpathObj ) {
			nodeset = xpathObj->nodesetval;

			xpathObj->nodesetval = NULL;

			xmlXPathFreeObject(xpathObj);
		}
		else {
			fprintf(stderr, "could not evaluate XPath expression\n");
		}

		xmlXPathFreeContext(xpathCtx);
	}
	else {
		fprintf(stderr, "could not create XPath context\n");
	}
	
	return nodeset;
}

int mrt_setenv(const char* name, const char* value)
{
#ifdef WIN32
	return _putenv_s(name, value);
#else
	return setenv(name, value, 1);
#endif
}
