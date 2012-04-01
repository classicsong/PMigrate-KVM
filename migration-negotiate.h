#ifndef MIGRATION_NEGOTIATE_H
#define MIGRATION_NEGOTIATE_H

extern int qemu_savevm_state_negotiate(FdMigrationState *s, QEMUFile *f);
extern struct parallel_param *default_config(const char *host_port);
extern void parse_migration_config_file(FdMigrationState *s, const char *f, const char *host_port);
#endif
