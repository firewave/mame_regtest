/*
mame_regtest
(c) Copyright 2005-2008 by Oliver Stoeneberg

http://mess.toseciso.org/tools:mame_regtest
http://mess.toseciso.org/tools:mame_regtest:config (documentation of the options in the XML)
http://www.breaken.de/mame_regtest (outdated)

notes:
	- you need libxml2-2.6.27 to compile this
	- you need zlib 1.2.3 to compile this
	- if you are using a system with big-endian byte-order you have to comment the LITTLE_ENDIAN and uncomment the BIG_ENDIAN define
	- XPath tutorial: http://www.zvon.org/xxl/XPathTutorial/General/examples.html

MAME/MESS returncodes:
          1 - terminated / ???
          2 - missing roms
          3 - assertion / fatalerror
          4 - no image specified (MESS-only)
          5 - no such game
          6 - invalid config
        128 - ???  
        100 - exception
-1073741819 - exception (SDLMAME/SDLMESS)
	
mame_regtest returncodes:
	0 - OK
	1 - error
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __GNUC__
#include <unistd.h>
#endif

#ifndef WIN32
#include <arpa/inet.h>
#define USE_VALGRIND 1
#else
#define USE_VALGRIND 0
#endif

/* libxml2 */
#include "libxml/parser.h"
#include "libxml/xpath.h"

/* zlib */
#include "zlib.h"

/* local */
#include "common.h"

#define VERSION "0.68"

struct driver_info {
	xmlChar* name;
	xmlChar* sourcefile;
	int savestate;
	int ram_count;
	int ramsizes[16];
	int bios_count;
	xmlChar* bioses[32];
	int device_count;
	xmlChar* devices[32];
	int device_mandatory;
	void* next;
};

struct image_entry {
	const xmlChar* device_type;
	const xmlChar* device_file;
	void* next;
};

struct driver_entry {
	const char* name;
	const char* sourcefile;
	int ramsize;
	const char* bios;
	struct image_entry* images;
	char postfix[1024];
	int autosave;
	int device_mandatory;
};

static char str_str[16] = "2";
static int pause_at = 100;
static char* mame_exe = NULL;
static char* gamelist_xml_file = NULL;
static int use_autosave = 0;
static int use_ramsize = 0;
static int write_mng = 0;
#if USE_VALGRIND
static int use_valgrind = 0;
static char* valgrind_binary = NULL;
static char* valgrind_parameters = NULL;
static const char* const valgrind_binary_def = "valgrind";
static const char* const valgrind_parameters_def = "--tool=memcheck --error-limit=no --leak-check=full --num-callers=50 --show-reachable=yes --track-fds=yes --leak-resolution=med";
static const char* const valgring_log_str ="--log-file=";
#endif
static char* rompath_folder = NULL;
static int app_type = 0;
static int use_custom_list = 0;
static int use_bios = 0;
static xmlChar* app_ver = NULL;
static int use_sound = 0;
static int use_throttle = 0;
static int use_debug = 0;
static int hack_debug = 0;
static int hack_ftr = 0;
static int use_devices = 1;
static int hack_biospath = 0;
static const char* const config_xml = "mame_regtest.xml";
static const char* const def_xpath_1 = "/mame/game";
static const char* const def_xpath_2 = "/mess/machine";
static char* xpath_expr = NULL;
static char* debugscript_file = NULL;
static const char* const debugscript_file_str = "mrt_debugscript";
static xmlDocPtr global_config_doc = NULL;
static xmlNodePtr global_config_root = NULL;
static int hack_mngwrite = 0;
static int use_nonrunnable = 0;
static int is_debug = 0;
static unsigned int exec_counter = 0;
static const char* const xpath_placeholder = "DRIVER_ROOT";
static int xpath_placeholder_size = 11;
static char output_folder[256] = "mrt_output";
static char* global_device_file = NULL;
static int use_isbios = 0;
static int store_output = 0;
static int clear_output_folder = 0;
static int test_createconfig = 1;
static char* listxml_output = NULL;
static char* additional_options = NULL;
static int skip_mandatory = 0;
static int osdprocessors = 1;
static int print_xpath_results = 0;
static int test_softreset = 0;
static char* temp_folder = NULL;
static char* stdout_temp_file = NULL;
static char* stderr_temp_file = NULL;
static char* dummy_ini_folder = NULL;
static const char* const temp_folder_str = "mrt_temp";
static const char* const stdout_temp_file_str = "tmp_stdout";
static const char* const stderr_temp_file_str = "tmp_stderr";
static const char* const dummy_ini_folder_str = "dummy_ini";
static char current_path[MAX_PATH] = "";
static int hack_pinmame = 0;

static unsigned const char png_sig[8] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
static unsigned const char mng_sig[8] = { 0x8a, 0x4d, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
static unsigned const char IDAT_name[4] = { 'I', 'D', 'A', 'T' };
static unsigned const char IEND_name[4] = { 'I', 'E', 'N', 'D' };
static unsigned const char MEND_name[4] = { 'M', 'E', 'N', 'D' };

static const char* const dummy_root_str = "dummy_root";

enum {
	APP_UNKNOWN = 0,
	APP_MAME 	= 1,
	APP_MESS	= 2
};

static int get_png_IDAT_data(const char* png_name, unsigned int *IDAT_size, unsigned int* IDAT_crc);
static void open_mng_and_skip_header(const char* mng_name, FILE** mng_fd);
static int internal_get_IDAT_data(FILE* in_fd, unsigned int *IDAT_size, unsigned int* IDAT_crc);
static void free_config();
static void cleanup_and_exit(int errcode, const char* errstr);

void append_driver_info(char** str, struct driver_entry* de)
{
	append_string(str, de->name);
	if( strlen(de->postfix) > 0 ) {
		append_string(str, "_");
		append_string(str, de->postfix);
	}
}

static void strip_sampleof_pinmame(const char* listxml_in, const char* listxml_out)
{
	FILE* in_fd = fopen(listxml_in, "rb");
	
	FILE* out_fd = fopen(listxml_out, "wb");
	
	if( in_fd ) {
		const size_t sampleof_size = strlen(" sampleof=\"pinmame\"");
		char buf[4096];
		while ( fgets(buf, sizeof(buf), in_fd) ) {
			char* pos = strstr(buf, "x&os2732.u2");
			if( pos )
				continue;

			pos = strstr(buf, " sampleof=\"pinmame\"");
			if( pos ) {
				char* pos2 = pos+sampleof_size;
				*pos = '\0';
				fprintf(out_fd, "%s%s", buf, pos2);
			}
			else {
				fwrite(buf, 1, strlen(buf), out_fd);
			}
		}
		
		fclose(out_fd);
		out_fd = NULL;
		
		fclose(in_fd);
		in_fd = NULL;
	}
}

static const char* get_inifile()
{
	switch( app_type ) {
		case APP_MAME:
			return "mame.ini";
		case APP_MESS:
			return "mess.ini";
		case APP_UNKNOWN:
			printf("invalid application type\n");
			cleanup_and_exit(1, "aborting");
	}
	
	return ""; /* shut up compiler */
}

static void get_executable(char** sys, struct driver_entry* de, const char* callstr)
{
#if USE_VALGRIND
	if( use_valgrind ) {
		append_string(sys, valgrind_binary);
		append_string(sys, " ");
		append_string(sys, valgrind_parameters);
		append_string(sys, " ");
		append_string(sys, valgring_log_str);
		append_string(sys, output_folder);
		append_string(sys, FILESLASH);
		if( de ) {
			append_driver_info(sys, de);
		}
		else {
			append_string(sys, callstr);			
		}
		append_string(sys, ".valgrind_%p");
		append_string(sys, " ");
	}
#else
	/* shut up compiler */
	(void)de;
	(void)callstr;
#endif
	append_quoted_string(sys, mame_exe);
}

static int mrt_mkdir(const char* path)
{
#ifdef WIN32
	return mkdir(path);
#else
	return mkdir(path, 0777); 
#endif
}

static int parse_mng(const char* file, xmlNodePtr filenode)
{
	int frame = 0;

	FILE *mng_fd = NULL;
	open_mng_and_skip_header(file, &mng_fd);
	if( mng_fd == NULL )
		return frame;

	unsigned int IDAT_size, IDAT_crc;
	int res = 0;

	do {
		res = internal_get_IDAT_data(mng_fd, &IDAT_size, &IDAT_crc);

		if( res == 1 ) {
			/* found IDAT chunk */
			++frame;

			xmlNodePtr framenode = xmlNewChild(filenode, NULL, (const xmlChar*)"frame", NULL);
			char tmp[128];
			snprintf(tmp, sizeof(tmp), "%d", frame);
			xmlNewProp(framenode, (const xmlChar*)"number", (const xmlChar*)tmp);
			snprintf(tmp, sizeof(tmp), "%u", IDAT_size);
			xmlNewProp(framenode, (const xmlChar*)"size", (const xmlChar*)tmp);
			snprintf(tmp, sizeof(tmp), "%x", IDAT_crc);
			xmlNewProp(framenode, (const xmlChar*)"crc", (const xmlChar*)tmp);

			/* jump to IEND chunk */
			internal_get_IDAT_data(mng_fd, &IDAT_size, &IDAT_crc);
			continue;
		}
		else if( res == 2 ) {
			/* end of file reached - do nothing - exit loop */
			break;
		}
		else if(res == 0 ) {
			printf("unexpected error parsing MNG '%s' - please report\n", file);
			break;
		}
	} while( 1 );

	fclose(mng_fd);
	mng_fd = NULL;
	
	return frame;
}

static void calc_crc32(const char* file, unsigned int* crc)
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

static FILE* mrt_fopen(const char* filename, const char* attr)
{
	FILE* result = NULL;
	
	char* outputfile = NULL;
	append_string(&outputfile, output_folder);
	append_string(&outputfile, FILESLASH);
	append_string(&outputfile, filename);
	
	result = fopen(outputfile, attr);
	
	free(outputfile);
	
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

static void clear_directory(const char* dirname, int delete_root)
{
	parse_directory(dirname, 0, clear_callback, (void*)&delete_root);
	
	if( delete_root )
		rmdir(dirname);
}

static void parse_callback(struct parse_callback_data* pcd)
{
	xmlNodePtr* node = (xmlNodePtr*)pcd->user_data;
	if( pcd->type == ENTRY_FILE ) {
		xmlNodePtr filenode = xmlNewChild(*node, NULL, (const xmlChar*)"file", NULL);
		xmlNewProp(filenode, (const xmlChar*)"name", (const xmlChar*)pcd->entry_name);
		char tmp[128];
		if( strstr(pcd->entry_name, ".png") ) {
			unsigned int IDAT_size, IDAT_crc;
			if( get_png_IDAT_data(pcd->fullname, &IDAT_size, &IDAT_crc) ) {
				snprintf(tmp, sizeof(tmp), "%u", IDAT_size);
				xmlNewProp(filenode, (const xmlChar*)"png_size", (const xmlChar*)tmp);
				snprintf(tmp, sizeof(tmp), "%x", IDAT_crc);
				xmlNewProp(filenode, (const xmlChar*)"png_crc", (const xmlChar*)tmp);
			}
		}
		else if( strstr(pcd->entry_name, ".mng") ) {
			int frames = parse_mng(pcd->fullname, filenode);
			snprintf(tmp, sizeof(tmp), "%d", frames);
			xmlNewProp(filenode, (const xmlChar*)"frames", (const xmlChar*)tmp);
		}
		else {
			/* only add file size and CRC for non-PNG and non-MNG files - it will differs for them, although they are identical */
			snprintf(tmp, sizeof(tmp), "%ld", pcd->s->st_size);
			xmlNewProp(filenode, (const xmlChar*)"size", (const xmlChar*)tmp);

			unsigned int crc = 0;
			calc_crc32(pcd->fullname, &crc);
			snprintf(tmp, sizeof(tmp), "%x", crc);
			xmlNewProp(filenode, (const xmlChar*)"crc", (const xmlChar*)tmp);
		}
	}
	else if (pcd->type == ENTRY_DIR_START) {
		xmlNodePtr dirnode = xmlNewChild(*node, NULL, (const xmlChar*)"dir", NULL);
		xmlNewProp(dirnode, (const xmlChar*)"name", (const xmlChar*)pcd->entry_name);
		*node = dirnode;
	}
	else if (pcd->type == ENTRY_DIR_END) {
		*node = (*node)->parent;
	}
}

static void build_output_xml(const char* dirname, xmlNodePtr node)
{
	parse_directory(dirname, 1, parse_callback, (void*)&node);
}

static int create_dummy_root_ini()
{
	printf("creating 'dummy_root' INI\n");
	
	int res = 0;
	if( (access(dummy_ini_folder, F_OK) == 0) || (mrt_mkdir(dummy_ini_folder) == 0) ) {
		char* app_string = NULL;
		append_string(&app_string, dummy_ini_folder);
		append_string(&app_string, FILESLASH);
		append_string(&app_string, get_inifile());

		FILE* fp = fopen(app_string, "w");
		if( fp ) {
			fprintf(fp, "video              none\n"); /* disable video output */
			if( !use_sound )
				fprintf(fp, "sound              0\n"); /* disable sound output */
			if( !use_throttle )
				fprintf(fp, "throttle           0\n"); /* disable throttle */
			if( hack_debug || is_debug ) {
				if( !use_debug )
					fprintf(fp, "debug              0\n"); /* disable debug window */
				else {
					fprintf(fp, "debug              1\n"); /* enable debug window */
					/* pass debugscript, so so debugger won't block the execution */
					fprintf(fp, "debugscript        %s\n", debugscript_file);
				}
			}
			fprintf(fp, "mouse              0\n"); /* disable "mouse" so it doesn't grabs the mouse pointer on window creation */
			fprintf(fp, "window             1\n"); /* run it in windowed mode */
			if( !hack_ftr )
				fprintf(fp, "seconds_to_run     %s\n", str_str);
			else
				fprintf(fp, "frames_to_run      %s\n", str_str);
			if( rompath_folder && (strlen(rompath_folder) > 0) ) {
				if( !hack_biospath )
					fprintf(fp, "rompath            %s\n", rompath_folder); /* rompath for MAME/MESS */
				else
					fprintf(fp, "biospath           %s\n", rompath_folder); /* old biospath for MESS */
			}
			fclose(fp);
			res = 1;
		}
		else
			printf("could not create '%s' in '%s'\n", get_inifile(), dummy_ini_folder);
			
		free(app_string);
		app_string = NULL;
	}
	else
		printf("could not create '%s'\n", dummy_ini_folder);

	return res;
}

static void convert_xpath_expr(char** real_xpath_expr)
{	
	char* xpath_copy = strdup(xpath_expr);
	char* pos = strstr(xpath_copy, xpath_placeholder);
	while( pos != NULL )
	{
		pos = pos + xpath_placeholder_size;
		char* pos1 = strstr(pos, xpath_placeholder);
		if( pos1 != NULL )
			*pos1 = 0;
		
		if( app_type == APP_MAME )
			append_string(real_xpath_expr, def_xpath_1);
		else if( app_type == APP_MESS )
			append_string(real_xpath_expr, def_xpath_2);
			
		append_string(real_xpath_expr, pos);
		
		pos = pos1;
	}
	
	free(xpath_copy);
	xpath_copy = NULL;
}

static void cleanup_and_exit(int errcode, const char* errstr)
{
	if( debugscript_file ) {
		free(debugscript_file);
		debugscript_file = NULL;
	}	

#if USE_VALGRIND
	if( valgrind_parameters ) {
		free(valgrind_parameters);
		valgrind_parameters = NULL;
	}
	if( valgrind_binary ) {
		free(valgrind_binary);
		valgrind_binary = NULL;
	}
#endif
	
	if( gamelist_xml_file ) {
		free(gamelist_xml_file);
		gamelist_xml_file = NULL;
	} 
	if( listxml_output ) {
		free(listxml_output);
		listxml_output = NULL;
	}
	free_config();
	if( global_config_doc ) {
		xmlFreeDoc(global_config_doc);
		global_config_doc = NULL;
	}

	if( stderr_temp_file ) {
		free(stderr_temp_file);
		stderr_temp_file = NULL;
	}
	if( dummy_ini_folder ) {
		free(dummy_ini_folder);
		dummy_ini_folder = NULL;
	}
	if( stdout_temp_file ) {
		free(stdout_temp_file);
		stdout_temp_file = NULL;
	}
	
	clear_directory(temp_folder, 1);
	free(temp_folder);
	temp_folder = NULL;

	clear_directory(dummy_root_str, 1);

	printf("%s\n", errstr);
	exit(errcode);
}

static void print_driver_info(struct driver_entry* de, FILE* print_fd)
{
	fprintf(print_fd, "%s: %s", de->sourcefile, de->name);
	if( de->bios && strlen(de->bios) > 0 )
		fprintf(print_fd, " (bios %s)", de->bios);
	if( de->ramsize > 0 )
		fprintf(print_fd, " (ramsize %d)", de->ramsize);
	struct image_entry* images = de->images;
	while( images ) {
		if( (images->device_type && xmlStrlen(images->device_type) > 0) &&
			(images->device_file && xmlStrlen(images->device_file) > 0) ) {
			fprintf(print_fd, " (%s %s)", images->device_type, images->device_file);
		}
		images = images->next;
	}
}

static void free_image_entries(struct image_entry* images)
{
	while( images ) {
		struct image_entry* temp = images;		
		images = images->next;
		free(temp);
	}
}

static int read_image_entries(const xmlNodePtr node, struct image_entry** images)
{
	*images = NULL;
	
	xmlAttrPtr attrs = node->properties;
	struct image_entry* last = NULL;
	while( attrs ) {
		struct image_entry* image = (struct image_entry*)malloc(sizeof(struct image_entry));
		if( !image )
			return 0;

		memset(image, 0, sizeof(struct image_entry));

		image->device_type = attrs->name;
		xmlNodePtr value = attrs->children;
		if( value )
			image->device_file = value->content;
		else
			image->device_file = NULL;
	
		if( last )
			last->next = image;
		else
			*images = image;
		
		last = image;

		attrs = attrs->next;
	}
	
	return 1;
}

static int internal_get_IDAT_data(FILE* in_fd, unsigned int *IDAT_size, unsigned int* IDAT_crc)
{
	unsigned int block_size = 0;
	unsigned int reversed_block_size = 0;
	unsigned char block_name[4] = "";
	
	do {
		if( fread(&block_size, sizeof(unsigned int), 1, in_fd) != 1 ) {
			printf("could not read block size\n");
			return 0;
		}
				
		reversed_block_size = htonl(block_size);
		
		if( fread(block_name, sizeof(unsigned char), 4, in_fd) != 4 ) {
			printf("could not read block name\n");
			return 0;
		}
		
		if( memcmp(IDAT_name, block_name, 4) == 0 ) {			
			fseek(in_fd, reversed_block_size, SEEK_CUR); /* jump data block */
			
			unsigned int block_crc = 0;
			if( fread(&block_crc, sizeof(unsigned int), 1, in_fd) != 1 ) {
				printf("could not read block CRC\n");
				return 0;
			}
			else {
				*IDAT_size = reversed_block_size;
				*IDAT_crc = block_crc;
				return 1;
			}
		}
		
		if( memcmp(IEND_name, block_name, 4) == 0 ) {
			fseek(in_fd, reversed_block_size, SEEK_CUR); /* jump data block */
			fseek(in_fd, 4, SEEK_CUR); /* jump CRC */
			return 0;
		}
		
		if( memcmp(MEND_name, block_name, 4) == 0 ) {
			fseek(in_fd, reversed_block_size, SEEK_CUR); /* jump data block */
			fseek(in_fd, 4, SEEK_CUR); /* jump CRC */
			return 2;
		}
		
		fseek(in_fd, reversed_block_size, SEEK_CUR); /* jump data block */
		fseek(in_fd, 4, SEEK_CUR); /* jump CRC */
	} while(1);
}

static int get_png_IDAT_data(const char* png_name, unsigned int *IDAT_size, unsigned int* IDAT_crc)
{
	FILE* png_fd = fopen(png_name, "rb");
	if( png_fd == NULL ) {
		printf("could not open %s\n", png_name);
		return 0;
	}

	fseek(png_fd, 0, SEEK_SET);

	unsigned char sig[8] = "";
	
	if( fread(sig, sizeof(unsigned char), 8, png_fd) != 8 ) {
		printf("could not read signature: %s\n", png_name);
		fclose(png_fd);
		return 0;
	}
	
	if( memcmp(png_sig, sig, 8) != 0 ) {
		printf("contains no PNG signature: %s\n", png_name);
		fclose(png_fd);
		return 0;		
	}
	
	int IDAT_res = internal_get_IDAT_data(png_fd, IDAT_size, IDAT_crc);
	if( IDAT_res != 1 ) 
		printf("error getting IDAT data: %s\n", png_name);
	
	fclose(png_fd);
	
	return IDAT_res;
}

static void open_mng_and_skip_header(const char* mng_name, FILE** mng_fd)
{
	*mng_fd = fopen(mng_name, "rb");
	if( *mng_fd == NULL ) {
		printf("could not open %s\n", mng_name);
		return;
	}
	
	fseek(*mng_fd, 0, SEEK_SET);
	
	unsigned char sig[8] = "";
	
	if( fread(sig, sizeof(unsigned char), 8, *mng_fd) != 8 ) {
		printf("could not read signature: %s\n", mng_name);
		fclose(*mng_fd);
		*mng_fd = NULL;
		return;
	}
	
	if( memcmp(mng_sig, sig, 8) != 0 ) {
		printf("contains no MNG signature: %s\n", mng_name);
		fclose(*mng_fd);
		*mng_fd = NULL;
		return;
	}
	
	return;
}

static int execute_mame(struct driver_entry* de, xmlNodePtr* result)
{
	print_driver_info(de, stdout);
	if( use_autosave && de->autosave )
		printf(" (autosave)");
	printf("\n");
	
	/* DEBUG!!! */
	/* return 1; */

	char* sys = NULL;
	
	/* the whole command-line has to be quoted - begin */
	append_string(&sys, "\" ");
	
	get_executable(&sys, de, NULL);
	append_string(&sys, " ");
	append_string(&sys, de->name);
	if( use_autosave && de->autosave )
		append_string(&sys, " -autosave");
	if( de->bios && strlen(de->bios) > 0 ) {
		append_string(&sys, " -bios ");
		append_string(&sys, de->bios);
	}
	if( de->ramsize > 0 ) {
		append_string(&sys, " -ramsize ");
		char ram_buf[16] = "";
		snprintf(ram_buf, sizeof(ram_buf), "%d", de->ramsize);
		append_string(&sys, ram_buf);
	}
	struct image_entry* images = de->images;
	while(images) {
		if( (images->device_type && xmlStrlen(images->device_type) > 0) && 
			(images->device_file && xmlStrlen(images->device_file) > 0) ) {
			append_string(&sys, " -");
			append_string(&sys, (const char*)images->device_type);
			append_string(&sys, " ");
			append_quoted_string(&sys,  (const char*)images->device_file);
		}
		
		images = images->next;
	}
	if( write_mng ) {
		if( hack_mngwrite )
			append_string(&sys, " -mngwrite mng"FILESLASH);
		else
			append_string(&sys, " -mngwrite ");
		append_driver_info(&sys, de);
		append_string(&sys, ".mng");
	}
	if( additional_options && (strlen(additional_options) > 0) ) {
		append_string(&sys, " ");
		append_string(&sys, additional_options);
	}
	append_string(&sys, " -inipath ");
	append_quoted_string(&sys, dummy_ini_folder);
	if( hack_pinmame ) {
		append_string(&sys, " -skip_disclaimer");
		append_string(&sys, " -skip_gameinfo");
	}

	append_string(&sys, " > ");
	append_quoted_string(&sys, stdout_temp_file);
	
	append_string(&sys, " 2> ");
	append_quoted_string(&sys, stderr_temp_file);

	/* the whole command-line has to be quoted - end */
	append_string(&sys, " \"");

	/*
	printf("system call: %s\n", sys);
	printf("press any key to continue\n");
	mrt_getch();
	*/

	/* TODO: errorhandling */
	mrt_mkdir(dummy_root_str);
	int ch_res = chdir(dummy_root_str);
	int sys_res = system(sys);
	ch_res = chdir(current_path);
	
	free(sys);
	sys = NULL;

	if( result ) {
		*result = xmlNewNode(NULL, (const xmlChar*)"result");
		char tmp[128];
		snprintf(tmp, sizeof(tmp), "%d", sys_res);
		xmlNewProp(*result, (const xmlChar*)"exitcode", (const xmlChar*)tmp);
		char* content = NULL;
		if( read_file(stdout_temp_file, &content) == 0 ) {
			xmlNewProp(*result, (const xmlChar*)"stdout", (const xmlChar*)content);
			free(content);
			content = NULL;
		}
		if( read_file(stderr_temp_file, &content) == 0 ) {
			xmlNewProp(*result, (const xmlChar*)"stderr", (const xmlChar*)content);
			free(content);
			content = NULL;
		}
	}

	if( sys_res != 0 ) {
	}
	else { /* sys_res == 0 */
	}
	
	return 1;
}

static void cleanup_driver_info_list(struct driver_info* driv_inf)
{
	if( app_ver ) {
		xmlFree(app_ver);
		app_ver = NULL;
	}
	
	if( driv_inf == NULL )
		return;
	
	struct driver_info* actual_driv_inf = driv_inf;
	do {
		if( actual_driv_inf->name )
			xmlFree(actual_driv_inf->name);
		if( actual_driv_inf->sourcefile )
			xmlFree(actual_driv_inf->sourcefile);
		if( actual_driv_inf->bios_count ) {
			int i = 0;
			for( ; i < actual_driv_inf->bios_count; ++i ) {
				xmlFree(actual_driv_inf->bioses[i]);
			}
		}
		if( actual_driv_inf->device_count ) {
			int i = 0;
			for( ; i < actual_driv_inf->device_count; ++i ) {
				xmlFree(actual_driv_inf->devices[i]);
			}
		}		
		if( actual_driv_inf->next ) {
			struct driver_info* tmp_driv_inf = actual_driv_inf;
			actual_driv_inf = (struct driver_info*)actual_driv_inf->next;
			free(tmp_driv_inf);
		}
		else {
			free(actual_driv_inf);
			break;
		}
	} while (1);
}

static int execute_mame2(struct driver_entry* de)
{
	if( !de->name || *de->name == 0 ) {
		printf("empty game name\n");
		return 0;
	}

	if( !de->sourcefile || *de->sourcefile == 0 ) {
		printf("empty sourcefile\n");
		return 0;
	}

	if( (pause_at > 0) && ((++exec_counter % pause_at) == 0) ) {
		printf("please press any key to continue\n");
		mrt_getch();
	}

	int res = 0;
	xmlNodePtr result1 = NULL;
	xmlNodePtr result2 = NULL;

	xmlDocPtr output_doc = xmlNewDoc((const xmlChar*)"1.0");
	xmlNodePtr output_node = xmlNewNode(NULL, (const xmlChar*)"output");
	xmlDocSetRootElement(output_doc, output_node);
	
	xmlNewProp(output_node, (const xmlChar*)"name", (const xmlChar*)de->name);
	xmlNewProp(output_node, (const xmlChar*)"sourcefile", (const xmlChar*)de->sourcefile);
	if( de->autosave )
		xmlNewProp(output_node, (const xmlChar*)"autosave", (const xmlChar*)"yes");
	if( de->bios && (strlen(de->bios) > 0) )
		xmlNewProp(output_node, (const xmlChar*)"bios", (const xmlChar*)de->bios);
	if( de->ramsize > 0 ) {
		char tmp[10] = "";
		sprintf(tmp, "%d", de->ramsize);
		xmlNewProp(output_node, (const xmlChar*)"ramsize", (const xmlChar*)tmp);
	}
	if( de->device_mandatory )
		xmlNewProp(output_node, (const xmlChar*)"mandatory", (const xmlChar*)"yes");

	if( de->images ) {
		xmlNodePtr devices_node = xmlNewChild(output_node, NULL, (const xmlChar*)"devices", NULL);
		
		struct image_entry* images = de->images;
		while(images) {
			if( (images->device_type && xmlStrlen(images->device_type) > 0) &&
				(images->device_file && xmlStrlen(images->device_file) > 0) ) {
				xmlNewProp(devices_node, images->device_type, images->device_file);
			}
	
			images = images->next;
		}			
	}

	res = execute_mame(de, &result1);

	if( result1 ) {
		xmlAddChild(output_node, result1);
		build_output_xml(dummy_root_str, result1);
	}

	if( res == 1 && use_autosave && de->autosave ) {
		res = execute_mame(de, &result2);

		if( result2 ) {
			xmlAddChild(output_node, result2);
			build_output_xml(dummy_root_str, result2);
		}
	}

	if( store_output && (access(dummy_root_str, F_OK) == 0) ) {
		char* outputdir = NULL;
		append_string(&outputdir, output_folder);
		append_string(&outputdir, FILESLASH);
		append_driver_info(&outputdir, de);

		if( rename(dummy_root_str, outputdir) != 0 )
			printf("could not rename '%s' to '%s'\n", dummy_root_str, outputdir);

		free(outputdir);
		outputdir = NULL;
	}
	else {
		clear_directory(dummy_root_str, 0);
	}

	char* outputname = NULL;
	append_string(&outputname, output_folder);
	append_string(&outputname, FILESLASH);
	append_driver_info(&outputname, de);
	append_string(&outputname, ".xml");

	xmlSaveFormatFileEnc(outputname, output_doc, "UTF-8", 1);
	
	free(outputname);
	outputname = NULL;
	
	if( output_doc ) {
		xmlFreeDoc(output_doc);
		output_doc = NULL;
	}

	return res;
}

static int execute_mame3(struct driver_entry* de, struct driver_info* actual_driv_inf)
{
	int res = 0;
	
	if( skip_mandatory && de->device_mandatory ) {
		res = 1;
	}
	else {
		res = execute_mame2(de);
		if( res == 0 )
			return res;
	}

	if( use_devices && actual_driv_inf->device_count ) {
		char* device_file = NULL;
		if( global_device_file && (strlen(global_device_file) > 0) )
			append_string(&device_file, global_device_file);
		else {
			append_string(&device_file, "mrt_");
			append_string(&device_file, (const char*)actual_driv_inf->name);
			append_string(&device_file, ".xml");
		}
		if( access(device_file, F_OK) == 0 ) {
			printf("found device file: %s\n", device_file);

			xmlDocPtr device_doc = xmlReadFile(device_file, NULL, 0);
			if( device_doc ) {
				int j = 0;

				char initial_postfix[1024];
				strcpy(initial_postfix, de->postfix);

				xmlNodePtr device_root = xmlDocGetRootElement(device_doc);
				xmlNodePtr device_node = device_root->children;
				while( device_node ) {
					if( device_node->type == XML_ELEMENT_NODE ) {
						struct image_entry* images = NULL;
						read_image_entries(device_node, &images);

						snprintf(de->postfix, sizeof(de->postfix), "%sdev%05d", initial_postfix, j);

						de->images = images;

						res = execute_mame2(de);

						free_image_entries(images);
						de->images = NULL;
						++j;
					}

					device_node = device_node->next;
				}

				xmlFreeDoc(device_doc);
				device_doc = NULL;
			}
		}
		
		free(device_file);
		device_file = NULL;
	}
	
	return res;
}

static void process_driver_info_list(struct driver_info* driv_inf)
{
	if( driv_inf == NULL )
		return;
	
	struct driver_info* actual_driv_inf = driv_inf;
	int res = 0;
	do {
		struct driver_entry de;
		de.name = (const char*)actual_driv_inf->name;
		de.sourcefile = (const char*)actual_driv_inf->sourcefile;
		de.ramsize = 0;
		de.bios = NULL;
		de.images = NULL;
		de.postfix[0] = '\0';
		de.autosave = actual_driv_inf->savestate;
		de.device_mandatory = actual_driv_inf->device_mandatory;
		
		/* only run with the default options when no additional bioses or ramsizes are available to avoid duplicate runs */
		if( actual_driv_inf->bios_count <= 1 &&
			actual_driv_inf->ram_count <= 1 ) {
			res = execute_mame3(&de, actual_driv_inf);
		}

		if( actual_driv_inf->bios_count > 1 ) {
			int i = 0;
			for( ; i < actual_driv_inf->bios_count; ++i )
			{
				snprintf(de.postfix, sizeof(de.postfix), "bios%05d", i);
				
				de.bios = (const char*)actual_driv_inf->bioses[i];
				
				res = execute_mame3(&de, actual_driv_inf);
			}
			
			de.bios = NULL;
		}
		
		if( actual_driv_inf->ram_count > 1 ) {
			int i = 0;
			for( ; i < actual_driv_inf->ram_count; ++i )
			{
				snprintf(de.postfix, sizeof(de.postfix), "ram%05d", i);

				de.ramsize = actual_driv_inf->ramsizes[i];

				res = execute_mame3(&de, actual_driv_inf);
			}
			
			de.ramsize = 0;
		}
		
		if( actual_driv_inf->bios_count > 0 &&
			actual_driv_inf->ram_count > 0 ) {
			int bios_i = 0;
			for( ; bios_i < actual_driv_inf->bios_count; ++bios_i )
			{
				de.bios = (const char*)actual_driv_inf->bioses[bios_i];

				int ram_i = 0;
				for( ; ram_i < actual_driv_inf->ram_count; ++ram_i )
				{
					snprintf(de.postfix, sizeof(de.postfix), "bios%05dram%05d", bios_i, ram_i);
	
					de.ramsize = actual_driv_inf->ramsizes[ram_i];
	
					res = execute_mame3(&de, actual_driv_inf);
				}
			}
		}
		
		if( actual_driv_inf->next )
			actual_driv_inf = (struct driver_info*)actual_driv_inf->next;
		else
			break;
	} while(res == 1);
}

static void parse_listxml_element(const xmlNodePtr game_child, struct driver_info** new_driv_inf)
{
	/* "game" in MAME and "machine" in MESS */
	if( ((app_type == APP_MAME) && (xmlStrcmp(game_child->name, (const xmlChar*)"game") == 0)) ||
		((app_type == APP_MESS) && (xmlStrcmp(game_child->name, (const xmlChar*)"machine") == 0)) ) {

		if( !use_nonrunnable ) {
			/* skip runnable="no" entries (bioses in MAME) */
			xmlChar* run_state = xmlGetProp(game_child, (const xmlChar*)"runnable");
			if( run_state ) {
				int cmp_res = xmlStrcmp(run_state, (const xmlChar*)"no");
				xmlFree(run_state);
				if( cmp_res == 0 ) {
					return;
				}
			}
		}

		if( !use_isbios ) {
			/* skip isbios="yes" entries (bioses in MAME) */
			xmlChar* bios_state = xmlGetProp(game_child, (const xmlChar*)"isbios");
			if( bios_state ) {
				int cmp_res = xmlStrcmp(bios_state, (const xmlChar*)"yes");
				xmlFree(bios_state);
				if( cmp_res == 0 ) {
					return;
				}
			}
		}
		
		xmlChar* sourcefile = xmlGetProp(game_child, (const xmlChar*)"sourcefile");
		if( !sourcefile ) {
			if( hack_pinmame ) {
				/* set a dummy sourcefile for PinMAME */
				char* sourcefile_c = NULL;
				append_string(&sourcefile_c, "pinmame.c");
				sourcefile = (xmlChar*)sourcefile_c;
			}
			else {
				printf("'sourcefile' attribute is empty\n");
				return;
			}
		}

		*new_driv_inf = (struct driver_info*)malloc(sizeof(struct driver_info));
		if( !*new_driv_inf ) {
			printf("could not allocate driver_info\n");
			return;
		}
		memset(*new_driv_inf, 0, sizeof(struct driver_info));
		
		(*new_driv_inf)->name = xmlGetProp(game_child, (const xmlChar*)"name");
		(*new_driv_inf)->sourcefile = sourcefile;
		
		(*new_driv_inf)->ram_count = 0;
		(*new_driv_inf)->bios_count = 0;
		(*new_driv_inf)->device_count = 0;
		(*new_driv_inf)->device_mandatory = 0;
		xmlNodePtr game_children = game_child->children;
		
		while( game_children ) {
			if( xmlStrcmp(game_children->name, (const xmlChar*)"driver") == 0 ) {
				/*
				xmlChar* game_status = xmlGetProp(game_children, (const xmlChar*)"status");
				if( game_status ) {
					printf("status: %s\n", (const char*)game_status);
					xmlFree(game_status);
				}
				*/
				xmlChar* game_status = xmlGetProp(game_children, (const xmlChar*)"savestate");
				if( game_status ) {
					if( xmlStrcmp(game_status, (const xmlChar*)"supported") == 0 )
						(*new_driv_inf)->savestate = 1;
					else
						(*new_driv_inf)->savestate = 0;
					xmlFree(game_status);
				}
			}
			
			if( (app_type == APP_MESS) && use_ramsize ) {
				if( xmlStrcmp(game_children->name, (const xmlChar*)"ramoption") == 0 ) {
					xmlChar* ram_content = xmlNodeGetContent(game_children);
					if( ram_content ) {
						(*new_driv_inf)->ramsizes[(*new_driv_inf)->ram_count++] = atoi((const char*)ram_content);
						xmlFree(ram_content);
					}
				}
			}
			
			if( use_bios ) {
				if( xmlStrcmp(game_children->name, (const xmlChar*)"biosset") == 0 ) {
					xmlChar* bios_content = xmlGetProp(game_children, (const xmlChar*)"name");
					if( bios_content ) {
						(*new_driv_inf)->bioses[(*new_driv_inf)->bios_count++] = bios_content;
					}
				}
			}
			
			if( (app_type == APP_MESS) ) {
				if( xmlStrcmp(game_children->name, (const xmlChar*)"device") == 0 ) {
					xmlChar* dev_man = xmlGetProp(game_children, (const xmlChar*)"mandatory");
					if( dev_man ) {
						(*new_driv_inf)->device_mandatory = 1;
						xmlFree(dev_man);
					}
					
					xmlNodePtr dev_childs = game_children->children;
					while( dev_childs != NULL ) {
						if( xmlStrcmp(dev_childs->name, (const xmlChar*)"instance") == 0 ) {
							xmlChar* dev_brief = xmlGetProp(dev_childs, (const xmlChar*)"briefname");
							if( dev_brief ) {
								(*new_driv_inf)->devices[(*new_driv_inf)->device_count++] = dev_brief;
							}					
						}
						
						dev_childs = dev_childs->next;
					}
				}
			}
									
			game_children = game_children->next;
		}
	}
}

static void parse_listxml(const char* filename, struct driver_info** driv_inf)
{
	int xml_options = XML_PARSE_DTDVALID;

	xmlDocPtr doc = xmlReadFile(filename, NULL, xml_options);
	if( doc ) {
		xmlNodePtr root = xmlDocGetRootElement(doc);
		if( root ) {
			if( xmlStrcmp(root->name, (const xmlChar*)"mame") == 0 )
				app_type = APP_MAME;
			else if( xmlStrcmp(root->name, (const xmlChar*)"mess") == 0 )
				app_type = APP_MESS;
			else
				printf("Unknown -listxml output\n");

			if( app_type != APP_UNKNOWN ) {
				xmlXPathContextPtr xpathCtx = NULL;
				xmlXPathObjectPtr xpathObj = NULL;

				xmlChar* debug_attr = xmlGetProp(root, (const xmlChar*)"debug");
				if( xmlStrcmp(debug_attr, (const xmlChar*)"yes") == 0 ) {
					is_debug = 1;
				}
				xmlFree(debug_attr);
				debug_attr = NULL;

				app_ver = xmlGetProp(root, (const xmlChar*)"build");
				if( app_ver ) {
					printf("build: %s%s\n", app_ver, is_debug ? " (debug)" : "");
				}

				if( xpath_expr && (strlen(xpath_expr) > 0) ) {
					xpathCtx = xmlXPathNewContext(doc);
					if( xpathCtx ) {
						char* real_xpath_expr = NULL;
						convert_xpath_expr(&real_xpath_expr);
						
						printf("using xpath expression: %s\n", real_xpath_expr);
					
						xpathObj = xmlXPathEvalExpression((const xmlChar*)real_xpath_expr, xpathCtx);
						if( xpathObj == NULL )
							xmlXPathFreeContext(xpathCtx);
						
						free(real_xpath_expr);
						real_xpath_expr = NULL;
					}
				}
					
				if( xpathObj ) {
					if( xpathObj->nodesetval )
					{
						printf("xpath found %d nodes\n", xpathObj->nodesetval->nodeNr);
						
						xmlBufferPtr xmlBuf = NULL;
						if( print_xpath_results )
							xmlBuf = xmlBufferCreate();

						struct driver_info* last_driv_inf = NULL;

						int i = 0;
						for( ; i < xpathObj->nodesetval->nodeNr; ++i )
						{
							if( print_xpath_results ) {
								xmlBufferAdd(xmlBuf, (const xmlChar*)"\t", 1);
								xmlNodeDump(xmlBuf, doc, xpathObj->nodesetval->nodeTab[i], 0, 1);
								xmlBufferAdd(xmlBuf, (const xmlChar*)"\n", 1);
							}
							
							struct driver_info* new_driv_inf = NULL;
								
							parse_listxml_element(xpathObj->nodesetval->nodeTab[i], &new_driv_inf);
								
							if( new_driv_inf != NULL )
							{
								if( last_driv_inf )
									last_driv_inf->next = new_driv_inf;
								else
									*driv_inf = new_driv_inf;
									
								last_driv_inf = new_driv_inf;
							}							
						}
						
						if( print_xpath_results ) {
							FILE *xpath_result_fd = mrt_fopen("xpath_results.xml", "w");
							fprintf(xpath_result_fd, "<xpath_result>\n");
							xmlBufferDump(xpath_result_fd, xmlBuf);
							fprintf(xpath_result_fd, "</xpath_result>\n");
							fclose(xpath_result_fd);
							xpath_result_fd = NULL;
							
							xmlBufferFree(xmlBuf);
							xmlBuf = NULL;
						}
					}
				}
				else {				
					xmlNodePtr game_child = root->children;
					struct driver_info* last_driv_inf = NULL;
					while( game_child ) {
						struct driver_info* new_driv_inf = NULL;
						
						parse_listxml_element(game_child, &new_driv_inf);
						
						if( new_driv_inf != NULL )
						{
							if( last_driv_inf )
								last_driv_inf->next = new_driv_inf;
							else
								*driv_inf = new_driv_inf;
							
							last_driv_inf = new_driv_inf;
						}
						game_child = game_child->next;
					}
				}
				
				if( xpathObj != NULL )
					xmlXPathFreeObject(xpathObj);
				if( xpathCtx != NULL )
					xmlXPathFreeContext(xpathCtx);
			}
		}
		xmlFreeDoc(doc);
	}
	else {
		printf("could not parse -listxml output\n");
	}
}

int get_option(const xmlNodePtr config_node, const char* name, xmlChar** option)
{
	int result = 0;
	
	if( config_node ) {
		xmlNodePtr config_child = config_node->children;
		while( config_child != NULL) {
			if( config_child->type == XML_ELEMENT_NODE ) {
				if( xmlStrcmp(config_child->name, (const xmlChar*)"option") == 0 ) {
					xmlChar* opt_name = xmlGetProp(config_child, (const xmlChar*)"name");
					if( opt_name ) {
						if( xmlStrcmp(opt_name, (const xmlChar*)name) == 0 ) {
							result = 1;
							*option = xmlGetProp(config_child, (const xmlChar*)"value");
							xmlFree(opt_name);
							break;
						}
						xmlFree(opt_name);
					}
				}
				else {
					printf("invalid node '%s' found\n", config_child->name);
				}
			}
			
			config_child = config_child->next;
		}
	}
	
	/*
	if( result == 0 )
		printf("unknown option '%s'\n", name);
	*/
	
	return result;
}

static void get_option_int(const xmlNodePtr config_node, const char* opt_name, int* value)
{
	xmlChar* opt = NULL;
	if( get_option(config_node, opt_name, &opt) ) {
		if( opt && xmlStrlen(opt) > 0 ) {
			*value = atoi((const char*)opt);
		}
	}
	xmlFree(opt);
}

static void get_option_str(const xmlNodePtr config_node, const char* opt_name, char* value, int size)
{
	xmlChar* opt = NULL;
	if( get_option(config_node, opt_name, &opt) ) {
		if( opt && xmlStrlen(opt) > 0 ) {
			strncpy(value, (const char*)opt, size-1);
			value[size-1] = '\0';
		}
	}
	xmlFree(opt);
}

static void get_option_str_ptr(const xmlNodePtr config_node, const char* opt_name, char** value)
{
	xmlChar* opt = NULL;
	if( get_option(config_node, opt_name, &opt) ) {
		xmlChar** tmp = (xmlChar**)value;
		if( *tmp )
			xmlFree(*tmp);
		*tmp = opt;
	}
}

int read_config(const char* config_name)
{
	int config_found = 0;
	
	xmlNodePtr global_config_child = global_config_root->children;
	while( global_config_child != NULL ) {
		if( global_config_child->type == XML_ELEMENT_NODE ) {
			if( xmlStrcmp(global_config_child->name, (const xmlChar*)config_name) == 0 ) {
				config_found = 1;
				break;
			}
		}
		
		global_config_child	= global_config_child->next;
	}
	
	if( !config_found ) {
		printf("could not find configuration '%s'\n", config_name);
		return 0;
	}
	
	get_option_str_ptr(global_config_child, "executable", &mame_exe);
	get_option_str(global_config_child, "str", str_str, sizeof(str_str));
	get_option_int(global_config_child, "pause_interval", &pause_at);
	get_option_str_ptr(global_config_child, "listxml_file", &gamelist_xml_file);
	get_option_int(global_config_child, "use_autosave", &use_autosave);
	get_option_int(global_config_child, "use_ramsize", &use_ramsize);
	get_option_int(global_config_child, "write_mng", &write_mng);
#if USE_VALGRIND
	get_option_int(global_config_child, "use_valgrind", &use_valgrind);
	get_option_str_ptr(global_config_child, "valgrind_binary", &valgrind_binary);
	get_option_str_ptr(global_config_child, "valgrind_parameters", &valgrind_parameters);
#endif
	get_option_str_ptr(global_config_child, "rompath", &rompath_folder);
	get_option_int(global_config_child, "use_bios", &use_bios);
	get_option_int(global_config_child, "use_sound", &use_sound);	
	get_option_int(global_config_child, "use_throttle", &use_throttle);
	get_option_int(global_config_child, "use_debug", &use_debug);
	get_option_str_ptr(global_config_child, "xpath_expr", &xpath_expr);
	get_option_int(global_config_child, "use_devices", &use_devices);
	get_option_int(global_config_child, "hack_ftr", &hack_ftr);
	get_option_int(global_config_child, "hack_biospath", &hack_biospath);
	get_option_int(global_config_child, "hack_debug", &hack_debug);
	get_option_int(global_config_child, "hack_mngwrite", &hack_mngwrite);
	get_option_int(global_config_child, "use_nonrunnable", &use_nonrunnable);
	get_option_str(global_config_child, "output_folder", output_folder, sizeof(output_folder));
	get_option_str_ptr(global_config_child, "device_file", &global_device_file);
	get_option_int(global_config_child, "use_isbios", &use_isbios);
	get_option_int(global_config_child, "store_output", &store_output);
	get_option_int(global_config_child, "clear_output_folder", &clear_output_folder);
	get_option_int(global_config_child, "test_createconfig", &test_createconfig);
	get_option_str_ptr(global_config_child, "additional_options", &additional_options);
	get_option_int(global_config_child, "skip_mandatory", &skip_mandatory);
	get_option_int(global_config_child, "osdprocessors", &osdprocessors);
	get_option_int(global_config_child, "print_xpath_results", &print_xpath_results);
	get_option_int(global_config_child, "test_softreset", &test_softreset);	
	get_option_int(global_config_child, "hack_pinmame", &hack_pinmame);	
	
	return 1;
}

void free_config()
{
	if( additional_options ) {
		xmlFree((xmlChar*)additional_options);
		additional_options = NULL;
	}

	if( global_device_file ) {
		xmlFree((xmlChar*)global_device_file);
		global_device_file = NULL;
	}

	if( xpath_expr ) {
		xmlFree((xmlChar*)xpath_expr);
		xpath_expr = NULL;
	}

	if( rompath_folder ) {
		xmlFree((xmlChar*)rompath_folder);
		rompath_folder = NULL;
	}

	if( mame_exe ) {
		xmlFree((xmlChar*)mame_exe);
		mame_exe = NULL;
	}
}

int main(int argc, char *argv[])
{
	printf("mame_regtest %s\n", VERSION);
	
	if( argc > 2) {
		printf("usage: mame_regtest <configname>\n");
		exit(1);
	}
	
	if( getcwd(current_path, sizeof(current_path)) == NULL ) {
		printf("could not get current working path\n");
		exit(1);
	}
	printf("\n");
	printf("current path: %s\n", current_path);
	printf("\n");

	if( access(config_xml, F_OK) == -1 ) {
		printf("'%s' does not exist\n", config_xml);
		cleanup_and_exit(1, "aborting");
	}
	printf("using configuration '%s'\n", config_xml);
	
	global_config_doc = xmlReadFile(config_xml, NULL, 0);
	if( !global_config_doc ) {
		printf("could not load configuration\n");
		cleanup_and_exit(1, "aborting");
	}

	global_config_root = xmlDocGetRootElement(global_config_doc);
	if( !global_config_root ) {
		printf("invalid configuration - no root element\n");
		cleanup_and_exit(1, "aborting");
	}

	if( xmlStrcmp(global_config_root->name, (const xmlChar*)"mame_regtest") != 0 ) {
		printf("invalid configuration - no 'mame_regtest' element\n");
		cleanup_and_exit(1, "aborting");
	}
	
	printf("reading configuration 'global'\n");
	int config_res = read_config("global");
	
	if( config_res && (argc == 2) ) {
		printf("reading configuration '%s'\n", argv[1]);
		config_res = read_config(argv[1]);
	}
	
	xmlFreeDoc(global_config_doc);
	global_config_doc = NULL;
	
	if( !config_res )
		cleanup_and_exit(1, "aborting");
		
	printf("\n"); /* for output formating */
	
	if( is_absolute_path(mame_exe) == 0 ) {
		char* tmp_mame_exe = NULL;
		append_string(&tmp_mame_exe, current_path);
		append_string(&tmp_mame_exe, FILESLASH);
		append_string(&tmp_mame_exe, mame_exe);
		free(mame_exe);
		mame_exe = tmp_mame_exe;
	}
	
	if( !mame_exe || (strlen(mame_exe) == 0) ) {
		printf("'executable' is empty or missing\n");
		cleanup_and_exit(1, "aborting");
	}

	if( access(mame_exe, F_OK ) == -1 ) {
		printf("'%s' does not exist\n", mame_exe);
		cleanup_and_exit(1, "aborting");
	}
	printf("executable: %s\n", mame_exe);	

	append_string(&listxml_output, output_folder);
	append_string(&listxml_output, FILESLASH);
	append_string(&listxml_output, "listxml.xml");
	
	printf("str: %s\n", str_str);
	printf("pause interval: %d\n", pause_at);
	
	if( gamelist_xml_file && (strlen(gamelist_xml_file) > 0) ) {
		printf("using custom list: %s\n", gamelist_xml_file);
		use_custom_list = 1;
	}

	printf("autosave: %d\n", use_autosave);

	printf("ramsize: %d\n", use_ramsize);

	if( write_mng ) {
		if( hack_mngwrite ) {
			if( mrt_mkdir("mng") != 0 ) {
				printf("could not create folder 'mng' - disabling MNG writing\n");
				write_mng = 0;
			}
		}
	}
	printf("write mng: %d\n", write_mng);

	if( (access(output_folder, F_OK) != 0) && mrt_mkdir(output_folder) != 0 ) {
		printf("could not create folder '%s'\n", output_folder);
		cleanup_and_exit(1, "aborting");
	}

	if( strlen(output_folder) > 0 ) {
		printf("using output folder: %s\n", output_folder);
		if( access(output_folder, F_OK) != 0 ) {
			printf("output folder not found\n");
			cleanup_and_exit(1, "aborting");
		}
	}
	
	append_string(&temp_folder, current_path);
	append_string(&temp_folder, FILESLASH);
	append_string(&temp_folder, temp_folder_str);

	if( (access(temp_folder, F_OK) != 0) && mrt_mkdir(temp_folder) != 0 ) {
		printf("could not create folder '%s'\n", temp_folder);
		cleanup_and_exit(1, "aborting");
	}

	if( strlen(temp_folder) > 0 ) {
		printf("using output folder: %s\n", temp_folder);
		if( access(temp_folder, F_OK) != 0 ) {
			printf("temp folder not found\n");
			cleanup_and_exit(1, "aborting");
		}
	}

	append_string(&stdout_temp_file, temp_folder);
	append_string(&stdout_temp_file, FILESLASH);
	append_string(&stdout_temp_file, stdout_temp_file_str);

	append_string(&stderr_temp_file, temp_folder);
	append_string(&stderr_temp_file, FILESLASH);
	append_string(&stderr_temp_file, stderr_temp_file_str);

	append_string(&dummy_ini_folder, temp_folder);
	append_string(&dummy_ini_folder, FILESLASH);
	append_string(&dummy_ini_folder, dummy_ini_folder_str);
	
#if USE_VALGRIND
	printf("valgrind: %d\n", use_valgrind);
	
	if( use_valgrind )
	{
		if( !valgrind_binary || (strlen(valgrind_binary) == 0) )
			append_string(&valgrind_binary, valgrind_binary_def);
		printf("valgrind_binary: %s\n", valgrind_binary);
		if( !valgrind_parameters || (strlen(valgrind_parameters) == 0) )
			append_string(&valgrind_parameters, valgrind_parameters_def);
		printf("valgrind_parameters: %s\n", valgrind_parameters);
	}
#endif

	if( rompath_folder && (strlen(rompath_folder) > 0) )
		printf("using rompath folder: %s\n", rompath_folder);

	printf("bios: %d\n", use_bios);
	printf("sound: %d\n", use_sound);
	printf("throttle: %d\n", use_throttle);
	printf("debug: %d\n", use_debug);
	printf("xpath_expr: %s\n", xpath_expr ? xpath_expr : "");
	printf("use_devices: %d\n", use_devices);
	printf("use_nonrunnable: %d\n", use_nonrunnable);
	if( global_device_file && (strlen(global_device_file) > 0) )
		printf("using device_file: %s\n", global_device_file);
	printf("use_isbios: %d\n", use_isbios);
	printf("store_output: %d\n", store_output);
	printf("clear_output_folder: %d\n", clear_output_folder);
	printf("test_createconfig: %d\n", test_createconfig);
	if( additional_options )
		printf("additional_options: %s\n", additional_options);
	printf("skip_mandatory: %d\n", skip_mandatory);
	printf("osdprocessors: %d\n", osdprocessors);
	printf("print_xpath_results: %d\n", print_xpath_results);
	printf("test_softreset: %d\n", test_softreset);

	printf("hack_ftr: %d\n", hack_ftr);
	printf("hack_biospath: %d\n", hack_biospath);
	printf("hack_debug: %d\n", hack_debug);
	printf("hack_mngwrite: %d\n", hack_mngwrite);
	printf("hack_pinmame: %d\n", hack_pinmame);
	
	printf("\n"); /* for output formating */

	if( hack_ftr && (atoi(str_str) < 2)) {
		printf("'str' value has to be at least '2' when used with 'hack_ftr'\n");
		cleanup_and_exit(1, "aborting");
	}

	if( clear_output_folder ) {
		printf("clearing existing output folder\n");
		clear_directory(output_folder, 0);
	}

	printf("clearing existing temp folder\n");
	clear_directory(temp_folder, 0);

	printf("\n"); /* for output formating */

	if( !gamelist_xml_file || (strlen(gamelist_xml_file) == 0) ) {
		append_string(&gamelist_xml_file, listxml_output); 
	
		printf("writing -listxml output\n");
		char* mame_call = NULL;
		get_executable(&mame_call, NULL, "listxml");
		append_string(&mame_call, " -listxml > ");
		append_string(&mame_call, gamelist_xml_file);

		/*
		printf("system call: %s\n", mame_call);
		printf("press any key to continue\n");
		mrt_getch();
		*/

		system(mame_call);
	
		if( hack_pinmame ) {
			char* tmp_gamelist_xml = NULL;
			/* TODO: quoting */
			append_string(&tmp_gamelist_xml, temp_folder);
			append_string(&tmp_gamelist_xml, FILESLASH);
			append_string(&tmp_gamelist_xml, "listxml.xml");

			strip_sampleof_pinmame(gamelist_xml_file, tmp_gamelist_xml);
			
			remove(gamelist_xml_file);
			copy_file(tmp_gamelist_xml, gamelist_xml_file);
			
			free(tmp_gamelist_xml);
			tmp_gamelist_xml = NULL;
		}
	
		free(mame_call);
		mame_call = NULL;
	}
	
	struct driver_info* driv_inf = NULL;
	
	printf("\n"); /* for output formating */
	printf("parsing -listxml output\n")	;
	parse_listxml(gamelist_xml_file, &driv_inf);

	printf("\n"); /* for output formating */

	if( test_createconfig ) {
		printf("writing '%s'\n", get_inifile());
		char* mame_call = NULL;
		get_executable(&mame_call, NULL, "createconfig");
		append_string(&mame_call, " -showconfig > ");
		append_string(&mame_call, output_folder);
		append_string(&mame_call, FILESLASH);
		append_string(&mame_call, get_inifile());

		system(mame_call);
		
		free(mame_call);
		mame_call = NULL;
	}
		
	if( driv_inf == NULL )
		cleanup_and_exit(0, "finished");

	printf("clearing existing dummy directories\n");
	clear_directory(dummy_root_str, 1);

	if( (hack_debug || is_debug) && use_debug ) {
		append_string(&debugscript_file, temp_folder);
		append_string(&debugscript_file, FILESLASH);
		append_string(&debugscript_file, debugscript_file_str);		
		
		FILE* debugscript_fd = fopen(debugscript_file, "w");
		if( !debugscript_fd ) {
			printf("could not open %s\n", debugscript_file);
			cleanup_and_exit(1, "aborted");
		}
		if( test_softreset )
			fprintf(debugscript_fd, "softreset\n");
		fprintf(debugscript_fd, "go\n");
		fclose(debugscript_fd);
		debugscript_fd = NULL;
	}

	printf("\n"); /* for output formating */
	if( !create_dummy_root_ini() )
		cleanup_and_exit(1, "aborted");
		
	/* setup OSDPROCESSORS */
	char osdprocessors_tmp[128];
	snprintf(osdprocessors_tmp, sizeof(osdprocessors_tmp), "OSDPROCESSORS=%d", osdprocessors);
	putenv(osdprocessors_tmp);
	
	printf("\n");
	process_driver_info_list(driv_inf);	
	printf("\n"); /* for output formating */
	cleanup_driver_info_list(driv_inf);

	cleanup_and_exit(0, "finished");
	return 0; /* to shut up compiler */
}
