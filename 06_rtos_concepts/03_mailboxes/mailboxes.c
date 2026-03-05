/**
 * @file mailboxes.c
 * @brief Phase 6 — RTOS Concepts: Message Queues (Mailboxes)
 *
 * Message queues decouple producers from consumers. A producer task (or ISR)
 * posts typed messages; a consumer task reads them in order.
 *
 * Concepts covered:
 *   1. Ring-buffer-based message queue (lock-free for single producer/consumer)
 *   2. Typed messages with a discriminator tag (tagged union pattern)
 *   3. Queue-full and queue-empty policies
 *   4. ISR → task communication via queue
 *   5. Task → task pipeline (sensor → filter → comms)
 *
 * BUILD:  cmake --build build --target p6_mailboxes
 */

#include "embedded_types.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * TYPED MESSAGE
 * Using a tagged union ensures the consumer can always dispatch correctly.
 * This pattern maps directly to FreeRTOS queues (which send fixed-size items).
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum
{
    MSG_SENSOR_RAW,    /* raw ADC sample from ISR */
    MSG_TEMP_FILTERED, /* filtered temperature in 0.1°C units */
    MSG_UART_TX,       /* bytes to send over UART */
    MSG_ALARM,         /* alert condition */
    MSG_SHUTDOWN       /* orderly terminate */
} msg_type_t;

typedef struct
{
    msg_type_t type;
    uint32_t timestamp;
    union
    {
        struct
        {
            uint16_t adc_counts;
            uint8_t channel;
        } raw;
        struct
        {
            int16_t celsius_x10;
        } temp;
        struct
        {
            uint8_t data[8];
            uint8_t len;
        } uart;
        struct
        {
            uint8_t code;
            const char *msg;
        } alarm;
    } payload;
} message_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * MESSAGE QUEUE (power-of-2 capacity ring buffer)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define QUEUE_CAPACITY 8U /* must be power of 2 */
#define QUEUE_MASK (QUEUE_CAPACITY - 1U)

typedef struct
{
    message_t buf[QUEUE_CAPACITY];
    volatile uint32_t head; /* producer writes here (index) */
    volatile uint32_t tail; /* consumer reads here (index) */
    const char *name;
    uint32_t overflow_count;
} queue_t;

static void queue_init(queue_t *q, const char *name)
{
    q->head = q->tail = 0U;
    q->name = name;
    q->overflow_count = 0U;
}

static bool queue_is_full(const queue_t *q)
{
    return ((q->head - q->tail) >= QUEUE_CAPACITY);
}

static bool queue_is_empty(const queue_t *q)
{
    return (q->head == q->tail);
}

static uint32_t queue_count(const queue_t *q)
{
    return q->head - q->tail;
}

/* Post message; returns false on overflow (drop policy) */
static bool queue_send(queue_t *q, const message_t *msg)
{
    if (queue_is_full(q))
    {
        q->overflow_count++;
        printf("  [%s] OVERFLOW! (dropped msg type=%d)\n", q->name, msg->type);
        return false;
    }
    q->buf[q->head & QUEUE_MASK] = *msg;
    q->head++; /* atomic on 32-bit Cortex-M (single-word write) */
    return true;
}

/* Receive message; returns false if empty */
static bool queue_recv(queue_t *q, message_t *out)
{
    if (queue_is_empty(q))
        return false;
    *out = q->buf[q->tail & QUEUE_MASK];
    q->tail++;
    return true;
}

/* ─── Simulated task/ISR pipeline ────────────────────────────────────────── */

static uint32_t g_tick = 0U;
static queue_t q_raw_samples; /* ISR → sensor_task */
static queue_t q_filtered;    /* sensor_task → comms_task */

/* Simulated ADC ISR: called every "tick" */
static void adc_dma_half_complete_isr(void)
{
    static uint16_t fake_adc = 2048U;
    fake_adc = (uint16_t)(fake_adc + (g_tick & 0x3FU) - 16U); /* drift */
    message_t m = {
        .type = MSG_SENSOR_RAW,
        .timestamp = g_tick,
        .payload.raw = {.adc_counts = fake_adc, .channel = 0U}};
    queue_send(&q_raw_samples, &m);
}

/* Sensor task: read raw, apply low-pass filter, forward filtered temperature */
static void sensor_task(void)
{
    message_t m;
    if (!queue_recv(&q_raw_samples, &m))
        return;
    /* Low-pass: temp = 0.875*prev + 0.125*new (IIR filter, integer math) */
    static int32_t filtered = 250; /* 25.0°C initial */
    int32_t raw_temp = (int32_t)(m.payload.raw.adc_counts * 33 / 4096) - 2;
    filtered = (filtered * 7 + raw_temp * 10) / 8;
    printf("[t%02u] SENSOR filtered=%d.%d°C  (raw ADC=%u)\n",
           g_tick, filtered / 10, filtered % 10, m.payload.raw.adc_counts);
    message_t out = {
        .type = MSG_TEMP_FILTERED,
        .timestamp = g_tick,
        .payload.temp.celsius_x10 = (int16_t)filtered};
    queue_send(&q_filtered, &out);
}

/* Comms task: pop filtered temperature, format & "send" over UART */
static void comms_task(void)
{
    message_t m;
    if (!queue_recv(&q_filtered, &m))
        return;
    int16_t t = m.payload.temp.celsius_x10;
    printf("[t%02u] COMMS  TX: \"TEMP %d.%d\\r\\n\"  (q_raw=%u pending)\n",
           g_tick, t / 10, t % 10, queue_count(&q_raw_samples));
}

/* Alarm task: checks for extreme values */
static void alarm_task(void)
{
    /* Peek at queue without consuming (scan) */
    uint32_t i = q_filtered.tail;
    while (i != q_filtered.head)
    {
        const message_t *m = &q_filtered.buf[i & QUEUE_MASK];
        if (m->payload.temp.celsius_x10 > 400)
        { /* >40.0°C */
            printf("[t%02u] ALARM! High temp %d.%d°C!\n",
                   g_tick,
                   m->payload.temp.celsius_x10 / 10,
                   m->payload.temp.celsius_x10 % 10);
        }
        i++;
    }
}

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 6 — Lesson 3: Message Queues         ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");
    printf("Pipeline: ADC ISR → q_raw → SENSOR_TASK → q_filtered → COMMS_TASK\n\n");

    queue_init(&q_raw_samples, "Q_RAW");
    queue_init(&q_filtered, "Q_FILT");

    /* Simulate 15 ticks */
    for (g_tick = 0U; g_tick < 15U; g_tick++)
    {
        adc_dma_half_complete_isr(); /* fills q_raw every tick */
        if (g_tick % 2 == 0U)
            sensor_task(); /* runs at half rate */
        if (g_tick % 3 == 0U)
        {
            comms_task();
            alarm_task();
        }
    }

    /* Test overflow */
    printf("\n── Queue overflow test ──\n");
    queue_t q_test;
    queue_init(&q_test, "Q_TEST");
    message_t tmp = {.type = MSG_ALARM};
    for (int i = 0; i <= (int)QUEUE_CAPACITY + 1; i++)
        queue_send(&q_test, &tmp);
    printf("  overflow_count = %u\n", q_test.overflow_count);

    printf("\nEXERCISES:\n");
    printf("  1. Change the overflow policy from DROP to OVERWRITE (ring\n");
    printf("     buffer drops the oldest item). When is each policy better?\n");
    printf("  2. Add queue_peek() — read without consuming. Use it to\n");
    printf("     implement a 'wait for specific message type' loop.\n");
    printf("  3. Extend message_t with a priority field. Implement\n");
    printf("     queue_send_urgent() that inserts at the head instead of tail.\n");
    return 0;
}
