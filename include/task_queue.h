#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

void task_queue_init(void);
void task_queue_push(int client_fd);
int task_queue_pop(void);

#endif // TASK_QUEUE_H