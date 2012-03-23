#ifndef PARA_CONFIG_H
#define PARA_CONFIG_H

struct ip_list {
    struct ip_list *next;
    const char *host_port;
    int len;
};

#define SSL_NO 0
#define SSL_WEAK 1
#define SSL_STRONG 2

#define DEFAULT_MAX_ITER 29 /*max 30 iterations*/
#define DEFAULT_MAX_FACTOR 3 /*never send more than 3x p2m_size*/
#define DEFAULT_MAX_DOWNTIME 30 /*max down time is 30ms*/

struct parallel_param {
    int SSL_type;
    struct ip_list *host_ip_list;
    struct ip_list *dest_ip_list;
    int num_ips;
    int num_slaves;
    int max_iter;
    int max_factor;
};

extern struct parallel_param *parse_file(char *file);
extern int reveal_param(struct parallel_param *param);
#endif
