#include <stdio.h>

#include "migration.h"

void 
host_memory_master(void *data) {
    struct FdMigrationState *s = (struct FdMigrationState *)data;
    int iter_num = 0, laster_iter = 0;
    unsigned long total_sent = 0;
    unsigned long sent_this_iter = 0, sent_last_iter = 0;
    unsigned long memory_size = ram_bytes_total();
    unsigned long pages_remaining = 0;

    /*
     * Default sequence is Disk, Memory
     * However, we may not start memory at the very beginning
     */
    while (s->migrate_memory != START_MEMORY_MIGRATION)
        pthread_yield();

    
    bwidth = qemu_get_clock_ns(rt_clock);

    /*
     * classicsong
     * dispatch job here
     */

    /*
     * add barrier here to sync for iterations
     */

    bwidth = qemu_get_clock_ns(rt_clock) - bwidth;
    bwidth = sent_last_iter / bwidth;

    expected_downtime = pages_remaining * TARGET_PAGE_SIZE / bwidth;

    if ((expected_downtime =< s->para_config.max_downtime) ||
        (iter_num >= s->para_config.max_iter) ||
        (sent_this_iter > sent_last_iter) ||
        (total_sent > s->para_config.max_factor * memory_size))
        laster_iter = 1;
}

void 
create_host_memory_master(struct FdMigrationState *s) {
    pthread_t tid;
    struct migration_master *master;
    pthread_create(&tid, NULL, host_memory_master, s);

    master = (struct migration_master *)malloc(sizeof(struct migration_master));
    master->type = MEMORY_MASTER;
    master->tid = tid;
    master->next = s->master_list;
    s->master_list = master;
}

void 
host_disk_master(void * data) {
    struct FdMigrationState *s = (struct FdMigrationState *) data;

    /*
     * dispatch disk jobs
     */
}

void 
create_host_disk_master(struct FdMigrationState *s) {
    pthread_t tid;
    struct migration_master *master;
    pthread_create(&tid, NULL, host_disk_master, s);

    master = (struct migration_master *)malloc(sizeof(struct migration_master));
    master->type = DISK_MASTER;
    master->tid = tid;
    master->next = s->master_list;
    s->master_list = master;
}

pthread_t create_dest_memory_master();

pthread_t create_dest_disk_master();
