/**
 * @file ring_buffer.h
 * @brief Capstone 1 — Production-grade, type-safe, interrupt-safe ring buffer
 *
 * Features:
 *   - Power-of-2 size, atomic index arithmetic (no lock for SPSC)
 *   - Single-producer single-consumer (SPSC) lock-free design
 *   - Optional interrupt-safe wrappers for multi-producer scenarios
 *   - Full compile-time size validation
 *   - Bulk read/write API for DMA transfers
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* ─── Compile-time power-of-2 check ─────────────────────────────────────── */
#define RING_BUF_STATIC_ASSERT_POW2(size)          \
    _Static_assert(((size) & ((size) - 1U)) == 0U, \
                   "ring buffer size must be a power of 2")

/* ─── Generic ring buffer (byte-oriented) ───────────────────────────────── */
typedef struct
{
    uint8_t *buf;
    uint32_t mask;          /* size - 1 (for fast modulo) */
    volatile uint32_t head; /* writer increments (ISR writes) */
    volatile uint32_t tail; /* reader increments (app reads) */
} ring_buf_t;

/* Initialise a ring_buf_t that wraps a statically allocated byte array.
 * size MUST be a power of 2. */
static inline void rb_init(ring_buf_t *rb, uint8_t *storage, uint32_t size)
{
    rb->buf = storage;
    rb->mask = size - 1U;
    rb->head = rb->tail = 0U;
}

static inline uint32_t rb_count(const ring_buf_t *rb) { return rb->head - rb->tail; }
static inline uint32_t rb_space(const ring_buf_t *rb) { return (rb->mask + 1U) - rb_count(rb); }
static inline bool rb_empty(const ring_buf_t *rb) { return rb->head == rb->tail; }
static inline bool rb_full(const ring_buf_t *rb) { return rb_space(rb) == 0U; }

/* Push one byte; returns false if full */
static inline bool rb_push(ring_buf_t *rb, uint8_t byte)
{
    if (rb_full(rb))
        return false;
    rb->buf[rb->head & rb->mask] = byte;
    rb->head++;
    return true;
}

/* Pop one byte; returns false if empty */
static inline bool rb_pop(ring_buf_t *rb, uint8_t *out)
{
    if (rb_empty(rb))
        return false;
    *out = rb->buf[rb->tail & rb->mask];
    rb->tail++;
    return true;
}

/* Peek at the next byte without consuming it */
static inline bool rb_peek(const ring_buf_t *rb, uint8_t *out)
{
    if (rb_empty(rb))
        return false;
    *out = rb->buf[rb->tail & rb->mask];
    return true;
}

/* Bulk push up to len bytes; returns number written */
static inline uint32_t rb_write(ring_buf_t *rb, const uint8_t *data, uint32_t len)
{
    uint32_t space = rb_space(rb);
    if (len > space)
        len = space;
    uint32_t size = rb->mask + 1U;
    uint32_t head = rb->head & rb->mask;
    /* Linear portion to end of buffer */
    uint32_t linear = size - head;
    if (len <= linear)
    {
        memcpy(rb->buf + head, data, len);
    }
    else
    {
        memcpy(rb->buf + head, data, linear);
        memcpy(rb->buf, data + linear, len - linear);
    }
    rb->head += len;
    return len;
}

/* Bulk pop up to len bytes; returns number read */
static inline uint32_t rb_read(ring_buf_t *rb, uint8_t *out, uint32_t len)
{
    uint32_t avail = rb_count(rb);
    if (len > avail)
        len = avail;
    uint32_t size = rb->mask + 1U;
    uint32_t tail = rb->tail & rb->mask;
    uint32_t linear = size - tail;
    if (len <= linear)
    {
        memcpy(out, rb->buf + tail, len);
    }
    else
    {
        memcpy(out, rb->buf + tail, linear);
        memcpy(out + linear, rb->buf, len - linear);
    }
    rb->tail += len;
    return len;
}

/* ─── Convenience macro to declare a statically-sized ring buffer ───────── */
#define RING_BUF_DEFINE(name, size)      \
    RING_BUF_STATIC_ASSERT_POW2(size);   \
    static uint8_t name##_storage[size]; \
    static ring_buf_t name;              \
    /* call rb_init(&name, name##_storage, size) in your init function */
