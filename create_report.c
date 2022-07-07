#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef __GNUC__
#include <unistd.h>
#endif

#ifdef _MSC_VER
#include <io.h>
#if _MSC_VER >= 1400
#undef strdup
#define strdup _strdup
#undef access
#define access _access
#endif
#define F_OK 00
#endif

/* libxml2 */
#include <libxml/entities.h>
#include <libxml/globals.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlstring.h>
#include <libxml/xpath.h>

/* local */
#include "common.h"
#include "config.h"

#define VERSION "0.75"

/*
	TODO:
		- errorhandling for _key/devices entries
		- sort group_data alphabetically
*/

struct driver_data
{
	char* filename;
	struct driver_data* next;
};

struct group_data
{
	xmlChar* srcfile;
	struct driver_data* drivers;
	struct group_data* next;	
};

struct build_group_cb_data
{
	struct group_data** gd_list;
};

static void build_group_cb(struct parse_callback_data* pcd)
{
	if( pcd->type == ENTRY_FILE ) {
		if( strstr(pcd->entry_name, ".xml") && strstr(pcd->entry_name, "listxml.xml") == NULL && strstr(pcd->entry_name, "_listsoftware.xml") == NULL )
		{
			printf("%s\n", pcd->entry_name);
			xmlDocPtr doc = NULL;
			{
				char* filecontent = NULL;
				int read_res = read_file(pcd->fullname, &filecontent);
				if( read_res == 0 )
				{
					const size_t len = strlen(filecontent);
					filter_unprintable(filecontent, len);
					doc = xmlReadMemory(filecontent, (int)len, NULL, "UTF-8", 0);
					free(filecontent);
				}
				else
				{
					printf("could not read file '%s' (%s)\n", pcd->fullname, strerror(read_res));
				}
			}
			if( doc ) {
				struct build_group_cb_data* bg_cb_data = (struct build_group_cb_data*)pcd->user_data;
				struct group_data** gd_list = bg_cb_data->gd_list;

				struct driver_data* new_dd = (struct driver_data*)malloc(sizeof(struct driver_data));
				/* TODO: check result */
				new_dd->filename = strdup(pcd->fullname);
				/* TODO: check result */
				new_dd->next = NULL;

				xmlNodePtr output_node = doc->children;

				xmlChar* sourcefile_key = xmlGetProp(output_node, (const xmlChar*)"sourcefile");
				
				int found_srcfile = 0;
				struct group_data* gd_entry = *gd_list;
				struct group_data* last_gd_entry = NULL;
				while( gd_entry != NULL ) {
					last_gd_entry = gd_entry;
					if( xmlStrcmp(sourcefile_key, gd_entry->srcfile) == 0 ) {
						found_srcfile = 1;
						break;
					}
					gd_entry = gd_entry->next;
				}
				
				if( !found_srcfile ) {
					struct group_data* new_gd_entry = (struct group_data*)malloc(sizeof(struct group_data));
					/* TODO: check result */
					new_gd_entry->srcfile = sourcefile_key;
					new_gd_entry->drivers = new_dd;
					new_gd_entry->next = NULL;

					if( !*gd_list )
						*gd_list = new_gd_entry;
					else
						last_gd_entry->next = new_gd_entry;
				}
				else {
					xmlFree(sourcefile_key);

					struct driver_data* dd_entry = last_gd_entry->drivers;
					struct driver_data* last_dd_entry = NULL;
					while( dd_entry != NULL ) {
						last_dd_entry = dd_entry;
						dd_entry = dd_entry->next;
					}
				
					if( !last_gd_entry->drivers )
						last_gd_entry->drivers = new_dd;
					else
						last_dd_entry->next = new_dd;
				}
				
				xmlFreeDoc(doc);
			}
		}
	}	
}

static void get_group_data(const char* dirname, int recursive, struct group_data** gd)
{	
	struct build_group_cb_data bg_cb_data;
	bg_cb_data.gd_list = gd;

	parse_directory(dirname, recursive, build_group_cb, (void*)&bg_cb_data);	
}

static void free_group_data(struct group_data* gd)
{
	struct group_data* gd_entry = gd;
	while( gd_entry ) {
		struct driver_data* dd_entry = gd_entry->drivers;
		while( dd_entry ) {
			struct driver_data* tmp_dd_entry = dd_entry;
			dd_entry = dd_entry->next;

			free(tmp_dd_entry->filename);
			tmp_dd_entry->filename = NULL;
			free(tmp_dd_entry);
		}
		
		struct group_data* tmp_gd_entry = gd_entry;
		gd_entry = gd_entry->next;

		xmlFree(tmp_gd_entry->srcfile);
		tmp_gd_entry->srcfile = NULL;
		free(tmp_gd_entry);
	}
}

static char* config_xml_folder = NULL;
static char* config_output_file = NULL;
static int config_print_stderr = 0;
static int config_use_markdown = 0;
static int config_show_memleaks = 0;
static int config_show_clipped = 0;
static int config_group_data = 0;
static int config_report_type = 0;
static char* config_compare_folder = NULL;
static int config_print_stdout = 0;
static char* config_output_folder = NULL;
static int config_recursive = 0;
static int config_speed_threshold = 0;
static int config_unexpected_stderr = 0;

static struct config_entry report_config[] =
{
	{ "xml_folder", 		CFG_STR_PTR, 	&config_xml_folder },
	{ "output_file", 		CFG_STR_PTR, 	&config_output_file },
	{ "print_stderr", 		CFG_INT, 		&config_print_stderr },
	{ "use_markdown", 		CFG_INT, 		&config_use_markdown },
	{ "show_memleaks", 		CFG_INT, 		&config_show_memleaks },
	{ "show_clipped", 		CFG_INT, 		&config_show_clipped },
	{ "group_data", 		CFG_INT, 		&config_group_data },
	{ "report_type",		CFG_INT,		&config_report_type },
	{ "compare_folder",		CFG_STR_PTR,	&config_compare_folder }, /* comparison report specific */
	{ "print_stdout", 		CFG_INT, 		&config_print_stdout },
	{ "output_folder", 		CFG_STR_PTR, 	&config_output_folder }, /* comparison report specific */
	{ "recursive", 			CFG_INT, 		&config_recursive },
	{ "speed_threshold", 	CFG_INT, 		&config_speed_threshold },
	{ "unexpected_stderr", 	CFG_INT, 		&config_unexpected_stderr },
	{ NULL, 				CFG_UNK, 		NULL }
};

struct report_summary
{
	int executed;
	int executed_autosave;
	int errors;
	int runtime_errors;
	int memleaks;
	int clipped;
	int mandatory;
	int missing;
	int unexpected_stderr;
};

static void summary_incr(struct report_summary* summary, const xmlChar* exitcode_key)
{
	if( xmlStrcmp(exitcode_key, (const xmlChar*)"0") == 0 )
		return;

	if( xmlStrcmp(exitcode_key, (const xmlChar*)"2") == 0 )
		summary->missing++;
	else if( xmlStrcmp(exitcode_key, (const xmlChar*)"4") == 0 )
		summary->mandatory++;
	else
		summary->errors++;
}

struct report_cb_data
{
	FILE* report_fd;
	int print_stderr;
	int use_markdown;
	int show_memleaks;
	int show_clipped;
	int report_type;
	const char* compare_folder;
	int print_stdout;
	struct report_summary summary;
	const char* output_folder;
	int speed_threshold;
	int unexpected_stderr;
};

/* TODO - handle multiple occurances */
static xmlChar* get_attribute_by_xpath(xmlXPathContextPtr xpathCtx, const xmlChar* xpath_expr, const xmlChar* attr_name)
{
	xmlChar* attr_value = NULL;

	xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpath_expr, xpathCtx);
	if( xpathObj && xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0 ) {
		attr_value = xmlGetProp(xpathObj->nodesetval->nodeTab[0], attr_name);
	}
	if( xpathObj )
		xmlXPathFreeObject(xpathObj);

	return attr_value;
}

/* TODO: handle this differently - casts away constness */
static void convert_br(xmlChar* str)
{
	xmlChar* s = NULL;
	while( (s = BAD_CAST xmlStrstr(str, (const xmlChar*)"&#13;")) != NULL )
	{
		*s++ = '<';
		*s++ = 'b';
		*s++ = 'r';
		*s++ = '/';
		*s = '>';
	}
}

/* TODO: read part if available */
#define PRINT_INFO(fd) \
	fprintf(fd, "%s: %s", sourcefile_key, name_key); \
	if( bios_key ) \
		fprintf(fd, " (bios %s)", bios_key); \
	if( ramsize_key ) \
		fprintf(fd, " (ramsize %s)", ramsize_key); \
	if( dipswitch_key && dipvalue_key ) \
		fprintf(fd, " (dipswitch %s value %s)", dipswitch_key, dipvalue_key); \
	if( configuration_key && confsetting_key ) \
		fprintf(fd, " (configuration %s value %s)", configuration_key, confsetting_key); \
	if( slot_key && slotoption_key ) \
		fprintf(fd, " (slot %s option %s)", slot_key, slotoption_key); \
	if( devices_node ) \
	{ \
		xmlAttrPtr dev_attrs = devices_node->properties; \
		while( dev_attrs ) { \
			fprintf(fd, " (%s %s)", dev_attrs->name, dev_attrs->children->content); \
			 \
			dev_attrs = dev_attrs->next; \
		} \
	} \
	if( autosave_key ) \
		fprintf(fd, " (autosave)");
		
#define COMPARE_ATTRIBUTE(xpath_expr, attr_name, differs) \
{ \
	xmlChar* attr1 = get_attribute_by_xpath(xpathCtx1, (const xmlChar*)xpath_expr, (const xmlChar*)attr_name); \
	xmlChar* attr2 = get_attribute_by_xpath(xpathCtx2, (const xmlChar*)xpath_expr, (const xmlChar*)attr_name); \
	\
	if( xmlStrcmp(attr1, attr2) != 0 ) \
	{ \
		differs = 1; \
		if( write_set_data ) \
		{ \
			write_set_data = 0; \
			xmlChar* name = get_attribute_by_xpath(xpathCtx1, (const xmlChar*)"/output", (const xmlChar*)"name"); \
			xmlChar* srcfile = get_attribute_by_xpath(xpathCtx1, (const xmlChar*)"/output", (const xmlChar*)"sourcefile"); \
			\
			fprintf(r_cb_data->report_fd, "<p>\n"); \
			PRINT_INFO(r_cb_data->report_fd) \
			fprintf(r_cb_data->report_fd, "<br/>\n"); \
			\
			xmlFree(name); \
			name = NULL; \
			xmlFree(srcfile); \
			srcfile = NULL; \
			fprintf(r_cb_data->report_fd, "<table border=\"1\">\n"); \
		} \
		fprintf(r_cb_data->report_fd, "<tr>\n"); \
		\
		fprintf(r_cb_data->report_fd, "<td>%s</td>\n", attr_name);\
		\
		xmlChar* tmp = xmlEncodeEntitiesReentrant(doc_old, attr1); \
		convert_br(tmp); \
		fprintf(r_cb_data->report_fd, "<td valign=\"top\">%s</td>\n", tmp); \
		xmlFree(tmp); \
		tmp = NULL; \
		\
		tmp = xmlEncodeEntitiesReentrant(doc, attr2); \
		convert_br(tmp); \
		fprintf(r_cb_data->report_fd, "<td valign=\"top\">%s</td>\n", tmp); \
		xmlFree(tmp); \
		tmp = NULL; \
		fprintf(r_cb_data->report_fd, "</tr>\n"); \
	} \
	\
	xmlFree(attr2); \
	attr2 = NULL; \
	xmlFree(attr1); \
	attr1 = NULL; \
}

#define PREPARE_KEYS(output_node) \
	xmlChar* name_key = xmlGetProp(output_node, (const xmlChar*)"name"); \
	xmlChar* sourcefile_key = xmlGetProp(output_node, (const xmlChar*)"sourcefile"); \
	xmlChar* bios_key = xmlGetProp(output_node, (const xmlChar*)"bios"); \
	xmlChar* ramsize_key = xmlGetProp(output_node, (const xmlChar*)"ramsize"); \
	xmlChar* autosave_key = xmlGetProp(output_node, (const xmlChar*)"autosave"); \
	xmlChar* dipswitch_key = xmlGetProp(output_node, (const xmlChar*)"dipswitch"); \
	xmlChar* dipvalue_key = xmlGetProp(output_node, (const xmlChar*)"dipvalue"); \
	xmlChar* configuration_key = xmlGetProp(output_node, (const xmlChar*)"configuration"); \
	xmlChar* confsetting_key = xmlGetProp(output_node, (const xmlChar*)"confsetting"); \
	xmlChar* cmd_key = xmlGetProp(output_node, (const xmlChar*)"cmd"); \
	xmlChar* slot_key = xmlGetProp(output_node, (const xmlChar*)"slot"); \
	xmlChar* slotoption_key = xmlGetProp(output_node, (const xmlChar*)"slotoption"); \
	
#define FREE_KEYS \
	xmlFree(slotoption_key); \
	slotoption_key = NULL; \
	xmlFree(slot_key); \
	slot_key = NULL; \
	xmlFree(cmd_key); \
	cmd_key = NULL; \
	xmlFree(confsetting_key); \
	confsetting_key = NULL; \
	xmlFree(configuration_key); \
	configuration_key = NULL; \
	xmlFree(dipvalue_key); \
	dipvalue_key = NULL; \
	xmlFree(dipswitch_key); \
	dipswitch_key = NULL; \
	xmlFree(autosave_key); \
	autosave_key = NULL; \
	xmlFree(ramsize_key); \
	ramsize_key = NULL; \
	xmlFree(bios_key); \
	bios_key = NULL; \
	xmlFree(sourcefile_key); \
	sourcefile_key = NULL; \
	xmlFree(name_key); \
	name_key = NULL;
	
#if 0
static xmlChar const *
xmlStrrstr (xmlChar const *haystack, xmlChar const *needle)
{
	xmlChar const *p = haystack;
	xmlChar const *last_match = NULL;

	while ((p = xmlStrstr (p, needle)) != NULL)
	{
	  last_match = p;
	  p++;
	}

	return last_match;
}
#endif

static int create_report_from_filename(const char *const filename, struct report_cb_data* r_cb_data, int write_src_header)
{
	int data_written = 0;
	
	switch( r_cb_data->report_type )
	{
		default:
		case 0:
		{
			xmlDocPtr doc = NULL;
			{
				char* filecontent = NULL;
				int read_res = read_file(filename, &filecontent);
				if( read_res == 0 )
				{
					const size_t len = strlen(filecontent);
					filter_unprintable(filecontent, len);
					doc = xmlReadMemory(filecontent, (int)len, NULL, "UTF-8", 0);
					free(filecontent);
				}
				else
				{
					printf("could not read file '%s' (%s)\n", filename, strerror(read_res));
				}
			}
			if( doc ) {
				xmlNodePtr output_node = doc->children;				
				PREPARE_KEYS(output_node)

				int result_found = 0;
			
				xmlNodePtr output_childs = output_node->children;
				xmlNodePtr devices_node = NULL;
				while( output_childs ) {
					if( output_childs->type == XML_ELEMENT_NODE ) {
						if( xmlStrcmp(output_childs->name, (const xmlChar*)"result") == 0 ) {
							if (!result_found) {
								r_cb_data->summary.executed++;
								result_found = 1;
							}
							else {
								r_cb_data->summary.executed_autosave++;
							}


							xmlChar* exitcode_key = xmlGetProp(output_childs, (const xmlChar*)"exitcode");
							xmlChar* stderr_key = xmlGetProp(output_childs, (const xmlChar*)"stderr");
							xmlChar* stdout_key = xmlGetProp(output_childs, (const xmlChar*)"stdout");

							int error_found = exitcode_key && (xmlStrcmp(exitcode_key, (const xmlChar*)"0") != 0);
							int memleak_found = stderr_key && xmlStrstr(stderr_key, (const xmlChar*)"--- memory leak warning ---");
							int clipped_found = stdout_key && xmlStrstr(stdout_key, (const xmlChar*)"clipped") && (xmlStrstr(stdout_key, (const xmlChar*)" 0% samples clipped") == NULL);
							int mandatory_found = stderr_key && xmlStrstr(stderr_key, (const xmlChar*)"Driver requires");
							/* TODO: make it optional */
							int reset_scope_found = stderr_key && xmlStrstr(stderr_key, (const xmlChar*)"called within reset scope by");
							/* TODO: make it optional */
							int runtime_error_found = stderr_key && xmlStrstr(stderr_key, (const xmlChar*)"runtime error");
							int unexpected_stderr = 0;
							if (r_cb_data->unexpected_stderr && stderr_key != NULL)
							{
								/* TODO: make this generic for the reports and merge with speed code */
#ifdef WIN32
								const char* sep = "\r\n";
#else
								const char* sep = "\n";
#endif
								char** lines = split_string((const char*)stderr_key, sep);
								int i = 0;
								for( ; lines != NULL && lines[i] != NULL; ++i )
								{
									/*
									 	examples:
										datum.bin ROM NEEDS REDUMP
									 	865jaa02.chd CHD NEEDS REDUMP
									*/
									if( strstr(lines[i], " NEEDS REDUMP") != NULL )
										continue;

									/*
										examples:
										dolphin_moni.rom NOT FOUND (NO GOOD DUMP KNOWN) (tried in dauphin dauphin)
									 	bbh2sp_v2.02.09.chd NOT FOUND
									*/
									if( strstr(lines[i], " NOT FOUND") != NULL )
										continue;

									/*
										examples:
										sun-8212.ic3 NO GOOD DUMP KNOWN
									*/
									if( strstr(lines[i], " NO GOOD DUMP KNOWN") != NULL )
										continue;

									if( strstr(lines[i], "WARNING: the machine might not run correctly.") != NULL )
										continue;

									if( strstr(lines[i], "Fatal error: Required files are missing, the machine cannot be run.") != NULL )
										continue;

									/*
									 	will appear as the last stderror entry - do not treat it as unexpected

									 	examples:
										-----------------------------------------------------
										Exception at EIP=00000000059e1763 (video_manager::begin_recording_avi(char const*, unsigned int, screen_device*)+0x0043): ACCESS VIOLATION
										While attempting to read memory at 0000000000000000
										-----------------------------------------------------
									*/
									if( strstr(lines[i], "-----------------------------------------------------") != NULL )
										break;

                                    /*
                                         will appear as the last stderror entry - do not treat it as unexpected

                                         examples:
                                        =================================================================
										==21396==ERROR: AddressSanitizer: allocator is out of memory trying to allocate 0xf0d2000 bytes
                                    */
                                    if( strstr(lines[i], "=================================================================") != NULL )
                                        break;

									/* TODO: remove this after it is fixed */
									if( strstr(lines[i], "Value internal not supported for option debugger - falling back to auto") != NULL )
										continue;

									/* TODO: remove this after it is fixed */
									if( strstr(lines[i], "Error opening translation file English") != NULL )
										continue;

									unexpected_stderr = 1;
									break;
								}
								free_array(lines);
							}

							summary_incr(&r_cb_data->summary, exitcode_key);
							if(memleak_found)
								r_cb_data->summary.memleaks++;
							if(clipped_found)
								r_cb_data->summary.clipped++;
							if(runtime_error_found)
								r_cb_data->summary.runtime_errors++;
							if(unexpected_stderr)
								r_cb_data->summary.unexpected_stderr++;

							int report_error = (error_found && !mandatory_found);
							int report_memleak = (memleak_found && r_cb_data->show_memleaks);
							int report_stdout = (r_cb_data->print_stdout && stdout_key && xmlStrlen(stdout_key) > 0);
							int report_stderr = (r_cb_data->print_stderr && stderr_key && xmlStrlen(stderr_key) > 0);
							int report_clipped = (r_cb_data->show_clipped && clipped_found);
							
							if( report_error || report_memleak || report_stdout || report_stderr || report_clipped || reset_scope_found || runtime_error_found || unexpected_stderr ) {
								if( r_cb_data->use_markdown )
								{
									if( write_src_header ) {
										fprintf(r_cb_data->report_fd, "#### %s\n", sourcefile_key);
										/* disable so the header is only written once */
										write_src_header = 0;
									}
									fprintf(r_cb_data->report_fd, "##### %s\n", name_key);
									if( bios_key )
										fprintf(r_cb_data->report_fd, "  * BIOS: ''%s''\n", bios_key);
									if( ramsize_key )
										fprintf(r_cb_data->report_fd, "  * Ramsize: ''%s''\n", ramsize_key);
									if( dipswitch_key && dipvalue_key )
										fprintf(r_cb_data->report_fd, "  * Dipswitch: ''%s'' Value: ''%s'' \n", dipswitch_key, dipvalue_key);
									if( configuration_key && confsetting_key )
										fprintf(r_cb_data->report_fd, "  * Configuration: ''%s'' Value: ''%s'' \n", configuration_key, confsetting_key);
									if( slot_key && slotoption_key )
										fprintf(r_cb_data->report_fd, "  * Slot: ''%s'' Option: ''%s'' \n", slot_key, slotoption_key);
									if( devices_node )
									{
										xmlAttrPtr dev_attrs = devices_node->properties;
										while( dev_attrs ) {
											fprintf(r_cb_data->report_fd, "  * Device: ''%s %s''\n", dev_attrs->name, dev_attrs->children->content);
			
											dev_attrs = dev_attrs->next;
										}
									}
									if( autosave_key )
										fprintf(r_cb_data->report_fd, " (autosave)");
									fprintf(r_cb_data->report_fd, "  * Error code: `%s`\n", exitcode_key);
		
									if( (report_error || report_stdout || report_clipped) && stdout_key && xmlStrlen(stdout_key) > 0 )
									{
										fprintf(r_cb_data->report_fd, "\n```\n%s\n```\n", stdout_key);
									}
			
									if( (report_error || report_memleak || report_stderr || reset_scope_found || runtime_error_found || unexpected_stderr) && stderr_key && xmlStrlen(stderr_key) > 0 )
									{
										fprintf(r_cb_data->report_fd, "\n```\n%s\n```\n", stderr_key);
									}
									
									if( cmd_key )
									{
										fprintf(r_cb_data->report_fd, "  * Command-Line:\n```\n%s\n```\n", cmd_key);
									}
									
									fprintf(r_cb_data->report_fd, "\n");
								}
								else
								{
									PRINT_INFO(r_cb_data->report_fd)
									fprintf(r_cb_data->report_fd, " (%s)\n", exitcode_key);
		
									if( (report_error || report_stdout || report_clipped) && stdout_key && xmlStrlen(stdout_key) > 0 )
										fprintf(r_cb_data->report_fd, "%s\n", stdout_key);
			
									if( (report_error || report_memleak || report_stderr || reset_scope_found || runtime_error_found) && stderr_key && xmlStrlen(stderr_key) > 0 )
										fprintf(r_cb_data->report_fd, "%s\n", stderr_key);
										
									if( cmd_key )
										fprintf(r_cb_data->report_fd, "%s\n", cmd_key);
								}
			
								data_written = 1;
							}
							xmlFree(stdout_key);
							xmlFree(stderr_key);
							xmlFree(exitcode_key);
						}
						// TODO: set this up differently
						else if( xmlStrcmp(output_childs->name, (const xmlChar*)"devices") == 0 )
						{
							devices_node = output_childs;
						}
					}
					output_childs = output_childs->next;
				}
				
				FREE_KEYS
		
				xmlFreeDoc(doc);
			}
		}
		break;
		
		case 1:
		{
			char* entry_name = get_filename(filename);
			char* entry_directory = get_directory(filename);
			char* old_path = NULL;
			append_string(&old_path, r_cb_data->compare_folder);
			append_string(&old_path, FILESLASH);
			append_string(&old_path, entry_name);
			if( access(old_path, F_OK) == 0 ) {
				int write_set_data = 1;
				int dummy = 0;
				int png_differs = 0;

				xmlDocPtr doc_old = xmlReadFile(old_path, NULL, 0);
				xmlDocPtr doc = xmlReadFile(filename, NULL, 0);

				xmlXPathContextPtr xpathCtx1 = xmlXPathNewContext(doc_old);
				xmlXPathContextPtr xpathCtx2 = xmlXPathNewContext(doc);
				
				xmlNodePtr output_node = doc->children;
				xmlNodePtr devices_node = NULL; // TODO
				PREPARE_KEYS(output_node)

				COMPARE_ATTRIBUTE("/output/result", "exitcode", dummy)
				COMPARE_ATTRIBUTE("/output/result", "stderr", dummy)
				COMPARE_ATTRIBUTE("/output/result/dir[@name='snap']//file", "png_crc", png_differs)
				(void)dummy;
				if(write_set_data == 0)
					fprintf(r_cb_data->report_fd, "</table>\n");
				
				if( png_differs ) {
					xmlChar* name = get_attribute_by_xpath(xpathCtx1, (const xmlChar*)"/output", (const xmlChar*)"name");
					xmlChar* srcfile = get_attribute_by_xpath(xpathCtx1, (const xmlChar*)"/output", (const xmlChar*)"sourcefile");
					char* entry_name_base = get_filename_base(entry_name);
					/* TODO: check result */

					char* png_path = NULL;
					append_string(&png_path, FILESLASH);
					append_string(&png_path, (const char*)entry_name_base);
					append_string(&png_path, FILESLASH);
					append_string(&png_path, "snap");
					append_string(&png_path, FILESLASH);
					append_string(&png_path, (const char*)name);
					append_string(&png_path, FILESLASH);
					append_string(&png_path, "final.png");
					
					char* path1 = NULL;
					append_string(&path1, r_cb_data->compare_folder);
					append_string(&path1, png_path);					

					char* path2 = NULL;
					append_string(&path2, entry_directory);
					append_string(&path2, png_path);
					
					char* outpath = NULL;
					append_string(&outpath, r_cb_data->output_folder);
					append_string(&outpath, FILESLASH);
					append_string(&outpath, entry_name_base);
					append_string(&outpath, "_diff.png");
					
					char* pngcmp_cmd = NULL;
					append_string(&pngcmp_cmd, "pngcmp");
					append_string(&pngcmp_cmd, " ");
					append_string(&pngcmp_cmd, path1);
					append_string(&pngcmp_cmd, " ");
					append_string(&pngcmp_cmd, path2);
					append_string(&pngcmp_cmd, " ");
					append_string(&pngcmp_cmd, outpath);
					
					// TODO: add all information about execution to report
					int pngcmp_res = system(pngcmp_cmd);
					if( pngcmp_res == 1 ) {
						char* outfile = get_filename(outpath);
						fprintf(r_cb_data->report_fd, "<img src=\"%s\" alt=\"%s\"/>\n", outfile, outfile);
						free(outfile);
					}
					
					free(pngcmp_cmd);
					pngcmp_cmd = NULL;
					free(outpath);
					outpath = NULL;
					free(path2);
					path2 = NULL;
					free(path1);
					path1 = NULL;
					free(entry_name_base);
					free(png_path);
					png_path = NULL;
					
					xmlFree(srcfile);
					xmlFree(name);
				}
				
				FREE_KEYS

				if( xpathCtx2 )
					xmlXPathFreeContext(xpathCtx2);
				if( xpathCtx1 )
					xmlXPathFreeContext(xpathCtx1);

				xmlFreeDoc(doc);
				xmlFreeDoc(doc_old);

				if( write_set_data == 0 )
					fprintf(r_cb_data->report_fd, "</p>\n");
			}
			else {
				fprintf(r_cb_data->report_fd, "<p>%s - new</p>\n", entry_name);
			}
			free(old_path);
			old_path = NULL;
			free(entry_directory);
			free(entry_name);
		}
		break;
		
		case 2:
		{
			char* entry_name = get_filename(filename);
			char* entry_directory = get_directory(filename);
			char* old_path = NULL;
			append_string(&old_path, r_cb_data->compare_folder);
			append_string(&old_path, FILESLASH);
			append_string(&old_path, entry_name);
			if( access(old_path, F_OK) == 0 ) {
				xmlDocPtr doc_old = xmlReadFile(old_path, NULL, 0);
				xmlDocPtr doc = xmlReadFile(filename, NULL, 0);

				xmlXPathContextPtr xpathCtx1 = xmlXPathNewContext(doc_old);
				xmlXPathContextPtr xpathCtx2 = xmlXPathNewContext(doc);
				
				xmlNodePtr output_node = doc->children;
				xmlNodePtr devices_node = NULL; // TODO
				PREPARE_KEYS(output_node)
				
				xmlChar* attr1 = get_attribute_by_xpath(xpathCtx1, (const xmlChar*)"/output/result", (const xmlChar*)"stdout");
				xmlChar* attr2 = get_attribute_by_xpath(xpathCtx2, (const xmlChar*)"/output/result", (const xmlChar*)"stdout");
				
				if( xmlStrcmp(attr1, attr2) != 0 )
				{
					//Average speed: 1055.38% (1 seconds)&#13;&#10;
					double speed1 = -1;
					double speed2 = -1;
					
					{
					char* pos1 = strstr((const char*)attr1, "Average speed:");
					char* pos2 = strstr((const char*)attr1, "% (");
					if( pos1 && pos2 )
					{
						pos1 += 14;
						char* temp = NULL;
						append_string_n(&temp, pos1, (size_t)(pos2-pos1));
						speed1 = atof(temp);
						free(temp);
						temp = NULL;
					}
					}
					{
					char* pos1 = strstr((const char*)attr2, "Average speed:");
					char* pos2 = strstr((const char*)attr2, "% (");
					if( pos1 && pos2 )
					{
						pos1 += 14;
						char* temp = NULL;
						append_string_n(&temp, pos1, (size_t)(pos2-pos1));
						speed2 = atof(temp);
						free(temp);
						temp = NULL;
					}
					}
					
					PRINT_INFO(r_cb_data->report_fd)
					fprintf(r_cb_data->report_fd, "|%.2f|%.2f|%.2f|%.2f\n", speed1, speed2, speed2 - speed1, (speed2 / speed1) * 100 - 100);
				}
				
				if( attr2 )
				{
					xmlFree(attr2);
				}
				if( attr1 )
				{
					xmlFree(attr1);
				}
				
				FREE_KEYS

				if( xpathCtx2 )
					xmlXPathFreeContext(xpathCtx2);
				if( xpathCtx1 )
					xmlXPathFreeContext(xpathCtx1);

				xmlFreeDoc(doc);
				xmlFreeDoc(doc_old);
			}
			else {
				fprintf(r_cb_data->report_fd, "%s - new\n", entry_name);
			}
			free(old_path);
			old_path = NULL;
			free(entry_directory);
			free(entry_name);
		}
		break;

		case 4:
		{
			xmlDocPtr doc = NULL;
			{
				char* filecontent = NULL;
				int read_res = read_file(filename, &filecontent);
				if( read_res == 0 )
				{
					const size_t len = strlen(filecontent);
					filter_unprintable(filecontent, len);
					doc = xmlReadMemory(filecontent, (int)len, NULL, "UTF-8", 0);
					free(filecontent);
				}
				else
				{
					printf("could not read file '%s' (%s)\n", filename, strerror(read_res));
				}

			}
			if( doc ) {
				xmlNodePtr output_node = doc->children;
				xmlNodePtr devices_node = NULL; // TODO				
				PREPARE_KEYS(output_node)
			
				xmlNodePtr output_childs = output_node->children;
				while( output_childs ) {
					if( output_childs->type == XML_ELEMENT_NODE ) {
						if( xmlStrcmp(output_childs->name, (const xmlChar*)"result") == 0 ) {
							xmlChar* exitcode_key = xmlGetProp(output_childs, (const xmlChar*)"exitcode");
							if (xmlStrcmp(exitcode_key, (const xmlChar*)"0") == 0) {
								xmlChar* stdout_key = xmlGetProp(output_childs, (const xmlChar*)"stdout");

								int found_speed = 0;
								/* TODO: use this code in speed comparison */
								/* TODO: make this generic for the reports and merge with other code */
	#ifdef WIN32
								const char* sep = "\r\n";
	#else
								const char* sep = "\n";
	#endif
								char** lines = split_string((const char*)stdout_key, sep);
								int i = 0;
								for( ; lines != NULL && lines[i] != NULL; ++i )
								{
									if( strstr(lines[i], "Average speed:") == NULL ) {
										continue;
									}
									found_speed = 1;
									float speed = 0;
									int seconds = 0;
									int sscanf_res = sscanf(lines[i], "Average speed: %f%% (%d seconds)", &speed, &seconds);
									if( sscanf_res != 2 ) {
										PRINT_INFO(stdout)
										printf(" - ERROR parsing '%s'\n", lines[i]);
										continue;
									}
									if( (int)speed < r_cb_data->speed_threshold ) {
										PRINT_INFO(stdout)
										printf(" - Average speed: %.2f%%\n", speed);
									}
								}

								if (!found_speed) {
									PRINT_INFO(stdout)
									printf(" - ERROR missing\n");
								}

								free_array(lines);

								xmlFree(stdout_key);
							}
							xmlFree(exitcode_key);
						}
						// TODO: set this up differently
						else if( xmlStrcmp(output_childs->name, (const xmlChar*)"devices") == 0 )
						{
							devices_node = output_childs;
						}
					}
					output_childs = output_childs->next;
				}
				
				FREE_KEYS
		
				xmlFreeDoc(doc);
			}
		}
		break;

		/* TODO: report unsupported report type */
	}
	
	return data_written;
}

static void create_report_from_group_data(struct group_data* gd, struct report_cb_data* r_cb_data)
{
	struct group_data* gd_entry = gd;
	while( gd_entry ) {
		int write_src_header = 1;
		
		struct driver_data* dd_entry = gd_entry->drivers;
		while( dd_entry ) {
			int data_written = create_report_from_filename(dd_entry->filename, r_cb_data, write_src_header);
			if( data_written )
				write_src_header = 0;
			dd_entry = dd_entry->next;
		}
		gd_entry = gd_entry->next;
	}
}

static void create_report_cb(struct parse_callback_data* pcd)
{
	if( pcd->type == ENTRY_FILE ) {		
		if( strstr(pcd->entry_name, ".xml") && strstr(pcd->entry_name, "listxml.xml") == NULL )
		{
			struct report_cb_data* r_cb_data = (struct report_cb_data*)pcd->user_data;

			printf("%s\n", pcd->entry_name);
			create_report_from_filename(pcd->fullname, r_cb_data, 1);
		}
	}
}

static int create_report(void)
{
	FILE* report_fd = NULL;

	struct report_cb_data r_cb_data;
	r_cb_data.print_stderr = config_print_stderr;
	r_cb_data.use_markdown = config_use_markdown;
	r_cb_data.show_memleaks = config_show_memleaks;
	r_cb_data.show_clipped = config_show_clipped;
	r_cb_data.report_type = config_report_type;
	r_cb_data.compare_folder = config_compare_folder;
	r_cb_data.print_stdout = config_print_stdout;
	r_cb_data.output_folder = config_output_folder;
	r_cb_data.speed_threshold = config_speed_threshold;
	r_cb_data.unexpected_stderr = config_unexpected_stderr;
	memset(&r_cb_data.summary, 0x00, sizeof(struct report_summary));

	if( config_report_type == 0 ) {
		report_fd = fopen(config_output_file, "wb");
		if (!report_fd) {
			printf("could not open file '%s' (%s) - aborting\n", config_output_file, strerror(errno));
			return 1;
		}
		if( config_use_markdown == 1 )
			fprintf(report_fd, "# mame_regtest result\n");
	}
	else if( config_report_type == 1 ) {
		char* outputfile = NULL;
		append_string(&outputfile, config_output_folder);
		append_string(&outputfile, FILESLASH);
		append_string(&outputfile, "mrt_diff.html");
		mrt_mkdir(config_output_folder);
		/* TODO: check result */
		report_fd = fopen(outputfile, "wb");
		if (!report_fd) {
			printf("could not open file '%s' (%s) - aborting\n", outputfile, strerror(errno));
			return 1;
		}
		free(outputfile);
		outputfile = NULL;
		
		fprintf(report_fd, "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n");
		fprintf(report_fd, "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\">\n");
		fprintf(report_fd, "<head>\n");
		fprintf(report_fd, "<title>mame_regtest comparison report</title>\n");
		fprintf(report_fd, "<meta http-equiv=\"Content-type\" content=\"text/html;charset=UTF-8\"/>\n");
		fprintf(report_fd, "</head>\n");
		fprintf(report_fd, "<body>\n");
	}
	else if ( config_report_type == 2 ) {
		report_fd = fopen(config_output_file, "wb");
		if (!report_fd) {
			printf("could not open file '%s' (%s) - aborting\n", config_output_file, strerror(errno));
			return 1;
		}
	}

	r_cb_data.report_fd = report_fd;
	
	char** folders = split_string(config_xml_folder, ";");
	int i;

	if( config_group_data ) {
		struct group_data* gd = NULL;
		printf("getting group data...\n");
		for(i = 0; folders[i] != NULL; ++i)
			get_group_data(folders[i], config_recursive, &gd);
	
		printf("creating report...\n");
		create_report_from_group_data(gd, &r_cb_data);

		free_group_data(gd);
	}
	else {
		printf("creating report...\n");
		for(i = 0; folders[i] != NULL; ++i)
			parse_directory(folders[i], config_recursive, create_report_cb, (void*)&r_cb_data);
	}

	free_array(folders);

	if( config_use_markdown && config_report_type == 0 ) {
		fprintf(report_fd, "## Summary\n");
		fprintf(report_fd, "  * %d executed\n", r_cb_data.summary.executed);
		fprintf(report_fd, "  * %d executed with autosave\n", r_cb_data.summary.executed_autosave);
		fprintf(report_fd, "  * %d with errors\n", r_cb_data.summary.errors);
		fprintf(report_fd, "  * %d with runtime errors\n", r_cb_data.summary.runtime_errors);
		fprintf(report_fd, "  * %d with missing roms\n", r_cb_data.summary.missing);
		fprintf(report_fd, "  * %d with memory leaks\n", r_cb_data.summary.memleaks);
		fprintf(report_fd, "  * %d with sound clipping\n", r_cb_data.summary.clipped);
		fprintf(report_fd, "  * %d with unexpected stderr messages\n", r_cb_data.summary.unexpected_stderr);
		fprintf(report_fd, "  * %d with missing mandatory devices\n", r_cb_data.summary.mandatory);
	}
	
	if( config_report_type == 1 )
		fprintf(report_fd, "</body>\n</html>\n");

	if (report_fd != NULL)
	{ 
		fclose(report_fd);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	printf("create_report %s\n", VERSION);
	
	if( argc != 2 ) {
		printf("usage: create_report <report_name>\n");
		return 1;
	}
	
	libxml2_init();
	
	if( !config_init("create_report.xml", "create_report") ) {
		printf("aborting\n");
		return 1;
	}

	if( !config_read(report_config, argv[1]) ) {
		printf("aborting\n");
		config_free(report_config);
		return 1;
	}
	
	const int res = create_report();
	
	config_free(report_config);

	return res;
}
