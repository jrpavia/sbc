#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <mqueue.h>

#include "common.h"

void init_queue (mqd_t * mq_desc, int open_flags) {
    struct mq_attr attr;

    // initialize the queue attributes
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;

    // create the message queue
    mq_desc = mq_open(QUEUE_NAME, open_flags, 0644, &attr);
    CHECK((mqd_t)-1 != mq_desc);
}

