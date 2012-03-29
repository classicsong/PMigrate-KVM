#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "read_config.h"
#include "para-config.h"

/* Init Parallel Param */
static void init_param(struct parallel_param *param) {
	param->SSL_type = 0;
	param->host_ip_list = NULL;
	param->dest_ip_list = NULL;
    param->num_slaves = 0;
    param->num_ips = 0;
    param->max_iter = DEFAULT_MAX_ITER;
    param->max_factor = DEFAULT_MAX_FACTOR;
    param->max_downtime = DEFAULT_MAX_DOWNTIME;
}

/* Get Number from List */
static int get_one_num(char *name, cfg_list *list, int *value, char* error) {
	num_list *n_list = NULL;
	if ( (n_list = get_num_list(name, list)) == NULL ) {
		perror(error);
		return -1;
	} else {
		*value= n_list->integer;
	}
	return 0;
}

/* Get IP Strings from List */
static int get_multi_ip(char *name, cfg_list *list, struct ip_list **ip, char *error) {
	str_list *s_list = NULL;
	if ( (s_list = get_str_list(name, list)) == NULL ) {
		perror(error);
		return -1;
	} else {
		for (;s_list != NULL; s_list = s_list->next) {
			struct ip_list *host = (struct ip_list*) calloc(sizeof(struct ip_list), 1); 
			host->host_port = s_list->string;
			host->len = strlen(s_list->string);
			if (!(*ip)) {
				*ip = host;
			} else {
				host->next = (*ip)->next;
				(*ip)->next = host;
			}
		}
	}
	return 0;
}

/* Parse Configure File Main Function */
struct parallel_param *parse_file(char *file) {
    struct parallel_param *para_config;
	cfg_list *list = NULL;

	init_config();
	read_cfg_file(file, &list);

    para_config = (struct parallel_param *)malloc(sizeof(struct parallel_param));
	init_param(para_config);

    //SSL type
	if (get_one_num("SSL_type", list, &para_config->SSL_type, "SSL_type error") < 0)
		goto error;

	//Host IP
	if (get_multi_ip("h_ip", list, &para_config->host_ip_list, "Host ip error") < 0)
		goto error;

	// Dest IP
	if (get_multi_ip("d_ip", list, &para_config->dest_ip_list, "Dest ip error") < 0)
		goto error;
	
	// Number Ips 
	if (get_one_num("ip_num", list, &para_config->num_ips, "ip_num error") < 0)
		goto error;
	
	
	// Number Slaves
	if (get_one_num("slave_num", list, &para_config->num_slaves, "slave_num error") < 0)
		goto error;

	// Max Iteration
	if (get_one_num("max_iter", list, &para_config->max_iter, "max_iter error") < 0)
		goto error;
	
	// Max Factor
	if (get_one_num("max_factor", list, &para_config->max_factor, "max_factor error") < 0)
		goto error;
	
	// Max Downtime
	if (get_one_num("max_downtime", list, &para_config->max_downtime, "max_downtime error") < 0)
		goto error;

    return para_config;
error:
	free(para_config);
	return NULL;
}

/* See what's going on Parallel Param for debug */
int reveal_param(struct parallel_param *param) {
	struct ip_list *list;
	printf("SSL_type: %d\n", param->SSL_type);
	printf("num_ips: %d\n", param->num_ips);
	printf("num_slaves: %d\n", param->num_slaves);
	printf("max_iter: %d\n", param->max_iter);
	printf("max_factor: %d\n", param->max_factor);
	printf("max_downtime: %d\n", param->max_downtime);

	printf("host_ip_list:\n");
	for (list = param->host_ip_list; list != NULL; list = list->next) {
		printf("\t%s\n", list->host_port);
	}

	printf("dest_ip_list:\n");
	for (list = param->dest_ip_list; list != NULL; list = list->next) {
		printf("\t%s\n", list->host_port);
	}
	return 0;
}
