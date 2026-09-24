#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "mem_utils.h"

#define SAMPLES   1000
#define BUF_SIZE  32768   // one page is plenty for L1/DRAM
 
int main(void)
{
    uint8_t *buf = malloc(BUF_SIZE);
    if (buf == NULL) {
        perror("malloc");
        return 1;
    }
 
    for (int i = 0; i < BUF_SIZE; i++)
        buf[i] = 1;
 
    uint8_t *target = buf;   // the address we will time
 
    uint32_t l1_latency[SAMPLES];
 
    for (int i = 0; i < SAMPLES; i++) {
 
        maccess(target);            // make sure the page/TLB entry is warm
        flush(target);              // clflush: evict line from L1, L2, L3
        asm volatile("mfence");     // wait for the flush to actually finish
	maccess(target);
	asm volatile("mfence"); 
	l1_latency[i] = memaccesstime(target);
 
 
    }
 
    printf("l1\n");
    for (int i = 0; i < SAMPLES; i++)
        printf("%u\n", l1_latency[i]);
 
    free(buf);
    return 0;
}
