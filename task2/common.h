#ifndef COMMON_H
#define COMMON_H

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "/u/yguo51/276_fall26/mem_utils.h"

#define SHARED_FILE "/u/yguo51/276_fall26/shared_file"

#define SLOT_CYCLES       3000ULL
#define SENDER_DELAY 400ULL
/*#define SENDER_DELAY       250ULL*/
#define SAMPLE_POINT       0.60
#define THRESHOLD          180u
#define VOTE_ROUNDS          1

#define PREAMBLE_SEND       64
#define PREAMBLE_DETECT     20

#define SAMPLE_CYCLES  ((uint64_t)((double)SLOT_CYCLES * SAMPLE_POINT))

static inline void *open_shared(void) {
    int fd = open(SHARED_FILE, O_RDONLY);
    if (fd < 0) { perror("open"); exit(1); }
    void *p = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) { perror("mmap"); exit(1); }
    close(fd);
    return p;
}

#endif /* COMMON_H */

