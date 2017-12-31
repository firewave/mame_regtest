#include "config.h"

#include <stdio.h>
#include <string.h>

#ifdef __GNUC__
#include <unistd.h>
#endif

#ifdef _MSC_VER
#include <io.h>
#if _MSC_VER >= 1400
#undef access
#define access _access
#undef strdup
#define strdup _strdup
#endif
#define F_OK 00
#endif

/* libxml2 */
#ifdef __MINGW32__
#define IN_LIBXML
#endif
#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "common.h"

static xmlDocPtr global_config_doc = NULL;
static xmlNodePtr global_config_root = NULL;

int config_init(const char* config_xml, const char* root_node)
{
	if( access(config_xml, F_OK) == -1 ) {
		printf("configuration file '%s' does not exist\n", config_xml);
		return 0;
	}
	printf("using configuration file '%s'\n", config_xml);

	global_config_doc = xmlReadFile(config_xml, NULL, 0);
	if( !global_config_doc ) {
		printf("could not load configuration file\n");
		return 0;
	}

	global_config_root = xmlDocGetRootElement(global_config_doc);
	if( !global_config_root ) {
		printf("invalid configuration file - no root element\n");
		return 0;
	}

	if( xmlStrcmp(global_config_root->name, (const xmlChar*)root_node) != 0 ) {
		printf("invalid configuration file - no '%s' element\n", root_node);
		return 0;
	}

	return 1;
}

static int config_read_option(const xmlNodePtr config_node, const char* name, xmlChar** option)
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

static void config_read_option_int(const xmlNodePtr config_node, const char* opt_name, int* value)
{
	xmlChar* opt = NULL;
	if( config_read_option(config_node, opt_name, &opt) ) {
		if( opt && xmlStrlen(opt) > 0 ) {
			*value = atoi((const char*)opt);
		}
	}
	xmlFree(opt);
}

static void config_read_option_str_ptr(const xmlNodePtr config_node, const char* opt_name, char** value)
{
	xmlChar* opt = NULL;
	if( config_read_option(config_node, opt_name, &opt) ) {
		if( *value )
			free(*value);
		*value = strdup((const char*)opt);
		xmlFree(opt);
		opt = NULL;
	}
}

int config_read(struct config_entry config_entries[], const char* config_name)
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

	int i = 0;
	for( ; config_entries[i].name != NULL; ++i )
	{
		struct config_entry* ce = &config_entries[i];
		/* printf("entry - %s - %d - ", ce->name, ce->type); */
		if( ce->type == CFG_INT ) {
			int* value = (int*)ce->value;
			config_read_option_int(global_config_child, ce->name, value);
			/* printf("%d\n", *value); */
		}
		else if( ce->type == CFG_STR_PTR ) {
			char** value = (char**)ce->value;
			config_read_option_str_ptr(global_config_child, ce->name, value);
			/* printf("%s\n", *value ? *value : ""); */
		}
		else
			printf("unknown config_entry type %d\n", ce->type);
	}

	return 1;
}

void config_free(struct config_entry config_entries[])
{
	int i = 0;
	for( ; config_entries[i].name != NULL; ++i )
	{
		struct config_entry* ce = &config_entries[i];
		if( ce->type == CFG_STR_PTR ) {
			char** value = (char**)ce->value;
			free(*value);
		}
	}
	
	if( global_config_doc ) {
		xmlFreeDoc(global_config_doc);
		global_config_doc = NULL;
	}
}
