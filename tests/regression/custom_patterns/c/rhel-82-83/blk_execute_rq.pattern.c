#include <stdbool.h>

void blk_account_io_start(struct request *req, bool new_io);

#define PATTERN_NAME blk_execute_rq_nowait
#define PATTERN_ARGS struct request *rq

PATTERN_OLD {}

PATTERN_NEW { blk_account_io_start(rq, true); }
