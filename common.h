#ifndef COMMON_H_
#define COMMON_H_

#define QUEUE_NAME     "/sbcd"
#define QUEUE_NAMEGSM  "/sbcdgsm"
#define MAX_SIZE       1024

#define MSG_STOP       99
#define MSG_SIGN        0

#define CHECK(x) \
    do { \
        if (!(x)) { \
            fprintf(stderr, "%s:%d: ", __func__, __LINE__); \
            perror(#x); \
            exit(-1); \
        } \
    } while (0) \



#endif /* #ifndef COMMON_H_ */
