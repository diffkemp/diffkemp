#include <linux/mmzone.h>

void __mod_node_page_state();
void __mod_lruvec_state();

struct pglist_data *FUNCTION_OLD(lruvec_pgdat, ); // avoid inlining

#define PATTERN_NAME __update_lru_size
#define PATTERN_ARGS struct lruvec *lruvec, enum lru_list lru, int nr_pages

PATTERN_OLD {
    struct pglist_data *pgdat = FUNCTION_OLD(lruvec_pgdat, lruvec);
    __mod_node_page_state(pgdat, NR_LRU_BASE + lru, (long)nr_pages);
    MAPPING(pgdat);
}

PATTERN_NEW {
    struct pglist_data *pgdat = FUNCTION_OLD(lruvec_pgdat, lruvec);
    __mod_lruvec_state(lruvec, NR_LRU_BASE + lru, nr_pages);
    MAPPING(pgdat);
}
