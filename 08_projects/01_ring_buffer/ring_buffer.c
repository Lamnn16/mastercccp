/**
 * @file ring_buffer.c
 * @brief Capstone 1 — Ring Buffer: tracked statistics implementation
 *
 * Provides a diagnostic wrapper around the header-only ring_buf_t.
 */

#include "ring_buffer.h"
#include <stdio.h>
#include <stdint.h>

/* Tracked ring buffer: wraps ring_buf_t with overflow/underflow counters */
typedef struct
{
    ring_buf_t rb;
    uint32_t overflow_count;
    uint32_t underflow_count;
    uint32_t peak_usage;
} tracked_rb_t;

static void tracked_rb_init(tracked_rb_t *t, uint8_t *storage, uint32_t size)
{
    rb_init(&t->rb, storage, size);
    t->overflow_count = t->underflow_count = t->peak_usage = 0U;
}

static bool tracked_push(tracked_rb_t *t, uint8_t byte)
{
    bool ok = rb_push(&t->rb, byte);
    if (!ok)
    {
        t->overflow_count++;
        return false;
    }
    uint32_t usage = rb_count(&t->rb);
    if (usage > t->peak_usage)
        t->peak_usage = usage;
    return true;
}

static bool tracked_pop(tracked_rb_t *t, uint8_t *out)
{
    bool ok = rb_pop(&t->rb, out);
    if (!ok)
    {
        t->underflow_count++;
        return false;
    }
    return true;
}

static void tracked_stats(const tracked_rb_t *t)
{
    printf("  capacity=%u  count=%u  peak=%u  overflow=%u  underflow=%u\n",
           t->rb.mask + 1U,
           rb_count(&t->rb),
           t->peak_usage,
           t->overflow_count,
           t->underflow_count);
}

/* Provide a real .c translation unit for the linker */
int ring_buffer_module_version(void) { return 1; }
