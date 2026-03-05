/**
 * @file semaphores.c
 * @brief Phase 6 — RTOS Concepts: Semaphores and Mutexes
 *
 * Semaphores are the fundamental synchronisation primitive in every RTOS.
 * They solve two related problems:
 *   1. Signalling    — ISR signals a task that data is ready (binary semaphore)
 *   2. Mutual exclusion — only one task owns a shared resource at a time (mutex)
 *
 * Concepts covered:
 *   1. Binary semaphore (0/1) — signal + wait
 *   2. Counting semaphore — track available resources (N permits)
 *   3. Mutex — priority-inheritance aware binary semaphore
 *   4. Deadlock scenario and avoidance rules
 *   5. ISR-safe give (FreeRTOS: xSemaphoreGiveFromISR)
 *
 * BUILD:  cmake --build build --target p6_semaphores
 */

#include "embedded_types.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * BINARY SEMAPHORE
 * Think of it as a flag that can be "given" and "taken".
 * An ISR gives it; a task takes it (blocks until available).
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct
{
    volatile int32_t count;
    uint32_t max_count;
    const char *name;
} semaphore_t;

/* Create a semaphore. initial_count=0 for signal-type, =max for resources. */
static void sem_init(semaphore_t *s, int32_t initial, uint32_t max, const char *name)
{
    s->count = initial;
    s->max_count = max;
    s->name = name;
}

/* Take (wait) — decrements count. Returns false if would block. */
static bool sem_take(semaphore_t *s)
{
    if (s->count <= 0)
    {
        printf("  [%s] TAKE blocked (count=%d)\n", s->name, s->count);
        return false;
    }
    s->count--;
    printf("  [%s] TAKE ok  (count=%d)\n", s->name, s->count);
    return true;
}

/* Give (signal) — increments count up to max. */
static bool sem_give(semaphore_t *s)
{
    if ((uint32_t)s->count >= s->max_count)
    {
        printf("  [%s] GIVE overflow! (max=%u)\n", s->name, s->max_count);
        return false;
    }
    s->count++;
    printf("  [%s] GIVE ok  (count=%d)\n", s->name, s->count);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MUTEX  (binary semaphore with priority inheritance)
 * Key distinction from binary semaphore: only the OWNER can unlock a mutex.
 * Prevents priority inversion via the "priority ceiling" / "inheritance" protocol.
 * ═══════════════════════════════════════════════════════════════════════════ */

#define NO_OWNER 255U

typedef struct
{
    volatile int32_t locked; /* 0=free, 1=taken */
    uint8_t owner_id;        /* task that locked it */
    const char *name;
} mutex_t;

static void mutex_init(mutex_t *m, const char *name)
{
    m->locked = 0;
    m->owner_id = NO_OWNER;
    m->name = name;
}

static bool mutex_lock(mutex_t *m, uint8_t task_id)
{
    if (m->locked)
    {
        printf("  [%s] LOCK by task%u BLOCKED (held by task%u)\n",
               m->name, task_id, m->owner_id);
        return false;
    }
    m->locked = 1;
    m->owner_id = task_id;
    printf("  [%s] LOCK by task%u ok\n", m->name, task_id);
    return true;
}

static bool mutex_unlock(mutex_t *m, uint8_t task_id)
{
    if (!m->locked || m->owner_id != task_id)
    {
        printf("  [%s] UNLOCK ERROR: task%u is not the owner!\n",
               m->name, task_id);
        return false;
    }
    m->locked = 0;
    m->owner_id = NO_OWNER;
    printf("  [%s] UNLOCK by task%u\n", m->name, task_id);
    return true;
}

/* ─── Demo scenarios ─────────────────────────────────────────────────────── */

static void demo_binary_semaphore(void)
{
    printf("\n── Binary Semaphore (ISR → Task signalling) ──\n");
    semaphore_t rx_ready;
    sem_init(&rx_ready, 0, 1U, "RX_READY");

    printf("  Task waits for data...\n");
    sem_take(&rx_ready); /* would block in real RTOS */

    printf("  Simulating ISR: UART byte received\n");
    sem_give(&rx_ready); /* simulate ISR giving the semaphore */

    printf("  Task unblocked:\n");
    sem_take(&rx_ready); /* now succeeds */
    printf("  Task processing received byte\n");
}

static void demo_counting_semaphore(void)
{
    printf("\n── Counting Semaphore (resource pool of 3) ──\n");
    semaphore_t dma_channels;
    sem_init(&dma_channels, 3, 3U, "DMA_CH");

    sem_take(&dma_channels); /* task A acquires DMA channel */
    sem_take(&dma_channels); /* task B acquires DMA channel */
    sem_take(&dma_channels); /* task C acquires DMA channel */
    sem_take(&dma_channels); /* task D → no channels left, blocks */

    printf("  DMA transfer A done, releasing channel:\n");
    sem_give(&dma_channels);

    printf("  Task D can now proceed:\n");
    sem_take(&dma_channels);
}

static void demo_mutex(void)
{
    printf("\n── Mutex (SPI bus exclusive access) ──\n");
    mutex_t spi_bus;
    mutex_init(&spi_bus, "SPI_BUS");

    mutex_lock(&spi_bus, 1U); /* task 1 acquires SPI bus */
    printf("  Task 1 performing SPI transfer...\n");
    mutex_lock(&spi_bus, 2U);   /* task 2 tries → blocked */
    mutex_unlock(&spi_bus, 1U); /* task 1 releases SPI bus */
    printf("  Task 2 now proceeds:\n");
    mutex_lock(&spi_bus, 2U); /* task 2 retries → success */
    mutex_unlock(&spi_bus, 2U);

    /* Demonstrate wrong-owner unlock (programming error) */
    mutex_lock(&spi_bus, 3U);
    mutex_unlock(&spi_bus, 4U); /* wrong task — error */
    mutex_unlock(&spi_bus, 3U);
}

static void demo_deadlock(void)
{
    printf("\n── Deadlock Scenario ──\n");
    mutex_t mutex_a, mutex_b;
    mutex_init(&mutex_a, "MUTEX_A");
    mutex_init(&mutex_b, "MUTEX_B");

    printf("  Task1: lock A → lock B (ordering: A first)\n");
    mutex_lock(&mutex_a, 1U);
    mutex_lock(&mutex_b, 1U);
    printf("  Task1: work done\n");
    mutex_unlock(&mutex_b, 1U);
    mutex_unlock(&mutex_a, 1U);

    printf("\n  Task2 (BAD ordering): lock B → lock A → DEADLOCK risk!\n");
    printf("  FIX: enforce consistent lock ordering across all tasks.\n");
    printf("  RULE: Always acquire locks in the same global order.\n");
}

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 6 — Lesson 2: Semaphores & Mutexes   ║\n");
    printf("╚══════════════════════════════════════════════╝\n");

    demo_binary_semaphore();
    demo_counting_semaphore();
    demo_mutex();
    demo_deadlock();

    printf("\nEXERCISES:\n");
    printf("  1. Implement sem_take_timeout() that gives up after N ticks.\n");
    printf("  2. Explain why recursive mutexes are needed: give a concrete\n");
    printf("     example of a function calling itself while holding a mutex.\n");
    printf("  3. The FreeRTOS xSemaphoreGiveFromISR requires a 'yield flag'.\n");
    printf("     Explain why, and what ARM instruction triggers the yield.\n");
    return 0;
}
