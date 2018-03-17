#include "os.h"
#include <stdint.h>
#include "common.h"

void queue_init(task_queue_t * list) {
    list->head = list->tail = NULL;
    list->len = 0;
}

void enqueue(task_queue_t * list, PD * task) {
    if (list->len == 0) {
        list->head = list->tail = task;
    } else {
        list->tail->next = task;
    }
    list->tail = task;
    task->next = NULL;
    list->len++;
}

PD * deque(task_queue_t * list) {
    if (list->len == 0) {
        return NULL;
    } else if (list->len == 1) {
        PD * p = list->head;
        list->head = list->tail = NULL;
        list->len = 0;
        return p;
    } else {
        PD * p = list->head;
        list->head = p->next;
        list->len--;
        return p;
    }
}

PD * peek(task_queue_t * list) {
    return list->head;
}

void Enqueue_periodic_offset(task_queue_t * list, PD * task) {
    if (list->len == 0) {
        list->head = list->tail = task;
    } else {
        PD * p = list->head;
        PD * p_prev = NULL;
        while (p != NULL && p->next_start < task->next_start) {
            p_prev = p;
            p = p->next;
        }

        if (p_prev == NULL) {
            // Insert as first element in list
            list->head = task;
            task->next = p;
        } else if (p == NULL) {
            // Insert as last element in list
            list->tail = task;
            p_prev->next = task;
            task->next = NULL;
        } else {
            // Somewhere in the middle, insert between p_prev and p
            p_prev->next = task;
            task->next = p;
        }
    }
    list->len++;
}
