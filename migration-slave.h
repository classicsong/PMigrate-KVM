#ifndef MIGRATION_SLAVE_H
#define MIGRATION_SLAVE_H

extern void init_host_slaves(struct FdMigrationState *s);

extern pthread_t create_dest_slave(char *listen_ip, int ssl_type);


#endif
