#include <stdio.h>

#include "migration.h"

void 
host_memory_master(void *data) {
    struct FdMigrationState *s = (struct FdMigrationState *)data;
    unsigned long total_sent = 0;
    //unsigned long sent_this_iter = 0, sent_last_iter = 0;
    unsigned long memory_size = ram_bytes_total();
    int num_slaves = s->num_slaves;
    unsigned long data_remaining;
    unsigned long bwidth;

    /*
     * wait for all slaves and master to be ready
     */
    pthread_barrier_wait(&(s->sender_barr.sender_iter_barr));
    s->sender_barr.mem_state = BARR_STATE_ITER_START;

    /*
     * Get dirty bitmap first
     * And start dirty tracking
     */
    if (cpu_physical_sync_dirty_bitmap(0, TARGET_PHYS_ADDR_MAX) != 0) {
        fprintf("get dirty bitmap error\n");
        qemu_file_set_error(f);
        return 0;
    }

    /* Enable dirty memory tracking */
    cpu_physical_memory_set_dirty_tracking(1);

    do {
        bwidth = qemu_get_clock_ns(rt_clock);

        /*
         * classicsong
         * dispatch job here
         * ram_save_iter will 
         */
        s->mem_task_queue->sent_this_iter = ram_save_iter(QEMU_VM_SECTION_PART, s->mem_task_queue, s->file);

    skip_iter:
        /*
         * add barrier here to sync for iterations
         */
        s->sender_barr.mem_state = BARR_STATE_ITER_END;
        pthread_barrier_wait(&s->sender_barr.sender_iter_barr);

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
            fprintf("get dirty bitmap error\n");
            qemu_file_set_error(f);
            return 0;
        }

        bwidth = qemu_get_clock_ns(rt_clock) - bwidth;
        bwidth = s->mem_task_queue->sent_this_iter / bwidth;

        data_remaining = ram_save_remaining() * TARGET_PAGE_SIZE;
        s->mem_task_queue->sent_last_iter = s->mem_task_queue->sent_this_iter;
        total_sent += s->mem_task_queue->sent_this_iter;

        if ((s->mem_task_queue->iter_num >= s->para_config.max_iter) ||
            (total_sent > s->para_config.max_factor * memory_size))
            s->mem_task_queue->force_end = 1;

        s->mem_task_queue->bwidth = bwidth;
        s->mem_task_queue->data_remaining = data_remaining;

        if (!pthread_mutex_trylock(&s->sender_barr.master_lock)) {
            /*
             * get lock fill memory info
             */
            DPRINTF("Iter [%d:%d], memory_remain %ld, bwidth %ld\n", 
                    s->mem_task_queue->iter_num, iter_num,
                    data_remaining, bwidth);
            pthread_mutex_unlock(&s->sender_barr.master_lock)
        }
        else {
            int total_expected_downtime;
            int sent_this_iter;
            int sent_last_iter;
            /*
             * failed to get lock first
             * check for disk info
             */
            pthread_mutex_lock(&s->sender_barr.master_lock);

            total_expected_downtime = (s->mem_task_queue->data_remaining + s->disk_task_queue->data_remaining)/
                (s->mem_task_queue->bwidth + s->disk_task_queue->bwidth);
            sent_this_iter = s->mem_task_queue->sent_this_iter + s->disk_task_queue->sent_this_iter;
            sent_last_iter = s->mem_task_queue->sent_last_iter + s->disk_task_queue->sent_last_iter;

            DPRINTF("Total Iter [%d:%d], data_remain %ld, bwidth %ld\n", s->mem_task_queue->iter_num, iter_num,
                    s->mem_task_queue->data_remaining + s->disk_task_queue->data_remaining, 
                    s->mem_task_queue->bwidth + s->disk_task_queue->bwidth);

            DPRINTF("Sent this iter %ld, sent last iter %ld", sent_this_iter, sent_last_iter);

            if (total_expected_downtime < s->para_config.max_downtime ||
                sent_this_iter > sent_last_iter ||
                s->disk_task_queue->force_end == 1 ||
                s->mem_task_queue->force_end == 1)
                s->laster_iter =1;
            pthread_mutex_unlock(&s->sender_barr.master_lock)
        }

        pthread_barrier_wait(&s->sender_barr.next_iter_barr);

        //total iteration number count
        iter_num++;

        /*
         * if the data left to send is small enough
         *    and the iteration is not the last iteration
         * skip the next mem iteration
         */
        if (((data_remaining/(s->mem_task_queue->bwidth + s->disk_task_queue->bwidth)) < 
             (s->para_config.max_downtime/2)) 
            && s->laster_iter != 1) {
            bwidth = qemu_get_clock_ns(rt_clock);
            s->mem_task_queue->sent_this_iter = 0;
            goto skip_iter;
        }
        /*
         * if skip the iteration
         * the iteration number of memory is not increased
         */
        s->mem_task_queue->iter_num ++;
    } while (s->laster_iter != 1);

    DPRINTF("done iterating\n");

    pthread_barrier_wait(&s->last_barr);

    //last iteration
    pthread_barrier_wait(&s->last_barr);
    ram_save_iter(QEMU_VM_SECTION_END, s->mem_task_queue, s->file);

    //last iteration end
    pthread_barrier_wait(&s->last_barr);
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

void 
host_disk_master(void * data) {
    struct FdMigrationState *s = (struct FdMigrationState *) data;
    int iter_num = 0;
    unsigned long total_sent = 0;
    //unsigned long sent_this_iter = 0, sent_last_iter = 0;
    unsigned long disk_size = 
    int num_slaves = s->num_slaves;
    unsigned long data_remaining;
    unsigned long bwidth;

    /*
     * wait for all slaves and master to be ready
     */
    pthread_barrier_wait(&(s->sender_barr.sender_iter_barr));
    s->sender_barr.disk_state = BARR_STATE_ITER_START;

    /* Enable dirty disk tracking */
    set_dirty_tracking(1);

    blk_mig_reset_dirty_cursor();

    do {
        bwidth = qemu_get_clock_ns(rt_clock);

        //flush old blks first
        flush_blks(f);

        if (qemu_file_has_error(f)) {
            blk_mig_cleanup(mon);
            return 0;
        }

        /*
         * classicsong
         * dispatch job here
         * ram_save_iter will 
         */
        s->disk_task_queue->sent_this_iter = disk_save_iter(QEMU_VM_SECTION_PART, s->disk_task_queue, s->file);

    skip_iter:
        /*
         * add barrier here to sync for iterations
         */
        s->sender_barr.disk_state = BARR_STATE_ITER_END;
        pthread_barrier_wait(&s->sender_barr.sender_iter_barr);

        /*
         * the dirty bitmap is reset in mig_save_device_dirty 
         * in blk_mig_save_dirty_blockf
         * through bdrv_reset_dirty(bmds->bs, sector, nr_sectors);
         */
        blk_mig_reset_dirty_cursor();

        bwidth = qemu_get_clock_ns(rt_clock) - bwidth;
        bwidth = s->disk_task_queue->sent_this_iter / bwidth;

        data_remaining = get_remaining_dirty();
        s->disk_task_queue->sent_last_iter = s->disk_task_queue->sent_this_iter;
        total_sent += s->disk_task_queue->sent_this_iter;

        if ((s->disk_task_queue->iter_num >= s->para_config.max_iter) ||
            (total_sent > s->para_config.max_factor * disk_size))
            s->disk_task_queue->force_end = 1;

        s->disk_task_queue->bwidth = bwidth;
        s->disk_task_queue->data_remaining = data_remaining;

        if (!pthread_mutex_trylock(&s->sender_barr.master_lock)) {
            /*
             * get lock fill disk info
             */
            DPRINTF("Iter [%d:%d], disk_remain %ld, bwidth %ld\n", 
                    s->disk_task_queue->iter_num,
                    iter_num, data_remaining, bwidth);
            pthread_mutex_unlock(&s->sender_barr.master_lock)
        }
        else {
            int total_expected_downtime;
            int sent_this_iter;
            int sent_last_iter;
            /*
             * failed to get lock first
             * check for disk info
             */
            pthread_mutex_lock(&s->sender_barr.master_lock);

            total_expected_downtime = (s->mem_task_queue->data_remaining + s->disk_task_queue->data_remaining)/
                (s->mem_task_queue->bwidth + s->disk_task_queue->bwidth);
            sent_this_iter = s->mem_task_queue->sent_this_iter + s->disk_task_queue->sent_this_iter;
            sent_last_iter = s->mem_task_queue->sent_last_iter + s->disk_task_queue->sent_last_iter;

            DPRINTF("Total Iter [%d:%d], data_remain %ld, bwidth %ld\n", s->disk_task_queue->iter_num, iter_num,
                    s->mem_task_queue->data_remaining + s->disk_task_queue->data_remaining, 
                    s->mem_task_queue->bwidth + s->disk_task_queue->bwidth);

            DPRINTF("Sent this iter %ld, sent last iter %ld", sent_this_iter, sent_last_iter);

            if (total_expected_downtime < s->para_config.max_downtime ||
                sent_this_iter > sent_last_iter ||
                s->disk_task_queue->force_end == 1 ||
                s->mem_task_queue->force_end == 1)
                s->laster_iter =1;
            pthread_mutex_unlock(&s->sender_barr.master_lock)
        }

        pthread_barrier_wait(&s->sender_barr.next_iter_barr);

        //total iteration number count
        iter_num++;

        /*
         * if the data left to send is small enough
         *    and the iteration is not the last iteration
         * skip the next mem iteration
         */
        if (((data_remaining/(s->mem_task_queue->bwidth + s->disk_task_queue->bwidth)) < 
             (s->para_config.max_downtime/2)) 
            && s->laster_iter != 1) {
            bwidth = qemu_get_clock_ns(rt_clock);
            s->mem_task_queue->sent_this_iter = 0;
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
    disk_save_iter(QEMU_VM_SECTION_END, s->mem_task_queue, s->file);

    //last iteration end
    pthread_barrier_wait(&s->last_barr);
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

pthread_t create_dest_memory_master();

pthread_t create_dest_disk_master();
