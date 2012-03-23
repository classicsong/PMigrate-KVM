#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "read_config.h"

#define SJC_DEBUG
/* Type String */
char *type_string[] = {
	"NUMBER",
	"STRING"
};

cfg_list *cfg_predefine;
static int search_in_predefine(char *str, int str_len)
{
	cfg_list *a = cfg_predefine;
	for (; a != NULL; a = a->next) {
		if (strncmp(str, a->pair->cfg_name, str_len) == 0) {
			return (int)a->pair->type;
		}
	}
	return -1;
}

static cfg_pair_t* parse_oneline(char *line) 
{
	char *token = CONFIG_TOKEN;
	char *str = strtok(line, token);
	cfg_value_type type;
	str_list *s_list = NULL;
	num_list *n_list = NULL;
	cfg_pair_t *pair =NULL;
	char *name = NULL;

	if ( (type = search_in_predefine(str,strlen(str))) < 0 )
		return NULL;
	name = (char*)malloc(strlen(str));
	strcpy(name, str);

	/* Parse List */
	while ((str = strtok(NULL, token)) != NULL) {
#ifdef SJC_DEBUG
		printf("DEBUG: Parsing token: %s\n", str);
#endif
		if (type == NUMBER) {
			num_list *tem;
			if (n_list == NULL) {
				n_list = (num_list*)malloc(sizeof(num_list));
				bzero(n_list, sizeof(num_list));
				tem = n_list;
			} else {
				tem = (num_list*)malloc(sizeof(num_list));
				tem->next = n_list->next;
				n_list->next = tem;
			}
			tem->integer = atoi(str);
		} else if (type == STRING){
			str_list *tem;
			if (s_list == NULL) {
				s_list = (str_list*)malloc(sizeof(str_list));
				bzero(s_list, sizeof(str_list));
				tem = s_list;
			} else {
				tem = (str_list*)malloc(sizeof(str_list));
				tem->next = s_list->next;
				s_list->next = tem;
			}
			tem->string = (char*)malloc(strlen(str));
			strcpy(tem->string ,str);
		}
	}

	pair = (cfg_pair_t*)calloc(sizeof(cfg_pair_t), 1);
	pair->cfg_name = name;
	if (type == NUMBER)
	{
		pair->cfg_value.n_list = n_list;
	} else if (type == STRING)
	{
		pair->cfg_value.s_list = s_list;
	}
	pair->type = type;
	return pair;
}

int read_cfg_file(char *file_path, cfg_list** list) 
{
	FILE *fd;
	char line[MAX_CONFIG_LINE];
	if ( ( fd = fopen(file_path, "r") ) <= 0 ) {
		perror("Config File Open Error");
		return -1;
	}
	bzero(line, MAX_CONFIG_LINE); 
	while ( fgets(line, MAX_CONFIG_LINE, fd) != NULL ) {
		cfg_pair_t *pair;
		line[strlen(line) - 1] = '\0';  // Delete new line charactor
		if ((pair = parse_oneline(line)) == NULL) {
			fprintf(stderr, "Parsing error: %s\n", line);
			return -1;
		}
		if (*list == NULL) {
			*list = (cfg_list*)calloc(sizeof(cfg_list), 1);
			(*list)->pair = pair;
		} else {
			cfg_list *tem = (cfg_list*)calloc(sizeof(cfg_list), 1);
			tem->pair = pair; 
			tem->next = (*list)->next;
			(*list)->next = tem;
		}
	}
	fclose(fd);
	return 0;
}

/* This function is used to process H file */
static int config_each(char *name, cfg_value_type type)
{
	cfg_list *n;
	if (cfg_predefine == NULL) {
		cfg_predefine = (cfg_list*)malloc(sizeof(cfg_list));
		bzero(cfg_predefine, sizeof(cfg_list));
		n = cfg_predefine;
	} else {
		n = (cfg_list*)malloc(sizeof(cfg_list));
		n->next = cfg_predefine->next;
		cfg_predefine->next = n;
	}
	n->pair = calloc(sizeof(cfg_pair_t), 1);
	n->pair->cfg_name = name;
	n->pair->type = type; 

	return 0;
}

/* Init From H File */
void init_config()
{
#define c_each(__a,__b) config_each(__a, __b)
#include "mc_config_list.h"
}

/* Print Config List */
int reveal_config_list(cfg_list* list) {
	/* Print List */
	for (;list != NULL; list = list->next) {
		str_list *s_list = NULL;
		num_list *n_list = NULL;
		cfg_pair_t *pair = list->pair;
		printf("%s: %s\n", pair->cfg_name, type_string[(int)pair->type]);
		if (pair->type == NUMBER) {
			n_list = pair->cfg_value.n_list;
		} else if (pair->type == STRING) {
			s_list = pair->cfg_value.s_list;
		}
		while ( n_list || s_list ){
			if (pair->type == NUMBER) {
				printf("\t%d\n", n_list->integer);
				n_list = n_list->next;
			} else if (pair->type == STRING) {
				printf("\t%s\n", s_list->string);
				s_list = s_list->next;
			}
		}
	}
	return 0;
}

/* Check Type, If no this entry return NONE type */
cfg_value_type cfg_which_type(char* name, cfg_list *list) {
	cfg_pair_t *pair;
	for (; list != NULL; list = list->next) {
		pair = list->pair;
		if ( strcmp(pair->cfg_name, name) == 0 ) {
			return pair->type;
		}
	}
	return NONE;
}

/* Return String List */
str_list* get_str_list(char* name, cfg_list *list){
	cfg_pair_t *pair;
	for (; list != NULL; list = list->next) {
		pair = list->pair;
		if ( strcmp(pair->cfg_name, name) == 0 && pair->type == STRING) {
			return pair->cfg_value.s_list;
		}
	}
	return NULL;
}

/* Return Number List */
num_list* get_num_list(char* name, cfg_list *list){
	cfg_pair_t *pair;
	for (; list != NULL; list = list->next) {
		pair = list->pair;
		if ( strcmp(pair->cfg_name, name) == 0 && pair->type == NUMBER) {
			return pair->cfg_value.n_list;
		}
	}
	return NULL;
}

