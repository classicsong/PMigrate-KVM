#include "atomic.h"

/*
 * host_port: the target connection ip:port
 */
FdMigrationStateSlave *tcp_start_outgoing_migration_slave(Monitor *mon,
                                                          char *host_ip,
                                                          char *dest_ip,
                                                          int64_t bandwidth_limit,
                                                          int detach,
                                                          int SSL_type)
{
    struct sockaddr_in addr;
    FdMigrationStateSlave *s;
    int ret;

    s = qemu_mallocz(sizeof(*s));

    s->get_error = socket_errno;//report socket error
    s->write = socket_write;//write data to the target fd
    s->close = tcp_close;//close tcp connection
    s->mig_state.cancel = migrate_fd_cancel;//migartion cancel callback func
    s->mig_state.get_status = migrate_fd_get_status;//get current migration status
    s->mig_state.release = migrate_fd_release;//release migration fd | callback func for the end of migration

    s->state = MIG_STATE_ACTIVE;
    s->mon = mon;
    s->bandwidth_limit = bandwidth_limit;
    s->host_ip = host_ip;
    s->dest_ip = dest_ip;
}

void start_host_slave(void *data) {
    struct FdMigrationStateSlave *s = (struct FdMigrationStateSlave *)data;
    struct task_body *body;
    int i;

    /*
     *      * create network connection
     *           */
    s->fd = socket(PF_INET, SOCK_STREAM, 0);
    if (s->fd == -1) {
        qemu_free(s);
        return NULL;
    } 

    /*
     * create file ops
     */
    s->file = qemu_fopen_ops_buffered_slave(s,
                                      s->bandwidth_limit,
                                      migrate_fd_put_buffer_slave,
                                      migrate_fd_put_ready_slave,
                                      migrate_fd_wait_for_unfreeze_slave,
                                      migrate_fd_close_slave);

    pthread_barrier_wait(&s->sender_barr.sender_iter_barr);

    /*
     * wait for following commands
     * As disk task maybe limited by the disk throughput, so we perfer to transfer disk first and then memory
     * While sending data in one iteration, we assume the total throughput of this iteration is static
     * So the effect of sending memory first and sending disk first is same.
     */
    while (1) {
        /* check for disk */
        if (queue_pop_task(s->disk_task_queue, &body) > 0) {
            /* Section type */
            qemu_put_byte(f, QEMU_VM_SECTION_PART);
            qemu_put_be32(f, s->disk_task_queue->section_id);
            /*
             * handle disk
             */
            for (i = 0; i < body->len; i++) {
                disk_save_block_slave(body->ptr, body->iter_num, s->file);
            }

            /* End of the single task */
            qemu_put_be64(f, BLK_MIG_FLAG_EOS);
            free(body);
        }
        /* check for memory */
        else if (queue_pop_task(s->mem_task_queue, &body) < 0) {
            /* Section type */
            qemu_put_byte(f, QEMU_VM_SECTION_PART);
            qemu_put_be32(f, s->mem_task_queue->section_id);

            for (i = 0; i < body->len; i++) {
                ram_save_block_slave(body->pages[i].ptr, body->pages[i].cont, s);
            }

            /* End of the single task */
            qemu_put_be64(f, RAM_SAVE_FLAG_EOS);
            free(body);
        }
        /* no disk and memory task */
        else {
            if (s->sender_barr.mem_state == BARR_STATE_ITER_END && 
                s->sender_barr.disk_state == BARR_STATE_ITER_END) {
                pthread_barrier_wait(&s->sender_barr.sender_iter_barr);
                pthread_barrier_wait(&s->sender_barr.next_iter_barr);
            }

            //get nothing, wait for a moment
            usleep(SLEEP_SHORT_TIME);
        }
    }
}

void init_host_slaves(struct FdMigrationState *s) {
    int i;

    init_migr_barrier(&s->sender_barr, s->para_config->num_slave);

    /*
     * no need for disk migration
     */
    if (s->mig_state.blk == 0)
        s->disk_barr.state = BARR_STATE_SKIP;

    for (i = 0; i < s->para_config->num_slave; i ++) {
        FdMigrationStateSlave *slave_s;
        pthread_t tid;
        struct migration_slave *slave = (struct migration_slave *)malloc(sizeof(struct migration_slave));
        char *host_ip = s->para_config->host_ip_list[i];
        char *dest_ip = s->para_config->dest_ip_list[i];
        int ssl_type = s->para_config->SSL_type;
        slave_s = tcp_start_outgoing_migration_slave(s->mon, host_ip, 
                                                     dest_ip, s->bandwidth_limit,
                                                     detach, ssl_type);

        pthread_create(&tid, NULL, start_host_slave, slave_s);
        slave->tid = tid;
        slave->next = s->slave_list;
        s->slave_list = slave;
    }
}

struct dest_slave_para{
    char *listen_ip;
    int ssl_type;
};

int slave_loadvm_state() {
    /*
     * receive memory and disk data
     */
}

void start_dest_slave(void *data) {
    struct dest_slave_para * para = (struct dest_slave_para *)data;

    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int fd;
    int con_fd;
    /*
     * create connection
     */
    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (s == -1)
        return -socket_error();

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        goto err;

    if (listen(fd, 1) == -1)
        goto err;

    do {
        con_fd = accept(fd, (struct sockaddr *)&addr, &addrlen);
    } while (con_fd == -1 && socket_error() == EINTR);

    /*
     * wait for further commands
     */
}

pthread_t create_dest_slave(char *listen_ip, int ssl_type) {
    struct dest_slave_para *data;
    pthread_t tid;
    data->listen_ip = listen_ip;
    data->ssl_type = ssl_type;
    pthread_create(&tid, NULL, start_dest_slave, data);

    return tid;
}

