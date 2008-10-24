#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* libxml2 */
#include "libxml/parser.h"

/* local */
#include "common.h"
#include "config.h"

#define VERSION "0.68"

/*
	TODO:
		- errorhandling for _key/devices entries
		- sort group_data alphabetically
*/

struct driver_data
{
	char* filename;
	void* next;
};

struct group_data
{
	xmlChar* srcfile;
	struct driver_data* drivers;
	void* next;	
};

struct build_group_cb_data
{
	struct group_data** gd_list;
};

static void build_group_cb(struct parse_callback_data* pcd)
{
	if( pcd->type == ENTRY_FILE ) {
		if( strstr(pcd->entry_name, ".xml") && strstr(pcd->entry_name, "listxml.xml") == NULL )
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

static void get_group_data(const char* dirname, struct group_data** gd)
{	
	struct build_group_cb_data bg_cb_data;
	bg_cb_data.gd_list = gd;

	parse_directory(dirname, 0, build_group_cb, (void*)&bg_cb_data);	
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
	{ NULL, 				-1, 			NULL }
};

struct report_cb_data
{
	FILE* report_fd;
	int print_stderr;
	int dokuwiki_format;
	int ignore_exitcode_4;
	int show_memleaks;
	int show_clipped;
};

static int create_report_from_filename(const char *const filename, const struct report_cb_data *const r_cb_data, int write_src_header)
{
	int data_written = 0;

	xmlDocPtr doc = xmlReadFile(filename, "UTF-8", 0);
	if( doc ) {
		xmlNodePtr output_node = doc->children;
	
		xmlChar* name_key = xmlGetProp(output_node, (const xmlChar*)"name");
		xmlChar* sourcefile_key = xmlGetProp(output_node, (const xmlChar*)"sourcefile");
		xmlChar* bios_key = xmlGetProp(output_node, (const xmlChar*)"bios");
		xmlChar* ramsize_key = xmlGetProp(output_node, (const xmlChar*)"ramsize");
		xmlChar* autosave_key = xmlGetProp(output_node, (const xmlChar*)"autosave");
	
		xmlNodePtr output_childs = output_node->children;
		xmlNodePtr devices_node = NULL;
		while( output_childs ) {
			if( output_childs->type == XML_ELEMENT_NODE ) {
				if( xmlStrcmp(output_childs->name, (const xmlChar*)"result") == 0 ) {
					xmlChar* exitcode_key = xmlGetProp(output_childs, (const xmlChar*)"exitcode");

					xmlChar* stderr_key = NULL;
					if( r_cb_data->print_stderr )
						stderr_key = xmlGetProp(output_childs, (const xmlChar*)"stderr");
					xmlChar* stdout_key = NULL;
					if( r_cb_data->show_clipped )
						stdout_key = xmlGetProp(output_childs, (const xmlChar*)"stdout");

					if( (exitcode_key && (xmlStrcmp(exitcode_key, (const xmlChar*)"0") != 0) && 
						!(r_cb_data->ignore_exitcode_4 && (xmlStrcmp(exitcode_key, (const xmlChar*)"4") == 0))) || 
						(stderr_key && xmlStrstr(stderr_key, (const xmlChar*)"--- memory leak warning ---") && r_cb_data->show_memleaks) ||
						(r_cb_data->show_clipped && stdout_key && xmlStrstr(stdout_key, (const xmlChar*)"clipped") && xmlStrstr(stdout_key, (const xmlChar*)" 0% samples clipped") == NULL) ) {
	
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

							if( stdout_key )
								fprintf(r_cb_data->report_fd, "<code>\n%s\n</code>\n", stdout_key);
	
							if( stderr_key )
								fprintf(r_cb_data->report_fd, "<code>\n%s\n</code>\n\n", stderr_key); // TODO: why two '\n'
						}
						else
						{
							fprintf(r_cb_data->report_fd, "%s: %s", sourcefile_key, name_key);
							if( bios_key )
								fprintf(r_cb_data->report_fd, " (bios %s)", bios_key);
							if( ramsize_key )
								fprintf(r_cb_data->report_fd, " (ramsize %s)", ramsize_key);
							if( devices_node )
							{
								xmlAttrPtr dev_attrs = devices_node->properties;
								while( dev_attrs ) {
									fprintf(r_cb_data->report_fd, " (%s %s)", dev_attrs->name, dev_attrs->children->content);
	
									dev_attrs = dev_attrs->next;
								}
							}
							if( autosave_key )
								fprintf(r_cb_data->report_fd, " (autosave)");
							fprintf(r_cb_data->report_fd, " (%s)\n", exitcode_key);

							if( stdout_key )
								fprintf(r_cb_data->report_fd, "%s\n", stdout_key);
	
							if( stderr_key )
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
	
		xmlFree(bios_key);
		bios_key = NULL;
		xmlFree(sourcefile_key);
		sourcefile_key = NULL;
		xmlFree(name_key);
		name_key = NULL;

		xmlFreeDoc(doc);
		doc = NULL;
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
	FILE* report_fd = fopen(config_output_file, "wb");

	struct report_cb_data r_cb_data;
	r_cb_data.report_fd = report_fd;
	r_cb_data.print_stderr = config_print_stderr;
	r_cb_data.dokuwiki_format = config_dokuwiki_format;
	r_cb_data.ignore_exitcode_4 = config_ignore_exitcode_4;
	r_cb_data.show_memleaks = config_show_memleaks;
	r_cb_data.show_clipped = config_show_clipped;

	if( config_group_data ) {
		struct group_data* gd = NULL;
		get_group_data(config_xml_folder, &gd);
	
		printf("creating report...\n");
		create_report_from_group_data(gd, &r_cb_data);

		free_group_data(gd);
	}
	else
		parse_directory(config_xml_folder, 0, create_report_cb, (void*)&r_cb_data);

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
	
	config_init("create_report.xml", "create_report");
	config_read(report_config, argv[1]);
	
	create_report();
	
	config_free(report_config);
	
	return 0;
}
