// run_sender.c -- Flush+Reload covert channel, SENDER side
//
// Build:  gcc -O2 -o run_sender run_sender.c
// Run:    taskset -c 4 ./run_sender
//
// Idea: sender and receiver both mmap the SAME read-only file. A read-only
// file page lives once in the OS page cache, so both processes' mappings are
// backed by the SAME physical memory. Cache lines are indexed by physical
// address, so a line the sender pulls into the shared L3 is visible (as a
// fast reload) to the receiver, even on another core. That shared line IS
// the wire.
//
// Channel layout (each signal on its own page so the HW prefetcher can't
// pull a neighbor in and fake a '1'):
//   DATA[0..7] : 8 bits sent in parallel = one byte per round
//   REQ        : sender strobes "a byte is ready"   (receiver polls this)
//   ACK        : receiver strobes "I read the byte" (sender polls this)
//
// Framing: send a 4-byte little-endian length first, then that many bytes.
// The receiver therefore knows exactly how many bytes to expect.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "mem_utils.h"

#define PATH    "/u/wtan10/channel_file"
#define STRIDE  4096          // one signal per page
#define N_DATA  8             // 8 data lines = 1 byte/round

// Line index map (indices 0..7 are data, 8 is REQ, 9 is ACK).
#define IDX_REQ  8
#define IDX_ACK  9
#define N_LINES  10           // file must be >= N_LINES * STRIDE bytes
#define THRESHOLD 136

// How many times to touch a line when asserting a signal. Touching once
// races with the receiver's flush; a short burst makes detection reliable.
#define ASSERT_REPS 4

static uint8_t *g_base;       // start of the shared mapping

static inline uint8_t *line(int idx) { return g_base + (size_t)idx * STRIDE; }

// Assert a signal / send a 1: touch the line a few times so a polling
// receiver reliably catches it in the cache.
static inline void set_line(int idx) {
    uint8_t *p = line(idx);
    for (int k = 0; k < ASSERT_REPS; k++) maccess(p);
}

// Poll a line until it is observed CACHED (fast). Used to wait for ACK.
// We flush, pause briefly to let the other side get an access in, then time
// a reload; fast reload means the other side is currently touching it.
static void wait_until_cached(int idx, uint32_t threshold) {
    uint8_t *p = line(idx);
    for (;;) {
        flush(p);
        for (volatile int d = 0; d < 50; d++) {}   // let asserter run
        if (memaccesstime(p) < threshold) return;
    }
}

// Poll a line until it is observed FLUSHED/absent (slow). Used to wait for
// the receiver to DROP its ACK before we start the next round, so one round's
// ACK can't be mistaken for the next round's.
// wait until receiver stop hitting "ACK"
static void wait_until_absent(int idx, uint32_t threshold) {
    uint8_t *p = line(idx);
    int misses = 0;
    for (;;) {
        // Do NOT flush here: we want to observe the natural state. Just time.
        if (memaccesstime(p) >= threshold) {
            if (++misses >= 3) return;   // several slow reads in a row = gone
        } else {
            misses = 0;
        }
    }
}

/*
// Measure a hit/miss threshold at startup: midpoint of a cached access and a
// flushed access to one of our own lines. Matches the Task-2 L3-vs-DRAM gap.
static uint32_t calibrate(void) {
    uint8_t *p = line(0);
    uint64_t hit = 0, miss = 0;
    const int N = 2000;
    for (int i = 0; i < N; i++) {
        maccess(p);                       // ensure cached
        asm volatile("mfence");
        hit += memaccesstime(p);
    }
    for (int i = 0; i < N; i++) {
        flush(p);                         // ensure from DRAM
        asm volatile("mfence");
        miss += memaccesstime(p);
    }
    uint32_t h = hit / N, m = miss / N;
    fprintf(stderr, "[sender] hit~%u miss~%u threshold=%u\n", h, m, (h + m) / 2);
    return (h + m) / 2;
}

*/

// Send one byte: set data lines for 1-bits, strobe REQ, wait for ACK, then
// wait for ACK to drop so the next round starts clean.
static void send_byte(uint8_t b, uint32_t threshold) {
    // 1. Present the data bits.
    for (int i = 0; i < N_DATA; i++)
        if (b & (1u << i)) set_line(i);

    // 2. Strobe REQ, and keep re-asserting REQ + data until the receiver ACKs.
    //    Re-asserting in the wait loop guarantees the bits stay "hot" long
    //    enough for the receiver to sample them, no matter the timing.
    uint8_t *ack = line(IDX_ACK);
    for (;;) {
        set_line(IDX_REQ);
        for (int i = 0; i < N_DATA; i++)
            if (b & (1u << i)) maccess(line(i));   // keep 1-bits hot

        flush(ack);
        for (volatile int d = 0; d < 50; d++) {}
        if (memaccesstime(ack) < threshold) break; // receiver has read it
    }

    // 3. Wait for the receiver to drop ACK before starting the next byte.
    wait_until_absent(IDX_ACK, threshold);
}

int main(void) {
    // Map the shared file read-only: same physical pages as the receiver.
    int fd = open(PATH, O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    struct stat st;
    if (fstat(fd, &st) < 0) { perror("fstat"); return 1; }
    if ((size_t)st.st_size < (size_t)N_LINES * STRIDE) {
        fprintf(stderr, "shared_file too small: need %d bytes, have %ld\n",
                N_LINES * STRIDE, (long)st.st_size);
        return 1;
    }
    g_base = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (g_base == MAP_FAILED) { perror("mmap"); return 1; }

    uint32_t threshold = calibrate();

    printf("Please type a message.\n");
    fflush(stdout);

    // Read a whole line of input (the message) from the user.
    char *msg = NULL;
    size_t cap = 0;
    ssize_t n = getline(&msg, &cap, stdin);
    if (n < 0) { perror("getline"); return 1; }
    if (n > 0 && msg[n - 1] == '\n') { msg[n - 1] = '\0'; n--; }   // strip newline

    uint32_t len = (uint32_t)n;

    // Frame: 4-byte little-endian length, then the payload bytes.
    uint8_t hdr[4] = { len & 0xff, (len >> 8) & 0xff,
                       (len >> 16) & 0xff, (len >> 24) & 0xff };
    for (int i = 0; i < 4; i++) send_byte(hdr[i], threshold);
    for (uint32_t i = 0; i < len; i++) send_byte((uint8_t)msg[i], threshold);

    fprintf(stderr, "[sender] sent %u bytes\n", len);
    free(msg);
    return 0;
}
