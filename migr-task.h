#ifndef MIGR_TASK_H
#define MIGR_TASK_H

#define MIGR_DISK_TASK 0
#define MIGR_MEM_TASK 1

#include <stdio.h>
#include <pthread.h>

struct task_body {
    int type;
    //...
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

struct migration_task {
    struct linked_list list;
    struct task_body *body;
};

struct migration_task_queue {
    struct linked_list list_head;
    pthread_mutex_t task_lock;
};

struct migration_slave{
    struct migration_slave *next;
    int slave_id;
};

void queue_init(struct migration_task_queue *task_queue) {
    INIT_LIST_HEAD(&(task_queue->list_head));
    pthread_mutex_init(&(task_queue->task_lock), NULL);
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
