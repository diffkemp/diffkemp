#include <stdbool.h>

void FUNCTION_OLD(task_numa_free, struct task_struct *tsk);
void FUNCTION_NEW(task_numa_free, struct task_struct *tsk, bool final);

#define PATTERN_NAME __put_task_struct
#define PATTERN_ARGS struct task_struct *tsk

PATTERN_OLD { FUNCTION_OLD(task_numa_free, tsk); }

PATTERN_NEW { FUNCTION_NEW(task_numa_free, tsk, true); }
