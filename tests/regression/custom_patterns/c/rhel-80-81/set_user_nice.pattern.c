#include <stdbool.h>

#define DEQUEUE_SLEEP 0x01
#define ENQUEUE_WAKEUP 0x01

void psi_enqueue(struct task_struct *p, bool wakeup);
void psi_dequeue(struct task_struct *p, bool wakeup);

#define PATTERN_NAME dequeue_task
#define PATTERN_ARGS struct rq *rq, struct task_struct *p, int flags

PATTERN_OLD {}

PATTERN_NEW { psi_dequeue(p, flags & DEQUEUE_SLEEP); }

#define PATTERN_NAME enqueue_task
#define PATTERN_ARGS struct rq *rq, struct task_struct *p, int flags

PATTERN_OLD {}

PATTERN_NEW { psi_enqueue(p, flags & ENQUEUE_WAKEUP); }
