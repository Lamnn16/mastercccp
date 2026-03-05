/**
 * @file event_bus.c
 * @brief Capstone 2 — Interrupt-Safe Event Bus
 *
 * An event bus that combines:
 *   - Phase 4 observer pattern (subscriber callbacks)
 *   - Phase 6 message queue (ISR-safe posting)
 *   - Phase 3 ring buffer (zero-dynamic-memory implementation)
 *
 * Key guarantees:
 *   1. event_post() is ISR-safe (no malloc, no blocking)
 *   2. event_dispatch() processes events in main context only
 *   3. No dynamic memory — all arrays are static and compile-time sized
 *   4. Subscription table is lock-free (only modified before run loop)
 *
 * BUILD:  cmake --build build --target p8_event_bus
 */

#include "event_bus.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* ─── Event queue (ring buffer of events) ────────────────────────────────── */
#define EVENT_QUEUE_SIZE 16U /* power of 2 */
#define EVENT_QUEUE_MASK (EVENT_QUEUE_SIZE - 1U)

typedef struct
{
    event_id_t event;
    uint32_t data;
    uint32_t timestamp;
} queued_event_t;

static queued_event_t s_queue[EVENT_QUEUE_SIZE];
static volatile uint32_t s_qhead = 0U, s_qtail = 0U;
static volatile uint32_t s_tick = 0U; /* simulated system tick */

/* ─── Subscription table ─────────────────────────────────────────────────── */
#define MAX_SUBSCRIBERS 8U

typedef struct
{
    event_id_t event_mask; /* which events this handler receives */
    event_handler_t handler;
    const char *name;
} subscription_t;

static subscription_t s_subs[MAX_SUBSCRIBERS];
static uint8_t s_sub_count = 0U;

/* ─── API implementation ─────────────────────────────────────────────────── */

bool event_subscribe(event_id_t event_mask, event_handler_t handler, const char *name)
{
    if (s_sub_count >= MAX_SUBSCRIBERS)
        return false;
    s_subs[s_sub_count++] = (subscription_t){event_mask, handler, name};
    return true;
}

/* ISR-safe: only increments head after writing. Loses event on overflow. */
bool event_post(event_id_t event, uint32_t data)
{
    uint32_t next = (s_qhead + 1U) & EVENT_QUEUE_MASK;
    if (next == s_qtail)
        return false; /* queue full — drop */
    s_queue[s_qhead] = (queued_event_t){event, data, s_tick};
    s_qhead = (s_qhead + 1U) & EVENT_QUEUE_MASK;
    return true;
}

/* Call from main loop — dispatches ONE event per call */
bool event_dispatch_one(void)
{
    if (s_qhead == s_qtail)
        return false;
    queued_event_t ev = s_queue[s_qtail];
    s_qtail = (s_qtail + 1U) & EVENT_QUEUE_MASK;
    for (uint8_t i = 0U; i < s_sub_count; i++)
    {
        if (s_subs[i].event_mask & ev.event)
        {
            s_subs[i].handler(ev.event, ev.data);
        }
    }
    return true;
}

/* Drain all pending events */
void event_dispatch_all(void)
{
    while (event_dispatch_one())
    {
    }
}

uint32_t event_queue_depth(void)
{
    return (s_qhead - s_qtail) & EVENT_QUEUE_MASK;
}

void event_tick(void) { s_tick++; }
