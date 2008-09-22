#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* libxml2 */
#include "libxml/parser.h"

/* local */
#include "common.h"

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

struct report_1_cb_data
{
	FILE* report_fd;
	int print_stderr;
	int dokuwiki_format;
	int ignore_exitcode_4;
	int show_memleaks;
	int show_clipped;
};

static int create_report_from_filename(const char *const filename, const struct report_1_cb_data *const r1_cb_data, int write_src_header)
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
					if( r1_cb_data->print_stderr )
						stderr_key = xmlGetProp(output_childs, (const xmlChar*)"stderr");
					xmlChar* stdout_key = NULL;
					if( r1_cb_data->show_clipped )
						stdout_key = xmlGetProp(output_childs, (const xmlChar*)"stdout");

					if( (exitcode_key && (xmlStrcmp(exitcode_key, (const xmlChar*)"0") != 0) && 
						!(r1_cb_data->ignore_exitcode_4 && (xmlStrcmp(exitcode_key, (const xmlChar*)"4") == 0))) || 
						(stderr_key && xmlStrstr(stderr_key, (const xmlChar*)"--- memory leak warning ---") && r1_cb_data->show_memleaks) ||
						(r1_cb_data->show_clipped && stdout_key && xmlStrstr(stdout_key, (const xmlChar*)"clipped") && xmlStrstr(stdout_key, (const xmlChar*)" 0% samples clipped") == NULL) ) {
	
						if( r1_cb_data->dokuwiki_format )
						{
							if( write_src_header ) {
								fprintf(r1_cb_data->report_fd, "===== %s =====\n", sourcefile_key);
								/* disable so the header is only written once */
								write_src_header = 0;
							}
							fprintf(r1_cb_data->report_fd, "==== %s ====\n", name_key);
							if( bios_key )
								fprintf(r1_cb_data->report_fd, "  * BIOS: ''%s''\n", bios_key);
							if( ramsize_key )
								fprintf(r1_cb_data->report_fd, "  * Ramsize: ''%s''\n", ramsize_key);
							if( devices_node )
							{
								xmlAttrPtr dev_attrs = devices_node->properties;
								while( dev_attrs ) {
									fprintf(r1_cb_data->report_fd, "  * Device: ''%s %s''\n", dev_attrs->name, dev_attrs->children->content);
	
									dev_attrs = dev_attrs->next;
								}
							}
							if( autosave_key )
								fprintf(r1_cb_data->report_fd, " (autosave)");
							fprintf(r1_cb_data->report_fd, "  * Error code: ''%s''\n", exitcode_key);

							if( stdout_key )
								fprintf(r1_cb_data->report_fd, "<code>\n%s\n</code>\n", stdout_key);
	
							if( stderr_key )
								fprintf(r1_cb_data->report_fd, "<code>\n%s\n</code>\n\n", stderr_key); // TODO: why two '\n'
						}
						else
						{
							fprintf(r1_cb_data->report_fd, "%s: %s", sourcefile_key, name_key);
							if( bios_key )
								fprintf(r1_cb_data->report_fd, " (bios %s)", bios_key);
							if( ramsize_key )
								fprintf(r1_cb_data->report_fd, " (ramsize %s)", ramsize_key);
							if( devices_node )
							{
								xmlAttrPtr dev_attrs = devices_node->properties;
								while( dev_attrs ) {
									fprintf(r1_cb_data->report_fd, " (%s %s)", dev_attrs->name, dev_attrs->children->content);
	
									dev_attrs = dev_attrs->next;
								}
							}
							if( autosave_key )
								fprintf(r1_cb_data->report_fd, " (autosave)");
							fprintf(r1_cb_data->report_fd, " (%s)\n", exitcode_key);

							if( stdout_key )
								fprintf(r1_cb_data->report_fd, "%s\n", stdout_key);
	
							if( stderr_key )
								fprintf(r1_cb_data->report_fd, "%s\n", stderr_key);
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

static void create_report_from_group_data(struct group_data* gd, struct report_1_cb_data* r1_cb_data)
{
	struct group_data* gd_entry = gd;
	while( gd_entry ) {
		int write_src_header = 1;
		
		struct driver_data* dd_entry = gd_entry->drivers;
		while( dd_entry ) {
			int data_written = create_report_from_filename(dd_entry->filename, r1_cb_data, write_src_header);
			if( data_written )
				write_src_header = 0;
			dd_entry = dd_entry->next;
		};
		gd_entry = gd_entry->next;
	};
}

static void create_report_1_cb(struct parse_callback_data* pcd)
{
	if( pcd->type == ENTRY_FILE ) {		
		if( strstr(pcd->entry_name, ".xml") && strstr(pcd->entry_name, "listxml.xml") == NULL )
		{
			struct report_1_cb_data* r1_cb_data = (struct report_1_cb_data*)pcd->user_data;

			printf("%s\n", pcd->entry_name);
			create_report_from_filename(pcd->fullname, r1_cb_data, 1);
		}
	}
}

static void create_report_1(const char* dirname)
{
	FILE* report_1_fd = fopen("d:\\report_1.txt", "wb");

	struct report_1_cb_data r1_cb_data;
	r1_cb_data.report_fd = report_1_fd;
	r1_cb_data.print_stderr = 0;
	r1_cb_data.dokuwiki_format = 0;
	r1_cb_data.ignore_exitcode_4 = 0;
	r1_cb_data.show_memleaks = 0;
	r1_cb_data.show_clipped = 0;

	parse_directory(dirname, 0, create_report_1_cb, (void*)&r1_cb_data);

	fclose(report_1_fd);
	report_1_fd = NULL;
}

static void create_report_2(const char* dirname)
{
	FILE* report_2_fd = fopen("d:\\report_2.txt", "wb");

	struct report_1_cb_data r1_cb_data;
	r1_cb_data.report_fd = report_2_fd;
	r1_cb_data.print_stderr = 1;
	r1_cb_data.dokuwiki_format = 0;
	r1_cb_data.ignore_exitcode_4 = 0;
	r1_cb_data.show_memleaks = 0;
	r1_cb_data.show_clipped = 0;

	parse_directory(dirname, 0, create_report_1_cb, (void*)&r1_cb_data);

	fclose(report_2_fd);
	report_2_fd = NULL;
}

static void create_report_3(const char* dirname)
{
	FILE* report_3_fd = fopen("d:\\report_3.txt", "wb");

	struct report_1_cb_data r1_cb_data;
	r1_cb_data.report_fd = report_3_fd;
	r1_cb_data.print_stderr = 1;
	r1_cb_data.dokuwiki_format = 1;
	r1_cb_data.ignore_exitcode_4 = 0;
	r1_cb_data.show_memleaks = 0;
	r1_cb_data.show_clipped = 0;

	parse_directory(dirname, 0, create_report_1_cb, (void*)&r1_cb_data);

	fclose(report_3_fd);
	report_3_fd = NULL;
}

static void create_report_4(const char* dirname)
{
	FILE* report_4_fd = fopen("d:\\report_4.txt", "wb");

	struct group_data* gd = NULL;
	get_group_data(dirname, &gd);

	struct report_1_cb_data r1_cb_data;
	r1_cb_data.report_fd = report_4_fd;
	r1_cb_data.print_stderr = 0;
	r1_cb_data.dokuwiki_format = 0;
	r1_cb_data.ignore_exitcode_4 = 0;
	r1_cb_data.show_memleaks = 0;
	r1_cb_data.show_clipped = 0;

	printf("creating report...\n");
	create_report_from_group_data(gd, &r1_cb_data);

	free_group_data(gd);

	fclose(report_4_fd);
	report_4_fd = NULL;
}

static void create_report_5(const char* dirname)
{
	FILE* report_5_fd = fopen("d:\\report_5.txt", "wb");

	struct group_data* gd = NULL;
	get_group_data(dirname, &gd);

	struct report_1_cb_data r1_cb_data;
	r1_cb_data.report_fd = report_5_fd;
	r1_cb_data.print_stderr = 1;
	r1_cb_data.dokuwiki_format =  0;
	r1_cb_data.ignore_exitcode_4 = 0;
	r1_cb_data.show_memleaks = 0;
	r1_cb_data.show_clipped = 0;

	printf("creating report...\n");
	create_report_from_group_data(gd, &r1_cb_data);

	free_group_data(gd);

	fclose(report_5_fd);
	report_5_fd = NULL;
}

static void create_report_6(const char* dirname)
{
	FILE* report_6_fd = fopen("d:\\report_6.txt", "wb");

	struct group_data* gd = NULL;
	get_group_data(dirname, &gd);

	struct report_1_cb_data r1_cb_data;
	r1_cb_data.report_fd = report_6_fd;
	r1_cb_data.print_stderr = 1;
	r1_cb_data.dokuwiki_format = 1;
	r1_cb_data.ignore_exitcode_4 = 0;
	r1_cb_data.show_memleaks = 0;
	r1_cb_data.show_clipped = 0;

	printf("creating report...\n");
	create_report_from_group_data(gd, &r1_cb_data);
	
	free_group_data(gd);

	fclose(report_6_fd);
	report_6_fd = NULL;
}

static void create_report_7(const char* dirname)
{
	FILE* report_7_fd = fopen("d:\\report_7.txt", "wb");

	struct group_data* gd = NULL;
	get_group_data(dirname, &gd);

	struct report_1_cb_data r1_cb_data;
	r1_cb_data.report_fd = report_7_fd;
	r1_cb_data.print_stderr = 1;
	r1_cb_data.dokuwiki_format = 1;
	r1_cb_data.ignore_exitcode_4 = 1;
	r1_cb_data.show_memleaks = 0;
	r1_cb_data.show_clipped = 0;

	printf("creating report...\n");
	create_report_from_group_data(gd, &r1_cb_data);

	free_group_data(gd);

	fclose(report_7_fd);
	report_7_fd = NULL;
}

static void create_report_8(const char* dirname)
{
	FILE* report_8_fd = fopen("d:\\report_8.txt", "wb");

	struct group_data* gd = NULL;
	get_group_data(dirname, &gd);

	struct report_1_cb_data r1_cb_data;
	r1_cb_data.report_fd = report_8_fd;
	r1_cb_data.print_stderr = 1;
	r1_cb_data.dokuwiki_format = 0;
	r1_cb_data.ignore_exitcode_4 = 1;
	r1_cb_data.show_memleaks = 1;
	r1_cb_data.show_clipped = 0;

	printf("creating report...\n");
	create_report_from_group_data(gd, &r1_cb_data);

	free_group_data(gd);

	fclose(report_8_fd);
	report_8_fd = NULL;
}

static void create_report_9(const char* dirname)
{
	FILE* report_9_fd = fopen("d:\\report_9.txt", "wb");

	struct group_data* gd = NULL;
	get_group_data(dirname, &gd);

	struct report_1_cb_data r1_cb_data;
	r1_cb_data.report_fd = report_9_fd;
	r1_cb_data.print_stderr = 1;
	r1_cb_data.dokuwiki_format = 1;
	r1_cb_data.ignore_exitcode_4 = 1;
	r1_cb_data.show_memleaks = 1;
	r1_cb_data.show_clipped = 0;

	printf("creating report...\n");
	create_report_from_group_data(gd, &r1_cb_data);

	free_group_data(gd);

	fclose(report_9_fd);
	report_9_fd = NULL;
}
static void create_report_10(const char* dirname)
{
	FILE* report_10_fd = fopen("d:\\report_10.txt", "wb");

	struct group_data* gd = NULL;
	get_group_data(dirname, &gd);

	struct report_1_cb_data r1_cb_data;
	r1_cb_data.report_fd = report_10_fd;
	r1_cb_data.print_stderr = 1;
	r1_cb_data.dokuwiki_format = 1;
	r1_cb_data.ignore_exitcode_4 = 1;
	r1_cb_data.show_memleaks = 1;
	r1_cb_data.show_clipped = 1;

	printf("creating report...\n");
	create_report_from_group_data(gd, &r1_cb_data);

	free_group_data(gd);

	fclose(report_10_fd);
	report_10_fd = NULL;
}

int main(int argc, char *argv[])
{
	if( argc != 3 ) {
		printf("usage: create_report <report_number> <folder>\n");
		printf("\n");
		printf("reports:\n");
		printf("\t 1 - list of drivers, that didn't return 0\n");
		printf("\t 2 - list of drivers, that didn't return 0 (with stderr)\n");
		printf("\t 3 - list of drivers, that didn't return 0 (with stderr) (DokuWiki format)\n");
		printf("\t 4 - list of drivers, that didn't return 0 (grouped by srcfile)\n");
		printf("\t 5 - list of drivers, that didn't return 0 (with stderr) (grouped by srcfile)\n");
		printf("\t 6 - list of drivers, that didn't return 0 (with stderr) (DokuWiki format) (grouped by srcfile)\n");
		printf("\t 7 - list of drivers, that didn't return 0 (with stderr) (DokuWiki format) (grouped by srcfile) (ignore exitcode 4)\n");
		printf("\t 8 - list of drivers, that didn't return 0 (with stderr) (grouped by srcfile) (ignore exitcode 4) (show memory leaks)\n");
		printf("\t 9 - list of drivers, that didn't return 0 (with stderr) (DokuWiki format) (grouped by srcfile) (ignore exitcode 4) (show memory leaks)\n");
		printf("\t10 - list of drivers, that didn't return 0 (with stderr) (DokuWiki format) (grouped by srcfile) (ignore exitcode 4) (show memory leaks) (show clipped)\n");
		exit(1);
	}
	
	int report_number = atoi(argv[1]);
	switch( report_number )
	{
		case 1:
			create_report_1(argv[2]);
			break;
		case 2:
			create_report_2(argv[2]);
			break;
		case 3:
			create_report_3(argv[2]);
			break;
		case 4:
			create_report_4(argv[2]);
			break;
		case 5:
			create_report_5(argv[2]);
			break;
		case 6:
			create_report_6(argv[2]);
			break;
		case 7:
			create_report_7(argv[2]);
			break;
		case 8:
			create_report_8(argv[2]);
			break;
		case 9:
			create_report_9(argv[2]);
			break;
		case 10:
			create_report_10(argv[2]);
			break;
		default:
			printf("unknown report\n");
			exit(1);
	}
	
	return 0;
}
