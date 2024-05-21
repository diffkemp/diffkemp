#include <asm/alternative.h>

#define PATTERN_NAME arch_atomic_dec
#define PATTERN_ARGS atomic_t *v

PATTERN_OLD { asm volatile(LOCK_PREFIX "decl %0" : "+m"(v->counter)); }

PATTERN_NEW {
    asm volatile(LOCK_PREFIX "decl %0" : "+m"(v->counter)::"memory");
}
