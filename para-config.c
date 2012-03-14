#include <stdio.h>

#include "para-config.h"

struct parallel_param *parse_file(char *file) {
    struct parallel_param *para_config;

    if (file == NULL)
        return NULL;

    para_config = (struct parallel_param *)malloc(sizeof(struct parallel_param));
    para_config.num_slaves = 0;
    para_config.num_ips = 0;
    para_config.max_iter = DEFAULT_MAX_ITER;
    para_config.max_factor = DEFAULT_MAX_FACTOR;
    para_config.max_downtime = DEFAULT_MAX_DOWNTIME;

    //parse file here

    return para_config;
}
