#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __GNUC__
#include <unistd.h>
#endif

#ifdef _MSC_VER
#include <direct.h>
#include <io.h>
#include <process.h>
#if _MSC_VER >= 1400
#undef access
#define access _access
#undef rmdir
#define rmdir _rmdir
#undef snprintf
#define snprintf _snprintf
#undef strdup
#define strdup _strdup
#undef chdir
#define chdir _chdir
#undef getcwd
#define getcwd _getcwd
#undef putenv
#define putenv _putenv
#undef getpid
#define getpid _getpid
#endif
#define F_OK 00
#endif

#ifndef WIN32
#include <arpa/inet.h>
#include <sys/wait.h>
#define USE_VALGRIND 1
#else
#define USE_VALGRIND 0
#endif

/* libxml2 */
#ifdef __MINGW32__
#define IN_LIBXML
#endif
#include "libxml/parser.h"
#include "libxml/xpath.h"

/* local */
#include "common.h"
#include "config.h"

#if defined(_MSC_VER) || defined(__MINGW32__)
//#define USE_MRT_SYSTEM
#endif

#ifndef _MAX_PATH
#include <limits.h>
#define _MAX_PATH PATH_MAX
#endif

#define VERSION "0.73"

struct device_info {
	xmlChar* name;
	xmlChar* briefname;	
	xmlChar* interface;
	int mandatory;
	struct device_info* next;
};

enum cfg_type {
	CFG_DIP 	= 1,
	CFG_CONF 	= 2
};

struct slot_info {
	xmlChar* name;
	int slotoption_count;
	xmlChar* slotoptions[256];
	struct slot_info* next;
};

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
	int ram_default;
	int bios_count;
	xmlChar* bioses[32];
	int bios_default;
	struct device_info* devices;
	xmlChar* device_mandatory;
	struct dipswitch_info* dipswitches;
	struct dipswitch_info* configurations;
	int has_softlist;
	xmlChar* softwarelist; /* TODO: might not be necessary since -listsoftware will list of images from all lists */
	xmlChar* softwarelist_filter;
	struct slot_info* slots;
	struct driver_info* next;
};

struct image_entry {
	xmlChar* device_briefname;
	xmlChar* device_file;
	xmlChar* device_interface; /* TODO: do we even need this? */
	xmlChar* device_filter;
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
	const char* device_mandatory;
	struct dipswitch_info* dipswitch;
	struct dipvalue_info* dipvalue;
	struct dipswitch_info* configuration;
	struct dipvalue_info* confsetting;
	struct slot_info* slot;
	const char* slotoption;
};

static int app_type = 0;
static int is_debug = 0;

static xmlChar* app_ver = NULL;
static char* debugscript_file = NULL;
static char* listxml_output = NULL;
static char* temp_folder = NULL;
#ifndef USE_MRT_SYSTEM
static char* stdout_temp_file = NULL;
static char* stderr_temp_file = NULL;
#endif
static char current_path[_MAX_PATH] = "";
static char* dummy_root = NULL;
static char* pause_file = NULL;
static char pid_str[10] = "";
static int mameconfig_ver = 10;
struct driver_info* global_driv_inf = NULL;

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
static char* config_additional_options = NULL;
static int config_skip_mandatory = 0;
static int config_osdprocessors = 1;
static int config_print_xpath_results = 0;
static int config_test_softreset = 0;
static int config_hack_pinmame = 0;
static int config_write_avi = 0;
static int config_verbose = 0;
static int config_use_gdb = 0;
static int config_write_wav = 0;
static int config_use_dipswitches = 0;
static int config_use_configurations = 0;
static char* config_hashpath_folder = NULL;
static int config_use_softwarelist = 0;
static int config_hack_softwarelist = 0;
static int config_test_frontend = 1;
static int config_use_slots = 0;

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
	{ "additional_options",		CFG_STR_PTR,	&config_additional_options },
	{ "skip_mandatory",			CFG_INT,		&config_skip_mandatory },
	{ "osdprocessors",			CFG_INT,		&config_osdprocessors },
	{ "print_xpath_results",	CFG_INT,		&config_print_xpath_results },
	{ "test_softreset",			CFG_INT,		&config_test_softreset },
	{ "hack_pinmame",			CFG_INT,		&config_hack_pinmame },
	{ "write_avi",				CFG_INT,		&config_write_avi },
	{ "verbose",				CFG_INT,		&config_verbose },
	{ "use_gdb",				CFG_INT,		&config_use_gdb },
	{ "write_wav",				CFG_INT,		&config_write_wav },
	{ "use_dipswitches",		CFG_INT,		&config_use_dipswitches },
	{ "use_configurations",		CFG_INT,		&config_use_configurations },
	{ "hashpath",				CFG_STR_PTR,	&config_hashpath_folder },
	{ "use_softwarelist",		CFG_INT,		&config_use_softwarelist },
	{ "hack_softwarelist",		CFG_INT,		&config_hack_softwarelist },
	{ "test_frontend",			CFG_INT,		&config_test_frontend },
	{ "use_slots",				CFG_INT,		&config_use_slots },
	{ NULL,						CFG_UNK,		NULL }
};

static int get_png_data(const char* png_name, unsigned int *IHDR_width, unsigned int* IHDR_height, unsigned int *IDAT_size, unsigned int* IDAT_crc);
static void open_mng_and_skip_sig(const char* mng_name, FILE** mng_fd);
static int internal_get_next_IDAT_data(FILE* in_fd, unsigned int *IDAT_size, unsigned int* IDAT_crc);
static void cleanup_and_exit(int exitcode, const char* errstr);
static int get_MHDR_data(FILE* in_fd, unsigned int* MHDR_width, unsigned int* MHDR_height);
static void cleanup_driver_info_list(struct driver_info* driv_inf);

/* result must be free'd with xmlXPathFreeNodeSet() */
static xmlNodeSetPtr get_xpath_nodeset(xmlDocPtr doc, const xmlChar* xpath_expr)
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
	
	if( in_fd && out_fd ) {
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
	}

	if( out_fd ) {
		fclose(out_fd);
		out_fd = NULL;
	}
	
	if( in_fd ) {
		fclose(in_fd);
		in_fd = NULL;
	}
}

/* TODO: how to set "callstr" for non-de calls? */
static void get_executable(char** sys, struct driver_entry* de, const char* callstr, const char* parameters)
{
#if !USE_VALGRIND
	/* shut up compiler */
	(void)de;
	(void)callstr;
#endif
	
	if( config_use_valgrind && de ) {
#if USE_VALGRIND
		append_string(sys, config_valgrind_binary);
		append_string(sys, " ");
		append_string(sys, config_valgrind_parameters);
		append_string(sys, " ");
		append_string(sys, "--log-file=valgrind.log");
		append_string(sys, " ");
#endif
	}
	else if( config_use_gdb && (!callstr || (strcmp(callstr, "listxml") != 0)) && (!parameters || (strstr(parameters, "-list") == NULL && strstr(parameters, "-validate") == NULL && strstr(parameters, "-show") == NULL && strstr(parameters, "-verify") == NULL)) ) {
		/* cannot be done with -listxml sine it messes up the output */
		append_string(sys, "gdb");
		append_string(sys, " ");
		append_string(sys, "--batch --eval-command=\"set target-charset UTF-8\" --eval-command=run --eval-command=bt --return-child-result --args");
		append_string(sys, " ");
	}

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
		fprintf(stderr, "could not get MHDR data from '%s'\n", file);
		fclose(mng_fd);
		mng_fd = NULL;
		return frame;
	}

	{
		char tmp[128];
		snprintf(tmp, sizeof(tmp), "%u", MHDR_width);
		xmlNewProp(filenode, (const xmlChar*)"width", (const xmlChar*)tmp);
		snprintf(tmp, sizeof(tmp), "%u", MHDR_height);
		xmlNewProp(filenode, (const xmlChar*)"height", (const xmlChar*)tmp);
	}

	for( ; ; ) {
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
			fprintf(stderr, "unexpected error parsing MNG '%s'\n", file);
			break;
		}
	}

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

static void clear_callback_2(struct parse_callback_data* pcd)
{
	/* do not delete the -log or valgrind output */
	if( pcd->type == ENTRY_FILE && strstr(pcd->entry_name, "valgrind") == NULL && strcmp(pcd->entry_name, "error.log") != 0 ) {
		remove(pcd->fullname);
	}
	/* do not delete the "snap" folder */
	else if( (pcd->type == ENTRY_DIR_END) && (strcmp(pcd->entry_name, "snap") != 0) ) {
		int delete_root = 1;
		parse_directory(pcd->fullname, 0, clear_callback_2, (void*)&delete_root);
	}
	else if( pcd->type == ENTRY_END ) {
		int* delete_root = (int*)pcd->user_data;
		if( *delete_root )
			rmdir(pcd->dirname);
	}
}

static void clear_directory_2(const char* dirname, int delete_root)
{
	parse_directory(dirname, 0, clear_callback_2, (void*)&delete_root);

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
		snprintf(tmp, sizeof(tmp), "%ld", pcd->s->st_size);
		xmlNewProp(filenode, (const xmlChar*)"size", (const xmlChar*)tmp);

		unsigned int crc = 0;
		calc_crc32(pcd->fullname, &crc);
		snprintf(tmp, sizeof(tmp), "%x", crc);
		xmlNewProp(filenode, (const xmlChar*)"crc", (const xmlChar*)tmp);

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

static void cleanup_and_exit(int exitcode, const char* errstr)
{
	if( global_driv_inf ) {
		cleanup_driver_info_list(global_driv_inf);
		global_driv_inf = NULL;
	}

	if( debugscript_file ) {
		free(debugscript_file);
		debugscript_file = NULL;
	}	

	if( listxml_output ) {
		free(listxml_output);
		listxml_output = NULL;
	}
	config_free(mrt_config);

#ifndef USE_MRT_SYSTEM
	if( stderr_temp_file ) {
		free(stderr_temp_file);
		stderr_temp_file = NULL;
	}
	if( stdout_temp_file ) {
		free(stdout_temp_file);
		stdout_temp_file = NULL;
	}
#endif
	
	clear_directory(temp_folder, 1);
	free(temp_folder);
	temp_folder = NULL;

	clear_directory(dummy_root, 1);

	free(pause_file);
	pause_file = NULL;
	
	free(dummy_root);
	dummy_root = NULL;
	
#ifdef LOG_ALLOC
	print_leaked_pointers();
#endif

	fprintf(stderr, "%s\n", errstr);
	exit(exitcode);
}

static void print_driver_info(struct driver_entry* de, FILE* print_fd)
{
	if( de == NULL )
		return;

	fprintf(print_fd, "%s: %s", de->sourcefile, de->name);
	if( de->bios && strlen(de->bios) > 0 )
		fprintf(print_fd, " (bios %s)", de->bios);
	if( de->ramsize > 0 )
		fprintf(print_fd, " (ramsize %d)", de->ramsize);
	if( de->dipswitch && de->dipswitch->name && de->dipvalue && de->dipvalue->name )
		fprintf(print_fd, " (dipswitch %s value %s)", de->dipswitch->name, de->dipvalue->name);
	if( de->configuration && de->configuration->name && de->confsetting && de->confsetting->name )
		fprintf(print_fd, " (configuration %s value %s)", de->configuration->name, de->confsetting->name);
	if( de->slot && de->slot->name && de->slotoption )
		fprintf(print_fd, " (slot %s option %s)", de->slot->name, de->slotoption);
	struct image_entry* images = de->images;
	while( images ) {
		if( (images->device_briefname && xmlStrlen(images->device_briefname) > 0) &&
			(images->device_file && xmlStrlen(images->device_file) > 0) ) {
			fprintf(print_fd, " (%s %s)", images->device_briefname, images->device_file);
		}
		images = images->next;
	}
}

static void free_image_entry(struct image_entry* image)
{
	xmlFree(image->device_briefname);
	xmlFree(image->device_file);
	xmlFree(image->device_interface);
	xmlFree(image->device_filter);
	free(image);
}

static void free_image_entries(struct image_entry* images)
{
	while( images ) {
		struct image_entry* temp = images;
		images = images->next;
		free_image_entry(temp);
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

		memset(image, 0x00, sizeof(struct image_entry));

		image->device_briefname = xmlStrdup(attrs->name);
		xmlNodePtr value = attrs->children;
		if( value )
			image->device_file = xmlStrdup(value->content);
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

/* reads data from a "software" element node */
static int read_softlist_entry(const xmlNodePtr node, struct image_entry** images, struct driver_info* driv_inf)
{
	*images = NULL;
	
	struct image_entry* image = (struct image_entry*)malloc(sizeof(struct image_entry));
	if( !image )
		return 0;
	
	memset(image, 0x00, sizeof(struct image_entry));
	
	image->device_file = xmlGetProp(node, (const xmlChar*)"name");
	
	xmlNodePtr soft_child = node->children;
	while( soft_child ) {
		if( soft_child->type == XML_ELEMENT_NODE ) {
			if( xmlStrcmp(soft_child->name, (const xmlChar*)"part") == 0 ) {
				image->device_interface = xmlGetProp(soft_child, (const xmlChar*)"interface");
				
				xmlNodePtr part_child = soft_child->children;
				while( part_child ) {
					if( part_child->type == XML_ELEMENT_NODE ) {
						if( xmlStrcmp(part_child->name, (const xmlChar*)"feature") == 0 ) {
							xmlChar* name_key = xmlGetProp(part_child, (const xmlChar*)"name");
							if( xmlStrcmp(name_key, (const xmlChar*)"compatibility") == 0 )
								image->device_filter = xmlGetProp(part_child, (const xmlChar*)"value");
							xmlFree(name_key);
							name_key = NULL;
							if( image->device_filter )
								break;
						}
					}
					
					part_child = part_child->next;
				}
				
				break;
			}
		}
		
		soft_child = soft_child->next;
	}
	
	/* TODO: move this somewhere else? */
	struct device_info* dev_info = driv_inf->devices;
	while(dev_info) {
		/* TODO: do a better match */
		//if( xmlStrcmp(dev_info->interface, image->device_interface) == 0 ) {
		if( xmlStrstr(dev_info->interface, image->device_interface) != NULL ) {
			image->device_briefname = xmlStrdup(dev_info->briefname);
			break;
		}
			
		dev_info = dev_info->next;
	}
	
	/* TODO: use better match */
	if( !image->device_briefname || (image->device_filter && driv_inf->softwarelist_filter && xmlStrstr(image->device_filter, driv_inf->softwarelist_filter) == NULL) ) {
		free_image_entry(image);
		return 0;
	}
	
	*images = image;
	
	return 1;
}

static int get_MHDR_data(FILE* in_fd, unsigned int* MHDR_width, unsigned int* MHDR_height)
{
	unsigned int chunk_size = 0;
	unsigned int reversed_chunk_size = 0;
	unsigned char chunk_name[4] = "";

	if( fread(&chunk_size, sizeof(unsigned int), 1, in_fd) != 1 ) {
		fprintf(stderr, "could not read chunk size\n");
		return 0;
	}

	reversed_chunk_size = htonl(chunk_size);

	if( fread(chunk_name, sizeof(unsigned char), 4, in_fd) != 4 ) {
		fprintf(stderr, "could not read chunk name\n");
		return 0;
	}

	if( memcmp(MHDR_name, chunk_name, 4) == 0 ) {
		unsigned int width = 0, height = 0;

		if( fread(&width, sizeof(unsigned int), 1, in_fd) != 1 ) {
				fprintf(stderr, "could not read MHDR chunk width\n");
				return 0;
		}

		if( fread(&height, sizeof(unsigned int), 1, in_fd) != 1 ) {
				fprintf(stderr, "could not read MHDR chunk height\n");
				return 0;
		}

		*MHDR_width = htonl(width);
		*MHDR_height = htonl(height);

		fseek(in_fd, reversed_chunk_size - (2 * sizeof(unsigned int)), SEEK_CUR); /* jump remaining MHDR chunk */
		fseek(in_fd, 4, SEEK_CUR); /* jump CRC */

		return 1;
	}

	fprintf(stderr, "could not find MHDR chunk\n");

	return 0;
}

static int get_IHDR_data(FILE* in_fd, unsigned int* IHDR_width, unsigned int* IHDR_height)
{
	unsigned int chunk_size = 0;
	unsigned int reversed_chunk_size = 0;
	unsigned char chunk_name[4] = "";

	if( fread(&chunk_size, sizeof(unsigned int), 1, in_fd) != 1 ) {
		fprintf(stderr, "could not read IHDR chunk size\n");
		return 0;
	}

	reversed_chunk_size = htonl(chunk_size);
	
	if( fread(chunk_name, sizeof(unsigned char), 4, in_fd) != 4 ) {
		fprintf(stderr, "could not read IHDR chunk name\n");
		return 0;
	}

	if( memcmp(IHDR_name, chunk_name, 4) == 0 ) {
		unsigned int width = 0, height = 0;
		
		if( fread(&width, sizeof(unsigned int), 1, in_fd) != 1 ) {
				fprintf(stderr, "could not read IHDR chunk width\n");
				return 0;
		}
		
		if( fread(&height, sizeof(unsigned int), 1, in_fd) != 1 ) {
				fprintf(stderr, "could not read IHDR chunk height\n");
				return 0;
		}

		*IHDR_width = htonl(width);
		*IHDR_height = htonl(height);
		
		fseek(in_fd, reversed_chunk_size - (2 * sizeof(unsigned int)), SEEK_CUR); /* jump remaining IHDR chunk */
		fseek(in_fd, 4, SEEK_CUR); /* jump CRC */
		
		return 1;
	}

	fprintf(stderr, "could not find IHDR chunk\n");

	return 0;
}

static int internal_get_next_IDAT_data(FILE* in_fd, unsigned int *IDAT_size, unsigned int* IDAT_crc)
{
	unsigned int chunk_size = 0;
	unsigned int reversed_chunk_size = 0;
	unsigned char chunk_name[4] = "";

	for( ; ; ) {
		if( fread(&chunk_size, sizeof(unsigned int), 1, in_fd) != 1 ) {
			fprintf(stderr, "could not read IDAT chunk size\n");
			return 0;
		}

		reversed_chunk_size = htonl(chunk_size);
		
		if( fread(chunk_name, sizeof(unsigned char), 4, in_fd) != 4 ) {
			fprintf(stderr, "could not read IDAT chunk name\n");
			return 0;
		}

		if( memcmp(IDAT_name, chunk_name, 4) == 0 ) {
			fseek(in_fd, reversed_chunk_size, SEEK_CUR); /* jump IDAT chunk */
			
			unsigned int chunk_crc = 0;
			if( fread(&chunk_crc, sizeof(unsigned int), 1, in_fd) != 1 ) {
				fprintf(stderr, "could not read IDAT chunk CRC\n");
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
	}
}

static int get_png_data(const char* png_name, unsigned int *IHDR_width, unsigned int* IHDR_height, unsigned int *IDAT_size, unsigned int* IDAT_crc)
{
	FILE* png_fd = fopen(png_name, "rb");
	if( png_fd == NULL ) {
		fprintf(stderr, "could not open %s\n", png_name);
		return 0;
	}

	fseek(png_fd, 0, SEEK_SET);

	unsigned char sig[8] = "";
	
	if( fread(sig, sizeof(unsigned char), 8, png_fd) != 8 ) {
		fprintf(stderr, "could not read PNG signature: %s\n", png_name);
		fclose(png_fd);
		return 0;
	}

	if( memcmp(png_sig, sig, 8) != 0 ) {
		fprintf(stderr, "contains no PNG signature: %s\n", png_name);
		fclose(png_fd);
		return 0;		
	}

	int IHDR_res = get_IHDR_data(png_fd, IHDR_width, IHDR_height);
	if( IHDR_res != 1 ) {
		fprintf(stderr, "error getting IHDR data: %s\n", png_name);
		fclose(png_fd);
		return IHDR_res;
	}

	int IDAT_res = internal_get_next_IDAT_data(png_fd, IDAT_size, IDAT_crc);
	if( IDAT_res != 1 ) 
		fprintf(stderr, "error getting IDAT data: %s\n", png_name);

	fclose(png_fd);

	return IDAT_res;
}

static void open_mng_and_skip_sig(const char* mng_name, FILE** mng_fd)
{
	*mng_fd = fopen(mng_name, "rb");
	if( *mng_fd == NULL ) {
		fprintf(stderr, "could not open %s\n", mng_name);
		return;
	}

	fseek(*mng_fd, 0, SEEK_SET);

	unsigned char sig[8] = "";

	if( fread(sig, sizeof(unsigned char), 8, *mng_fd) != 8 ) {
		fprintf(stderr, "could not read MNG signature: %s\n", mng_name);
		fclose(*mng_fd);
		*mng_fd = NULL;
		return;
	}

	if( memcmp(mng_sig, sig, 8) != 0 ) {
		fprintf(stderr, "contains no MNG signature: %s\n", mng_name);
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
	
	int res = 0;
	struct dipswitch_info* inp_name = NULL;
	struct dipvalue_info* inp_value = NULL;
	const char* type_str = NULL;
	
	if( de == NULL )
		return res;
	
	if( type == CFG_DIP ) {
		inp_name = de->dipswitch;
		inp_value = de->dipvalue;
		type_str = "DIPSWITCH";
	}
	else if ( type == CFG_CONF ) {
		inp_name = de->configuration;
		inp_value = de->confsetting;
		type_str = "CONFIG";
	}
	
	if( !inp_name || !inp_value )
		return res;
	
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
		snprintf(tmp, sizeof(tmp), "%d", mameconfig_ver);
		xmlNewProp(cfg_node, (const xmlChar*)"version", (const xmlChar*)tmp);
		
		xmlDocSetRootElement(cfg_doc, cfg_node);
		
		xmlNodePtr system_node = xmlNewChild(cfg_node, NULL, (const xmlChar*)"system", NULL);
		xmlNewProp(system_node, (const xmlChar*)"name", (const xmlChar*)de->name);
	
		xmlNodePtr input_node = xmlNewChild(system_node, NULL, (const xmlChar*)"input", NULL);
	
		xmlNodePtr port_node = xmlNewChild(input_node, NULL, (const xmlChar*)"port", NULL);
		xmlNewProp(port_node, (const xmlChar*)"tag", (const xmlChar*)inp_name->tag);
		xmlNewProp(port_node, (const xmlChar*)"type", (const xmlChar*)type_str);
		snprintf(tmp, sizeof(tmp), "%d", inp_name->mask);
		xmlNewProp(port_node, (const xmlChar*)"mask", (const xmlChar*)tmp);
		snprintf(tmp, sizeof(tmp), "%d", inp_name->defvalue);
		xmlNewProp(port_node, (const xmlChar*)"defvalue", (const xmlChar*)tmp);
		snprintf(tmp, sizeof(tmp), "%d", inp_value->value);
		xmlNewProp(port_node, (const xmlChar*)"value", (const xmlChar*)tmp);
		
		res = 1;
	}
	else {
		fprintf(stderr, "unknown 'mameconfig' version\n");
	}

	if( res == 1 )	
		xmlSaveFormatFileEnc(cfgfile, cfg_doc, "UTF-8", 1);
	
	xmlFreeDoc(cfg_doc);
	cfg_doc = NULL;

	free(cfgfile);
	cfgfile = NULL;

	return res;
}

static char* create_commandline(struct driver_entry* de)
{
	char* sys = NULL;	
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
	if( de->slot && de->slotoption ) {
		append_string(&sys, " -");
		append_string(&sys, (const char*)de->slot->name);
		append_string(&sys, " ");
		append_string(&sys, de->slotoption);
	}
	struct image_entry* images = de->images;
	while(images) {
		if( (images->device_briefname && xmlStrlen(images->device_briefname) > 0) && 
			(images->device_file && xmlStrlen(images->device_file) > 0) ) {
			append_string(&sys, " -");
			append_string(&sys, (const char*)images->device_briefname);
			append_string(&sys, " ");
			append_quoted_string(&sys, (const char*)images->device_file);
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
	if( config_additional_options && (*config_additional_options != 0) ) {
		append_string(&sys, " ");
		append_string(&sys, config_additional_options);
	}
	
	append_string(&sys, " -video none"); /* disable video output */
	if( !config_use_sound )
		append_string(&sys, " -nosound"); /* disable sound output */
	if( !config_use_throttle )
		append_string(&sys, " -nothrottle"); /* disable throttle */
	if( !config_use_debug )
		append_string(&sys, " -nodebug"); /* disable debug window */
	else {
		append_string(&sys, " -debug"); /* enable debug window */
		/* pass debugscript, so so debugger won't block the execution */
		append_string(&sys, " -debugscript ");
		append_quoted_string(&sys, debugscript_file);
	}
	append_string(&sys, " -nomouse"); /* disable "mouse" so it doesn't grabs the mouse pointer on window creation */
	append_string(&sys, " -window"); /* run it in windowed mode */
	if( !config_hack_ftr )
		append_string(&sys, " -seconds_to_run ");
	else
		append_string(&sys, " -frames_to_run ");
	append_string(&sys, config_str_str);
	if( config_rompath_folder && (*config_rompath_folder != 0) ) {
		if( !config_hack_biospath )
			append_string(&sys, " -rompath "); /* rompath for MAME/MESS */
		else
			append_string(&sys, " -biospath "); /* old biospath for MESS */
		append_quoted_string(&sys, config_rompath_folder);
	}
	if( (app_type == APP_MESS) && config_hashpath_folder && (*config_hashpath_folder != 0) )
	{
		append_string(&sys, " -hashpath "); /* hashpath for MESS */
		append_quoted_string(&sys, config_hashpath_folder);
	}
	if( config_hack_pinmame ) {
		append_string(&sys, " -skip_disclaimer");
		append_string(&sys, " -skip_gameinfo");
	}
	
	return sys;
}

static int execute_mame(struct driver_entry* de, const char* parameters, int redirect, int change_dir, xmlNodePtr* result, char** cmd_out, char** stdout_out)
{
	print_driver_info(de, stdout);
	if( config_use_autosave && de && de->autosave )
		printf(" (autosave)");
	printf("\n");

	/* DEBUG!!! */
	/* return 1; */

	char* sys = NULL;
	
#ifndef USE_MRT_SYSTEM
#ifdef WIN32
	/* the whole command-line has to be quoted - begin */
	append_string(&sys, "\" ");
#endif
#endif

	get_executable(&sys, de, NULL, parameters);
	append_string(&sys, " ");
	append_string(&sys, parameters);

#ifndef USE_MRT_SYSTEM
	if( redirect || stdout_out ) {
		append_string(&sys, " > ");
		append_quoted_string(&sys, stdout_temp_file);
	}

	if( redirect ) {
		append_string(&sys, " 2> ");
		append_quoted_string(&sys, stderr_temp_file);
	}
	
#ifdef WIN32
	/* the whole command-line has to be quoted - end */
	append_string(&sys, " \"");
#endif
#endif
	
	if( config_verbose )
		printf("system call: %s\n", sys);
		
	if( cmd_out )
		*cmd_out = sys;

	/* TODO: errorhandling */
	mrt_mkdir(dummy_root);
	create_cfg(de, 1);
	create_cfg(de, 2);
	int ch_res = -1;
	if( change_dir )
		ch_res = chdir(dummy_root);
#ifndef USE_MRT_SYSTEM
	int sys_res = system(sys);
#ifndef WIN32
	sys_res = WEXITSTATUS(sys_res);
#endif
#else
	char* stdout_str = NULL;
	char* stderr_str = NULL;
	int sys_res = -1;
	if( redirect )
		sys_res = mrt_system(sys, &stdout_str, &stderr_str);
	else if( stdout_out )
		sys_res = mrt_system(sys, stdout_out, NULL);
	else
		sys_res = mrt_system(sys, NULL, NULL);
#endif
	if( change_dir )
		ch_res = chdir(current_path);
	
	/* TODO: check result */
	(void)ch_res;
	
	if( !cmd_out )
		free(sys);
	sys = NULL;
	
#ifndef USE_MRT_SYSTEM
	/* TODO: errorhandling */
	if( stdout_out )
		read_file(stdout_temp_file, stdout_out);
#endif

	if( result ) {
		*result = xmlNewNode(NULL, (const xmlChar*)"result");
		char tmp[128];
		snprintf(tmp, sizeof(tmp), "%d", sys_res);
		xmlNewProp(*result, (const xmlChar*)"exitcode", (const xmlChar*)tmp);
#ifndef USE_MRT_SYSTEM
		char* stdout_str = NULL;
		if( redirect && read_file(stdout_temp_file, &stdout_str) == 0 ) {
#else
		if( stdout_str ) {
#endif
			filter_unprintable(stdout_str, strlen(stdout_str));
			xmlNewProp(*result, (const xmlChar*)"stdout", (const xmlChar*)stdout_str);
			free(stdout_str);
			stdout_str = NULL;
		}
#ifndef USE_MRT_SYSTEM
		char* stderr_str = NULL;
		if( redirect && read_file(stderr_temp_file, &stderr_str) == 0 ) {
#else
		if( stderr_str ) {
#endif
			filter_unprintable(stderr_str, strlen(stderr_str));
			xmlNewProp(*result, (const xmlChar*)"stderr", (const xmlChar*)stderr_str);
			free(stderr_str);
			stderr_str = NULL;
		}
	}

	return sys_res == 0 ? 1 : 0;
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
	for( ; ; ) {
		if( actual_driv_inf->name )
			xmlFree(actual_driv_inf->name);
		if( actual_driv_inf->sourcefile )
			xmlFree(actual_driv_inf->sourcefile);
		if( actual_driv_inf->bios_count ) {
			int i = 0;
			for( ; i < actual_driv_inf->bios_count; ++i )
				xmlFree(actual_driv_inf->bioses[i]);
		}
		if( actual_driv_inf->device_mandatory )
			xmlFree(actual_driv_inf->device_mandatory);
		if( actual_driv_inf->softwarelist )
			xmlFree(actual_driv_inf->softwarelist);
		if( actual_driv_inf->softwarelist_filter )
			xmlFree(actual_driv_inf->softwarelist_filter);
			
		struct device_info* devices = actual_driv_inf->devices;
		while( devices != NULL ) {
			struct device_info* next_device = devices->next;
			xmlFree(devices->name);
			xmlFree(devices->briefname);
			xmlFree(devices->interface);
			free(devices);
			devices = next_device;
		}

		struct dipswitch_info* dipswitch = actual_driv_inf->dipswitches;
		while( dipswitch != NULL ) {
			xmlFree(dipswitch->name);
			xmlFree(dipswitch->tag);
			struct dipvalue_info* dipvalue = dipswitch->values;
			while( dipvalue != NULL ) {
				xmlFree(dipvalue->name);
				struct dipvalue_info* next_dipvalue = dipvalue->next;
				free(dipvalue);
				dipvalue = next_dipvalue;
			};
			
			struct dipswitch_info* next_dipswitch = dipswitch->next;
			free(dipswitch);				
			dipswitch = next_dipswitch;
		};

		struct dipswitch_info* configuration = actual_driv_inf->configurations;
		while( configuration != NULL ) {
			xmlFree(configuration->name);
			xmlFree(configuration->tag);
			struct dipvalue_info* confsetting = configuration->values;
			while( confsetting != NULL ) {
				xmlFree(confsetting->name);
				struct dipvalue_info* next_confsetting = confsetting->next;
				free(confsetting);
				confsetting = next_confsetting;
			};
			
			struct dipswitch_info* next_configuration = configuration->next;
			free(configuration);				
			configuration = next_configuration;
		};
		
		struct slot_info* slot = actual_driv_inf->slots;
		while( slot != NULL ) {
			xmlFree(slot->name);
			int i = 0;
			for( ; i < slot->slotoption_count; ++i )
			{
				xmlFree(slot->slotoptions[i]);
				slot->slotoptions[i] = NULL;
			}
			
			struct slot_info* next_slot = slot->next;
			free(slot);
			slot = next_slot;
		};

		if( actual_driv_inf->next ) {
			struct driver_info* tmp_driv_inf = actual_driv_inf;
			actual_driv_inf = actual_driv_inf->next;
			free(tmp_driv_inf);
		}
		else {
			free(actual_driv_inf);
			break;
		}
	}
}

static void execute_mame2(struct driver_entry* de)
{
	if( !de->name || *de->name == 0 ) {
		fprintf(stderr, "empty game name\n");
		return;
	}

	if( !de->sourcefile || *de->sourcefile == 0 ) {
		fprintf(stderr, "empty sourcefile\n");
		return;
	}

	if( access(pause_file, F_OK) == 0 ) {
		printf("\n");
		printf("found pause file\n");
		printf("please press any key to continue\n");
		(void)mrt_getch();
		printf("\n");
	}

	xmlNodePtr result1 = NULL;
	xmlNodePtr result2 = NULL;

	xmlDocPtr output_doc = xmlNewDoc((const xmlChar*)"1.0");
	xmlNodePtr output_node = xmlNewNode(NULL, (const xmlChar*)"output");
	xmlDocSetRootElement(output_doc, output_node);

	xmlNewProp(output_node, (const xmlChar*)"name", (const xmlChar*)de->name);
	xmlNewProp(output_node, (const xmlChar*)"sourcefile", (const xmlChar*)de->sourcefile);
	if( config_use_autosave && de->autosave )
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
		xmlNewProp(output_node, (const xmlChar*)"mandatory", (const xmlChar*)de->device_mandatory);

	if( de->images ) {
		xmlNodePtr devices_node = xmlNewChild(output_node, NULL, (const xmlChar*)"devices", NULL);
		
		struct image_entry* images = de->images;
		while(images) {
			if( (images->device_briefname && xmlStrlen(images->device_briefname) > 0) &&
				(images->device_file && xmlStrlen(images->device_file) > 0) ) {
				xmlNewProp(devices_node, images->device_briefname, images->device_file);
			}

			images = images->next;
		}
	}
	if( de->slot && de->slot->name && de->slotoption ) {
		xmlNewProp(output_node, (const xmlChar*)"slot", (const xmlChar*)de->slot->name);
		xmlNewProp(output_node, (const xmlChar*)"slotoption", (const xmlChar*)de->slotoption);
	}

	char* params = create_commandline(de);	
	char* cmd = NULL;
	int res = execute_mame(de, params, 1, 1, &result1, &cmd, NULL);
	
	if( cmd ) {
		xmlNewProp(output_node, (const xmlChar*)"cmd", (const xmlChar*)cmd);
		free(cmd);
		cmd = NULL;
	}

	if( result1 ) {
		xmlAddChild(output_node, result1);
		build_output_xml(dummy_root, result1);
	}

	if( res == 1 && config_use_autosave && de->autosave ) {
		execute_mame(de, params, 1, 1, &result2, NULL, NULL);

		if( result2 ) {
			xmlAddChild(output_node, result2);
			build_output_xml(dummy_root, result2);
		}
	}
	
	free(params);
	params = NULL;

	if( (config_store_output > 0) && (access(dummy_root, F_OK) == 0) ) {
		char* outputdir = NULL;
		append_string(&outputdir, config_output_folder);
		append_string(&outputdir, FILESLASH);
		append_driver_info(&outputdir, de);

		while( rename(dummy_root, outputdir) != 0 )
		{
			/* TODO: sleep */
			fprintf(stderr, "could not rename '%s' to '%s'\n", dummy_root, outputdir);
		}
			
		/* clear everything but the "snap" folder */
		if( config_store_output == 2)
			clear_directory_2(outputdir, 0);

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
}

static void execute_mame3(struct driver_entry* de, struct driver_info* actual_driv_inf)
{
	if( !(config_skip_mandatory && de->device_mandatory) )
		execute_mame2(de);
	
	int software_count = 0;

	/* process softlist output */
	if( config_use_softwarelist && config_hashpath_folder && actual_driv_inf->has_softlist ) {
		char* driver_softlist_file = NULL;
		append_string(&driver_softlist_file, config_output_folder);
		append_string(&driver_softlist_file, FILESLASH);
		append_string(&driver_softlist_file, (const char*)actual_driv_inf->name);
		append_string(&driver_softlist_file, "_listsoftware");
		append_string(&driver_softlist_file, ".xml");
	
		/* process softlist entries */
		xmlDocPtr soft_doc = xmlReadFile(driver_softlist_file, NULL, XML_PARSE_DTDVALID);
		if( soft_doc ) {
			xmlNodeSetPtr soft_nodeset = NULL;
			if( config_hack_softwarelist )
				soft_nodeset = get_xpath_nodeset(soft_doc, (const xmlChar*)"/softwarelist/software");
			else
				soft_nodeset = get_xpath_nodeset(soft_doc, (const xmlChar*)"/softwarelists/softwarelist/software");
			if( soft_nodeset )
			{
				char initial_postfix[1024];
				snprintf(initial_postfix, sizeof(initial_postfix), "%s", de->postfix);

				int i = 0;
				for( ; i < soft_nodeset->nodeNr; ++i )
				{
					if( config_use_softwarelist == 2 && software_count == 1 )
						break;
				
					struct image_entry* images = NULL;
					if( read_softlist_entry(soft_nodeset->nodeTab[i], &images, actual_driv_inf) == 0 )
						continue;
						
					snprintf(de->postfix, sizeof(de->postfix), "%ssfw%05d", initial_postfix, software_count);

					de->images = images;

					execute_mame2(de);

					free_image_entries(images);
					de->images = NULL;
					
					snprintf(de->postfix, sizeof(de->postfix), "%s", initial_postfix);
					
					software_count++;
				}
				
				xmlXPathFreeNodeSet(soft_nodeset);
				soft_nodeset = NULL;
			}
			
			xmlFreeDoc(soft_doc);
			soft_doc = NULL;
			
			if( software_count == 0 )
				printf("could not find any software entry to use\n");
		}
		else {
			fprintf(stderr, "could not read '%s'\n", driver_softlist_file);
		}

		free(driver_softlist_file);
		driver_softlist_file = NULL;
	}

	if( config_use_devices && actual_driv_inf->devices ) {
		char* device_file = NULL;
		if( config_global_device_file && (*config_global_device_file != 0) )
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
				char initial_postfix[1024];
				snprintf(initial_postfix, sizeof(initial_postfix), "%s", de->postfix);

				xmlNodePtr device_root = xmlDocGetRootElement(device_doc);
				xmlNodePtr device_node = device_root->children;
				while( device_node ) {
					if( device_node->type == XML_ELEMENT_NODE ) {
						struct image_entry* images = NULL;
						read_image_entries(device_node, &images);

						snprintf(de->postfix, sizeof(de->postfix), "%ssfw%05d", initial_postfix, software_count);

						de->images = images;

						execute_mame2(de);

						free_image_entries(images);
						de->images = NULL;
						
						snprintf(de->postfix, sizeof(de->postfix), "%s", initial_postfix);
						
						software_count++;
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
}

static void process_driver_info_list(struct driver_info* driv_inf)
{
	if( driv_inf == NULL )
		return;

	struct driver_info* actual_driv_inf = driv_inf;
	for(;;)
	{
		struct driver_entry de;
		memset(&de, 0x00, sizeof(struct driver_entry));
		de.name = (const char*)actual_driv_inf->name;
		de.sourcefile = (const char*)actual_driv_inf->sourcefile;
		de.autosave = actual_driv_inf->savestate;
		de.device_mandatory = (const char*)actual_driv_inf->device_mandatory;

		/* create softlist output */
		if( config_use_softwarelist && config_hashpath_folder && actual_driv_inf->has_softlist ) {
			char* driver_softlist = NULL;
			append_string(&driver_softlist, (const char*)actual_driv_inf->name);
			append_string(&driver_softlist, "_listsoftware");
			
			if( config_hack_softwarelist ) {
				char* softlist = NULL;
				append_string(&softlist, config_hashpath_folder);
				append_string(&softlist, FILESLASH);
				append_string(&softlist, (const char*)actual_driv_inf->softwarelist);
				append_string(&softlist, ".xml");
				
				char* driver_softlist_path = NULL;
				append_string(&driver_softlist_path, config_output_folder);
				append_string(&driver_softlist_path, FILESLASH);
				append_string(&driver_softlist_path, driver_softlist);
				append_string(&driver_softlist_path, ".xml");
				
				copy_file(softlist, driver_softlist_path);
				
				free(driver_softlist_path);
				driver_softlist_path = NULL;				
				free(softlist);
				softlist = NULL;
			}
			else {
				char* stdout_str = NULL;
				
				char* mame_call = NULL;					
				append_string(&mame_call, "-hashpath ");
				append_string(&mame_call, config_hashpath_folder);
				append_string(&mame_call, " ");
				append_string(&mame_call, (const char*)actual_driv_inf->name);
				append_string(&mame_call, " -listsoftware");
				
				if( config_verbose )
					printf("writing -listsoftware %s output\n", (const char*)actual_driv_inf->name);
				execute_mame(NULL, mame_call, 0, 0, NULL, NULL, &stdout_str);
				
				free(mame_call);
				mame_call = NULL;

				char* out_file = NULL;
				append_string(&out_file, config_output_folder);
				append_string(&out_file, FILESLASH);
				append_string(&out_file, driver_softlist);
				append_string(&out_file, ".xml");
				
				/* TODO: errorhandling */
				FILE* f = fopen(out_file, "w+b");
				if( f ) {
					fwrite(stdout_str, 1, strlen(stdout_str), f);
					fclose(f);
					f = NULL;
				}
				
				free(out_file);
				out_file = NULL;
				
				free(stdout_str);
				stdout_str = NULL;
			}
	
			free(driver_softlist);
			driver_softlist = NULL;
		}

		/* only run with the default options when no additional bioses or ramsizes are available to avoid duplicate runs */
		if( actual_driv_inf->bios_count <= 1 &&
			actual_driv_inf->ram_count <= 1 ) {
			execute_mame3(&de, actual_driv_inf);
		}

		if( actual_driv_inf->bios_count > 1 ) {
			int i = 0;
			for( ; i < actual_driv_inf->bios_count; ++i )
			{
				snprintf(de.postfix, sizeof(de.postfix), "bios%05d", i);
				
				de.bios = (const char*)actual_driv_inf->bioses[i];
				
				execute_mame3(&de, actual_driv_inf);
			}

			de.bios = NULL;
		}

		if( actual_driv_inf->ram_count > 1 ) {
			int i = 0;
			for( ; i < actual_driv_inf->ram_count; ++i )
			{
				snprintf(de.postfix, sizeof(de.postfix), "ram%05d", i);

				de.ramsize = actual_driv_inf->ramsizes[i];

				execute_mame3(&de, actual_driv_inf);
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
					
					execute_mame3(&de, actual_driv_inf);
					
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
					
					execute_mame3(&de, actual_driv_inf);
					
					confsetting = confsetting->next;
				};

				configuration = configuration->next;
			};
			
			de.configuration = NULL;
			de.confsetting = NULL;
		}

		if( actual_driv_inf->bios_count > 0 &&
			actual_driv_inf->ram_count > 1 ) {
			int bios_i = 0;
			for( ; bios_i < actual_driv_inf->bios_count; ++bios_i )
			{
				de.bios = (const char*)actual_driv_inf->bioses[bios_i];

				int ram_i = 0;
				for( ; ram_i < actual_driv_inf->ram_count; ++ram_i )
				{
					/* do not check again - already done in the -bios runs */
					if( actual_driv_inf->ramsizes[ram_i] == actual_driv_inf->ram_default )
						continue;
				
					snprintf(de.postfix, sizeof(de.postfix), "bios%05dram%05d", bios_i, ram_i);

					de.ramsize = actual_driv_inf->ramsizes[ram_i];

					execute_mame3(&de, actual_driv_inf);
				}
			}
			
			de.bios = NULL;
			de.ramsize = 0;
		}
		
		if( actual_driv_inf->slots ) {
			int slot_count = -1;
			struct slot_info* slot = actual_driv_inf->slots;
			while( slot != NULL ) {
				++slot_count;
				de.slot = slot;

				int slotoption_count = 0;
				for( ; slotoption_count < slot->slotoption_count; ++slotoption_count )
				{
					snprintf(de.postfix, sizeof(de.postfix), "slt%05dopt%05d", slot_count, slotoption_count);

					de.slotoption = (const char*)slot->slotoptions[slotoption_count];
					
					execute_mame3(&de, actual_driv_inf);
				}

				slot = slot->next;
			};
			
			de.slot = NULL;
			de.slotoption = NULL;
		}
		
		if( actual_driv_inf->next )
			actual_driv_inf = actual_driv_inf->next;
		else
			break;
	}
}

static int parse_listxml_element_cfg(xmlNodePtr game_children, struct driver_info** new_driv_inf, struct dipswitch_info** last_dip_info, int type)
{
	const xmlChar* cfg_xml_type_1 = NULL;
	const xmlChar* cfg_xml_type_2 = NULL;
	
	if( type == CFG_DIP ) {
		cfg_xml_type_1 = (const xmlChar*)"dipswitch";
		cfg_xml_type_2 = (const xmlChar*)"dipvalue";
	}
	else if( type == CFG_CONF ) {
		cfg_xml_type_1 = (const xmlChar*)"configuration";
		cfg_xml_type_2 = (const xmlChar*)"confsetting";
	}
	
	if( xmlStrcmp(game_children->name, cfg_xml_type_1) == 0 ) {
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
			if( xmlStrcmp(dipswitch_children->name, cfg_xml_type_2) == 0 ) {
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
						
						xmlFree(dipvalue_value);
						dipvalue_value = NULL;
					}
					
					xmlFree(dipvalue_default);
				}
			}
			
			dipswitch_children = dipswitch_children->next;
		}
		
		if( type == CFG_DIP ) {
			if( (*new_driv_inf)->dipswitches == NULL)
				(*new_driv_inf)->dipswitches = new_dip_info;
		}
		else if( type == CFG_CONF )
		{
			if( (*new_driv_inf)->configurations == NULL)
				(*new_driv_inf)->configurations = new_dip_info;
		}
		
		if( *last_dip_info )
			(*last_dip_info)->next = new_dip_info;
		*last_dip_info = new_dip_info;
		
		xmlFree(dip_mask);
		dip_mask = NULL;
		
		return 1;
	}
	
	return 0;
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
				fprintf(stderr, "'sourcefile' attribute is empty\n");
				return;
			}
		}

		*new_driv_inf = (struct driver_info*)malloc(sizeof(struct driver_info));
		if( !*new_driv_inf ) {
			fprintf(stderr, "could not allocate driver_info\n");
			return;
		}
		memset(*new_driv_inf, 0x00, sizeof(struct driver_info));

		(*new_driv_inf)->name = xmlGetProp(game_child, (const xmlChar*)"name");
		(*new_driv_inf)->sourcefile = sourcefile;
		
		if( config_verbose )
			printf("parsing '%s'\n", (*new_driv_inf)->name);
		
		struct device_info* last_dev_info = NULL;
		struct dipswitch_info* last_dip_info = NULL;
		struct dipswitch_info* last_cfg_info = NULL;
		struct slot_info* last_slot_info = NULL;

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
				goto next;
			}

			if( config_use_bios ) {
				if( xmlStrcmp(game_children->name, (const xmlChar*)"biosset") == 0 ) {
					xmlChar* bios_content = xmlGetProp(game_children, (const xmlChar*)"name");
					if( bios_content ) {
						(*new_driv_inf)->bioses[(*new_driv_inf)->bios_count++] = bios_content;
						{
							int i = 0;
							for(; i < (*new_driv_inf)->bios_count-1; ++i)
							{
								if( xmlStrcmp((*new_driv_inf)->bioses[i], bios_content) == 0 )
									printf("%s - %s - duplicated bios '%s' found\n", (const char*)(*new_driv_inf)->sourcefile, (const char*)(*new_driv_inf)->name, (const char*)bios_content);
							}
						}
					}

					xmlChar* bios_default = xmlGetProp(game_children, (const xmlChar*)"default");
					if( bios_default ) {
						if( xmlStrcmp(bios_default, (const xmlChar*)"1") == 0 )
							(*new_driv_inf)->bios_default = (*new_driv_inf)->bios_count;
						xmlFree(bios_default);
					}
					goto next;
				}
			}
			
			if( config_use_dipswitches ) {
				if( parse_listxml_element_cfg(game_children, new_driv_inf, &last_dip_info, CFG_DIP) == 1 )
					goto next;
			}

			if( config_use_configurations ) {
				if( parse_listxml_element_cfg(game_children, new_driv_inf, &last_cfg_info, CFG_CONF) == 1 )
					goto next;
			}

			if( app_type == APP_MESS ) {
				if( config_use_ramsize && (xmlStrcmp(game_children->name, (const xmlChar*)"ramoption") == 0) ) {
					xmlChar* ram_content = xmlNodeGetContent(game_children);
					if( ram_content ) {
						(*new_driv_inf)->ramsizes[(*new_driv_inf)->ram_count++] = atoi((const char*)ram_content);
						{
							int i = 0;
							for(; i < (*new_driv_inf)->ram_count-1; ++i)
							{
								if( (*new_driv_inf)->ramsizes[i] == (*new_driv_inf)->ramsizes[(*new_driv_inf)->ram_count] )
									printf("%s - duplicated ramsize '%d' found\n", (const char*)(*new_driv_inf)->name, (*new_driv_inf)->ramsizes[i]);
							}
						}
						xmlFree(ram_content);
					}
					xmlChar* ram_default = xmlGetProp(game_children, (const xmlChar*)"default");
					if( ram_default ) {
						if( xmlStrcmp(ram_default, (const xmlChar*)"1") == 0 )
							(*new_driv_inf)->ram_default = (*new_driv_inf)->ram_count;
						xmlFree(ram_default);
					}
					goto next;
				}
				
				if( (config_use_devices || config_use_softwarelist) && xmlStrcmp(game_children->name, (const xmlChar*)"device") == 0 ) {
					struct device_info* new_dev_info = (struct device_info*)malloc(sizeof(struct device_info));
					/* TODO: check allocation */
					memset(new_dev_info, 0x00, sizeof(struct device_info));

					xmlChar* dev_man = xmlGetProp(game_children, (const xmlChar*)"mandatory");
					if( dev_man ) {
						new_dev_info->mandatory = 1;
						xmlFree(dev_man);
					}

					xmlChar* dev_intf = xmlGetProp(game_children, (const xmlChar*)"interface");
					if( dev_intf )
						new_dev_info->interface = dev_intf;

					xmlNodePtr dev_childs = game_children->children;
					while( dev_childs != NULL ) {
						if( xmlStrcmp(dev_childs->name, (const xmlChar*)"instance") == 0 ) {
							xmlChar* dev_name = xmlGetProp(dev_childs, (const xmlChar*)"name");
							if( dev_name )
								new_dev_info->name = dev_name;

							xmlChar* dev_brief = xmlGetProp(dev_childs, (const xmlChar*)"briefname");
							if( dev_brief ) {
								new_dev_info->briefname = dev_brief;
								if( new_dev_info->mandatory )
									(*new_driv_inf)->device_mandatory = xmlStrdup(dev_brief);
							}
							break;
						}

						dev_childs = dev_childs->next;
					}
					
					if( (*new_driv_inf)->devices == NULL )
						(*new_driv_inf)->devices = new_dev_info;
					
					if( last_dev_info )
						last_dev_info->next = new_dev_info;
					last_dev_info = new_dev_info;
					
					goto next;
				}
				/* TODO: add support for multiple softwarelists */
				if( xmlStrcmp(game_children->name, (const xmlChar*)"softwarelist") == 0 ) {
					/* TODO: hack to avoid memory leaks */
					if( (*new_driv_inf)->has_softlist == 0 ) {
						(*new_driv_inf)->has_softlist = 1;
						(*new_driv_inf)->softwarelist = xmlGetProp(game_children, (const xmlChar*)"name");
						(*new_driv_inf)->softwarelist_filter = xmlGetProp(game_children, (const xmlChar*)"filter");
					}
					
					goto next;
				}
				
				if( config_use_slots && (xmlStrcmp(game_children->name, (const xmlChar*)"slot") == 0) ) {
					struct slot_info* new_slot_info = (struct slot_info*)malloc(sizeof(struct slot_info));
					/* TODO: check allocation */
					memset(new_slot_info, 0x00, sizeof(struct slot_info));
				
					xmlChar* slot_name = xmlGetProp(game_children, (const xmlChar*)"name");
					if( slot_name )
					{
						//printf("slot_name: %s\n", (const char*)slot_name);
						new_slot_info->name = slot_name;
					}
					
					xmlNodePtr slot_childs = game_children->children;
					while( slot_childs != NULL ) {
						xmlChar* slotoption_default = xmlGetProp(slot_childs, (const xmlChar*)"default");
						if( slotoption_default ) {
							//printf("slotoption_default: %s\n", (const char*)slotoption_default);
							int def = 0;
							if( xmlStrcmp(slotoption_default, (const xmlChar*)"yes") == 0 )
								def = 1;
							xmlFree(slotoption_default);
							if( def == 1)
							{
								slot_childs = slot_childs->next;
								continue;
							}
						}						
						
						xmlChar* slotoption_name = xmlGetProp(slot_childs, (const xmlChar*)"name");
						if( slotoption_name )
						{
							//printf("slotoption_name: %s\n", (const char*)slotoption_name);
							new_slot_info->slotoptions[new_slot_info->slotoption_count++] = slotoption_name;
						}
					
						slot_childs = slot_childs->next;
					}
					
					if( new_slot_info->slotoptions[0] == NULL ) {
						//printf("no non-default slotoptions\n");
						free(new_slot_info);
						new_slot_info = NULL;
						goto next;
					}
					
					if( (*new_driv_inf)->slots == NULL )
						(*new_driv_inf)->slots = new_slot_info;
					
					if( last_slot_info )
						last_slot_info->next = new_slot_info;
					last_slot_info = new_slot_info;
				
					goto next;
				}
			}

			next:
				game_children = game_children->next;
		}
	}
}

static void parse_listxml(const char* filename, struct driver_info** driv_inf)
{
	xmlDocPtr doc = xmlReadFile(filename, NULL, XML_PARSE_DTDVALID);
	if( doc ) {
		xmlNodePtr root = xmlDocGetRootElement(doc);
		if( root ) {
			if( xmlStrcmp(root->name, (const xmlChar*)"mame") == 0 )
				app_type = APP_MAME;
			else if( xmlStrcmp(root->name, (const xmlChar*)"mess") == 0 )
				app_type = APP_MESS;
			else
				fprintf(stderr, "Unknown -listxml output\n");

			if( app_type != APP_UNKNOWN ) {
				xmlChar* debug_attr = xmlGetProp(root, (const xmlChar*)"debug");
				if( xmlStrcmp(debug_attr, (const xmlChar*)"yes") == 0 )
					is_debug = 1;
				xmlFree(debug_attr);
				debug_attr = NULL;

				app_ver = xmlGetProp(root, (const xmlChar*)"build");
				if( app_ver )
					printf("build: %s%s\n", app_ver, is_debug ? " (debug)" : "");

				xmlChar* mameconfig_attr = xmlGetProp(root, (const xmlChar*)"mameconfig");
				if( mameconfig_attr )
					mameconfig_ver = atoi((const char*)mameconfig_attr);
				xmlFree(mameconfig_attr);
				mameconfig_attr = NULL;

				if( config_xpath_expr && (*config_xpath_expr != 0) ) {
					if( strstr(config_xpath_expr, xpath_placeholder) == NULL )
					{
						fprintf(stderr, "Invalid 'xpath_expr' - DRIVER_ROOT missing");
						return;
					}
				
					char* real_xpath_expr = NULL;
					if( app_type == APP_MAME )
						replace_string(config_xpath_expr, &real_xpath_expr, xpath_placeholder, "/mame/game");
					else if( app_type == APP_MESS )
						replace_string(config_xpath_expr, &real_xpath_expr, xpath_placeholder, "/mess/machine");
				
					printf("using XPath expression: %s\n", real_xpath_expr);

					xmlNodeSetPtr nodeset = get_xpath_nodeset(doc, (const xmlChar*)real_xpath_expr);
					if( nodeset )
					{
						printf("XPath found %d nodes\n", nodeset->nodeNr);

						xmlBufferPtr xmlBuf = NULL;
						if( config_print_xpath_results )
							xmlBuf = xmlBufferCreate(); /* TODO: check allocation */

						struct driver_info* last_driv_inf = NULL;

						int i = 0;
						for( ; i < nodeset->nodeNr; ++i )
						{
							if( config_print_xpath_results ) {
								xmlBufferAdd(xmlBuf, (const xmlChar*)"\t", 1);
								xmlNodeDump(xmlBuf, doc, nodeset->nodeTab[i], 0, 1);
								xmlBufferAdd(xmlBuf, (const xmlChar*)"\n", 1);
							}

							struct driver_info* new_driv_inf = NULL;

							parse_listxml_element(nodeset->nodeTab[i], &new_driv_inf);

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
						
						xmlXPathFreeNodeSet(nodeset);
						nodeset = NULL;
					}

					free(real_xpath_expr);
					real_xpath_expr = NULL;
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
			}
		}
		xmlFreeDoc(doc);
	}
	else {
		fprintf(stderr, "could not parse -listxml output\n");
	}
}

int main(int argc, char *argv[])
{
	printf("mame_regtest %s\n", VERSION);
	
	if( argc > 2) {
		printf("usage: mame_regtest <configname>\n");
		exit(1);
	}

	snprintf(pid_str, sizeof(pid_str), "%d", getpid());

	printf("\n");
	printf("process: %s\n", pid_str);

	if( getcwd(current_path, sizeof(current_path)) == NULL ) {
		fprintf(stderr, "could not get current working path\n");
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
	
	libxml2_init();

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

	if( !config_mame_exe || (*config_mame_exe == 0) ) {
		fprintf(stderr, "'executable' is empty or missing\n");
		cleanup_and_exit(1, "aborting");
	}

	if( access(config_mame_exe, F_OK ) == -1 ) {
		fprintf(stderr, "'%s' does not exist\n", config_mame_exe);
		cleanup_and_exit(1, "aborting");
	}
	if( config_verbose )
		printf("executable: %s\n", config_mame_exe);

	if( access(config_output_folder, F_OK) == 0 ) {
		if( !config_clear_output_folder ) {
			fprintf(stderr, "output folder '%s' already exists\n", config_output_folder);
			cleanup_and_exit(1, "aborting");
		}
	}
	else {
		if( mrt_mkdir(config_output_folder) != 0 ) {
			fprintf(stderr, "could not create folder '%s'\n", config_output_folder);
			cleanup_and_exit(1, "aborting");
		}
	}

	if( *config_output_folder != 0 ) {
		if( config_verbose )
			printf("using output folder: %s\n", config_output_folder);
	}

	append_string(&listxml_output, config_output_folder);
	append_string(&listxml_output, FILESLASH);
	append_string(&listxml_output, "listxml.xml");

	if( config_verbose ) {
		printf("str: %s\n", config_str_str);

		if( config_gamelist_xml_file && (*config_gamelist_xml_file != 0) ) {
			printf("using custom list: %s\n", config_gamelist_xml_file);
		}

		printf("autosave: %d\n", config_use_autosave);

		printf("ramsize: %d\n", config_use_ramsize);
	}

	if( config_write_mng ) {
		if( config_hack_mngwrite ) {
			if( mrt_mkdir("mng") != 0 ) {
				fprintf(stderr, "could not create folder 'mng' - disabling MNG writing\n");
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
		fprintf(stderr, "could not create folder '%s'\n", temp_folder);
		cleanup_and_exit(1, "aborting");
	}

	if( *temp_folder != 0 ) {
		if( config_verbose )
			printf("using output folder: %s\n", temp_folder);
		if( access(temp_folder, F_OK) != 0 ) {
			fprintf(stderr, "temp folder '%s' not found\n", temp_folder);
			cleanup_and_exit(1, "aborting");
		}
	}

#ifndef USE_MRT_SYSTEM
	append_string(&stdout_temp_file, temp_folder);
	append_string(&stdout_temp_file, FILESLASH);
	append_string(&stdout_temp_file, "tmp_stdout");

	append_string(&stderr_temp_file, temp_folder);
	append_string(&stderr_temp_file, FILESLASH);
	append_string(&stderr_temp_file, "tmp_stderr");
#endif

	if( config_verbose )
		printf("valgrind: %d\n", config_use_valgrind);

	if( config_use_valgrind )
	{
#if USE_VALGRIND
		if( !config_valgrind_binary || (*config_valgrind_binary == 0) )
			append_string(&config_valgrind_binary, "valgrind");
		if( config_verbose )
			printf("valgrind_binary: %s\n", config_valgrind_binary);
		if( !config_valgrind_parameters || (*config_valgrind_parameters == 0) )
			append_string(&config_valgrind_parameters, "--tool=memcheck --error-limit=no --leak-check=full --num-callers=50 --show-reachable=yes --track-fds=yes --track-origins=yes");
		if( config_verbose )
			printf("valgrind_parameters: %s\n", config_valgrind_parameters);
#else
		fprintf(stderr, "valgrind support not available on this platform");
#endif
	}

	if( config_verbose ) {
		if( config_rompath_folder && (*config_rompath_folder != 0) )
			printf("using rompath folder: %s\n", config_rompath_folder);

		printf("bios: %d\n", config_use_bios);
		printf("sound: %d\n", config_use_sound);
		printf("throttle: %d\n", config_use_throttle);
		printf("debug: %d\n", config_use_debug);
		printf("xpath_expr: %s\n", config_xpath_expr ? config_xpath_expr : "");
		printf("use_devices: %d\n", config_use_devices);
		printf("use_nonrunnable: %d\n", config_use_nonrunnable);
		if( config_global_device_file && (*config_global_device_file != 0) )
			printf("using device_file: %s\n", config_global_device_file);
		printf("use_isbios: %d\n", config_use_isbios);
		printf("store_output: %d\n", config_store_output);
		printf("clear_output_folder: %d\n", config_clear_output_folder);
		if( config_additional_options )
			printf("additional_options: %s\n", config_additional_options);
		printf("skip_mandatory: %d\n", config_skip_mandatory);
		printf("osdprocessors: %d\n", config_osdprocessors);
		printf("print_xpath_results: %d\n", config_print_xpath_results);
		printf("test_softreset: %d\n", config_test_softreset);
		printf("write_avi: %d\n", config_write_avi);
		printf("verbose: %d\n", config_verbose);
		printf("use_gdb: %d\n", config_use_gdb);
		printf("write_wav: %d\n", config_write_wav);
		printf("use_dipswitches: %d\n", config_use_dipswitches);
		printf("use_configurations: %d\n", config_use_configurations);
		if( config_hashpath_folder && (*config_hashpath_folder != 0) )
			printf("using hashpath folder: %s\n", config_hashpath_folder);
		printf("use_softwarelist: %d\n", config_use_softwarelist);
		printf("test_frontend: %d\n", config_test_frontend);
		printf("use_slots: %d\n", config_use_slots);

		printf("hack_ftr: %d\n", config_hack_ftr);
		printf("hack_biospath: %d\n", config_hack_biospath);
		printf("hack_mngwrite: %d\n", config_hack_mngwrite);
		printf("hack_pinmame: %d\n", config_hack_pinmame);
		printf("hack_softwarelist: %d\n", config_hack_softwarelist);

		printf("\n"); /* for output formating */
	}

	if( *config_str_str == 0 ) {
		fprintf(stderr, "'str' value cannot be empty\n");
		cleanup_and_exit(1, "aborting");		
	}

	if( config_hack_ftr && (atoi(config_str_str) < 2)) {
		fprintf(stderr, "'str' value has to be at least '2' when used with 'hack_ftr'\n");
		cleanup_and_exit(1, "aborting");
	}
	
	if( config_use_valgrind && config_use_gdb ) {
		fprintf(stderr, "'use_valgrind' and 'use_gdb' cannot be used together\n");
		cleanup_and_exit(1, "aborting");	
	}
	
	if( config_use_softwarelist && (!config_hashpath_folder || (*config_hashpath_folder == 0)) )
		fprintf(stderr, "'hashpath' is empty - no software lists available for testing\n");

	if( config_clear_output_folder ) {
		printf("clearing existing output folder\n");
		clear_directory(config_output_folder, 0);
	}

	printf("clearing existing temp folder\n");
	clear_directory(temp_folder, 0);

	printf("\n"); /* for output formating */
	
	if( config_test_frontend ) {
		printf("testing frontend options\n");
		static const char* const frontend_opts[] = { "validate", "listfull", "listclones", "listbrothers", "listcrc", "listroms", "listsamples", "verifyroms", "verifysamples", "verifysoftware", "listdevices", "listslots", "listmedia", "listsoftware", "showusage", "showconfig" };
		unsigned int i = 0;
		for(; i < sizeof(frontend_opts) / sizeof(char*); ++i)
		{
			/* TODO: disable again when frontends write output XMLs */
			/*if( config_verbose )*/
				printf("testing frontend option -%s\n", frontend_opts[i]);
		
			char* stdout_str = NULL;
		
			char* cmd = NULL;
			append_string(&cmd, "-");
			append_string(&cmd, frontend_opts[i]);

			execute_mame(NULL, cmd, 0, 0, NULL, NULL, &stdout_str);
			
			free(cmd);
			cmd = NULL;
			
			char* out_file = NULL;
			append_string(&out_file, config_output_folder);
			append_string(&out_file, FILESLASH);
			append_string(&out_file, frontend_opts[i]);
			append_string(&out_file, ".txt");
			
			/* TODO: errorhandling */
			FILE* f = fopen(out_file, "w+b");
			if( f ) {
				fwrite(stdout_str, 1, strlen(stdout_str), f);
				fclose(f);
				f = NULL;
			}
			
			free(out_file);
			out_file = NULL;

			free(stdout_str);
			stdout_str = NULL;
		}
	}
	
	printf("\n"); /* for output formating */

	if( !config_gamelist_xml_file || (*config_gamelist_xml_file == 0) ) {
		append_string(&config_gamelist_xml_file, listxml_output); 

		/* TODO: merge this with frontend code? */
		printf("writing -listxml output\n");
		char* stdout_str = NULL;
		
		char* mame_call = NULL;
		append_string(&mame_call, "-listxml");

		int listxml_res = execute_mame(NULL, mame_call, 0, 0, NULL, NULL, &stdout_str);

		free(mame_call);
		mame_call = NULL;
		
		/* TODO: errorhandling */
		FILE* f = fopen(config_gamelist_xml_file, "w+b");
		if( f ) {
			fwrite(stdout_str, 1, strlen(stdout_str), f);
			fclose(f);
			f = NULL;
		}

		free(stdout_str);
		stdout_str = NULL;
		
		if( listxml_res == 0)
		{
			fprintf(stderr, "-listxml writing failed\n");
			cleanup_and_exit(1, "aborting");
		}

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
	}

	printf("\n"); /* for output formating */
	printf("parsing -listxml output\n")	;
	parse_listxml(config_gamelist_xml_file, &global_driv_inf);

	printf("\n"); /* for output formating */
	
	if( global_driv_inf == NULL )
		cleanup_and_exit(0, "finished");

	printf("clearing existing dummy directory\n");
	clear_directory(dummy_root, 1);

	if( config_use_debug ) {
		append_string(&debugscript_file, temp_folder);
		append_string(&debugscript_file, FILESLASH);
		append_string(&debugscript_file, "mrt_debugscript");

		FILE* debugscript_fd = fopen(debugscript_file, "w");
		if( !debugscript_fd ) {
			fprintf(stderr, "could not open %s\n", debugscript_file);
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
			fprintf(stderr, "'test_softreset' can only be used with 'use_debug'\n");
	}

	/* setup OSDPROCESSORS */
	char osdprocessors_tmp[128];
	snprintf(osdprocessors_tmp, sizeof(osdprocessors_tmp), "OSDPROCESSORS=%d", config_osdprocessors);
	putenv(osdprocessors_tmp);

	printf("\n");
	process_driver_info_list(global_driv_inf);	
	printf("\n"); /* for output formating */

	cleanup_and_exit(0, "finished");
#ifndef _MSC_VER
	return 0; /* to shut up compiler */
#endif
}
