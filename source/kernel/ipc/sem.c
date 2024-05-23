#include "ipc/sem.h"
#include "tools/list.h"
#include "core/task.h"

void sem_init(sem_t *sem, int init_count)
{
    sem->count = init_count;
    list_init(&sem->wait_list);
}
void sem_wait(sem_t *sem)
{
    if (sem->count > 0)
    {
        sem->count--;
    }
    else
    {
        task_t *curr_task = task_current();
        task_set_block(curr_task);
        list_insert_last(&sem->wait_list, &curr_task->wait_node);
        task_dispatch();
    }
}
void sem_wakeup(sem_t *sem)
{
    if (list_count(&sem->wait_list))
    {
        list_node_t *node = list_remove_first(&sem->wait_list);
        task_t *task = list_node_parent(node, task_t, wait_node);
        task_set_ready(task);
        task_dispatch();
    }
    else
    {
        sem->count++;
    }
}
int sem_count(sem_t *sem)
{
    int count = sem->count;
    return count;
}