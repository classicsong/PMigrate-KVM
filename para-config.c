#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "read_config.h"
#include "para-config.h"

struct parallel_param *parse_file(char *file) {
    struct parallel_param *para_config;
	cfg_list *list = NULL;
	str_list *s_list = NULL;
	num_list *n_list = NULL;

	init_config();
	read_cfg_file(file, &list);

    para_config = (struct parallel_param *)malloc(sizeof(struct parallel_param));
	para_config->SSL_type = 0;
	para_config->host_ip_list = NULL;
	para_config->dest_ip_list = NULL;
    para_config->num_slaves = 0;
    para_config->num_ips = 0;
    para_config->max_iter = DEFAULT_MAX_ITER;
    para_config->max_factor = DEFAULT_MAX_FACTOR;
    //para_config->max_downtime = DEFAULT_MAX_DOWNTIME;

    //SSL type
	if ( (n_list = get_num_list("SSL_type", list)) == NULL ) {
		perror("SSL_type error");
		goto error;
	} else {
		para_config->SSL_type = n_list->integer;
	}

	//Host IP
	if ( (s_list = get_str_list("h_ip", list)) == NULL ) {
		perror("Host ip error");
		goto error;
	} else {
		for (;s_list != NULL; s_list = s_list->next) {
			struct ip_list *host = (struct ip_list*) calloc(sizeof(struct ip_list), 1); 
			host->host_port = s_list->string;
			host->len = strlen(s_list->string);
			if (!para_config->host_ip_list) {
				para_config->host_ip_list = host;
			} else {
				host->next = para_config->host_ip_list->next;
				para_config->host_ip_list->next = host;
			}
		}
	}

	// Dest IP
	if ( (s_list = get_str_list("d_ip", list)) == NULL ) {
		perror("Dest ip error");
		goto error;
	} else {
		for (;s_list != NULL; s_list = s_list->next) {
			struct ip_list *host= (struct ip_list*) calloc(sizeof(struct ip_list), 1); 
			host->host_port = s_list->string;
			host->len = strlen(s_list->string);
			if (!para_config->dest_ip_list) {
				para_config->dest_ip_list = host;
			} else {
				host->next = para_config->dest_ip_list->next;
				para_config->dest_ip_list->next = host;
			}
		}
	}
	
	// Number Ips 
	if ( (n_list = get_num_list("ip_num", list)) == NULL ) {
		perror("ip_num");
		goto error;
	} else {
		para_config->num_ips = n_list->integer;
	}
	
	// Number Slaves
	if ( (n_list = get_num_list("slave_num", list)) == NULL ) {
		perror("slave_num");
		goto error;
	} else {
		para_config->num_slaves = n_list->integer;
	}

	// Max Iteration
	if ( (n_list = get_num_list("max_iter", list)) == NULL ) {
		perror("max_iter");
		goto error;
	} else {
		para_config->max_iter = n_list->integer;
	}
	
	// Max Factor
	if ( (n_list = get_num_list("max_factor", list)) == NULL ) {
		perror("max_factor");
		goto error;
	} else {
		para_config->max_factor = n_list->integer;
	}
	
    return para_config;
error:
	free(para_config);
	return NULL;
}

int reveal_param(struct parallel_param *param) {
	struct ip_list *list;
	printf("SSL_type: %d\n", param->SSL_type);
	printf("num_ips: %d\n", param->num_ips);
	printf("num_slaves: %d\n", param->num_slaves);
	printf("max_iter: %d\n", param->max_iter);
	printf("max_factor: %d\n", param->max_factor);

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
