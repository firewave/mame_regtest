#ifndef CONFIG_H_
#define CONFIG_H_

enum config_entry_type {
	CFG_INT 	= 0,
	CFG_STR 	= 1,
	CFG_STR_PTR = 2
};

struct config_entry {
	const char* name;
	enum config_entry_type type;
	void* value;
};

int config_init(const char* config_xml);
int config_read(struct config_entry config_entries[], const char* config_name);
void config_free(struct config_entry config_entries[]);

#endif /* CONFIG_H_ */
