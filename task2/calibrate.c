// calibrate.c
#include "/u/yguo51/276_fall26/mem_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define SHARED_FILE "/u/yguo51/276_fall26/shared_file"
#define N 2000

int main(void) {
    int fd = open(SHARED_FILE, O_RDONLY);
    void *p = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);

    /* warm up */
    for (int i = 0; i < 100; i++) { maccess(p); flush(p); }

    /* measure RAM miss: flush then immediately reload */
    printf("=== RAM MISS (flush -> measure) ===\n");
    uint32_t miss_min = ~0u, miss_max = 0, miss_sum = 0;
    for (int i = 0; i < N; i++) {
        flush(p);
        asm volatile("mfence");
        uint32_t t = memaccesstime(p);
        if (t < miss_min) miss_min = t;
        if (t > miss_max) miss_max = t;
        miss_sum += t;
    }
    printf("  min=%-5u  avg=%-5u  max=%u\n",
           miss_min, miss_sum / N, miss_max);

    /* measure cache hit: load then immediately reload */
    printf("=== CACHE HIT (access -> measure) ===\n");
    uint32_t hit_min = ~0u, hit_max = 0, hit_sum = 0;
    for (int i = 0; i < N; i++) {
        maccess(p);
        asm volatile("mfence");
        uint32_t t = memaccesstime(p);
        if (t < hit_min) hit_min = t;
        if (t > hit_max) hit_max = t;
        hit_sum += t;
    }
    printf("  min=%-5u  avg=%-5u  max=%u\n",
           hit_min, hit_sum / N, hit_max);

    uint32_t suggested = (miss_sum / N + hit_sum / N) / 2;
    printf("\nSuggested THRESHOLD = %u\n", suggested);
    printf("(midpoint between avg hit and avg miss)\n");
    return 0;
}

