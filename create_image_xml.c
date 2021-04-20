#include <stdio.h>

/* libxml2 */
#include <libxml/parser.h>

#include "common.h"

#define VERSION "0.75"

struct image_file_data
{
	const char* type;
	xmlNodePtr node;
};

static void image_list_callback(struct parse_callback_data* pcd)
{
	struct image_file_data* data = (struct image_file_data*)pcd->user_data;
	if( pcd->type == ENTRY_FILE ) {
		xmlNodePtr image_node = xmlNewChild(data->node, NULL, (const xmlChar*)"image", NULL);
		xmlNewProp(image_node, (const xmlChar*)data->type, (const xmlChar*)pcd->fullname);		
	}
}

static void build_image_list(const char* dirname, const char* type, xmlNodePtr node)
{
	struct image_file_data data;
	data.type = type;
	data.node = node;
	parse_directory(dirname, 0, image_list_callback, (void*)&data);
}

int main(int argc, char *argv[])
{
	printf("create_image_xml %s\n", VERSION);

	if( argc != 4 ) {
		printf("usage: create_image_xml <path> <output> <type>\n");
		return 1;
	}
	
	xmlDocPtr images_doc = xmlNewDoc((const xmlChar*)"1.0");
	xmlNodePtr images_node = xmlNewNode(NULL, (const xmlChar*)"images");
	xmlDocSetRootElement(images_doc, images_node);
	
	build_image_list(argv[1], argv[3], images_node);
	
	xmlSaveFormatFileEnc(argv[2], images_doc, "UTF-8", 1);
	
	xmlFreeDoc(images_doc);

	return 0;
}
