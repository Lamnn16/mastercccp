/**
 * @file rtos.h
 * @brief Capstone 5 — Mini Cooperative RTOS API
 *
 * Integrates Lessons 6.1 (scheduler) + 6.2 (semaphores) + 6.3 (mailboxes)
 * into a single coherent RTOS-like API that mirrors FreeRTOS naming conventions.
 * This makes migration to FreeRTOS straightforward.
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ─── Task ───────────────────────────────────────────────────────────────── */
#define RTOS_MAX_TASKS 8U
#define RTOS_TICK_LIMIT 100U /* stop after N ticks in simulation */

typedef void (*task_fn_t)(void *param);

typedef struct rtos_task_s
{
    task_fn_t fn;
    void *param;
    uint8_t priority;
    uint32_t delay_ticks;
    uint32_t run_count;
    bool ready;
    bool deleted;
    const char *name;
} rtos_tcb_t;

typedef rtos_tcb_t *rtos_task_handle_t;

/* ─── Semaphore ──────────────────────────────────────────────────────────── */
typedef struct
{
    volatile int32_t count;
    uint32_t max;
} rtos_sem_t;

/* ─── Mailbox (queue of uint32_t) ────────────────────────────────────────── */
#define RTOS_MAILBOX_SIZE 8U
typedef struct
{
    uint32_t buf[RTOS_MAILBOX_SIZE];
    volatile uint32_t head, tail;
} rtos_mailbox_t;

/* ─── Scheduler API ─────────────────────────────────────────────────────── */
void rtos_init(void);
rtos_task_handle_t rtos_task_create(task_fn_t fn, void *param,
                                    uint8_t priority, const char *name);
void rtos_start(void);           /* never returns (in real RTOS) */
void rtos_yield(void);           /* cooperative yield */
void rtos_delay(uint32_t ticks); /* block for N ticks */
void rtos_task_delete(rtos_task_handle_t handle);

/* Current tick counter */
uint32_t rtos_tick(void);

/* ─── Semaphore API ─────────────────────────────────────────────────────── */
void rtos_sem_init(rtos_sem_t *sem, int32_t initial, uint32_t max);
bool rtos_sem_take(rtos_sem_t *sem);
bool rtos_sem_give(rtos_sem_t *sem);
bool rtos_sem_give_from_isr(rtos_sem_t *sem);

/* ─── Mailbox API ────────────────────────────────────────────────────────── */
void rtos_mailbox_init(rtos_mailbox_t *mb);
bool rtos_mailbox_send(rtos_mailbox_t *mb, uint32_t msg);
bool rtos_mailbox_send_from_isr(rtos_mailbox_t *mb, uint32_t msg);
bool rtos_mailbox_receive(rtos_mailbox_t *mb, uint32_t *msg);
uint32_t rtos_mailbox_count(const rtos_mailbox_t *mb);

/* Stats */
void rtos_print_stats(void);
