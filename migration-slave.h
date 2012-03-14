#ifndef MIGRATION_SLAVE_H
#define MIGRATION_SLAVE_H

extern FdMigrationStateSlave *tcp_start_outgoing_migration_slave(Monitor *mon, char *host_ip, char *dest_ip,
                                                                 int64_t bandwidth_limit, int detach, int SSL_type);

extern void init_host_slaves(struct FdMigrationState *s);

extern pthread_t create_dest_slave(char *listen_ip, int ssl_type);


#endif
