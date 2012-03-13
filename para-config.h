#ifndef PARA_CONFIG_H
#define PARA_CONFIG_H

struct ip_list {
    struct ip_list *next;
    const char *host_port;
    int len;
};

#define SSL_NO 0;
#define SSL_WEAK 1;
#define SSL_STRONG 2;

struct parallel_param {
    int SSL_type;
    struct ip_list *host_ip_list;
    struct ip_list *dest_ip_list;
    int num_ips;
    int num_slaves;
};

extern struct parallel_param *parse_file(char *file);
#endif
