/*
mame_regtest
(c) Copyright 2005-2010 by Oliver Stoeneberg

http://mess.redump.net/tools:mame_regtest
http://mess.redump.net/tools:mame_regtest:config (documentation of the options in the XML)
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

#ifdef _MSC_VER
#include <direct.h>
#include <io.h>
#if _MSC_VER >= 1400
#undef access
#define access _access
#undef rmdir
#define rmdir _rmdir
#undef snprintf
#define snprintf _snprintf
#undef strdup
#define strdup _strdup
#undef itoa
#define itoa _itoa
#undef chdir
#define chdir _chdir
#undef getcwd
#define getcwd _getcwd
#undef putenv
#define putenv _putenv
#endif
#define F_OK 00
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

/* local */
#include "common.h"
#include "config.h"

#define VERSION "0.69"

struct dipvalue_info {
	xmlChar* name;
	unsigned int value;
	struct dipvalue_info* next;
};

struct dipswitch_info {
	xmlChar* name;
	xmlChar* tag;
	unsigned int mask;
	unsigned int defvalue;
	struct dipvalue_info* values;
	struct dipswitch_info* next;
};

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
	struct dipswitch_info* dipswitches;
	struct dipswitch_info* configurations;
	struct driver_info* next;
};

struct image_entry {
	const xmlChar* device_type;
	const xmlChar* device_file;
	struct image_entry* next;
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
	struct dipswitch_info* dipswitch;
	struct dipvalue_info* dipvalue;
	struct dipswitch_info* configuration;
	struct dipvalue_info* confsetting;
};

static int app_type = 0;
static int is_debug = 0;
static const int xpath_placeholder_size = 11;

static xmlChar* app_ver = NULL;
static char* debugscript_file = NULL;
static char* listxml_output = NULL;
static char* temp_folder = NULL;
static char* stdout_temp_file = NULL;
static char* stderr_temp_file = NULL;
static char* dummy_ini_folder = NULL;
static char current_path[_MAX_PATH] = "";
static char* dummy_root = NULL;
static char* pause_file = NULL;
static char pid_str[10] = "";
static int mameconfig_ver = 10;

/* constant string variables */
static const char* const xpath_placeholder = "DRIVER_ROOT";

/* PNG/MNG signatures */
static const unsigned char png_sig[8] = { 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
static const unsigned char mng_sig[8] = { 0x8a, 0x4d, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
static const unsigned char IHDR_name[4] = { 'I', 'H', 'D', 'R' };
static const unsigned char IDAT_name[4] = { 'I', 'D', 'A', 'T' };
static const unsigned char IEND_name[4] = { 'I', 'E', 'N', 'D' };
static const unsigned char MHDR_name[4] = { 'M', 'H', 'D', 'R' };
static const unsigned char MEND_name[4] = { 'M', 'E', 'N', 'D' };

enum {
	APP_UNKNOWN = 0,
	APP_MAME 	= 1,
	APP_MESS	= 2
};

/* configuration variables */
static char* config_str_str = NULL;
static char* config_mame_exe = NULL;
static char* config_gamelist_xml_file = NULL;
static int config_use_autosave = 0;
static int config_use_ramsize = 0;
static int config_write_mng = 0;
static int config_use_valgrind = 0;
#if USE_VALGRIND
static char* config_valgrind_binary = NULL;
static char* config_valgrind_parameters = NULL;
#endif
static char* config_rompath_folder = NULL;
static int config_use_bios = 0;
static int config_use_sound = 0;
static int config_use_throttle = 0;
static int config_use_debug = 0;
static int config_hack_ftr = 0;
static int config_use_devices = 1;
static int config_hack_biospath = 0;
static char* config_xpath_expr = NULL;
static int config_hack_mngwrite = 0;
static int config_use_nonrunnable = 0;
static char* config_output_folder = NULL;
static char* config_global_device_file = NULL;
static int config_use_isbios = 0;
static int config_store_output = 0;
static int config_clear_output_folder = 0;
static int config_test_createconfig = 1;
static char* config_additional_options = NULL;
static int config_skip_mandatory = 0;
static int config_osdprocessors = 1;
static int config_print_xpath_results = 0;
static int config_test_softreset = 0;
static int config_hack_pinmame = 0;
static int config_write_avi = 0;
static int config_verbose = 0;
/* static int config_use_gdb = 0; */
static int config_write_wav = 0;
static int config_use_dipswitches = 0;
static int config_use_configurations = 0;
static char* config_hashpath_folder = NULL;

struct config_entry mrt_config[] =
{
	{ "executable",				CFG_STR_PTR,	&config_mame_exe },
	{ "str",					CFG_STR_PTR,	&config_str_str },
	{ "listxml_file",			CFG_STR_PTR,	&config_gamelist_xml_file },
	{ "use_autosave",			CFG_INT,		&config_use_autosave },
	{ "use_ramsize",			CFG_INT,		&config_use_ramsize },
	{ "write_mng",				CFG_INT,		&config_write_mng },
	{ "use_valgrind",			CFG_INT,		&config_use_valgrind },
#if USE_VALGRIND
	{ "valgrind_binary",		CFG_STR_PTR,	&config_valgrind_binary },
	{ "valgrind_parameters",	CFG_STR_PTR,	&config_valgrind_parameters },
#endif
	{ "rompath",				CFG_STR_PTR,	&config_rompath_folder },
	{ "use_bios",				CFG_INT,		&config_use_bios },
	{ "use_sound",				CFG_INT,		&config_use_sound },
	{ "use_throttle",			CFG_INT,		&config_use_throttle },
	{ "use_debug",				CFG_INT,		&config_use_debug },
	{ "xpath_expr",				CFG_STR_PTR,	&config_xpath_expr },
	{ "use_devices",			CFG_INT,		&config_use_devices },
	{ "hack_ftr",				CFG_INT,		&config_hack_ftr },
	{ "hack_biospath",			CFG_INT,		&config_hack_biospath },
	{ "hack_mngwrite",			CFG_INT,		&config_hack_mngwrite },
	{ "use_nonrunnable",		CFG_INT,		&config_use_nonrunnable },
	{ "output_folder",			CFG_STR_PTR,	&config_output_folder },
	{ "device_file",			CFG_STR_PTR,	&config_global_device_file },
	{ "use_isbios",				CFG_INT,		&config_use_isbios },
	{ "store_output",			CFG_INT,		&config_store_output },
	{ "clear_output_folder",	CFG_INT,		&config_clear_output_folder },
	{ "test_createconfig",		CFG_INT,		&config_test_createconfig },
	{ "additional_options",		CFG_STR_PTR,	&config_additional_options },
	{ "skip_mandatory",			CFG_INT,		&config_skip_mandatory },
	{ "osdprocessors",			CFG_INT,		&config_osdprocessors },
	{ "print_xpath_results",	CFG_INT,		&config_print_xpath_results },
	{ "test_softreset",			CFG_INT,		&config_test_softreset },
	{ "hack_pinmame",			CFG_INT,		&config_hack_pinmame },
	{ "write_avi",				CFG_INT,		&config_write_avi },
	{ "verbose",				CFG_INT,		&config_verbose },
/*	{ "use_gdb",				CFG_INT,		&config_use_gdb }, */
	{ "write_wav",				CFG_INT,		&config_write_wav },
	{ "use_dipswitches",		CFG_INT,		&config_use_dipswitches },
	{ "use_configurations",		CFG_INT,		&config_use_configurations },
	{ "hashpath",				CFG_STR_PTR,	&config_hashpath_folder },
	{ NULL,						CFG_UNK,		NULL }
};

static int get_png_data(const char* png_name, unsigned int *IHDR_width, unsigned int* IHDR_height, unsigned int *IDAT_size, unsigned int* IDAT_crc);
static void open_mng_and_skip_sig(const char* mng_name, FILE** mng_fd);
static int internal_get_next_IDAT_data(FILE* in_fd, unsigned int *IDAT_size, unsigned int* IDAT_crc);
static void cleanup_and_exit(int errcode, const char* errstr);
static int get_MHDR_data(FILE* in_fd, unsigned int* MHDR_width, unsigned int* MHDR_height);

static void append_driver_info(char** str, struct driver_entry* de)
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
	if( config_use_valgrind ) {
#if USE_VALGRIND
		append_string(sys, config_valgrind_binary);
		append_string(sys, " ");
		append_string(sys, config_valgrind_parameters);
		append_string(sys, " ");
		append_string(sys, "--log-file=");
		append_string(sys, config_output_folder);
		append_string(sys, FILESLASH);
		if( de ) {
			append_driver_info(sys, de);
		}
		else {
			append_string(sys, callstr);
		}
		append_string(sys, ".valgrind_%p");
		append_string(sys, " ");
#else
	}
	/*
	else if( config_use_gdb && (!callstr || (strcmp(callstr, "listxml") != 0)) ) {
		append_string(sys, "gdb");
		append_string(sys, " ");
		append_string(sys, "--batch --eval-command=run --args");
		append_string(sys, " ");
	}
	*/
	/* shut up compiler */
	(void)de;
	(void)callstr;
#endif
	append_quoted_string(sys, config_mame_exe);
}

static int parse_mng(const char* file, xmlNodePtr filenode)
{
	int frame = 0;

	FILE *mng_fd = NULL;
	open_mng_and_skip_sig(file, &mng_fd);
	if( mng_fd == NULL )
		return frame;

	unsigned int MHDR_width, MHDR_height, IDAT_size, IDAT_crc;

	int res = get_MHDR_data(mng_fd, &MHDR_width, &MHDR_height);
	if( res == 0 ) {
		printf("could not get MHDR data from '%s'\n", file);
		return frame;
	}

	{
		char tmp[128];
		snprintf(tmp, sizeof(tmp), "%u", MHDR_width);
		xmlNewProp(filenode, (const xmlChar*)"width", (const xmlChar*)tmp);
		snprintf(tmp, sizeof(tmp), "%u", MHDR_height);
		xmlNewProp(filenode, (const xmlChar*)"height", (const xmlChar*)tmp);
	}

	do {
		res = internal_get_next_IDAT_data(mng_fd, &IDAT_size, &IDAT_crc);
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
			internal_get_next_IDAT_data(mng_fd, &IDAT_size, &IDAT_crc);
			continue;
		}
		else if( res == 2 ) {
			/* end of file reached - do nothing - exit loop */
			break;
		}
		else if( res == 0 ) {
			printf("unexpected error parsing MNG '%s'\n", file);
			break;
		}
	} while( 1 );

	fclose(mng_fd);
	mng_fd = NULL;

	return frame;
}

static FILE* mrt_fopen(const char* filename, const char* attr)
{
	FILE* result = NULL;

	char* outputfile = NULL;
	append_string(&outputfile, config_output_folder);
	append_string(&outputfile, FILESLASH);
	append_string(&outputfile, filename);

	result = fopen(outputfile, attr);

	free(outputfile);

	return result;
}

static void clear_callback_nosnap(struct parse_callback_data* pcd)
{
	if( pcd->type == ENTRY_FILE ) {
		remove(pcd->fullname);
	}
	/* do not delete the "snap" folder */
	else if( (pcd->type == ENTRY_DIR_END) && (strcmp(pcd->entry_name, "snap") != 0) ) {
		int delete_root = 1;
		parse_directory(pcd->fullname, 0, clear_callback_nosnap, (void*)&delete_root);
	}
	else if( pcd->type == ENTRY_END ) {
		int* delete_root = (int*)pcd->user_data;
		if( *delete_root )
			rmdir(pcd->dirname);
	}
}

void clear_directory_nosnap(const char* dirname, int delete_root)
{
	parse_directory(dirname, 0, clear_callback_nosnap, (void*)&delete_root);

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
			unsigned int IHDR_width, IHDR_height, IDAT_size, IDAT_crc;
			if( get_png_data(pcd->fullname, &IHDR_width, &IHDR_height, &IDAT_size, &IDAT_crc) ) {
				snprintf(tmp, sizeof(tmp), "%u", IHDR_width);
				xmlNewProp(filenode, (const xmlChar*)"png_width", (const xmlChar*)tmp);
				snprintf(tmp, sizeof(tmp), "%u", IHDR_height);
				xmlNewProp(filenode, (const xmlChar*)"png_height", (const xmlChar*)tmp);
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
			if( !config_use_sound )
				fprintf(fp, "sound              0\n"); /* disable sound output */
			if( !config_use_throttle )
				fprintf(fp, "throttle           0\n"); /* disable throttle */
			if( !config_use_debug )
				fprintf(fp, "debug              0\n"); /* disable debug window */
			else {
				fprintf(fp, "debug              1\n"); /* enable debug window */
				/* pass debugscript, so so debugger won't block the execution */
				fprintf(fp, "debugscript        %s\n", debugscript_file);
			}
			fprintf(fp, "mouse              0\n"); /* disable "mouse" so it doesn't grabs the mouse pointer on window creation */
			fprintf(fp, "window             1\n"); /* run it in windowed mode */
			if( !config_hack_ftr )
				fprintf(fp, "seconds_to_run     %s\n", config_str_str);
			else
				fprintf(fp, "frames_to_run      %s\n", config_str_str);
			if( config_rompath_folder && (strlen(config_rompath_folder) > 0) ) {
				if( !config_hack_biospath )
					fprintf(fp, "rompath            %s\n", config_rompath_folder); /* rompath for MAME/MESS */
				else
					fprintf(fp, "biospath           %s\n", config_rompath_folder); /* old biospath for MESS */
			}
			if( (app_type == APP_MESS) && config_hashpath_folder && (strlen(config_hashpath_folder) > 0) )
				fprintf(fp, "hashpath            %s\n", config_hashpath_folder); /* hashpath for MESS */
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
	char* xpath_copy = strdup(config_xpath_expr);
	char* pos = strstr(xpath_copy, xpath_placeholder);
	while( pos != NULL )
	{
		pos = pos + xpath_placeholder_size;
		char* pos1 = strstr(pos, xpath_placeholder);
		if( pos1 != NULL )
			*pos1 = 0;
		
		if( app_type == APP_MAME )
			append_string(real_xpath_expr, "/mame/game");
		else if( app_type == APP_MESS )
			append_string(real_xpath_expr, "/mess/machine");
			
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

	if( listxml_output ) {
		free(listxml_output);
		listxml_output = NULL;
	}
	config_free(mrt_config);

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

	clear_directory(dummy_root, 1);

	free(pause_file);
	pause_file = NULL;
	
	free(dummy_root);
	dummy_root = NULL;

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
	if( de->dipswitch && de->dipswitch->name && de->dipvalue && de->dipvalue->name )
		fprintf(print_fd, " (dipswitch %s value %s)", de->dipswitch->name, de->dipvalue->name);
	if( de->configuration && de->configuration->name && de->confsetting && de->confsetting->name )
		fprintf(print_fd, " (configuration %s value %s)", de->configuration->name, de->confsetting->name);
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

static int get_MHDR_data(FILE* in_fd, unsigned int* MHDR_width, unsigned int* MHDR_height)
{
	unsigned int chunk_size = 0;
	unsigned int reversed_chunk_size = 0;
	unsigned char chunk_name[4] = "";

	if( fread(&chunk_size, sizeof(unsigned int), 1, in_fd) != 1 ) {
		printf("could not read chunk size\n");
		return 0;
	}

	reversed_chunk_size = htonl(chunk_size);

	if( fread(chunk_name, sizeof(unsigned char), 4, in_fd) != 4 ) {
		printf("could not read chunk name\n");
		return 0;
	}

	if( memcmp(MHDR_name, chunk_name, 4) == 0 ) {
		unsigned int width = 0, height = 0;

		if( fread(&width, sizeof(unsigned int), 1, in_fd) != 1 ) {
				printf("could not read MHDR chunk width\n");
				return 0;
		}

		if( fread(&height, sizeof(unsigned int), 1, in_fd) != 1 ) {
				printf("could not read MHDR chunk height\n");
				return 0;
		}

		*MHDR_width = htonl(width);
		*MHDR_height = htonl(height);

		fseek(in_fd, reversed_chunk_size - (2 * sizeof(unsigned int)), SEEK_CUR); /* jump remaining MHDR chunk */
		fseek(in_fd, 4, SEEK_CUR); /* jump CRC */

		return 1;
	}

	printf("could not find MHDR chunk\n");

	return 0;
}

static int get_IHDR_data(FILE* in_fd, unsigned int* IHDR_width, unsigned int* IHDR_height)
{
	unsigned int chunk_size = 0;
	unsigned int reversed_chunk_size = 0;
	unsigned char chunk_name[4] = "";

	if( fread(&chunk_size, sizeof(unsigned int), 1, in_fd) != 1 ) {
		printf("could not read chunk size\n");
		return 0;
	}

	reversed_chunk_size = htonl(chunk_size);
	
	if( fread(chunk_name, sizeof(unsigned char), 4, in_fd) != 4 ) {
		printf("could not read chunk name\n");
		return 0;
	}

	if( memcmp(IHDR_name, chunk_name, 4) == 0 ) {
		unsigned int width = 0, height = 0;
		
		if( fread(&width, sizeof(unsigned int), 1, in_fd) != 1 ) {
				printf("could not read IHDR chunk width\n");
				return 0;
		}
		
		if( fread(&height, sizeof(unsigned int), 1, in_fd) != 1 ) {
				printf("could not read IHDR chunk height\n");
				return 0;
		}

		*IHDR_width = htonl(width);
		*IHDR_height = htonl(height);
		
		fseek(in_fd, reversed_chunk_size - (2 * sizeof(unsigned int)), SEEK_CUR); /* jump remaining IHDR chunk */
		fseek(in_fd, 4, SEEK_CUR); /* jump CRC */
		
		return 1;
	}

	printf("could not find IHDR chunk\n");

	return 0;
}

static int internal_get_next_IDAT_data(FILE* in_fd, unsigned int *IDAT_size, unsigned int* IDAT_crc)
{
	unsigned int chunk_size = 0;
	unsigned int reversed_chunk_size = 0;
	unsigned char chunk_name[4] = "";

	do {
		if( fread(&chunk_size, sizeof(unsigned int), 1, in_fd) != 1 ) {
			printf("could not read chunk size\n");
			return 0;
		}

		reversed_chunk_size = htonl(chunk_size);
		
		if( fread(chunk_name, sizeof(unsigned char), 4, in_fd) != 4 ) {
			printf("could not read chunk name\n");
			return 0;
		}

		if( memcmp(IDAT_name, chunk_name, 4) == 0 ) {
			fseek(in_fd, reversed_chunk_size, SEEK_CUR); /* jump IDAT chunk */
			
			unsigned int chunk_crc = 0;
			if( fread(&chunk_crc, sizeof(unsigned int), 1, in_fd) != 1 ) {
				printf("could not read IDAT chunk CRC\n");
				return 0;
			}
			else {
				*IDAT_size = reversed_chunk_size;
				*IDAT_crc = chunk_crc;
				return 1;
			}
		}

		if( memcmp(IEND_name, chunk_name, 4) == 0 ) {
			fseek(in_fd, reversed_chunk_size, SEEK_CUR); /* jump IEND chunk */
			fseek(in_fd, 4, SEEK_CUR); /* jump CRC */
			return 0;
		}

		if( memcmp(MEND_name, chunk_name, 4) == 0 ) {
			fseek(in_fd, reversed_chunk_size, SEEK_CUR); /* jump MEND chunk */
			fseek(in_fd, 4, SEEK_CUR); /* jump CRC */
			return 2;
		}

		fseek(in_fd, reversed_chunk_size, SEEK_CUR); /* jump chunk */
		fseek(in_fd, 4, SEEK_CUR); /* jump CRC */
	} while(1);
}

static int get_png_data(const char* png_name, unsigned int *IHDR_width, unsigned int* IHDR_height, unsigned int *IDAT_size, unsigned int* IDAT_crc)
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

	int IHDR_res = get_IHDR_data(png_fd, IHDR_width, IHDR_height);
	if( IHDR_res != 1 ) {
		printf("error getting IHDR data: %s\n", png_name);
		fclose(png_fd);
		return IHDR_res;
	}

	int IDAT_res = internal_get_next_IDAT_data(png_fd, IDAT_size, IDAT_crc);
	if( IDAT_res != 1 ) 
		printf("error getting IDAT data: %s\n", png_name);

	fclose(png_fd);

	return IDAT_res;
}

static void open_mng_and_skip_sig(const char* mng_name, FILE** mng_fd)
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
}

/*
	types:
		1 - dipswitch
		2 - configuration
*/
static int create_cfg(struct driver_entry* de, int type)
{
	/*
	dipswitch:	
	<?xml version="1.0"?>
	<mameconfig version="10">
	    <system name="a2600">
	        <input>
	            <port tag="SWB" type="DIPSWITCH" mask="64" defvalue="0" value="64" />
	        </input>
	    </system>
	</mameconfig>
	
	configuration:
	<?xml version="1.0"?>
	<mameconfig version="10">
	    <system name="a5200">
	        <input>
	            <port tag="artifacts" type="CONFIG" mask="64" defvalue="0" value="64" />
	        </input>
	    </system>
	</mameconfig>
	*/
	
	struct dipswitch_info* inp_name = NULL;
	struct dipvalue_info* inp_value = NULL;
	const char* type_str = NULL;
	
	if( type == 1 ) {
		inp_name = de->dipswitch;
		inp_value = de->dipvalue;
		type_str = "DIPSWITCH";
	}
	else if ( type == 2 ) {
		inp_name = de->configuration;
		inp_value = de->confsetting;
		type_str = "CONFIG";
	}
	
	if( !inp_name || !inp_value )
		return 0;
	
	char* cfgfile = NULL;
	append_string(&cfgfile, dummy_root);
	append_string(&cfgfile, FILESLASH);
	append_string(&cfgfile, "cfg");
	
	mrt_mkdir(cfgfile);
	
	append_string(&cfgfile, FILESLASH);
	append_string(&cfgfile, de->name);
	append_string(&cfgfile, ".cfg");

	xmlDocPtr cfg_doc = xmlNewDoc((const xmlChar*)"1.0");
	
	if( mameconfig_ver == 10 ) {
		xmlNodePtr cfg_node = xmlNewNode(NULL, (const xmlChar*)"mameconfig");
		char tmp[10];
		itoa(mameconfig_ver, tmp, 10);
		xmlNewProp(cfg_node, (const xmlChar*)"version", (const xmlChar*)tmp);
		
		xmlDocSetRootElement(cfg_doc, cfg_node);
		
		xmlNodePtr system_node = xmlNewChild(cfg_node, NULL, (const xmlChar*)"system", NULL);
		xmlNewProp(system_node, (const xmlChar*)"name", (const xmlChar*)de->name);
	
		xmlNodePtr input_node = xmlNewChild(system_node, NULL, (const xmlChar*)"input", NULL);
	
		xmlNodePtr port_node = xmlNewChild(input_node, NULL, (const xmlChar*)"port", NULL);
		xmlNewProp(port_node, (const xmlChar*)"tag", (const xmlChar*)inp_name->tag);
		xmlNewProp(port_node, (const xmlChar*)"type", (const xmlChar*)type_str);
		itoa(inp_name->mask, tmp, 10);
		xmlNewProp(port_node, (const xmlChar*)"mask", (const xmlChar*)tmp);
		itoa(inp_name->defvalue, tmp, 10);
		xmlNewProp(port_node, (const xmlChar*)"defvalue", (const xmlChar*)tmp);
		itoa(inp_value->value, tmp, 10);
		xmlNewProp(port_node, (const xmlChar*)"value", (const xmlChar*)tmp);
	}
	/*
	else {
		printf("unknown 'mameconfig' version\n");
		return 1;
	}
	*/
	
	xmlSaveFormatFileEnc(cfgfile, cfg_doc, "UTF-8", 1);

	free(cfgfile);
	cfgfile = NULL;

	return 0;
}

static int execute_mame(struct driver_entry* de, xmlNodePtr* result)
{
	print_driver_info(de, stdout);
	if( config_use_autosave && de->autosave )
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
	if( config_use_autosave && de->autosave )
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
	if( config_write_mng ) {
		if( config_hack_mngwrite )
			append_string(&sys, " -mngwrite mng"FILESLASH);
		else
			append_string(&sys, " -mngwrite ");
		append_driver_info(&sys, de);
		append_string(&sys, ".mng");
	}
	if( config_write_avi ) {
		append_string(&sys, " -aviwrite ");
		append_driver_info(&sys, de);
		append_string(&sys, ".avi");
	}
	if( config_write_wav ) {
		append_string(&sys, " -wavwrite ");
		append_driver_info(&sys, de);
		append_string(&sys, ".wav");
	}
	if( config_additional_options && (strlen(config_additional_options) > 0) ) {
		append_string(&sys, " ");
		append_string(&sys, config_additional_options);
	}
	append_string(&sys, " -inipath ");
	append_quoted_string(&sys, dummy_ini_folder);
	if( config_hack_pinmame ) {
		append_string(&sys, " -skip_disclaimer");
		append_string(&sys, " -skip_gameinfo");
	}

	append_string(&sys, " > ");
	append_quoted_string(&sys, stdout_temp_file);

	append_string(&sys, " 2> ");
	append_quoted_string(&sys, stderr_temp_file);

	/* the whole command-line has to be quoted - end */
	append_string(&sys, " \"");

	if( config_verbose )
		printf("system call: %s\n", sys);

	/* TODO: errorhandling */
	mrt_mkdir(dummy_root);
	create_cfg(de, 1);
	create_cfg(de, 2);
	int ch_res = chdir(dummy_root);
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
		if( actual_driv_inf->dipswitches ) {
			struct dipswitch_info* dipswitch = actual_driv_inf->dipswitches;
			while( dipswitch != NULL ) {
				struct dipvalue_info* dipvalue = dipswitch->values;
				while( dipvalue != NULL ) {
					struct dipvalue_info* next_dipvalue = dipvalue->next;
					free(dipvalue);
					dipvalue = next_dipvalue;
				};
				
				struct dipswitch_info* next_dipswitch = dipswitch->next;
				free(dipswitch);				
				dipswitch = next_dipswitch;
			};
		}
		if( actual_driv_inf->configurations ) {
			struct dipswitch_info* configuration = actual_driv_inf->configurations;
			while( configuration != NULL ) {
				struct dipvalue_info* confsetting = configuration->values;
				while( confsetting != NULL ) {
					struct dipvalue_info* next_confsetting = confsetting->next;
					free(confsetting);
					confsetting = next_confsetting;
				};
				
				struct dipswitch_info* next_configuration = configuration->next;
				free(configuration);				
				configuration = next_configuration;
			};
		}
		if( actual_driv_inf->next ) {
			struct driver_info* tmp_driv_inf = actual_driv_inf;
			actual_driv_inf = actual_driv_inf->next;
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

	if( access(pause_file, F_OK) == 0 ) {
		printf("\n");
		printf("found pause file\n");
		printf("please press any key to continue\n");
		mrt_getch();
		printf("\n");
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
	if( de->dipswitch && de->dipswitch->name && de->dipvalue && de->dipvalue->name ) {
		xmlNewProp(output_node, (const xmlChar*)"dipswitch", (const xmlChar*)de->dipswitch->name);
		xmlNewProp(output_node, (const xmlChar*)"dipvalue", (const xmlChar*)de->dipvalue->name);
	}
	if( de->configuration && de->configuration->name && de->confsetting && de->confsetting->name ) {
		xmlNewProp(output_node, (const xmlChar*)"configuration", (const xmlChar*)de->configuration->name);
		xmlNewProp(output_node, (const xmlChar*)"confsetting", (const xmlChar*)de->confsetting->name);
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
		build_output_xml(dummy_root, result1);
	}

	if( res == 1 && config_use_autosave && de->autosave ) {
		res = execute_mame(de, &result2);

		if( result2 ) {
			xmlAddChild(output_node, result2);
			build_output_xml(dummy_root, result2);
		}
	}

	if( (config_store_output > 0) && (access(dummy_root, F_OK) == 0) ) {
		char* outputdir = NULL;
		append_string(&outputdir, config_output_folder);
		append_string(&outputdir, FILESLASH);
		append_driver_info(&outputdir, de);

		/* TODO: bail out on error */
		if( rename(dummy_root, outputdir) != 0 )
			printf("could not rename '%s' to '%s'\n", dummy_root, outputdir);
			
		/* clear everything but the "snap" folder */
		if( config_store_output == 2)
			clear_directory_nosnap(outputdir, 0);

		free(outputdir);
		outputdir = NULL;
	}
	else {
		clear_directory(dummy_root, 0);
	}

	char* outputname = NULL;
	append_string(&outputname, config_output_folder);
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
	
	if( config_skip_mandatory && de->device_mandatory ) {
		res = 1;
	}
	else {
		res = execute_mame2(de);
		if( res == 0 )
			return res;
	}

	if( config_use_devices && actual_driv_inf->device_count ) {
		char* device_file = NULL;
		if( config_global_device_file && (strlen(config_global_device_file) > 0) )
			append_string(&device_file, config_global_device_file);
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
				snprintf(initial_postfix, sizeof(initial_postfix), "%s", de->postfix);

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
		memset(&de, 0x00, sizeof(struct driver_entry));
		de.name = (const char*)actual_driv_inf->name;
		de.sourcefile = (const char*)actual_driv_inf->sourcefile;
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
		
		if( actual_driv_inf->dipswitches ) {
			int dipswitch_count = -1;
			struct dipswitch_info* dipswitch = actual_driv_inf->dipswitches;
			while( dipswitch != NULL ) {
				++dipswitch_count;
				de.dipswitch = dipswitch;

				int dipvalue_count = -1;
				struct dipvalue_info* dipvalue = dipswitch->values;
				while( dipvalue != NULL ) {
					++dipvalue_count;

					snprintf(de.postfix, sizeof(de.postfix), "dip%05dval%05d", dipswitch_count, dipvalue_count);

					de.dipvalue = dipvalue;
					
					res = execute_mame3(&de, actual_driv_inf);
					
					dipvalue = dipvalue->next;
				};

				dipswitch = dipswitch->next;
			};
			
			de.dipswitch = NULL;
			de.dipvalue = NULL;
		}

		if( actual_driv_inf->configurations ) {
			int configuration_count = -1;
			struct dipswitch_info* configuration = actual_driv_inf->configurations;
			while( configuration != NULL ) {
				++configuration_count;
				de.configuration = configuration;

				int confsetting_count = -1;
				struct dipvalue_info* confsetting = configuration->values;
				while( confsetting != NULL ) {
					++confsetting_count;

					snprintf(de.postfix, sizeof(de.postfix), "cfg%05dval%05d", configuration_count, confsetting_count);

					de.confsetting = confsetting;
					
					res = execute_mame3(&de, actual_driv_inf);
					
					confsetting = confsetting->next;
				};

				configuration = configuration->next;
			};
			
			de.configuration = NULL;
			de.confsetting = NULL;
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
			actual_driv_inf = actual_driv_inf->next;
		else
			break;
	} while(res == 1);
}

static void parse_listxml_element(const xmlNodePtr game_child, struct driver_info** new_driv_inf)
{
	/* "game" in MAME and "machine" in MESS */
	if( ((app_type == APP_MAME) && (xmlStrcmp(game_child->name, (const xmlChar*)"game") == 0)) ||
		((app_type == APP_MESS) && (xmlStrcmp(game_child->name, (const xmlChar*)"machine") == 0)) ) {

		if( !config_use_nonrunnable ) {
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

		if( !config_use_isbios ) {
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
			if( config_hack_pinmame ) {
				/* set a dummy sourcefile for PinMAME */
				sourcefile = xmlStrdup((const xmlChar*)"pinmame.c");
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

		struct dipswitch_info* last_dip_info = NULL;
		struct dipswitch_info* last_conf_info = NULL;

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

			if( (app_type == APP_MESS) && config_use_ramsize ) {
				if( xmlStrcmp(game_children->name, (const xmlChar*)"ramoption") == 0 ) {
					xmlChar* ram_content = xmlNodeGetContent(game_children);
					if( ram_content ) {
						(*new_driv_inf)->ramsizes[(*new_driv_inf)->ram_count++] = atoi((const char*)ram_content);
						xmlFree(ram_content);
					}
				}
			}

			if( config_use_bios ) {
				if( xmlStrcmp(game_children->name, (const xmlChar*)"biosset") == 0 ) {
					xmlChar* bios_content = xmlGetProp(game_children, (const xmlChar*)"name");
					if( bios_content )
						(*new_driv_inf)->bioses[(*new_driv_inf)->bios_count++] = bios_content;
				}
			}
			
			if( config_use_dipswitches ) {
				if( xmlStrcmp(game_children->name, (const xmlChar*)"dipswitch") == 0 ) {
					xmlChar* dip_name = xmlGetProp(game_children, (const xmlChar*)"name");
					xmlChar* dip_tag = xmlGetProp(game_children, (const xmlChar*)"tag");
					xmlChar* dip_mask = xmlGetProp(game_children, (const xmlChar*)"mask");

					struct dipswitch_info* new_dip_info = (struct dipswitch_info*)malloc(sizeof(struct dipswitch_info));
					/* TODO: check allocation */
					memset(new_dip_info, 0x00, sizeof(struct dipswitch_info));
					
					if( dip_name ) new_dip_info->name = dip_name;
					if( dip_tag ) new_dip_info->tag = dip_tag;
					if( dip_mask ) new_dip_info->mask = atoi((const char*)dip_mask);

					struct dipvalue_info* last_dipvalue = NULL;

					xmlNodePtr dipswitch_children = game_children->children;					
					while( dipswitch_children != NULL ) {
						if( xmlStrcmp(dipswitch_children->name, (const xmlChar*)"dipvalue") == 0 ) {
							xmlChar* dipvalue_default = xmlGetProp(dipswitch_children, (const xmlChar*)"default");
							if( dipvalue_default ) {
								if( xmlStrcmp(dipvalue_default, (const xmlChar*)"yes") == 0 ) {
									xmlChar* dipvalue_value = xmlGetProp(dipswitch_children, (const xmlChar*)"value");
									if( dipvalue_value ) new_dip_info->defvalue = atoi((const char*)dipvalue_value);
									xmlFree(dipvalue_value);
								}
								else
								{
									xmlChar* dipvalue_name = xmlGetProp(dipswitch_children, (const xmlChar*)"name");
									xmlChar* dipvalue_value = xmlGetProp(dipswitch_children, (const xmlChar*)"value");

									struct dipvalue_info* new_dipvalue = (struct dipvalue_info*)malloc(sizeof(struct dipvalue_info));
									/* TODO: check allocation */
									memset(new_dipvalue, 0x00, sizeof(struct dipvalue_info));

									if( dipvalue_name ) new_dipvalue->name = dipvalue_name;
									if( dipvalue_value ) new_dipvalue->value = atoi((const char*)dipvalue_value);
									
									if( new_dip_info->values == NULL )
										new_dip_info->values = new_dipvalue;
									
									if( last_dipvalue )
										last_dipvalue->next = new_dipvalue;
									last_dipvalue = new_dipvalue;
								}
								
								xmlFree(dipvalue_default);
							}
						}
						
						dipswitch_children = dipswitch_children->next;
					}
					
					if( (*new_driv_inf)->dipswitches == NULL)
						(*new_driv_inf)->dipswitches = new_dip_info;
					
					if( last_dip_info )
						last_dip_info->next = new_dip_info;
					last_dip_info = new_dip_info;
				}
			}

			if( config_use_configurations ) {
				if( xmlStrcmp(game_children->name, (const xmlChar*)"configuration") == 0 ) {
					xmlChar* conf_name = xmlGetProp(game_children, (const xmlChar*)"name");
					xmlChar* conf_tag = xmlGetProp(game_children, (const xmlChar*)"tag");
					xmlChar* conf_mask = xmlGetProp(game_children, (const xmlChar*)"mask");

					struct dipswitch_info* new_conf_info = (struct dipswitch_info*)malloc(sizeof(struct dipswitch_info));
					/* TODO: check allocation */
					memset(new_conf_info, 0x00, sizeof(struct dipswitch_info));
					
					if( conf_name ) new_conf_info->name = conf_name;
					if( conf_tag ) new_conf_info->tag = conf_tag;
					if( conf_mask ) new_conf_info->mask = atoi((const char*)conf_mask);

					struct dipvalue_info* last_confsetting = NULL;

					xmlNodePtr configuration_children = game_children->children;					
					while( configuration_children != NULL ) {
						if( xmlStrcmp(configuration_children->name, (const xmlChar*)"confsetting") == 0 ) {
							xmlChar* confsetting_default = xmlGetProp(configuration_children, (const xmlChar*)"default");
							if( confsetting_default ) {
								if( xmlStrcmp(confsetting_default, (const xmlChar*)"yes") == 0 ) {
									xmlChar* confsetting_value = xmlGetProp(configuration_children, (const xmlChar*)"value");
									if( confsetting_value ) new_conf_info->defvalue = atoi((const char*)confsetting_value);
									xmlFree(confsetting_value);
								}
								else
								{
									xmlChar* confsetting_name = xmlGetProp(configuration_children, (const xmlChar*)"name");
									xmlChar* confsetting_value = xmlGetProp(configuration_children, (const xmlChar*)"value");

									struct dipvalue_info* new_confsetting = (struct dipvalue_info*)malloc(sizeof(struct dipvalue_info));
									/* TODO: check allocation */
									memset(new_confsetting, 0x00, sizeof(struct dipvalue_info));

									if( confsetting_name ) new_confsetting->name = confsetting_name;
									if( confsetting_value ) new_confsetting->value = atoi((const char*)confsetting_value);
									
									if( new_conf_info->values == NULL )
										new_conf_info->values = new_confsetting;
									
									if( last_confsetting )
										last_confsetting->next = new_confsetting;
									last_confsetting = new_confsetting;
								}
								
								xmlFree(confsetting_default);
							}
						}
						
						configuration_children = configuration_children->next;
					}
					
					if( (*new_driv_inf)->configurations == NULL)
						(*new_driv_inf)->configurations = new_conf_info;
					
					if( last_conf_info )
						last_conf_info->next = new_conf_info;
					last_conf_info = new_conf_info;
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
				if( xmlStrcmp(debug_attr, (const xmlChar*)"yes") == 0 )
					is_debug = 1;
				xmlFree(debug_attr);
				debug_attr = NULL;

				app_ver = xmlGetProp(root, (const xmlChar*)"build");
				if( app_ver )
					printf("build: %s%s\n", app_ver, is_debug ? " (debug)" : "");


				if( config_xpath_expr && (strlen(config_xpath_expr) > 0) ) {
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
						if( config_print_xpath_results )
							xmlBuf = xmlBufferCreate();

						struct driver_info* last_driv_inf = NULL;

						int i = 0;
						for( ; i < xpathObj->nodesetval->nodeNr; ++i )
						{
							if( config_print_xpath_results ) {
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

						if( config_print_xpath_results ) {
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

int main(int argc, char *argv[])
{
	printf("mame_regtest %s\n", VERSION);

	if( argc > 2) {
		printf("usage: mame_regtest <configname>\n");
		exit(1);
	}

#ifndef _MSC_VER
	itoa(getpid(), pid_str, 10);
#else
	// TODO
#endif
	printf("\n");
	printf("process: %s\n", pid_str);

	if( getcwd(current_path, sizeof(current_path)) == NULL ) {
		printf("could not get current working path\n");
		exit(1);
	}
	
	printf("\n");
	printf("current path: %s\n", current_path);
	printf("\n");

	/* create dummy root with pid, so we can run multiple instances at the same time */
	append_string(&dummy_root, "dummy_root");
	append_string(&dummy_root, ".");
	append_string(&dummy_root, pid_str);

	append_string(&pause_file, "pause");
	append_string(&pause_file, ".");
	append_string(&pause_file, pid_str);

	printf("initializing configuration\n");
	int config_res = config_init("mame_regtest.xml", "mame_regtest");
	if( !config_res )
		cleanup_and_exit(1, "aborting");

	printf("reading configuration 'global'\n");
	config_res = config_read(mrt_config, "global");

	if( config_res && (argc == 2) ) {
		printf("reading configuration '%s'\n", argv[1]);
		config_res = config_read(mrt_config, argv[1]);
	}

	if( !config_res )
		cleanup_and_exit(1, "aborting");

	printf("\n"); /* for output formating */

	if( is_absolute_path(config_mame_exe) == 0 ) {
		char* tmp_mame_exe = NULL;
		append_string(&tmp_mame_exe, current_path);
		append_string(&tmp_mame_exe, FILESLASH);
		append_string(&tmp_mame_exe, config_mame_exe);
		free(config_mame_exe);
		config_mame_exe = tmp_mame_exe;
	}

	if( !config_mame_exe || (strlen(config_mame_exe) == 0) ) {
		printf("'executable' is empty or missing\n");
		cleanup_and_exit(1, "aborting");
	}

	if( access(config_mame_exe, F_OK ) == -1 ) {
		printf("'%s' does not exist\n", config_mame_exe);
		cleanup_and_exit(1, "aborting");
	}
	if( config_verbose )
		printf("executable: %s\n", config_mame_exe);

	if( access(config_output_folder, F_OK) == 0 ) {
		if( !config_clear_output_folder ) {
			printf("output folder '%s' found\n", config_output_folder);
			cleanup_and_exit(1, "aborting");
		}
	}
	else {
		if( mrt_mkdir(config_output_folder) != 0 ) {
			printf("could not create folder '%s'\n", config_output_folder);
			cleanup_and_exit(1, "aborting");
		}
	}

	if( strlen(config_output_folder) > 0 ) {
		if( config_verbose )
			printf("using output folder: %s\n", config_output_folder);
	}

	append_string(&listxml_output, config_output_folder);
	append_string(&listxml_output, FILESLASH);
	append_string(&listxml_output, "listxml.xml");

	if( config_verbose ) {
		printf("str: %s\n", config_str_str);

		if( config_gamelist_xml_file && (strlen(config_gamelist_xml_file) > 0) ) {
			printf("using custom list: %s\n", config_gamelist_xml_file);
		}

		printf("autosave: %d\n", config_use_autosave);

		printf("ramsize: %d\n", config_use_ramsize);
	}

	if( config_write_mng ) {
		if( config_hack_mngwrite ) {
			if( mrt_mkdir("mng") != 0 ) {
				printf("could not create folder 'mng' - disabling MNG writing\n");
				config_write_mng = 0;
			}
		}
	}
	if( config_verbose )
		printf("write mng: %d\n", config_write_mng);
		
	/* create temporary folder with pid, so we can run multiple instances at the same time */
	append_string(&temp_folder, current_path);
	append_string(&temp_folder, FILESLASH);
	append_string(&temp_folder, "mrt_temp");
	append_string(&temp_folder, ".");
	append_string(&temp_folder, pid_str);

	if( (access(temp_folder, F_OK) != 0) && mrt_mkdir(temp_folder) != 0 ) {
		printf("could not create folder '%s'\n", temp_folder);
		cleanup_and_exit(1, "aborting");
	}

	if( strlen(temp_folder) > 0 ) {
		if( config_verbose )
			printf("using output folder: %s\n", temp_folder);
		if( access(temp_folder, F_OK) != 0 ) {
			printf("temp folder '%s' not found\n", temp_folder);
			cleanup_and_exit(1, "aborting");
		}
	}

	append_string(&stdout_temp_file, temp_folder);
	append_string(&stdout_temp_file, FILESLASH);
	append_string(&stdout_temp_file, "tmp_stdout");

	append_string(&stderr_temp_file, temp_folder);
	append_string(&stderr_temp_file, FILESLASH);
	append_string(&stderr_temp_file, "tmp_stderr");

	append_string(&dummy_ini_folder, temp_folder);
	append_string(&dummy_ini_folder, FILESLASH);
	append_string(&dummy_ini_folder, "dummy_ini");

	if( config_verbose )
		printf("valgrind: %d\n", config_use_valgrind);


	if( config_use_valgrind )
	{
#if USE_VALGRIND
		if( !config_valgrind_binary || (strlen(config_valgrind_binary) == 0) )
			append_string(&config_valgrind_binary, "valgrind");
		if( config_verbose )
			printf("valgrind_binary: %s\n", config_valgrind_binary);
		if( !config_valgrind_parameters || (strlen(config_valgrind_parameters) == 0) )
			append_string(&config_valgrind_parameters, "--tool=memcheck --error-limit=no --leak-check=full --num-callers=50 --show-reachable=yes --track-fds=yes --leak-resolution=med");
		if( config_verbose )
			printf("valgrind_parameters: %s\n", config_valgrind_parameters);
#else
		printf("valgrind support not available on this platform");
#endif
	}

	if( config_verbose ) {
		if( config_rompath_folder && (strlen(config_rompath_folder) > 0) )
			printf("using rompath folder: %s\n", config_rompath_folder);

		printf("bios: %d\n", config_use_bios);
		printf("sound: %d\n", config_use_sound);
		printf("throttle: %d\n", config_use_throttle);
		printf("debug: %d\n", config_use_debug);
		printf("xpath_expr: %s\n", config_xpath_expr ? config_xpath_expr : "");
		printf("use_devices: %d\n", config_use_devices);
		printf("use_nonrunnable: %d\n", config_use_nonrunnable);
		if( config_global_device_file && (strlen(config_global_device_file) > 0) )
			printf("using device_file: %s\n", config_global_device_file);
		printf("use_isbios: %d\n", config_use_isbios);
		printf("store_output: %d\n", config_store_output);
		printf("clear_output_folder: %d\n", config_clear_output_folder);
		printf("test_createconfig: %d\n", config_test_createconfig);
		if( config_additional_options )
			printf("additional_options: %s\n", config_additional_options);
		printf("skip_mandatory: %d\n", config_skip_mandatory);
		printf("osdprocessors: %d\n", config_osdprocessors);
		printf("print_xpath_results: %d\n", config_print_xpath_results);
		printf("test_softreset: %d\n", config_test_softreset);
		printf("write_avi: %d\n", config_write_avi);
		printf("verbose: %d\n", config_verbose);
		/* printf("use_gdb: %d\n", config_use_gdb); */
		printf("write_wav: %d\n", config_write_wav);
		printf("use_dipswitches: %d\n", config_use_dipswitches);
		printf("use_configurations: %d\n", config_use_configurations);
		if( config_hashpath_folder && (strlen(config_hashpath_folder) > 0) )
			printf("using hashpath folder: %s\n", config_hashpath_folder);

		printf("hack_ftr: %d\n", config_hack_ftr);
		printf("hack_biospath: %d\n", config_hack_biospath);
		printf("hack_mngwrite: %d\n", config_hack_mngwrite);
		printf("hack_pinmame: %d\n", config_hack_pinmame);

		printf("\n"); /* for output formating */
	}

	if( strlen(config_str_str) == 0 ) {
		printf("'str' value cannot be empty\n");
		cleanup_and_exit(1, "aborting");		
	}

	if( config_hack_ftr && (atoi(config_str_str) < 2)) {
		printf("'str' value has to be at least '2' when used with 'hack_ftr'\n");
		cleanup_and_exit(1, "aborting");
	}

	if( config_clear_output_folder ) {
		printf("clearing existing output folder\n");
		clear_directory(config_output_folder, 0);
	}

	printf("clearing existing temp folder\n");
	clear_directory(temp_folder, 0);

	printf("\n"); /* for output formating */

	if( !config_gamelist_xml_file || (strlen(config_gamelist_xml_file) == 0) ) {
		append_string(&config_gamelist_xml_file, listxml_output); 

		printf("writing -listxml output\n");
		char* mame_call = NULL;
		get_executable(&mame_call, NULL, "listxml");
		append_string(&mame_call, " -listxml > ");
		append_string(&mame_call, config_gamelist_xml_file);

		if( config_verbose )
			printf("system call: %s\n", mame_call);

		system(mame_call);

		if( config_hack_pinmame ) {
			char* tmp_gamelist_xml = NULL;
			/* TODO: quoting */
			append_string(&tmp_gamelist_xml, temp_folder);
			append_string(&tmp_gamelist_xml, FILESLASH);
			append_string(&tmp_gamelist_xml, "listxml.xml");

			strip_sampleof_pinmame(config_gamelist_xml_file, tmp_gamelist_xml);
			
			remove(config_gamelist_xml_file);
			copy_file(tmp_gamelist_xml, config_gamelist_xml_file);
			
			free(tmp_gamelist_xml);
			tmp_gamelist_xml = NULL;
		}

		free(mame_call);
		mame_call = NULL;
	}

	struct driver_info* driv_inf = NULL;

	printf("\n"); /* for output formating */
	printf("parsing -listxml output\n")	;
	parse_listxml(config_gamelist_xml_file, &driv_inf);

	printf("\n"); /* for output formating */

	if( config_test_createconfig ) {
		printf("writing '%s'\n", get_inifile());
		char* mame_call = NULL;
		get_executable(&mame_call, NULL, "createconfig");
		append_string(&mame_call, " -showconfig > ");
		append_string(&mame_call, config_output_folder);
		append_string(&mame_call, FILESLASH);
		append_string(&mame_call, get_inifile());

		system(mame_call);
		
		free(mame_call);
		mame_call = NULL;
	}

	if( driv_inf == NULL )
		cleanup_and_exit(0, "finished");

	printf("clearing existing dummy directory\n");
	clear_directory(dummy_root, 1);

	if( config_use_debug ) {
		append_string(&debugscript_file, temp_folder);
		append_string(&debugscript_file, FILESLASH);
		append_string(&debugscript_file, "mrt_debugscript");

		FILE* debugscript_fd = fopen(debugscript_file, "w");
		if( !debugscript_fd ) {
			printf("could not open %s\n", debugscript_file);
			cleanup_and_exit(1, "aborted");
		}
		if( config_test_softreset )
			fprintf(debugscript_fd, "softreset\n");
		fprintf(debugscript_fd, "go\n");
		fclose(debugscript_fd);
		debugscript_fd = NULL;
	}
	else {
		if( config_test_softreset )
			printf("'test_softreset' can only be used with 'use_debug'\n");
	}

	printf("\n"); /* for output formating */
	if( !create_dummy_root_ini() )
		cleanup_and_exit(1, "aborted");

	/* setup OSDPROCESSORS */
	char osdprocessors_tmp[128];
	snprintf(osdprocessors_tmp, sizeof(osdprocessors_tmp), "OSDPROCESSORS=%d", config_osdprocessors);
	putenv(osdprocessors_tmp);

	printf("\n");
	process_driver_info_list(driv_inf);	
	printf("\n"); /* for output formating */
	cleanup_driver_info_list(driv_inf);

	cleanup_and_exit(0, "finished");
	return 0; /* to shut up compiler */
}
