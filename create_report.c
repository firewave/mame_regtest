#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "libxml/parser.h"
#include "libxml/xpath.h"

/* local */
#include "common.h"
#include "config.h"

#define VERSION "0.72"

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
			xmlDocPtr doc = xmlReadFile(pcd->fullname, "UTF-8", 0);
			if( doc ) {
				struct build_group_cb_data* bg_cb_data = (struct build_group_cb_data*)pcd->user_data;
				struct group_data** gd_list = bg_cb_data->gd_list;

				struct driver_data* new_dd = (struct driver_data*)malloc(sizeof(struct driver_data));
				new_dd->filename = strdup(pcd->fullname);
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
				};
				
				if( !found_srcfile ) {
					struct group_data* new_gd_entry = (struct group_data*)malloc(sizeof(struct group_data));
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
					sourcefile_key = NULL;
					
					struct driver_data* dd_entry = last_gd_entry->drivers;
					struct driver_data* last_dd_entry = NULL;
					while( dd_entry != NULL ) {
						last_dd_entry = dd_entry;
						dd_entry = dd_entry->next;
					};
				
					if( !last_gd_entry->drivers )
						last_gd_entry->drivers = new_dd;
					else
						last_dd_entry->next = new_dd;
				}
				
				xmlFreeDoc(doc);
				doc = NULL;
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
			tmp_dd_entry = NULL;
		};
		
		struct group_data* tmp_gd_entry = gd_entry;
		gd_entry = gd_entry->next;

		xmlFree(tmp_gd_entry->srcfile);
		tmp_gd_entry->srcfile = NULL;
		free(tmp_gd_entry);
		tmp_gd_entry = NULL;
	};
}

static char* config_xml_folder = NULL;
static char* config_output_file = NULL;
static int config_print_stderr = 0;
static int config_dokuwiki_format = 0;
static int config_ignore_exitcode_4 = 0;
static int config_show_memleaks = 0;
static int config_show_clipped = 0;
static int config_group_data = 0;
static int config_report_type = 0; /* 0 = result report - 1 = comparison report */
static char* config_compare_folder = NULL;
static int config_print_stdout = 0;
static char* config_output_folder = NULL;
static int config_recursive = 0;

struct config_entry report_config[] =
{
	{ "xml_folder", 		CFG_STR_PTR, 	&config_xml_folder },
	{ "output_file", 		CFG_STR_PTR, 	&config_output_file },
	{ "print_stderr", 		CFG_INT, 		&config_print_stderr },
	{ "dokuwiki_format", 	CFG_INT, 		&config_dokuwiki_format },
	{ "ignore_exitcode_4", 	CFG_INT, 		&config_ignore_exitcode_4 },
	{ "show_memleaks", 		CFG_INT, 		&config_show_memleaks },
	{ "show_clipped", 		CFG_INT, 		&config_show_clipped },
	{ "group_data", 		CFG_INT, 		&config_group_data },
	{ "report_type",		CFG_INT,		&config_report_type },
	{ "compare_folder",		CFG_STR_PTR,	&config_compare_folder }, /* comparison report specific */
	{ "print_stdout", 		CFG_INT, 		&config_print_stdout },
	{ "output_folder", 		CFG_STR_PTR, 	&config_output_folder }, /* comparison report specific */
	{ "recursive", 			CFG_INT, 		&config_recursive },
	{ NULL, 				CFG_UNK, 		NULL }
};

struct report_summary
{
	int executed;
	int errors;
	int memleaks;
	int crashed;
	int clipped;
	int mandatory;
	int aborted;
	int missing;
	int unknown;
};

static void summary_incr(struct report_summary* summary, const xmlChar* exitcode_key)
{
	if( xmlStrcmp(exitcode_key, (const xmlChar*)"0") == 0 )
		return;

	if( xmlStrcmp(exitcode_key, (const xmlChar*)"1") == 0 )
		summary->aborted++;
	else if( xmlStrcmp(exitcode_key, (const xmlChar*)"2") == 0 )
		summary->missing++;
	else if( xmlStrcmp(exitcode_key, (const xmlChar*)"3") == 0 )
		summary->errors++;
	else if( xmlStrcmp(exitcode_key, (const xmlChar*)"4") == 0 )
		summary->mandatory++;
	else if( xmlStrcmp(exitcode_key, (const xmlChar*)"100") == 0 || xmlStrcmp(exitcode_key, (const xmlChar*)"-1073740771") == 0 || xmlStrcmp(exitcode_key, (const xmlChar*)"-1073740940") == 0 || xmlStrcmp(exitcode_key, (const xmlChar*)"-1073741819") == 0 || xmlStrcmp(exitcode_key, (const xmlChar*)"-2147483645") == 0 )
		summary->crashed++;
	else
		summary->unknown++;
}

struct report_cb_data
{
	FILE* report_fd;
	int print_stderr;
	int dokuwiki_format;
	int ignore_exitcode_4;
	int show_memleaks;
	int show_clipped;
	int report_type;
	const char* compare_folder;
	int print_stdout;
	struct report_summary summary;
	const char* output_folder;
	int recursive;
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

#define PRINT_INFO \
	fprintf(r_cb_data->report_fd, "%s: %s", sourcefile_key, name_key); \
	if( bios_key ) \
		fprintf(r_cb_data->report_fd, " (bios %s)", bios_key); \
	if( ramsize_key ) \
		fprintf(r_cb_data->report_fd, " (ramsize %s)", ramsize_key); \
	if( dipswitch_key && dipvalue_key ) \
		fprintf(r_cb_data->report_fd, " (dipswitch %s value %s)", dipswitch_key, dipvalue_key); \
	if( configuration_key && confsetting_key ) \
		fprintf(r_cb_data->report_fd, " (configuration %s value %s)", configuration_key, confsetting_key); \
	if( devices_node ) \
	{ \
		xmlAttrPtr dev_attrs = devices_node->properties; \
		while( dev_attrs ) { \
			fprintf(r_cb_data->report_fd, " (%s %s)", dev_attrs->name, dev_attrs->children->content); \
			 \
			dev_attrs = dev_attrs->next; \
		} \
	} \
	if( autosave_key ) \
		fprintf(r_cb_data->report_fd, " (autosave)"); \

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
			PRINT_INFO \
			fprintf(r_cb_data->report_fd, "<br/>\n"); \
			\
			xmlFree(name); \
			name = NULL; \
			xmlFree(srcfile); \
			srcfile = NULL; \
		} \
		fprintf(r_cb_data->report_fd, "'%s' differs<br/>\n", attr_name);\
		\
		xmlChar* tmp = xmlEncodeEntitiesReentrant(doc_old, attr1); \
		fprintf(r_cb_data->report_fd, "old: %s<br/>\n", tmp); \
		xmlFree(tmp); \
		tmp = NULL; \
		\
		tmp = xmlEncodeEntitiesReentrant(doc, attr2); \
		fprintf(r_cb_data->report_fd, "new: %s<br/>\n", tmp); \
		xmlFree(tmp); \
		tmp = NULL; \
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
	xmlChar* confsetting_key = xmlGetProp(output_node, (const xmlChar*)"confsetting");
	
#define FREE_KEYS \
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

static int create_report_from_filename(const char *const filename, struct report_cb_data* r_cb_data, int write_src_header)
{
	int data_written = 0;
	
	switch( r_cb_data->report_type )
	{
		default:
		case 0:
		{
			xmlDocPtr doc = xmlReadFile(filename, "UTF-8", 0);
			if( doc ) {
				xmlNodePtr output_node = doc->children;				
				PREPARE_KEYS(output_node)
			
				xmlNodePtr output_childs = output_node->children;
				xmlNodePtr devices_node = NULL;
				while( output_childs ) {
					if( output_childs->type == XML_ELEMENT_NODE ) {
						if( xmlStrcmp(output_childs->name, (const xmlChar*)"result") == 0 ) {
							r_cb_data->summary.executed++;

							xmlChar* exitcode_key = xmlGetProp(output_childs, (const xmlChar*)"exitcode");		
							xmlChar* stderr_key = xmlGetProp(output_childs, (const xmlChar*)"stderr");
							xmlChar* stdout_key = xmlGetProp(output_childs, (const xmlChar*)"stdout");
								
							int error_found = exitcode_key && (xmlStrcmp(exitcode_key, (const xmlChar*)"0") != 0);
							int error4_found = xmlStrcmp(exitcode_key, (const xmlChar*)"4") == 0;							
							int memleak_found = stderr_key && xmlStrstr(stderr_key, (const xmlChar*)"--- memory leak warning ---");
							int clipped_found = stdout_key && xmlStrstr(stdout_key, (const xmlChar*)"clipped") && (xmlStrstr(stdout_key, (const xmlChar*)" 0% samples clipped") == NULL);
							/* TODO: replace with option */
							int reset_scope_found = stderr_key && xmlStrstr(stderr_key, (const xmlChar*)"called within reset scope by");
							
							summary_incr(&r_cb_data->summary, exitcode_key);
							if(memleak_found)
								r_cb_data->summary.memleaks++;
							if(clipped_found)
								r_cb_data->summary.clipped++;

							int report_error = (error_found && !(r_cb_data->ignore_exitcode_4 && error4_found));
							int report_memleak = (memleak_found && r_cb_data->show_memleaks);
							int report_stdout = (r_cb_data->print_stdout && stdout_key && xmlStrlen(stdout_key) > 0);
							int report_stderr = (r_cb_data->print_stderr && stderr_key && xmlStrlen(stderr_key) > 0);
							int report_clipped = (r_cb_data->show_clipped && clipped_found);
							
							if( report_error || report_memleak || report_stdout || report_stderr || report_clipped || reset_scope_found ) {
								if( r_cb_data->dokuwiki_format )
								{
									if( write_src_header ) {
										fprintf(r_cb_data->report_fd, "===== %s =====\n", sourcefile_key);
										/* disable so the header is only written once */
										write_src_header = 0;
									}
									fprintf(r_cb_data->report_fd, "==== %s ====\n", name_key);
									if( bios_key )
										fprintf(r_cb_data->report_fd, "  * BIOS: ''%s''\n", bios_key);
									if( ramsize_key )
										fprintf(r_cb_data->report_fd, "  * Ramsize: ''%s''\n", ramsize_key);
									if( dipswitch_key && dipvalue_key )
										fprintf(r_cb_data->report_fd, "  * Dipswitch: ''%s'' Value: ''%s'' \n", dipswitch_key, dipvalue_key);
									if( configuration_key && confsetting_key )
										fprintf(r_cb_data->report_fd, "  * Configuration: ''%s'' Value: ''%s'' \n", configuration_key, confsetting_key);
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
									fprintf(r_cb_data->report_fd, "  * Error code: ''%s''\n", exitcode_key);
		
									if( (report_error || report_stdout || report_clipped) && stdout_key && xmlStrlen(stdout_key) > 0 )
										fprintf(r_cb_data->report_fd, "<code>\n%s\n</code>\n", stdout_key);
			
									if( (report_error || report_memleak || report_stderr || reset_scope_found) && stderr_key && xmlStrlen(stderr_key) > 0 )
										fprintf(r_cb_data->report_fd, "<code>\n%s\n</code>\n", stderr_key);
									
									fprintf(r_cb_data->report_fd, "\n");
								}
								else
								{
									PRINT_INFO
									fprintf(r_cb_data->report_fd, " (%s)\n", exitcode_key);
		
									if( (report_error || report_stdout || report_clipped) && stdout_key && xmlStrlen(stdout_key) > 0 )
										fprintf(r_cb_data->report_fd, "%s\n", stdout_key);
			
									if( (report_error || report_memleak || report_stderr) && stderr_key && xmlStrlen(stderr_key) > 0 )
										fprintf(r_cb_data->report_fd, "%s\n", stderr_key);
								}
			
								data_written = 1;
							}
							xmlFree(stdout_key);
							stdout_key = NULL;
		
							xmlFree(stderr_key);
							stderr_key = NULL;
		
							xmlFree(exitcode_key);
							exitcode_key = NULL;
						}
						else if( xmlStrcmp(output_childs->name, (const xmlChar*)"devices") == 0 )
						{
							devices_node = output_childs;
						}
					}
					output_childs = output_childs->next;
				}
				
				FREE_KEYS
		
				xmlFreeDoc(doc);
				doc = NULL;
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
				
				if( png_differs ) {
					xmlChar* name = get_attribute_by_xpath(xpathCtx1, (const xmlChar*)"/output", (const xmlChar*)"name");
					xmlChar* srcfile = get_attribute_by_xpath(xpathCtx1, (const xmlChar*)"/output", (const xmlChar*)"sourcefile");
					
					char* png_path = NULL;
					append_string(&png_path, FILESLASH);
					append_string(&png_path, (const char*)name);
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
					
					// TODO: use basename instead of entry_name
					char* outpath = NULL;
					append_string(&outpath, config_output_folder);
					append_string(&outpath, FILESLASH);
					append_string(&outpath, entry_name);
					append_string(&outpath, "_diff.png");
					
					// TODO: switch the paths for now since somehow pngcmp prints the old one on the right
					char* pngcmp_cmd = NULL;
					append_string(&pngcmp_cmd, "pngcmp");
					append_string(&pngcmp_cmd, " ");
					//append_string(&pngcmp_cmd, path1);
					append_string(&pngcmp_cmd, path2);
					append_string(&pngcmp_cmd, " ");
					//append_string(&pngcmp_cmd, path2);
					append_string(&pngcmp_cmd, path1);
					append_string(&pngcmp_cmd, " ");
					append_string(&pngcmp_cmd, outpath);
					
					// TODO: add all information about execution to report
					int pngcmp_res = system(pngcmp_cmd);
					if( pngcmp_res == 1 ) {
						char* outfile = get_filename(outpath);
						fprintf(r_cb_data->report_fd, "<img src=\"%s\" alt=\"%s\"/>\n", outfile, outfile);
						free(outfile);
						outfile = NULL;
					}
					
					free(pngcmp_cmd);
					pngcmp_cmd = NULL;
					free(outpath);
					outpath = NULL;
					free(path2);
					path2 = NULL;
					free(path1);
					path1 = NULL;
					free(png_path);
					png_path = NULL;
					
					xmlFree(srcfile);
					srcfile = NULL;
					
					xmlFree(name);
					name = NULL;
				}
				
				FREE_KEYS

				if( xpathCtx2 )
					xmlXPathFreeContext(xpathCtx2);
				if( xpathCtx1 )
					xmlXPathFreeContext(xpathCtx1);

				xmlFreeDoc(doc);
				doc = NULL;
				xmlFreeDoc(doc_old);
				doc = NULL;
				
				if( write_set_data == 0 )
					fprintf(r_cb_data->report_fd, "</p>\n");
			}
			else {
				fprintf(r_cb_data->report_fd, "<p>%s - new</p>\n", entry_name);
			}
			free(old_path);
			old_path = NULL;
			free(entry_directory);
			entry_directory = NULL;
			free(entry_name);
			entry_name = NULL;
		}
		break;
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
		};
		gd_entry = gd_entry->next;
	};
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

static void create_report()
{
	FILE* report_fd = NULL;

	struct report_cb_data r_cb_data;
	r_cb_data.print_stderr = config_print_stderr;
	r_cb_data.dokuwiki_format = config_dokuwiki_format;
	r_cb_data.ignore_exitcode_4 = config_ignore_exitcode_4;
	r_cb_data.show_memleaks = config_show_memleaks;
	r_cb_data.show_clipped = config_show_clipped;
	r_cb_data.report_type = config_report_type;
	r_cb_data.compare_folder = config_compare_folder;
	r_cb_data.print_stdout = config_print_stdout;
	r_cb_data.output_folder = config_output_folder;
	r_cb_data.recursive = config_recursive;
	memset(&r_cb_data.summary, 0x00, sizeof(struct report_summary));

	if( config_report_type == 0 )
		report_fd = fopen(config_output_file, "wb");
	else if( config_report_type == 1 ) {
		char* outputfile = NULL;
		append_string(&outputfile, config_output_folder);
		append_string(&outputfile, FILESLASH);
		append_string(&outputfile, "mrt_diff.html");
		report_fd = fopen(outputfile, "wb");
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

	if( config_dokuwiki_format && config_report_type == 0 ) {
		fprintf(report_fd, "===== Summary =====\n");
		fprintf(report_fd, "  * %d executed\n", r_cb_data.summary.executed);
		fprintf(report_fd, "  * %d crashed\n", r_cb_data.summary.crashed);
		fprintf(report_fd, "  * %d with errors\n", r_cb_data.summary.errors);
		fprintf(report_fd, "  * %d with missing roms\n", r_cb_data.summary.missing);
		fprintf(report_fd, "  * %d with memory leaks\n", r_cb_data.summary.memleaks);
		fprintf(report_fd, "  * %d with sound clipping\n", r_cb_data.summary.clipped);
		fprintf(report_fd, "  * %d with missing mandatory devices\n", r_cb_data.summary.mandatory);
		fprintf(report_fd, "  * %d unknown\n", r_cb_data.summary.unknown);
	}
	
	if( config_report_type == 1 )
		fprintf(report_fd, "</body>\n</html>\n");

	fclose(report_fd);
	report_fd = NULL;
}

int main(int argc, char *argv[])
{
	printf("create_report %s\n", VERSION);
	
	if( argc != 2 ) {
		printf("usage: create_report <report_name>\n");
		exit(1);
	}
	
	if( !config_init("create_report.xml", "create_report") ) {
		printf("aborting\n");
		exit(1);
	}

	if( !config_read(report_config, argv[1]) ) {
		printf("aborting\n");
		config_free(report_config);
		exit(1);
	}
	
	create_report();
	
	config_free(report_config);
	
	return 0;
}
