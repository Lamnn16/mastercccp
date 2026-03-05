/**
 * @file volatile_const.c
 * @brief Phase 1 — volatile and const: Two of the Most Misused Keywords
 *
 * volatile and const are CRITICAL in embedded C and are misunderstood even by
 * experienced developers. Getting them wrong leads to:
 *   - volatile missing  → optimizer removes hardware reads → firmware hangs
 *   - volatile excess   → optimizer disabled → slow code
 *   - const missing     → compiler can't place data in Flash → wastes RAM
 *
 * RULE: Every hardware register access MUST be volatile.
 * RULE: Every read-only constant MUST be const.
 *
 * BUILD:  cmake --build build --target p1_volatile
 * RUN:    .\build\01_advanced_c\p1_volatile.exe
 */

#include "embedded_types.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. WHY volatile EXISTS
 *
 * The compiler optimizes code assuming that memory only changes when YOUR
 * code changes it. For hardware registers, this assumption is FALSE.
 *
 * A hardware register can change at any time:
 *   - A UART receives a byte → RXDR register changes (ISR reads it)
 *   - A timer overflows → SR register changes (flag set by hardware)
 *   - DMA completes a transfer → NDTR register changes
 *
 * volatile tells the compiler: "this memory can change at any time from
 * outside your view. Do NOT cache it in a register. Read it from memory
 * on every access."
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Simulated hardware status register (imagine this is a real MCU register) */
static volatile uint32_t SIM_STATUS_REG = 0U;

/* Simulated ISR — in real firmware this runs in interrupt context */
static void sim_hardware_event(void)
{
    BIT_SET(SIM_STATUS_REG, 0); /* hardware sets "data ready" flag */
}

static void demo_volatile_polling(void)
{
    printf("── 1. volatile polling loop ──\n");

    /*
     * WITHOUT volatile, an optimizing compiler would transform:
     *
     *   while (!(SIM_STATUS_REG & 1)) {}   // check flag
     *
     * into:
     *
     *   uint32_t cached = SIM_STATUS_REG;  // read ONCE
     *   while (!(cached & 1)) {}           // loop forever — BUG!
     *
     * WITH volatile, the register is re-read on every loop iteration.
     */
    SIM_STATUS_REG = 0U; /* flag starts cleared */

    printf("  Waiting for hardware event...\n");
    sim_hardware_event(); /* simulate hardware setting the flag */

    /* Correct polling loop */
    while (!BIT_IS_SET(SIM_STATUS_REG, 0))
    {
        /* busy-wait (in real firmware, sleep or yield here) */
    }
    printf("  Data-ready flag detected! STATUS=0x%08X\n", SIM_STATUS_REG);

    /* Clear flag (write 0 to clear on many STM32 peripherals) */
    BIT_CLEAR(SIM_STATUS_REG, 0);
    printf("  Flag cleared. STATUS=0x%08X\n\n", SIM_STATUS_REG);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. volatile AND ISR — SHARED VARIABLES BETWEEN MAIN AND ISR
 *
 * Any variable read in main() but WRITTEN in an ISR (or vice versa) MUST be
 * volatile. Otherwise the compiler may cache the value in a CPU register,
 * and the ISR update will not be visible to main.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Shared between main() and ISR — must be volatile */
static volatile uint32_t g_tick_count = 0U;
static volatile bool g_data_ready = false;
static volatile uint8_t g_isr_rx_byte = 0U;

/* Simulated UART ISR */
static void sim_uart_isr(void)
{
    g_isr_rx_byte = 0x42U; /* "received" byte 'B' */
    g_data_ready = true;   /* signal main loop */
}

static void demo_isr_shared_variable(void)
{
    printf("── 2. volatile shared between main and ISR ──\n");

    g_data_ready = false;

    /* Simulate ISR firing */
    sim_uart_isr();

    /* main() polling — works because g_data_ready is volatile */
    if (g_data_ready)
    {
        printf("  ISR received byte: 0x%02X ('%c')\n", g_isr_rx_byte, g_isr_rx_byte);
        g_data_ready = false; /* ACK — clear the flag */
    }
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. const AND volatile TOGETHER: const volatile
 *
 * A register that is read-only (from your code's perspective) but can still
 * change asynchronously (by hardware) should be BOTH const AND volatile.
 *
 * Example: RCC_CR (Clock Control Register) — you can read it to check if the
 * PLL is locked, but some bits are read-only (set by hardware).
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Read-only hardware ID register — simulated */
static const volatile uint32_t SIM_CHIP_ID = 0x00000411U; /* STM32F411 */

static void demo_const_volatile(void)
{
    printf("── 3. const volatile (read-only hardware register) ──\n");

    uint32_t id = SIM_CHIP_ID;  /* read every time — volatile */
    /* SIM_CHIP_ID = 0x5555; */ /* compile error — const prevents write */

    printf("  Chip ID: 0x%08X\n", id);
    printf("  (const protects from accidental writes,\n");
    printf("   volatile ensures fresh read every access)\n\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. const FOR FLASH PLACEMENT (the ROM optimization)
 *
 * On STM32: large const arrays go to .rodata section → stored in Flash.
 * Without const, they go to .data section → copied from Flash to RAM on boot
 * → wastes precious RAM.
 *
 * If you have a 1KB sine table: const saves 1KB of RAM automatically.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* 64-point Q15 sine table — const = stays in Flash, NOT copied to RAM */
static const int16_t SINE_TABLE_Q15[64] = {
    0, 3212, 6393, 9512, 12539, 15447, 18204, 20787,
    23170, 25330, 27245, 28898, 30273, 31357, 32138, 32610,
    32767, 32610, 32138, 31357, 30273, 28898, 27245, 25330,
    23170, 20787, 18204, 15447, 12539, 9512, 6393, 3212,
    0, -3212, -6393, -9512, -12539, -15447, -18204, -20787,
    -23170, -25330, -27245, -28898, -30273, -31357, -32138, -32610,
    -32767, -32610, -32138, -31357, -30273, -28898, -27245, -25330,
    -23170, -20787, -18204, -15447, -12539, -9512, -6393, -3212};

static void demo_const_flash(void)
{
    printf("── 4. const for Flash placement ──\n");
    printf("  SINE_TABLE_Q15 has %zu entries, %zu bytes\n",
           ARRAY_SIZE(SINE_TABLE_Q15),
           sizeof(SINE_TABLE_Q15));
    printf("  Q15 sine(0°)=%-6d  sine(90°)=%-6d  sine(180°)=%-6d\n",
           SINE_TABLE_Q15[0], SINE_TABLE_Q15[16], SINE_TABLE_Q15[32]);
    printf("  On MCU: this table lives in Flash, not RAM.\n\n");

    UNUSED(g_tick_count);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * EXERCISES
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * EXERCISE 1:
 *   Write a function bool wait_flag(volatile uint32_t *reg, uint32_t bit,
 *                                   uint32_t timeout_ms)
 *   that polls until `bit` is set in `*reg` or timeout reaches 0.
 *   Simulate the timeout by decrementing a counter.
 *
 * EXERCISE 2:
 *   Why can't you use the same flag variable in an ISR and in main() without
 *   volatile, even if the ISR runs in the same thread? (Hint: -O2 optimization)
 *   Answer in a comment.
 *
 * EXERCISE 3:
 *   What is wrong with this code?
 *     static uint32_t *p_reg = (uint32_t*)0x40020014U;
 *     void gpio_set(void) { *p_reg |= (1U << 5); }
 *   Fix it for correct embedded usage.
 */

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 1 — Lesson 3: volatile & const       ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    demo_volatile_polling();
    demo_isr_shared_variable();
    demo_const_volatile();
    demo_const_flash();

    return 0;
}
