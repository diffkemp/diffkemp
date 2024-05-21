#include <linux/cpumask.h>
#include <linux/sched/isolation.h>

int set_cpus_allowed_ptr();

#define PATTERN_NAME __kthread_create_on_node
#define PATTERN_ARGS struct task_struct *task, const struct cpumask *new_mask

PATTERN_OLD { set_cpus_allowed_ptr(task, cpu_all_mask); }

PATTERN_NEW {
    set_cpus_allowed_ptr(task, housekeeping_cpumask(HK_FLAG_KTHREAD));
}
