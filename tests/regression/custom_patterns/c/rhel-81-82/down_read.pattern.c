// TODO, doesn't work atm
// Generates:
//  %3 = icmp slt i64 %2, 1
// Instead of:
//  %3 = icmp sle i64 %2, 0
//  %4 = zext i1 %3 to i32
//  %5 = sext i32 %4 to i64
//  %6 = icmp ne i64 %5, 0
// Both are equal though.

#include <linux/spinlock_types.h>
#include <linux/rwsem.h>
#include <stdbool.h>

#define RWSEM_READER_OWNED (1UL << 0)
#define TASK_UNINTERRUPTIBLE 2
#define DEBUG_RWSEMS_WARN_ON(c, sem)

long FUNCTION_OLD(atomic_long_inc_return_acquire, ); // avoid inlining
struct rw_semaphore *rwsem_down_read_failed();
void rwsem_set_reader_owned();
bool rwsem_read_trylock();
void *rwsem_down_read_slowpath();

#define PATTERN_NAME __down_read
#define PATTERN_ARGS struct rw_semaphore *sem

PATTERN_OLD {
    if (unlikely((FUNCTION_OLD(atomic_long_inc_return_acquire, &sem->count) <= 0))) {
        rwsem_down_read_failed(sem);
        DEBUG_RWSEMS_WARN_ON(!((unsigned long)sem->owner & RWSEM_READER_OWNED),
                             sem);
    } else {
        rwsem_set_reader_owned(sem);
    }
}

PATTERN_NEW {
    if (!rwsem_read_trylock(sem)) {
        rwsem_down_read_slowpath(sem, TASK_UNINTERRUPTIBLE);
        DEBUG_RWSEMS_WARN_ON(!is_rwsem_reader_owned(sem), sem);
    } else {
        rwsem_set_reader_owned(sem);
    }
}
