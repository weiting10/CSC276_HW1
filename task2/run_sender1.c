#include "common.h"
#include <string.h>

static void send_bit(void *addr, uint64_t slot, int bit)
{
    uint64_t start = slot * SLOT_CYCLES;

    while (rdtscp64() < start);

    if (bit) {
        uint64_t go = start + SENDER_DELAY;
        while (rdtscp64() < go);
        maccess(addr);
        maccess(addr);
        maccess(addr);
    }
}

static void send_byte(void *addr, uint64_t *slot, uint8_t byte)
{
    for (int i = 7; i >= 0; i--)
        send_bit(addr, (*slot)++, (byte >> i) & 1);
}

int main(void)
{
    void *addr = open_shared();

    printf("Please type a message.\n");
    fflush(stdout);

    char buf[65536];
    if (!fgets(buf, sizeof(buf), stdin))
        buf[0] = '\0';

    size_t len = strlen(buf);
    if (len && buf[len - 1] == '\n')
        buf[--len] = '\0';

    uint64_t slot = rdtscp64() / SLOT_CYCLES + 5;

    /* preamble */
    for (int i = 0; i < PREAMBLE_SEND; i++)
        send_bit(addr, slot++, 1);

    /* delimiter */
    for (int i = 0; i < 8; i++)
        send_bit(addr, slot++, 0);

    /* data */
    for (size_t i = 0; i < len; i++)
        send_byte(addr, &slot, (uint8_t)buf[i]);

    /* null terminator */
    send_byte(addr, &slot, 0x00);

    while (rdtscp64() < slot * SLOT_CYCLES);

    return 0;
}

