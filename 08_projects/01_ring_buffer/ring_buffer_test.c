/**
 * @file ring_buffer_test.c
 * @brief Capstone 1 — Ring Buffer: Comprehensive Test Harness
 *
 * A production component needs tests. This harness exercises:
 *   - Boundary conditions (empty/full)
 *   - Wraparound correctness
 *   - Bulk read/write aligned and unaligned to buffer boundary
 *   - Overflow and underflow accounting
 *   - SPSC interleaved producer-consumer simulation
 *
 * BUILD:  cmake --build build --target p8_ring_buffer
 * RUN:    .\build\08_projects\p8_ring_buffer.exe
 */

#include "ring_buffer.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* ─── Test framework ─────────────────────────────────────────────────────── */
static int s_pass = 0, s_fail = 0;
#define TEST(name, expr)                                       \
    do                                                         \
    {                                                          \
        if (expr)                                              \
        {                                                      \
            printf("  PASS  %s\n", name);                      \
            s_pass++;                                          \
        }                                                      \
        else                                                   \
        {                                                      \
            printf("  FAIL  %s  (line %d)\n", name, __LINE__); \
            s_fail++;                                          \
        }                                                      \
    } while (0)

/* ─── Test cases ─────────────────────────────────────────────────────────── */

static void test_basic(void)
{
    puts("\n── Basic operations ──");
    static uint8_t storage[16];
    ring_buf_t rb;
    rb_init(&rb, storage, 16U);

    TEST("empty on init", rb_empty(&rb));
    TEST("not full on init", !rb_full(&rb));
    TEST("count=0 on init", rb_count(&rb) == 0U);
    TEST("space=16 on init", rb_space(&rb) == 16U);

    /* Push one byte */
    bool ok = rb_push(&rb, 0xA5U);
    TEST("push succeeds", ok);
    TEST("count=1 after push", rb_count(&rb) == 1U);
    TEST("not empty after push", !rb_empty(&rb));

    /* Peek without consuming */
    uint8_t peeked = 0U;
    TEST("peek succeeds", rb_peek(&rb, &peeked));
    TEST("peek value correct", peeked == 0xA5U);
    TEST("count still 1", rb_count(&rb) == 1U);

    /* Pop */
    uint8_t out = 0U;
    ok = rb_pop(&rb, &out);
    TEST("pop succeeds", ok);
    TEST("pop value correct", out == 0xA5U);
    TEST("empty after pop", rb_empty(&rb));

    /* Underflow */
    out = 0U;
    ok = rb_pop(&rb, &out);
    TEST("pop on empty fails", !ok);
}

static void test_fill_and_drain(void)
{
    puts("\n── Fill and drain ──");
    static uint8_t storage[8];
    ring_buf_t rb;
    rb_init(&rb, storage, 8U);

    /* Fill completely */
    for (uint8_t i = 0U; i < 8U; i++)
        rb_push(&rb, i);
    TEST("full after 8 pushes", rb_full(&rb));
    TEST("overflow push fails", !rb_push(&rb, 0xFFU));
    TEST("count=8", rb_count(&rb) == 8U);

    /* Drain completely */
    for (uint8_t i = 0U; i < 8U; i++)
    {
        uint8_t v = 0U;
        rb_pop(&rb, &v);
        if (v != i)
        { /* inline check */
            printf("  FAIL  FIFO order error at i=%u got v=%u\n", i, v);
            s_fail++;
            return;
        }
    }
    TEST("FIFO order preserved", true);
    TEST("empty after drain", rb_empty(&rb));
}

static void test_wraparound(void)
{
    puts("\n── Wraparound ──");
    static uint8_t storage[4];
    ring_buf_t rb;
    rb_init(&rb, storage, 4U);

    /* Push 3, pop 3 to offset indices */
    for (int i = 0; i < 3; i++)
        rb_push(&rb, (uint8_t)i);
    for (int i = 0; i < 3; i++)
    {
        uint8_t v;
        rb_pop(&rb, &v);
    }
    TEST("indices offset (not reset to 0)", rb.head == 3U && rb.tail == 3U);

    /* Now push 4 bytes spanning the buffer boundary */
    rb_push(&rb, 0xAAU);
    rb_push(&rb, 0xBBU);
    rb_push(&rb, 0xCCU);
    rb_push(&rb, 0xDDU);
    TEST("full after wrap push", rb_full(&rb));

    uint8_t v = 0U;
    rb_pop(&rb, &v);
    TEST("wrap pop A = 0xAA", v == 0xAAU);
    rb_pop(&rb, &v);
    TEST("wrap pop B = 0xBB", v == 0xBBU);
    rb_pop(&rb, &v);
    TEST("wrap pop C = 0xCC", v == 0xCCU);
    rb_pop(&rb, &v);
    TEST("wrap pop D = 0xDD", v == 0xDDU);
    TEST("empty after wrap drain", rb_empty(&rb));
}

static void test_bulk_io(void)
{
    puts("\n── Bulk read / write ──");
    static uint8_t storage[32];
    ring_buf_t rb;
    rb_init(&rb, storage, 32U);

    const uint8_t src[20] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
        0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13};
    uint32_t written = rb_write(&rb, src, 20U);
    TEST("bulk write 20 bytes", written == 20U);
    TEST("count=20", rb_count(&rb) == 20U);

    uint8_t dst[20] = {0};
    uint32_t read = rb_read(&rb, dst, 20U);
    TEST("bulk read 20 bytes", read == 20U);
    TEST("data matches", memcmp(src, dst, 20U) == 0);

    /* Bulk spanning wraparound */
    /* Offset by 24: push 24, discard 24 */
    for (int i = 0; i < 24; i++)
        rb_push(&rb, 0U);
    for (int i = 0; i < 24; i++)
    {
        uint8_t v;
        rb_pop(&rb, &v);
    }
    /* Now head=tail=24, buffer has space=32. Push 20 bytes spanning boundary */
    written = rb_write(&rb, src, 20U);
    TEST("bulk write spans boundary", written == 20U);
    memset(dst, 0U, 20U);
    read = rb_read(&rb, dst, 20U);
    TEST("bulk read spanning boundary", read == 20U);
    TEST("spanning data correct", memcmp(src, dst, 20U) == 0);
}

static void test_spsc_simulation(void)
{
    puts("\n── SPSC producer-consumer simulation ──");
    static uint8_t storage[64];
    ring_buf_t rb;
    rb_init(&rb, storage, 64U);

    /* Simulate: producer sends 100 bytes in bursts, consumer reads in smaller chunks */
    uint8_t send_val = 0U, recv_val = 0U;
    uint32_t sent = 0U, received = 0U;
    uint32_t burst_sizes[] = {5, 13, 1, 7, 20, 3, 8};
    uint32_t read_sizes[] = {3, 1, 10, 4, 6, 2};
    uint32_t bi = 0U, ri = 0U;
    uint32_t total_sent = 0U, total_recv = 0U;

    while (total_recv < 100U)
    {
        /* Producer burst */
        uint32_t burst = burst_sizes[bi % 7U];
        for (uint32_t b = 0U; b < burst && total_sent < 100U; b++)
        {
            if (rb_push(&rb, send_val))
            {
                send_val++;
                total_sent++;
                sent++;
            }
        }
        bi++;
        /* Consumer read */
        uint32_t chunk = read_sizes[ri % 6U];
        for (uint32_t c = 0U; c < chunk; c++)
        {
            uint8_t v = 0U;
            if (rb_pop(&rb, &v))
            {
                if (v != recv_val)
                {
                    printf("  FAIL  SPSC order: expected %u got %u\n", recv_val, v);
                    s_fail++;
                    return;
                }
                recv_val++;
                total_recv++;
                received++;
            }
        }
        ri++;
    }
    TEST("SPSC: 100 bytes in order", received >= 100U);
}

int main(void)
{
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Capstone 1 — Ring Buffer Test Harness              ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    test_basic();
    test_fill_and_drain();
    test_wraparound();
    test_bulk_io();
    test_spsc_simulation();

    printf("\n── Results ──\n");
    printf("  Passed: %d\n  Failed: %d\n", s_pass, s_fail);
    printf("  %s\n\n", s_fail == 0 ? "ALL TESTS PASS" : "SOME TESTS FAILED");

    printf("EXERCISES:\n");
    printf("  1. Add rb_write_isr() that can be called safely from an ISR even\n");
    printf("     if rb_read() is running in main (verify no data corruption).\n");
    printf("  2. Implement a 'watermark callback': call a function when the\n");
    printf("     buffer crosses 75%% full and when it drops below 25%%.\n");
    printf("  3. Change ring_buf_t to a typed ring buffer using a C macro that\n");
    printf("     takes an element type (e.g. TYPED_RING_BUF(can_frame_t, 16)).\n");
    return s_fail == 0 ? 0 : 1;
}
