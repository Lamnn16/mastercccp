/**
 * @file priority_inversion.c
 * @brief Phase 6 — RTOS Concepts: Priority Inversion and Inheritance
 *
 * Priority inversion is a subtle but dangerous bug in RTOS systems.
 * It famously brought down the Mars Pathfinder rover in 1997.
 *
 * The problem:
 *   HIGH task  waits for a mutex held by LOW task.
 *   MED task   preempts LOW task (MED > LOW).
 *   Now LOW (holding the mutex) can't run → HIGH is effectively blocked by MED.
 *   HIGH has been INVERTED to MED's priority.
 *
 * Solution:
 *   Priority Inheritance — while LOW holds a mutex that HIGH is waiting for,
 *   temporarily boost LOW's priority to HIGH's priority.
 *
 * BUILD:  cmake --build build --target p6_priority_inv
 */

#include "embedded_types.h"

/* ─── Task priorities ────────────────────────────────────────────────────── */
#define PRI_LOW 1U
#define PRI_MED 5U
#define PRI_HIGH 10U

/* ─── Mutex with priority inheritance ───────────────────────────────────── */

typedef struct task_t task_t;

struct task_t
{
    const char *name;
    uint8_t base_priority;
    uint8_t effective_priority; /* boosted by inheritance */
    uint32_t blocked_until;     /* tick when task unblocks */
    uint32_t total_run_ticks;
};

typedef struct
{
    const char *name;
    bool locked;
    task_t *owner;
} pi_mutex_t; /* Priority-Inheritance Mutex */

static void pi_mutex_init(pi_mutex_t *m, const char *name)
{
    m->name = name;
    m->locked = false;
    m->owner = NULL;
}

static bool pi_mutex_lock(pi_mutex_t *m, task_t *requester, uint32_t tick)
{
    if (!m->locked)
    {
        m->locked = true;
        m->owner = requester;
        return true;
    }
    /* Lock held — apply priority inheritance */
    if (requester->effective_priority > m->owner->effective_priority)
    {
        printf("[t%02u] PI: boosting %s pri %u → %u (held by %s, needed by %s)\n",
               tick,
               m->owner->name,
               m->owner->effective_priority,
               requester->effective_priority,
               m->owner->name,
               requester->name);
        m->owner->effective_priority = requester->effective_priority;
    }
    return false; /* blocked */
}

static void pi_mutex_unlock(pi_mutex_t *m, task_t *owner, uint32_t tick)
{
    if (!m->locked || m->owner != owner)
        return;
    /* Restore effective priority to base */
    if (owner->effective_priority != owner->base_priority)
    {
        printf("[t%02u] PI: restoring %s pri %u → %u\n",
               tick, owner->name,
               owner->effective_priority, owner->base_priority);
    }
    owner->effective_priority = owner->base_priority;
    m->locked = false;
    m->owner = NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SIMULATION
 * We run a time-stepped simulation of three tasks competing for one mutex.
 * "Preemption" is simulated by always picking the highest-priority READY task.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum
{
    TS_READY,
    TS_RUNNING,
    TS_BLOCKED_ON_MUTEX,
    TS_DONE
} tsim_state_t;

#define SIM_TICKS 30U

typedef struct
{
    task_t info;
    tsim_state_t state;
    uint32_t work_remaining;
    uint32_t start_tick;
    bool needs_mutex;
    bool holds_mutex;
} sim_task_t;

static pi_mutex_t g_mutex;

/* Pick the currently runnable task with the highest effective priority */
static sim_task_t *pick_running(sim_task_t *tasks, uint8_t n)
{
    sim_task_t *best = NULL;
    for (uint8_t i = 0U; i < n; i++)
    {
        if (tasks[i].state == TS_READY)
        {
            if (!best || tasks[i].info.effective_priority > best->info.effective_priority)
                best = &tasks[i];
        }
    }
    return best;
}

static void run_scenario(const char *title,
                         bool enable_priority_inheritance)
{
    printf("\n%s\n", title);
    for (int j = 0; j < (int)strlen(title); j++)
        putchar('-');
    putchar('\n');

    /* Reset mutex */
    pi_mutex_init(&g_mutex, "SPI_BUS");

    /* LOW, MED, HIGH tasks */
    sim_task_t tasks[3] = {
        /* LOW:  arrives at t=0, needs mutex for 8 ticks */
        {.info = {"LOW", PRI_LOW, PRI_LOW, 0, 0},
         .state = TS_READY,
         .work_remaining = 10U,
         .start_tick = 0U,
         .needs_mutex = true,
         .holds_mutex = false},
        /* MED:  arrives at t=4, CPU-heavy, no mutex needed */
        {.info = {"MED", PRI_MED, PRI_MED, 0, 0},
         .state = TS_BLOCKED_ON_MUTEX, /* simulate: not ready yet */
         .work_remaining = 6U,
         .start_tick = 4U,
         .needs_mutex = false,
         .holds_mutex = false},
        /* HIGH: arrives at t=6, needs the same mutex */
        {.info = {"HIGH", PRI_HIGH, PRI_HIGH, 0, 0},
         .state = TS_BLOCKED_ON_MUTEX,
         .work_remaining = 4U,
         .start_tick = 7U,
         .needs_mutex = true,
         .holds_mutex = false}};

    for (uint32_t t = 0U; t < SIM_TICKS; t++)
    {
        /* Arrive: unblock at their start tick */
        if (t == tasks[1].start_tick)
            tasks[1].state = TS_READY;
        if (t == tasks[2].start_tick)
            tasks[2].state = TS_READY;

        sim_task_t *running = pick_running(tasks, 3U);
        if (!running)
        {
            printf("[t%02u] IDLE\n", t);
            continue;
        }

        /* If task needs mutex and doesn't hold it, try to acquire */
        if (running->needs_mutex && !running->holds_mutex)
        {
            bool acquired = enable_priority_inheritance
                                ? pi_mutex_lock(&g_mutex, &running->info, t)
                                : (!g_mutex.locked
                                       ? (g_mutex.locked = true, g_mutex.owner = &running->info, true)
                                       : false);
            if (!acquired)
            {
                running->state = TS_BLOCKED_ON_MUTEX;
                printf("[t%02u] %s blocked on mutex (held by %s)\n",
                       t, running->info.name,
                       g_mutex.owner ? g_mutex.owner->name : "?");
                running = pick_running(tasks, 3U);
                if (!running)
                {
                    printf("[t%02u] IDLE\n", t);
                    continue;
                }
            }
            else
            {
                running->holds_mutex = true;
            }
        }

        /* Run for one tick */
        running->info.total_run_ticks++;
        running->work_remaining--;
        printf("[t%02u] RUNNING: %s (pri_eff=%u, work_left=%u%s)\n",
               t, running->info.name, running->info.effective_priority,
               running->work_remaining, running->holds_mutex ? ", MUTEX" : "");

        /* Release mutex when done (LOW task example: after 6 ticks of holding) */
        if (running->holds_mutex && running->info.total_run_ticks % 6U == 0U)
        {
            running->holds_mutex = false;
            if (enable_priority_inheritance)
                pi_mutex_unlock(&g_mutex, &running->info, t);
            else
            {
                g_mutex.locked = false;
                g_mutex.owner = NULL;
            }
            /* Unblock any task waiting for mutex */
            for (int k = 0; k < 3; k++)
            {
                if (tasks[k].state == TS_BLOCKED_ON_MUTEX)
                    tasks[k].state = TS_READY;
            }
        }

        if (running->work_remaining == 0U)
        {
            running->state = TS_DONE;
            printf("[t%02u] %s FINISHED (ran %u ticks)\n",
                   t, running->info.name, running->info.total_run_ticks);
        }

        if (tasks[0].state == TS_DONE && tasks[1].state == TS_DONE &&
            tasks[2].state == TS_DONE)
            break;
    }

    printf("\nCompletion order: ");
    for (int k = 0; k < 3; k++)
    {
        if (tasks[k].state == TS_DONE)
            printf("%s(%u ticks) ", tasks[k].info.name, tasks[k].info.total_run_ticks);
    }
    putchar('\n');
}

int main(void)
{
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║  Phase 6 — Lesson 4: Priority Inversion         ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");

    run_scenario("SCENARIO A: Without Priority Inheritance (broken)", false);
    run_scenario("SCENARIO B: With Priority Inheritance (fixed)", true);

    printf("\n── ANALYSIS ──\n");
    printf("In scenario A: HIGH is effectively blocked by MED (inversion).\n");
    printf("  MED runs freely while HIGH waits for mutex held by LOW.\n");
    printf("  HIGH finishes AFTER MED — priority inversion!\n\n");
    printf("In scenario B: LOW is boosted to HIGH's priority while holding\n");
    printf("  the mutex, preventing MED from preempting it.\n");
    printf("  LOW finishes quickly → releases mutex → HIGH runs immediately.\n");
    printf("  HIGH finishes before MED — correct priority order!\n");

    printf("\nEXERCISES:\n");
    printf("  1. How did FreeRTOS handle the Mars Pathfinder incident?\n");
    printf("     (hint: look up vTaskPrioritySet and INCLUDE_vTaskPrioritySet)\n");
    printf("  2. Implement pi_mutex_lock_timeout() that gives up if the mutex\n");
    printf("     isn't available within N ticks. What priority should the\n");
    printf("     inheritance be reverted to on timeout?\n");
    printf("  3. What is the 'Priority Ceiling Protocol'? How does it differ\n");
    printf("     from priority inheritance and what are the tradeoffs?\n");
    return 0;
}
