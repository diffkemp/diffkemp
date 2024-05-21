#include <linux/mm_types.h>
#include <linux/swap.h>
#include <linux/swapops.h>

#define RADIX_TREE_EXCEPTIONAL_SHIFT 2
#define SWP_TYPE_SHIFT_80                                                      \
    ((sizeof(unsigned long) * 8)                                               \
     - (MAX_SWAPFILES_SHIFT + RADIX_TREE_EXCEPTIONAL_SHIFT))
#define SWP_OFFSET_MASK_80 (1UL << SWP_TYPE_SHIFT_80) - 1

#define PATTERN_NAME swp_offset
#define PATTERN_ARGS unsigned long i

PATTERN_OLD { MAPPING(i & SWP_OFFSET_MASK_80); }

PATTERN_NEW { MAPPING(i & SWP_OFFSET_MASK); }
