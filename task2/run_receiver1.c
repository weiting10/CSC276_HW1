#include "common.h"

static int recv_bit(void *addr, uint64_t slot)
{
    uint64_t start  = slot * SLOT_CYCLES;
    uint64_t sample = start + SAMPLE_CYCLES;

    while (rdtscp64() < start);
    flush(addr);

    while (rdtscp64() < sample);

#if VOTE_ROUNDS == 1
    uint32_t t = memaccesstime(addr);

#ifdef DEBUG_TIMING
    fprintf(stderr, "slot %7lu  t=%4u  %s\n",
            (unsigned long)slot, t,
            (t < THRESHOLD) ? "HIT=1" : "MISS=0");
#endif

    return (t < THRESHOLD) ? 1 : 0;

#else
    int votes = 0;
    for (int r = 0; r < VOTE_ROUNDS; r++) {
        for (volatile int d = 0; d < 5; d++);
        uint32_t t = memaccesstime(addr);
#ifdef DEBUG_TIMING
        fprintf(stderr, "slot %7lu  round %d  t=%4u  %s\n",
                (unsigned long)slot, r, t,
                (t < THRESHOLD) ? "HIT=1" : "MISS=0");
#endif
        votes += (t < THRESHOLD) ? 1 : 0;
    }
    return (votes > VOTE_ROUNDS / 2) ? 1 : 0;
#endif
}

int main(void)
{
    void *addr = open_shared();

    printf("Please press enter.\n");
    fflush(stdout);
    getchar();
    printf("Receiver now listening.\n");
    fflush(stdout);

    uint64_t slot = rdtscp64() / SLOT_CYCLES + 2;

    typedef enum { PREAMBLE, WAIT_DELIM, DATA } State;

    State   state       = PREAMBLE;
    int     consec_ones = 0;
    int     zero_count  = 0;
    uint8_t cur_byte    = 0;
    int     bit_pos     = 7;
    char    outbuf[65536];
    int     outlen      = 0;

    for (;;) {
        int bit = recv_bit(addr, slot++);

        switch (state) {

        case PREAMBLE:
            if (bit) {
                if (++consec_ones >= PREAMBLE_DETECT)
                    state = WAIT_DELIM;
            } else {
                consec_ones = 0;
            }
            break;

        case WAIT_DELIM:
            if (!bit) {
                if (++zero_count >= 8) {
                    state    = DATA;
                    cur_byte = 0;
                    bit_pos  = 7;
                }
            } else {
                zero_count = 0;
            }
            break;

        case DATA:
            cur_byte |= (uint8_t)((unsigned)bit << bit_pos);
            if (--bit_pos < 0) {
                if (cur_byte == 0x00)
                    goto done;
                if (outlen < (int)(sizeof(outbuf) - 1))
                    outbuf[outlen++] = (char)cur_byte;
                cur_byte = 0;
                bit_pos  = 7;
            }
            break;
        }
    }

done:
    outbuf[outlen] = '\0';
    printf("%s\n", outbuf);
    fflush(stdout);
    return 0;
}

