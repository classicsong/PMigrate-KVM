#ifndef __READ_CONFIG_H__
#define __READ_CONFIG_H__

#define MAX_CONFIG_LINE 1000
#define CONFIG_TOKEN " ,="

/* Two Types */
typedef enum CFG_VALUE_TYPE {
	NUMBER,
	STRING,
	NONE
} cfg_value_type;

typedef struct str_list str_list;
typedef struct num_list num_list;

struct str_list{
	struct str_list* next;
	char* string;
};
struct num_list{
	struct num_list* next;
	int integer;
};

/* Configure Pair */
typedef struct cfg_pair {
	char *cfg_name;
	union {
		str_list *s_list;
		num_list *n_list;
	} cfg_value;
	cfg_value_type type;
} cfg_pair_t;
 
typedef struct cfg_list cfg_list;

/* Configure Pair List */
struct cfg_list{ 
	struct cfg_list *next; 
	cfg_pair_t *pair;
}; 

void init_config();
int read_cfg_file(char *file_path, cfg_list** list);
int reveal_config_list(cfg_list* list);
cfg_value_type cfg_which_type(char* name, cfg_list *list);
str_list* get_str_list(char* name, cfg_list *list);
num_list* get_num_list(char* name, cfg_list *list);
#endif
