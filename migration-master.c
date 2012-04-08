#include <stdio.h>

#include "qemu-common.h"
#include "qemu_socket.h"
#include "migration.h"
#include "qemu-char.h"
#include "sysemu.h"
#include "buffered_file.h"
#include "block.h"
#include "hw/hw.h"
#include "qemu-timer.h"


#define TARGET_PHYS_ADDR_BITS 64
#include "targphys.h"

#define DEBUG_MIGRATION_MASTER

#ifdef DEBUG_MIGRATION_MASTER
#define DPRINTF(fmt, ...) \
    do { printf("migration_master: " fmt, ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
    do { } while (0)
#endif

void* host_memory_master(void *data);
void create_host_memory_master(void *opaque);
void* host_disk_master(void * data);
void create_host_disk_master(void *opaque);

//from exec.c
extern int cpu_physical_sync_dirty_bitmap(target_phys_addr_t start_addr,
                                          target_phys_addr_t end_addr);
extern int cpu_physical_memory_set_dirty_tracking(int enable);

//from arch_init.c
extern unsigned long
ram_save_iter(int stage, struct migration_task_queue *task_queue, QEMUFile *f);

//from block-migration.c
extern uint64_t blk_mig_bytes_total(void);
extern void set_dirty_tracking_master(int enable);
extern void blk_mig_reset_dirty_cursor_master(void);
extern void blk_mig_cleanup_master(Monitor *mon);
extern unsigned long block_save_iter(int stage, Monitor *mon, 
                                     struct migration_task_queue *task_queue, QEMUFile *f);
extern int64_t get_remaining_dirty_master(void);
extern uint64_t blk_read_remaining(void);

//borrowed from savevm.c
#define QEMU_VM_EOF                  0x00
#define QEMU_VM_SECTION_START        0x01
#define QEMU_VM_SECTION_PART         0x02
#define QEMU_VM_SECTION_END          0x03
#define QEMU_VM_SECTION_FULL         0x04
#define QEMU_VM_SUBSECTION           0x05

void *
host_memory_master(void *data) {
    struct FdMigrationState *s = (struct FdMigrationState *)data;
    unsigned long total_sent = 0;
    //unsigned long sent_this_iter = 0, sent_last_iter = 0;
    unsigned long memory_size = ram_bytes_total();
    unsigned long data_remaining;
    double bwidth;
    QEMUFile *f = s->file;
    int iter_num = 0;
    int hold_lock = 0;

    DPRINTF("Start memory master\n");
    /*
     * wait for all slaves and master to be ready
     */
    pthread_barrier_wait(&(s->sender_barr->sender_iter_barr));
    s->mem_task_queue->sent_last_iter = memory_size;
    s->sender_barr->mem_state = BARR_STATE_ITER_START;

    DPRINTF("Start processing memory, %lx\n", s->mem_task_queue->sent_last_iter);

    do {
        bwidth = qemu_get_clock_ns(rt_clock);

        DPRINTF("Start mem iter %d\n", s->mem_task_queue->iter_num);
        /*
         * classicsong
         * dispatch job here
         * ram_save_iter will 
         */
        s->mem_task_queue->sent_this_iter += ram_save_iter(QEMU_VM_SECTION_PART, s->mem_task_queue, s->file);

    skip_iter:
        /*
         * add barrier here to sync for iterations
         */
        s->sender_barr->mem_state = BARR_STATE_ITER_END;
        hold_lock = !pthread_mutex_trylock(&s->sender_barr->master_lock);
        
        pthread_barrier_wait(&s->sender_barr->sender_iter_barr);

        /*
         * sync_dirty_bitmap in iteration for the next iter
         * the sync operation
         * 1. invoke ioctl KVM_GET_DIRTY_LOG to get dirty bitmap
         *    a. get dirty bitmap
         *    b. reset dirty bit in page table
         * 2. copy the dirty bitmap to user bitmap KVMDirtyLog d.dirty_bitmap
         *    a. KVMDirtyLog.dirty_bitmap is local
         * 3. copy the dirty_bitmap to a global bitmap using cpu_physical_memory_set_dirty that is 
         *    modifying ram_list.phys_dirty
         * Thus calling cpu_physical_sync_dirty_bitmap will not clean the ram_list.phys_dirty
         *   The dirty flag is reset by cpu_physical_memory_reset_dirty(va, vb, MIGRATION_DIRTY_FLAG)
         */
        if (cpu_physical_sync_dirty_bitmap(0, TARGET_PHYS_ADDR_MAX) != 0) {
            fprintf(stderr, "get dirty bitmap error\n");
            qemu_file_set_error(f);
            return 0;
        }

        bwidth = qemu_get_clock_ns(rt_clock) - bwidth;
        DPRINTF("Mem send this iter %lx, bwidth %f\n", s->mem_task_queue->sent_this_iter, bwidth);
        bwidth = s->mem_task_queue->sent_this_iter / bwidth;

        data_remaining = ram_bytes_remaining();
        total_sent += s->mem_task_queue->sent_this_iter;

        if ((s->mem_task_queue->iter_num >= s->para_config->max_iter) ||
            (total_sent > s->para_config->max_factor * memory_size))
            s->mem_task_queue->force_end = 1;

        s->mem_task_queue->bwidth = bwidth;
        s->mem_task_queue->data_remaining = data_remaining;

        if (hold_lock) {
            /*
             * get lock fill memory info
             */
            DPRINTF("Iter [%d:%d], memory_remain %ld, bwidth %f\n", 
                    s->mem_task_queue->iter_num, iter_num,
                    data_remaining, bwidth);
            pthread_mutex_unlock(&s->sender_barr->master_lock);
        }
        else {
            int total_expected_downtime;
            uint64_t sent_this_iter;
            uint64_t sent_last_iter;
            /*
             * failed to get lock first
             * check for disk info
             */
            pthread_mutex_lock(&s->sender_barr->master_lock);

            total_expected_downtime = (s->mem_task_queue->data_remaining + s->disk_task_queue->data_remaining)/
                (s->mem_task_queue->bwidth + s->disk_task_queue->bwidth);
            sent_this_iter = s->mem_task_queue->sent_this_iter + s->disk_task_queue->sent_this_iter;
            sent_last_iter = s->mem_task_queue->sent_last_iter + s->disk_task_queue->sent_last_iter;

            DPRINTF("Total Iter [%d:%d], data_remain %lx, bwidth %f\n", s->mem_task_queue->iter_num, iter_num,
                    s->mem_task_queue->data_remaining + s->disk_task_queue->data_remaining, 
                    s->mem_task_queue->bwidth + s->disk_task_queue->bwidth);

            DPRINTF("Sent this iter %lx, sent last iter %lx", sent_this_iter, sent_last_iter);

            if (total_expected_downtime < s->para_config->max_downtime ||
                sent_this_iter > sent_last_iter ||
                s->disk_task_queue->force_end == 1 ||
                s->mem_task_queue->force_end == 1)
                s->laster_iter =1;
            pthread_mutex_unlock(&s->sender_barr->master_lock);
        }

        //set last iter and reset this iter
        s->mem_task_queue->sent_last_iter = s->mem_task_queue->sent_this_iter;
        s->mem_task_queue->sent_this_iter = 0;
        //start the next iteration for slaves
        s->sender_barr->mem_state = BARR_STATE_ITER_START;
        pthread_barrier_wait(&s->sender_barr->next_iter_barr);

        //total iteration number count
        iter_num++;

        /*
         * if the data left to send is small enough
         *    and the iteration is not the last iteration
         * skip the next mem iteration
         */
        if (((data_remaining/(s->mem_task_queue->bwidth + s->disk_task_queue->bwidth)) < 
             (s->para_config->max_downtime/2)) 
            && s->laster_iter != 1) {
            bwidth = qemu_get_clock_ns(rt_clock);
            goto skip_iter;
        }
        /*
         * if skip the iteration
         * the iteration number of memory is not increased
         */
        s->mem_task_queue->iter_num ++;
    } while (s->laster_iter != 1);

    DPRINTF("Done mem iterating\n");

    pthread_barrier_wait(&s->last_barr);

    //last iteration
    pthread_barrier_wait(&s->last_barr);
    ram_save_iter(QEMU_VM_SECTION_END, s->mem_task_queue, s->file);


    //wait for slave end
    s->sender_barr->mem_state = BARR_STATE_ITER_TERMINATE;
    pthread_barrier_wait(&s->sender_barr->sender_iter_barr);
    //last iteration end
    pthread_barrier_wait(&s->last_barr);

    DPRINTF("Mem master end\n");
    return NULL;
}

void 
create_host_memory_master(void *opaque) {
    struct FdMigrationState *s = (struct FdMigrationState *)opaque;
    pthread_t tid;
    struct migration_master *master;
    s->mem_task_queue->section_id = s->section_id;
    pthread_create(&tid, NULL, host_memory_master, s);

    master = (struct migration_master *)malloc(sizeof(struct migration_master));
    master->type = MEMORY_MASTER;
    master->tid = tid;
    master->next = s->master_list;
    s->master_list = master;
}

void *
host_disk_master(void * data) {
    struct FdMigrationState *s = (struct FdMigrationState *) data;
    int iter_num = 0;
    unsigned long total_sent = 0;
    //unsigned long sent_this_iter = 0, sent_last_iter = 0;
    unsigned long disk_size = blk_mig_bytes_total();
    unsigned long data_remaining;
    double bwidth;
    Monitor *mon = s->mon;
    int hold_lock;

    DPRINTF("Start disk master\n");
    /*
     * no need for disk migration
     */
    if (s->mig_state.blk == 0) {
        fprintf(stderr, "Does not support memory only version here\n");
        return NULL;
    }

    DPRINTF("The default disk size is %lx\n", disk_size);

    /*
     * wait for all slaves and master to be ready
     */
    pthread_barrier_wait(&(s->sender_barr->sender_iter_barr));
    s->disk_task_queue->sent_last_iter = disk_size;
    s->sender_barr->disk_state = BARR_STATE_ITER_START;

    /* Enable dirty disk tracking */
    set_dirty_tracking_master(1);

    blk_mig_reset_dirty_cursor_master();

    do {
        DPRINTF("Start iteration %d\n", s->disk_task_queue->iter_num);
        bwidth = qemu_get_clock_ns(rt_clock);

        if (qemu_file_has_error(s->file)) {
            blk_mig_cleanup_master(mon);
            return NULL;
        }

        /*
         * classicsong
         * dispatch job here
         * ram_save_iter will 
         */
        s->disk_task_queue->sent_this_iter += block_save_iter(QEMU_VM_SECTION_PART, mon,
                                                              s->disk_task_queue, s->file);

    skip_iter:
        /*
         * add barrier here to sync for iterations
         */
        s->sender_barr->disk_state = BARR_STATE_ITER_END;
        hold_lock = !pthread_mutex_trylock(&s->sender_barr->master_lock);

        pthread_barrier_wait(&s->sender_barr->sender_iter_barr);

        /*
         * the dirty bitmap is reset in mig_save_device_dirty 
         * in blk_mig_save_dirty_blockf
         * through bdrv_reset_dirty(bmds->bs, sector, nr_sectors);
         */
        blk_mig_reset_dirty_cursor_master();

        bwidth = qemu_get_clock_ns(rt_clock) - bwidth;
        DPRINTF("Disk send this iter %lx, bwidth %f\n", s->disk_task_queue->sent_this_iter, bwidth);
        bwidth = s->disk_task_queue->sent_this_iter / bwidth;

        /*
         * The data_remaining includes dirty blocks, block have been reading using AIO
         *                             and blocks have bee read but not sent
         */
        data_remaining = get_remaining_dirty_master() + blk_read_remaining();
        DPRINTF("The data_remaining %lx; %lx\n", get_remaining_dirty_master(), data_remaining); 

        total_sent += s->disk_task_queue->sent_this_iter;

        if ((s->disk_task_queue->iter_num >= s->para_config->max_iter) ||
            (total_sent > s->para_config->max_factor * disk_size))
            s->disk_task_queue->force_end = 1;

        s->disk_task_queue->bwidth = bwidth;
        s->disk_task_queue->data_remaining = data_remaining;

        if (hold_lock) {
            /*
             * get lock fill disk info
             */
            DPRINTF("Iter [%d:%d], disk_remain %ld, bwidth %f\n", 
                    s->disk_task_queue->iter_num,
                    iter_num, data_remaining, bwidth);
            pthread_mutex_unlock(&s->sender_barr->master_lock);
        }
        else {
            int total_expected_downtime;
            uint64_t sent_this_iter;
            uint64_t sent_last_iter;
            /*
             * failed to get lock first
             * check for disk info
             */
            pthread_mutex_lock(&s->sender_barr->master_lock);

            total_expected_downtime = (s->mem_task_queue->data_remaining + s->disk_task_queue->data_remaining)/
                (s->mem_task_queue->bwidth + s->disk_task_queue->bwidth);
            sent_this_iter = s->mem_task_queue->sent_this_iter + s->disk_task_queue->sent_this_iter;
            sent_last_iter = s->mem_task_queue->sent_last_iter + s->disk_task_queue->sent_last_iter;

            DPRINTF("Total Iter [%d:%d], data_remain %ld, bwidth %f\n", s->disk_task_queue->iter_num, iter_num,
                    s->mem_task_queue->data_remaining + s->disk_task_queue->data_remaining, 
                    s->mem_task_queue->bwidth + s->disk_task_queue->bwidth);

            DPRINTF("Sent this iter %ld, sent last iter %ld\n", sent_this_iter, sent_last_iter);

            if (total_expected_downtime < s->para_config->max_downtime ||
                sent_this_iter > sent_last_iter ||
                s->disk_task_queue->force_end == 1 ||
                s->mem_task_queue->force_end == 1)
                s->laster_iter =1;
            pthread_mutex_unlock(&s->sender_barr->master_lock);
        }

        //set last iter and reset this iter
        s->disk_task_queue->sent_last_iter = s->disk_task_queue->sent_this_iter;
        s->disk_task_queue->sent_this_iter = 0;
        //start the next iteration for slaves
        s->sender_barr->disk_state = BARR_STATE_ITER_START;
        pthread_barrier_wait(&s->sender_barr->next_iter_barr);

        //total iteration number count
        iter_num++;

        /*
         * if the data left to send is small enough
         *    and the iteration is not the last iteration
         * skip the next mem iteration
         */
        if (((data_remaining/(s->mem_task_queue->bwidth + s->disk_task_queue->bwidth)) < 
             (s->para_config->max_downtime/2)) 
            && s->laster_iter != 1) {
            
            bwidth = qemu_get_clock_ns(rt_clock);
            goto skip_iter;
        }
        /*
         * if skip the iteration
         * the iteration number is not increased
         */
        s->disk_task_queue->iter_num ++;
    } while (s->laster_iter != 1);

    DPRINTF("done iterating\n");

    pthread_barrier_wait(&s->last_barr);

    //last iteration
    pthread_barrier_wait(&s->last_barr);
    block_save_iter(QEMU_VM_SECTION_END, s->mon, s->mem_task_queue, s->file);

    //wait for slave end
    s->sender_barr->disk_state = BARR_STATE_ITER_TERMINATE;
    pthread_barrier_wait(&s->sender_barr->sender_iter_barr);
    //last iteration end
    pthread_barrier_wait(&s->last_barr);

    DPRINTF("Disk master end\n");
    return NULL;
}

void 
create_host_disk_master(void *opaque) {
    struct FdMigrationState *s = (struct FdMigrationState *)opaque;
    pthread_t tid;
    struct migration_master *master;
    s->mem_task_queue->section_id = s->section_id;
    pthread_create(&tid, NULL, host_disk_master, s);

    master = (struct migration_master *)malloc(sizeof(struct migration_master));
    master->type = DISK_MASTER;
    master->tid = tid;
    master->next = s->master_list;
    s->master_list = master;
}

//pthread_t create_dest_memory_master();

//pthread_t create_dest_disk_master();
