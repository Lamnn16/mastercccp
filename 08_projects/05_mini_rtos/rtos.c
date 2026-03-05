/**
 * @file rtos.c
 * @brief Capstone 5 — Mini Cooperative RTOS Implementation
 *
 * This is a ground-up cooperative RTOS in ~200 lines of C.
 * Compare each function to its FreeRTOS equivalent:
 *   rtos_task_create()  ↔  xTaskCreate()
 *   rtos_delay()        ↔  vTaskDelay()
 *   rtos_sem_take()     ↔  xSemaphoreTake()
 *   rtos_sem_give()     ↔  xSemaphoreGive()
 *   rtos_mailbox_send() ↔  xQueueSend()
 *   rtos_start()        ↔  vTaskStartScheduler()
 */

#include "rtos.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ─── Scheduler state ────────────────────────────────────────────────────── */
static rtos_tcb_t s_tasks[RTOS_MAX_TASKS];
static uint8_t s_task_count = 0U;
static int8_t s_current = -1;
static volatile uint32_t s_ticks = 0U;

/* ─── Init ───────────────────────────────────────────────────────────────── */
void rtos_init(void)
{
    memset(s_tasks, 0, sizeof(s_tasks));
    s_task_count = 0U;
    s_current = -1;
    s_ticks = 0U;
}

rtos_task_handle_t rtos_task_create(task_fn_t fn, void *param,
                                    uint8_t priority, const char *name)
{
    if (s_task_count >= RTOS_MAX_TASKS)
        return NULL;
    rtos_tcb_t *t = &s_tasks[s_task_count++];
    t->fn = fn;
    t->param = param;
    t->priority = priority;
    t->ready = true;
    t->deleted = false;
    t->name = name;
    t->run_count = 0U;
    t->delay_ticks = 0U;
    return t;
}

/* ─── Scheduler: pick highest-priority ready task ────────────────────────── */
static int8_t pick_next(void)
{
    int8_t best = -1;
    uint8_t best_pri = 0U;
    static uint8_t last = 0U;
    uint8_t start = (uint8_t)((last + 1U) % s_task_count);
    for (uint8_t pass = 0U; pass < s_task_count; pass++)
    {
        uint8_t i = (uint8_t)((start + pass) % s_task_count);
        if (s_tasks[i].ready && !s_tasks[i].deleted)
        {
            if (best < 0 || s_tasks[i].priority > best_pri)
            {
                best = (int8_t)i;
                best_pri = s_tasks[i].priority;
            }
        }
    }
    if (best >= 0)
        last = (uint8_t)best;
    return best;
}

static void tick(void)
{
    s_ticks++;
    for (uint8_t i = 0U; i < s_task_count; i++)
    {
        if (!s_tasks[i].ready && !s_tasks[i].deleted)
        {
            if (s_tasks[i].delay_ticks > 0U)
                s_tasks[i].delay_ticks--;
            if (s_tasks[i].delay_ticks == 0U)
                s_tasks[i].ready = true;
        }
    }
}

void rtos_start(void)
{
    uint32_t ticks_run = 0U;
    while (ticks_run < RTOS_TICK_LIMIT)
    {
        int8_t idx = pick_next();
        if (idx >= 0)
        {
            s_current = idx;
            s_tasks[idx].run_count++;
            s_tasks[idx].fn(s_tasks[idx].param);
            if (s_tasks[idx].ready)
            { /* still ready — just ran */
            }
        }
        tick();
        ticks_run++;
        s_current = -1;
    }
}

void rtos_yield(void)
{
    /* Cooperative: just return to scheduler (task returns from fn) */
}

void rtos_delay(uint32_t ticks)
{
    if (s_current < 0)
        return;
    s_tasks[s_current].ready = false;
    s_tasks[s_current].delay_ticks = ticks;
}

void rtos_task_delete(rtos_task_handle_t handle)
{
    if (handle)
        handle->deleted = true;
}

uint32_t rtos_tick(void) { return s_ticks; }

/* ─── Semaphore ──────────────────────────────────────────────────────────── */
void rtos_sem_init(rtos_sem_t *sem, int32_t initial, uint32_t max)
{
    sem->count = initial;
    sem->max = max;
}

bool rtos_sem_take(rtos_sem_t *sem)
{
    if (sem->count <= 0)
        return false;
    sem->count--;
    return true;
}

bool rtos_sem_give(rtos_sem_t *sem)
{
    if ((uint32_t)sem->count >= sem->max)
        return false;
    sem->count++;
    return true;
}

bool rtos_sem_give_from_isr(rtos_sem_t *sem)
{
    return rtos_sem_give(sem); /* same on cooperative; real ISR needs memory barrier */
}

/* ─── Mailbox ────────────────────────────────────────────────────────────── */
#define MB_MASK (RTOS_MAILBOX_SIZE - 1U)

void rtos_mailbox_init(rtos_mailbox_t *mb)
{
    mb->head = mb->tail = 0U;
}

bool rtos_mailbox_send(rtos_mailbox_t *mb, uint32_t msg)
{
    if (rtos_mailbox_count(mb) >= RTOS_MAILBOX_SIZE)
        return false;
    mb->buf[mb->head & MB_MASK] = msg;
    mb->head++;
    return true;
}

bool rtos_mailbox_send_from_isr(rtos_mailbox_t *mb, uint32_t msg)
{
    return rtos_mailbox_send(mb, msg);
}

bool rtos_mailbox_receive(rtos_mailbox_t *mb, uint32_t *msg)
{
    if (mb->head == mb->tail)
        return false;
    *msg = mb->buf[mb->tail & MB_MASK];
    mb->tail++;
    return true;
}

uint32_t rtos_mailbox_count(const rtos_mailbox_t *mb)
{
    return mb->head - mb->tail;
}

/* ─── Stats ──────────────────────────────────────────────────────────────── */
void rtos_print_stats(void)
{
    printf("\n── RTOS Task Statistics (after %u ticks) ──\n", s_ticks);
    for (uint8_t i = 0U; i < s_task_count; i++)
    {
        printf("  %-16s pri=%u  ran=%u  %s\n",
               s_tasks[i].name,
               s_tasks[i].priority,
               s_tasks[i].run_count,
               s_tasks[i].deleted ? "[DELETED]" : "");
    }
}
