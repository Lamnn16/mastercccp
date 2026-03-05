/**
 * @file scheduler.c
 * @brief Phase 6 — RTOS Concepts: Cooperative Round-Robin Scheduler
 *
 * Real-time operating systems at their core are just a scheduler.
 * This lesson builds a minimal cooperative scheduler from scratch to
 * demystify what FreeRTOS or Zephyr actually do underneath.
 *
 * Concepts covered:
 *   1. Task Control Block (TCB) — every RTOS "task" is described by one
 *   2. Round-robin scheduling — tasks take turns, no preemption here
 *   3. Cooperative yield — task voluntarily surrenders CPU
 *   4. Tick-based delay — task suspends for N "ticks"
 *   5. Priority scheduling — higher priority tasks run first
 *
 * Real RTOS difference: preemptive schedulers use SysTick interrupt to
 * forcibly switch tasks. Context save/restore happens in the ISR via
 * push/pop of CPU registers to/from the task's stack.
 *
 * BUILD:  cmake --build build --target p6_scheduler
 */

#include "embedded_types.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * TASK CONTROL BLOCK
 * In a real RTOS this also stores the stack pointer, saved registers,
 * and other per-task state. Here we simulate on the PC without stack switching.
 * ═══════════════════════════════════════════════════════════════════════════ */

#define MAX_TASKS 8U
#define TICK_LIMIT 30U /* simulate 30 scheduler ticks */

typedef enum
{
    TASK_READY = 0,
    TASK_RUNNING = 1,
    TASK_BLOCKED = 2, /* waiting for delay to expire */
    TASK_DELETED = 3
} task_state_t;

typedef void (*task_fn_t)(void);

typedef struct
{
    task_fn_t fn;
    task_state_t state;
    uint8_t priority;     /* 0=low, 255=high */
    uint32_t delay_ticks; /* remaining ticks before unblock */
    uint32_t run_count;
    const char *name;
} tcb_t;

static tcb_t s_tasks[MAX_TASKS];
static uint8_t s_task_count = 0U;
static uint32_t s_tick = 0U;
static int8_t s_current = -1; /* index of currently-running task */

/* ─── Public API ─────────────────────────────────────────────────────────── */

static bool task_create(task_fn_t fn, uint8_t priority, const char *name)
{
    if (s_task_count >= MAX_TASKS)
        return false;
    s_tasks[s_task_count++] = (tcb_t){
        .fn = fn, .state = TASK_READY, .priority = priority, .delay_ticks = 0U, .run_count = 0U, .name = name};
    return true;
}

/* Called by a task to suspend itself for `ticks` scheduler ticks */
static void task_delay(uint32_t ticks)
{
    if (s_current < 0)
        return;
    s_tasks[s_current].state = TASK_BLOCKED;
    s_tasks[s_current].delay_ticks = ticks;
    /* In a preemptive RTOS: trigger PendSV here to switch context */
}

/* ─── Scheduler internals ────────────────────────────────────────────────── */

static void scheduler_tick(void)
{
    s_tick++;
    /* Decrement delays and unblock waiting tasks */
    for (uint8_t i = 0U; i < s_task_count; i++)
    {
        if (s_tasks[i].state == TASK_BLOCKED)
        {
            if (s_tasks[i].delay_ticks > 0U)
                s_tasks[i].delay_ticks--;
            if (s_tasks[i].delay_ticks == 0U)
                s_tasks[i].state = TASK_READY;
        }
    }
}

/* Priority-based round-robin: find highest-priority READY task */
static int8_t scheduler_pick_next(void)
{
    int8_t best = -1;
    uint8_t best_pri = 0U;
    /* Simple: scan all, pick highest priority READY; ties go to next in list */
    static uint8_t last_run = 0U;
    uint8_t start = (last_run + 1U) % s_task_count;
    for (uint8_t pass = 0U; pass < s_task_count; pass++)
    {
        uint8_t i = (uint8_t)((start + pass) % s_task_count);
        if (s_tasks[i].state == TASK_READY)
        {
            if (best < 0 || s_tasks[i].priority > best_pri)
            {
                best = (int8_t)i;
                best_pri = s_tasks[i].priority;
            }
        }
    }
    if (best >= 0)
        last_run = (uint8_t)best;
    return best;
}

/* ─── Tasks (each simulates one "tick" of work, then yields) ─────────────── */

static void led_task(void)
{
    /* Blink an LED — runs every 5 ticks */
    static uint8_t led_state = 0U;
    led_state ^= 1U;
    printf("[t%02u] LED_TASK       LED=%s\n", s_tick, led_state ? "ON " : "OFF");
    task_delay(5U);
}

static void sensor_task(void)
{
    /* Read sensor — runs every 3 ticks */
    static uint16_t fake_temp = 230U; /* 23.0 °C */
    fake_temp += (uint16_t)(s_tick & 1U);
    printf("[t%02u] SENSOR_TASK    temp=%u.%u°C\n",
           s_tick, fake_temp / 10U, fake_temp % 10U);
    task_delay(3U);
}

static void comms_task(void)
{
    /* Send a packet every 7 ticks */
    static uint8_t seq = 0U;
    printf("[t%02u] COMMS_TASK     TX seq=%u\n", s_tick, seq++);
    task_delay(7U);
}

static void idle_task(void)
{
    /* Lowest priority — runs whenever nothing else is ready */
    printf("[t%02u] IDLE_TASK      (sleep/WFI)\n", s_tick);
    task_delay(1U); /* yield every tick */
}

static void scheduler_run(void)
{
    uint32_t total_ticks = 0U;
    while (total_ticks < TICK_LIMIT)
    {
        int8_t idx = scheduler_pick_next();
        if (idx >= 0)
        {
            s_current = idx;
            s_tasks[idx].state = TASK_RUNNING;
            s_tasks[idx].run_count++;
            s_tasks[idx].fn(); /* cooperative: task runs to yield */
            if (s_tasks[idx].state == TASK_RUNNING)
                s_tasks[idx].state = TASK_READY;
        }
        scheduler_tick();
        total_ticks++;
        s_current = -1;
    }
}

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 6 — Lesson 1: Cooperative Scheduler  ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");
    printf("Tasks: LED(pri=1,delay=5), SENSOR(pri=2,delay=3), "
           "COMMS(pri=1,delay=7), IDLE(pri=0,delay=1)\n");
    printf("Running %u scheduler ticks...\n\n", TICK_LIMIT);

    task_create(led_task, 1U, "LED");
    task_create(sensor_task, 2U, "SENSOR");
    task_create(comms_task, 1U, "COMMS");
    task_create(idle_task, 0U, "IDLE");

    scheduler_run();

    printf("\n── Task statistics ──\n");
    for (uint8_t i = 0U; i < s_task_count; i++)
    {
        printf("  %-12s  ran=%u times\n", s_tasks[i].name, s_tasks[i].run_count);
    }

    printf("\nEXERCISES:\n");
    printf("  1. Add task_suspend() and task_resume() to the API.\n");
    printf("  2. Implement a deadline-miss detector: if a READY task has\n");
    printf("     priority≥2 and hasn't run in 10 ticks, print a warning.\n");
    printf("  3. Explain why a preemptive scheduler needs to save/restore\n");
    printf("     all CPU registers. Which ARM registers are callee-saved?\n");
    return 0;
}
