#include <linux/rcupdate.h>

#define PATTERN_NAME blk_execute_rq_nowait_first
#define PATTERN_ARGS

PATTERN_OLD { rcu_read_lock_sched(); }

PATTERN_NEW { rcu_read_lock(); }

#define PATTERN_NAME blk_execute_rq_nowait_second
#define PATTERN_ARGS

PATTERN_OLD { rcu_read_unlock_sched(); }

PATTERN_NEW { rcu_read_unlock(); }
