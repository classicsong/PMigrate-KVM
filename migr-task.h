#ifndef MIGR_TASK_H
#define MIGR_TASK_H

#include <stdio.h>
#include <pthread.h>

#define SLEEP_SHORT_TIME 1000

#define MIGR_DISK_TASK 0
#define MIGR_MEM_TASK 1

#define TASK_TYPE_MEM 0
#define TASK_TYPE_DISK 1

#define DEFAULT_MEM_BATCH_SIZE 2 * 1024 * 1024 //2M
#define DEFAULT_DISK_BATCH_SIZE 2 * 1024 * 1024 //2M

#define DEFAULT_MEM_BATCH_LEN (DEFAULT_MEM_BATCH_SIZE/TARGET_PAGE_SIZE)
#define DEFAULT_DISK_BATCH_LEN 128
#define DEFAULT_DISK_BATCH_MIN_LEN (DEFAULT_DISK_BATCH_LEN/2)

struct task_body {
    int type;
    int len;
    union {
        struct {
            uint8_t *ptr;
            int cont;
        } pages[DEFAULT_MEM_BATCH_LEN];
        struct {
            char *ptr;
        } blocks[DEFAULT_DISK_BATCH_LEN];
    };
    int iter_num;
};

struct linked_list {
    linked_list *next;
    linked_list *prev;
};

static inline void INIT_LIST_HEAD(struct linked_list *list) {
    list->next = list;
    list->prev = list;
}

static inline void __list_add(struct linked_list *new, struct linked_list *prev, struct linked_list *next) {
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

static inline void list_add_tail(struct linked_list *new, struct linked_list *head) {
    __list_add(new, head->prev, head);
}

static inline void __list_del(struct linked_list *prev, struct linked_list *next) {
    next->prev = prev;
    prev->next = next;
}

#define BARR_STATE_ITER_ERR 0
#define BARR_STATE_ITER_START 1
#define BARR_STATE_ITER_END 2
#define BARR_STATE_SKIP 3

struct migration_barrier {
    volatile int mem_state;
    volatile int disk_state;
    pthread_barrier_t sender_iter_barr;
    pthread_barrier_t next_iter_barr;
    pthread_mutex_t master_lock;
};

struct migration_task {
    struct linked_list list;
    struct task_body *body;
};

struct migration_task_queue {
    struct linked_list list_head;
    pthread_mutex_t task_lock;
    int version_id;
    int force_end;
    int iter_num;
    unsigned long data_remaining;
    unsigned long bwidth;
    unsigned long sent_this_iter;
    unsigned long sent_last_iter;
};

struct migration_slave{
    struct migration_slave *next;
    int slave_id;
};

static void 
init_migr_barrier(struct migration_barrier *barr, int num_slaves) {
    barr->mem_state = BARR_STATE_ITER_ERR;
    barr->disk_state = BARR_STATE_ITER_ERR;
    //barrier for master and the main process
    pthread_barrier_init(&barr->sender_iter_barr, NULL, num_slaves + 2);
    pthread_barrier_init(&barr->next_iter_barr, NULL, num_slaves + 2);
    pthread_mutex_init(&barr->master_lock, NULL);
}

struct migration_task_queue *new_task_queue(void) {
    struct migration_task_queue *task_queue = (struct migration_task_queue *)malloc(sizeof(struct migration_task_queue));
    INIT_LIST_HEAD(&(task_queue->list_head));
    pthread_mutex_init(&(task_queue->task_lock), NULL);
    task_queue->version_id = 0;
    task_queue->force_end = 0;
    task_queue->iter_num = 0;
    task_queue->data_remaining = 0;
    task_queue->bwidth = 0;
    task_queue->sent_this_iter = 0;
    task_queue->sent_last_iter = 0;

    return task_queue;
}

int queue_pop_task(struct migration_task_queue *task_queue, struct task_body **arg) {
    pthread_mutex_lock(&(task_queue->task_lock));
    if (task_queue->list_head.next == &task_queue->list_head) {
        pthread_mutex_unlock(&(task_queue->task_lock));
        return -1;
    } else {
        struct migration_task *task;
        task = (struct migration_task *)task_queue->list_head.next;
        __list_del(task->list.prev, task->list.next);
        
        *arg = task->body;
        free(task);
    }    
    pthread_mutex_unlock(&(task_queue->task_lock));

    return 0;
}

int queue_push_task(struct migration_task_queue *task_queue, struct task_body *body) {
    struct migration_task *task = (struct migration_task *)malloc(sizeof(struct migration_task));

    if (task < 0) {
        fprintf(stderr, "error allock task");
        return -1;
    }

    pthread_mutex_lock(&(task_queue->task_lock));
    task->body = body;
    list_add_tail(&task_queue->list_head, &task->list);

    pthread_mutex_unlock(&(task_queue->task_lock));

    return 0;
}

#endif
