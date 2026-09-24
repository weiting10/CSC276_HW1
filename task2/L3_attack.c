 
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "mem_utils.h"   // put mem_utils.h in the same folder (or use -I)
 
#define SAMPLES   1000
#define BUF_SIZE  4096   // one page is plenty for L1/DRAM
#define EVICT_BUF_SIZE 46137344   // double the size of L2 Dcache


int main(void)
{
    uint8_t *buf = malloc(BUF_SIZE);
    if (buf == NULL) {
        perror("malloc");
        return 1;
    }

    uint8_t *evict_buf = malloc(EVICT_BUF_SIZE);
    if (evict_buf == NULL) {
        perror("malloc");
        return 1;
    }
 
    // Write to every byte so the OS gives us a real physical page.
    // (Untouched malloc'd pages can all map to one shared "zero page".)
    for (int i = 0; i < BUF_SIZE; i++){
        buf[i] = 1;
    }

    for (int i = 0; i < EVICT_BUF_SIZE; i++){
        evict_buf[i] = 1;
    }
 
    uint8_t *target = buf;   // the address we will time
 
    // Store results in arrays; printing inside the loop would pollute the cache.
    uint32_t l3_latency[SAMPLES];
 
    for (int i = 0; i < SAMPLES; i++) {
 
        maccess(target);            // make sure the page/TLB entry is warm
        flush(target);              // clflush: evict line from L1, L2, L3
        asm volatile("mfence");     // wait for the flush to actually finish
 
        maccess(target);            // bring it into L1 (it already is, but be sure)
        asm volatile("mfence");

	for (int rep=0; rep < 3 ; rep++){
		for (int i=0; i < EVICT_BUF_SIZE; i += 64){
			maccess(evict_buf+i);
		}
	}
	
	
	asm volatile("mfence");
        l3_latency[i] = memaccesstime(target);
 
    }
 
    // ---- Output as CSV so you can plot a histogram later ----------------
    printf("l3\n");
    for (int i = 0; i < SAMPLES; i++){
        printf("%u\n", l3_latency[i]);
    }
    free(buf);
    free(evict_buf);
    return 0;
}
