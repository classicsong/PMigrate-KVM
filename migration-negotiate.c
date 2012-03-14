#include "para-config.h"

void qemu_savevm_state_negotiate(FdMigrationState *s, QEMUFile *f) {
    int num_ips = s->para_config.num_ips;
    struct ip_list *tmp_ip_list = s->para_config.dest_ip_list;
    int i;

    /*
     * negotiate
     * 1. num of dest ip used
     * 2. SSL type
     */
    qemu_put_byte(f, QEMU_VM_SECTION_NEGOTIATE);
    qemu_put_be32(f, num_ips);

    qemu_put_be32(f, s->para_config.SSL_type);

    for (i = 0; i < num_ips; i++) {
        qemu_put_be32(tmp_ip_list->len);
        qemu_put_buffer(f, tmp_ip_list->host_port, tmp_ip_list->len);
        tmp_ip_list = tmp_ip_list->next;
    }

    qemu_flush(f);
}

struct parallel_param *default_config(const char *host_port) {
    struct parallel_param *para_config = (struct parallel_param *)malloc(sizeof(struct parallel_param));
    struct ip_list *dest = (struct ip_list *)malloc(sizeof(struct ip_list));
    dest->host_port = host_port;
    dest->len = strlen(host_port);
    dest->next = NULL;

    para_config->SSL_type = SSL_NO;
    para_config->num_ips = 1;
    para_config->num_slaves = 1;
    para_config->dest_ip_list = dest;  //only init the dest ip
    para_config->host_ip_list = NULL;
}

void parse_migration_config_file(FdMigrationState *s, char *f, const char *host_port) {
    struct parallel_param *param = parse_file(f);

    if (param == NULL)
        param = default_config(host_port);

    s->para_config = param;
}
