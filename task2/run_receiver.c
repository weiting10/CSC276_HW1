// run_receiver.c -- Flush+Reload covert channel, RECEIVER side
//
// Build:  gcc -O2 -o run_receiver run_receiver.c
// Run:    taskset -c 2 ./run_receiver
//
// See run_sender.c for the shared-memory idea. This side flushes the shared
// lines and times reloads to recover the sender's bits.
//
// Per byte round:
//   1. Flush DATA[0..7] and REQ  (reset the wire to a known "empty" state).
//   2. Poll REQ until it appears (sender says "byte ready").
//   3. Read the 8 data lines by TIMING a reload of each -- fast = 1, slow = 0.
//      (No flush right before reading, or we'd erase the sender's 1s.)
//   4. Strobe ACK until the sender drops REQ, then stop asserting ACK.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "mem_utils.h"

#define PATH    "/u/wtan10/channel_file"
#define STRIDE  4096
#define N_DATA  8
#define IDX_REQ 8
#define IDX_ACK 9
#define N_LINES 10
#define ASSERT_REPS 4
#define THRESHOLD 136

static uint8_t *g_base;

// This function takes in the index input, and output the memory address of that index;
// 0-7 is for the 8 bits of binary code of the letters; and 8 and 9th bits are for REQ and ACK
static inline uint8_t *line(int idx) { return g_base + (size_t)idx * STRIDE; }

// ?????????
static inline void set_line(int idx) {
    uint8_t *p = line(idx);
    for (int k = 0; k < ASSERT_REPS; k++) maccess(p);
}

// Time a reload WITHOUT flushing first: is this line currently cached?
static inline int is_cached(int idx, uint32_t threshold) {
    return memaccesstime(line(idx)) < threshold;
}

// Poll REQ (flush + time) until the sender is observed touching it.
// This function will repeatedly flushing and accessing the "REQ" index; if it sees the "REQ" is accessed/latency < threshold, then 
// it will return and exit this function and move on
static void wait_req(uint32_t threshold) {
    uint8_t *p = line(IDX_REQ);
    for (;;) {
        flush(p);
        for (volatile int d = 0; d < 50; d++) {}
        if (memaccesstime(p) < threshold) return;
    }
}

/*

static uint32_t calibrate(void) {
    uint8_t *p = line(0);
    uint64_t hit = 0, miss = 0;
    const int N = 2000;
    for (int i = 0; i < N; i++) {
        maccess(p);
        asm volatile("mfence");
        hit += memaccesstime(p);
    }
    for (int i = 0; i < N; i++) {
        flush(p);
        asm volatile("mfence");
        miss += memaccesstime(p);
    }
    uint32_t h = hit / N, m = miss / N;
    fprintf(stderr, "[receiver] hit~%u miss~%u threshold=%u\n", h, m, (h + m) / 2);
    return (h + m) / 2;
}

*/


// Receive one byte using the round protocol above.
static uint8_t recv_byte(uint32_t threshold) {
    // 1. Reset the wire.
    for (int i = 0; i < N_DATA; i++) flush(line(i));
    flush(line(IDX_REQ));
    asm volatile("mfence");

    // 2. Wait for the sender's REQ strobe.
    wait_req(threshold);

    // 3. Sample the 8 data lines. The sender keeps re-touching REQ and its
    //    1-bits while waiting for our ACK, so the bits are hot right now.
    // Initialize b = 0 , and go through each of the 8 bits; for the bits that is cached, the functions turns the corresponding bit
    // in b to 1
    uint8_t b = 0;
    for (int i = 0; i < N_DATA; i++)
        if (is_cached(i, threshold)) b |= (1u << i);

    // 4. Acknowledge: assert ACK until the sender stops asserting REQ.
    uint8_t *req = line(IDX_REQ);
    for (;;) {
        set_line(IDX_ACK);    // This will access the IDX_ACK multiple times
        flush(req);           // kick req back to DRAM
        for (volatile int d = 0; d < 50; d++) {}
        if (memaccesstime(req) >= threshold) break;  // The sender will repeatedly hammer REQ until it receives the ACK; if req is 
        // still in the cache after "flush(req)", then we know the sender hasn't received ACK yet.
    }
    return b;
}

int main(void) {
    // open the file in "fd"
    int fd = open(PATH, O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }

    // make sure the file is big enough/ greater than 10 cache line size
    struct stat st;
    if (fstat(fd, &st) < 0) { perror("fstat"); return 1; }
    if ((size_t)st.st_size < (size_t)N_LINES * STRIDE) {
        fprintf(stderr, "shared_file too small: need %d bytes, have %ld\n",
                N_LINES * STRIDE, (long)st.st_size);
        return 1;
    }

    // map the file into memory
    g_base = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (g_base == MAP_FAILED) { perror("mmap"); return 1; }

    uint32_t threshold = THRESHOLD;

    printf("Please press enter.\n");
    fflush(stdout);
    getchar();                       // wait for the user to press Enter
    printf("Receiver now listening.\n");
    fflush(stdout);

    // Frame: 4-byte little-endian length, then that many payload bytes.
    // this makes arbitrary length possible
    uint32_t len = 0;
    for (int i = 0; i < 4; i++)
        len |= ((uint32_t)recv_byte(threshold)) << (8 * i);

    // Sanity clamp so a corrupted length can't make us loop forever.
    if (len > 1u << 20) {
        fprintf(stderr, "[receiver] bad length %u, aborting\n", len);
        return 1;
    }

    
    for (uint32_t i = 0; i < len; i++) {
        uint8_t c = recv_byte(threshold);
        putchar(c);      // print the character on the screen 
    }
    putchar('\n');
    fflush(stdout);
    return 0;
}
