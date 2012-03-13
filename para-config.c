#include <stdio.h>

#include "para-config.h"

struct parallel_param *parse_file(char *file) {
    struct parallel_param *para_config;

    if (file == NULL)
        return NULL;

    para_config = (struct parallel_param *)malloc(sizeof(struct parallel_param));
    para_config.num_slaves = 0;
    para_config.num_ips = 0;

    //parse file here

    return para_config;
}
