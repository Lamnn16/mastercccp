/**
 * @file interrupt_patterns.c
 * @brief Phase 3 — Interrupt Handler Patterns: The Core of Real-Time Firmware
 *
 * Interrupt-driven code is the foundation of all responsive embedded firmware.
 * This lesson covers:
 *
 *   1. ISR anatomy — what an ISR must and must NOT do
 *   2. Deferred processing — ISR sets flag, main loop does work
 *   3. ISR → main data transfer — ring buffer pattern
 *   4. UART TX interrupt — the state machine ISR
 *   5. Tick timer — systick ISR, delay, and scheduling
 *   6. Priority inversion preview
 *
 * BUILD:  cmake --build build --target p3_interrupts
 * RUN:    .\build\03_embedded_c\p3_interrupts.exe
 */

#include "embedded_types.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. ISR RULES (memorize these)
 *
 * MUST:
 *   - Be as fast as possible (<= 1 µs is ideal, <10 µs acceptable)
 *   - Clear the interrupt source flag BEFORE or AFTER depending on peripheral
 *   - Use volatile for all shared variables
 *   - Be reentrant-safe (may be preempted by higher-priority ISR)
 *
 * MUST NOT:
 *   - Call malloc/free
 *   - Call printf (it has internal locks, may deadlock)
 *   - Block/wait
 *   - Call HAL functions that may block
 *   - Perform floating-point if FPU not saved (Cortex-M4: save FPSCR)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ─── Simulated UART RX ring buffer (ISR-to-main pattern) ────────────────── */
#define RX_BUF_SIZE 32U /* MUST be power of 2 */
#define RX_BUF_MASK (RX_BUF_SIZE - 1U)

static volatile uint8_t g_rx_buf[RX_BUF_SIZE];
static volatile uint32_t g_rx_head = 0U; /* write index (ISR writes) */
static volatile uint32_t g_rx_tail = 0U; /* read  index (main reads) */

/* Called from UART ISR — must be fast */
static void uart_rx_isr(uint8_t byte)
{
    uint32_t next_head = (g_rx_head + 1U) & RX_BUF_MASK;
    if (next_head != g_rx_tail)
    { /* not full */
        g_rx_buf[g_rx_head] = byte;
        g_rx_head = next_head; /* atomic 32-bit write on ARM */
    }
    /* Silently drop if full — in real firmware, set an overflow flag */
}

/* Called from main loop — reads from buffer */
static bool uart_rx_pop(uint8_t *out)
{
    if (g_rx_head == g_rx_tail)
        return false; /* empty */
    *out = g_rx_buf[g_rx_tail];
    g_rx_tail = (g_rx_tail + 1U) & RX_BUF_MASK;
    return true;
}

static uint32_t uart_rx_count(void)
{
    return (g_rx_head - g_rx_tail) & RX_BUF_MASK;
}

/* ─── Simulated UART TX interrupt-driven send ────────────────────────────── */
typedef struct
{
    const uint8_t *tx_buf;
    volatile size_t tx_len;
    volatile size_t tx_pos;
    volatile bool tx_busy;
} uart_tx_state_t;

static uart_tx_state_t g_uart_tx = {0};

/* Start a TX — called from main context */
static bool uart_tx_start(const uint8_t *buf, size_t len)
{
    if (g_uart_tx.tx_busy)
        return false; /* can only start one TX at a time */

    g_uart_tx.tx_buf = buf;
    g_uart_tx.tx_len = len;
    g_uart_tx.tx_pos = 0;
    g_uart_tx.tx_busy = true;

    /* Send first byte — TXE interrupt will send the rest */
    printf("    [TX ISR] byte[0]=0x%02X\n", buf[0]);
    return true;
}

/* Called from USART_TXE ISR when TX buffer is empty */
static void uart_txe_isr(void)
{
    if (!g_uart_tx.tx_busy)
        return;

    g_uart_tx.tx_pos++;
    if (g_uart_tx.tx_pos < g_uart_tx.tx_len)
    {
        /* More bytes to send */
        printf("    [TX ISR] byte[%zu]=0x%02X\n",
               g_uart_tx.tx_pos, g_uart_tx.tx_buf[g_uart_tx.tx_pos]);
    }
    else
    {
        /* All bytes sent */
        g_uart_tx.tx_busy = false;
        printf("    [TX ISR] transfer complete\n");
    }
}

/* ─── SysTick (1 ms tick) ────────────────────────────────────────────────── */
static volatile uint32_t g_tick_ms = 0U;

/* Called from SysTick_Handler every 1 ms */
static void systick_isr(void)
{
    g_tick_ms++;
}

static uint32_t get_tick_ms(void) { return g_tick_ms; }

static void sim_delay_ms(uint32_t ms)
{
    uint32_t start = get_tick_ms();
    while ((get_tick_ms() - start) < ms)
    {
        /* In real firmware: call __WFI() here to sleep until next IRQ */
        systick_isr(); /* simulate tick advancing */
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DEFERRED PROCESSING PATTERN
 *
 * Best practice: ISR does minimal work, sets a flag or writes to a queue.
 * Main loop does the heavy lifting.
 *
 * Benefit: ISR exits quickly → higher-priority IRQs are not blocked
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct
{
    volatile bool button_pressed;
    volatile bool uart_frame_ready;
    volatile bool adc_conversion_done;
    volatile uint16_t adc_value;
} event_flags_t;

static event_flags_t g_events = {0};

/* Simulated ISRs — each does minimal work */
static void exti_isr_button(void) { g_events.button_pressed = true; }
static void dma_adc_isr(void)
{
    g_events.adc_conversion_done = true;
    g_events.adc_value = 2048U;
} /* 12-bit ADC mid */

/* Main loop processes events */
static void process_events(void)
{
    if (g_events.button_pressed)
    {
        g_events.button_pressed = false;
        printf("    [main] button press processed\n");
    }
    if (g_events.adc_conversion_done)
    {
        g_events.adc_conversion_done = false;
        float voltage = (g_events.adc_value / 4095.0f) * 3.3f;
        printf("    [main] ADC done: raw=%u voltage=%.3fV\n",
               g_events.adc_value, voltage);
    }
}

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 3 — Lesson 2: Interrupt Patterns     ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    printf("── 1. UART RX ring buffer (ISR → main) ──\n");
    /* Simulate ISR receiving bytes */
    const char *incoming = "Hello!";
    for (size_t i = 0; incoming[i]; i++)
    {
        uart_rx_isr((uint8_t)incoming[i]);
    }
    printf("  bytes in buffer: %u\n", (unsigned)uart_rx_count());
    uint8_t byte;
    printf("  draining: ");
    while (uart_rx_pop(&byte))
    {
        printf("'%c' ", (char)byte);
    }
    printf("\n\n");

    printf("── 2. UART TX interrupt-driven ──\n");
    uint8_t tx_data[] = {0x01, 0x02, 0x03, 0x04};
    uart_tx_start(tx_data, sizeof(tx_data));
    /* Simulate TXE interrupts firing */
    while (g_uart_tx.tx_busy)
    {
        uart_txe_isr();
    }
    printf("\n");

    printf("── 3. SysTick delay ──\n");
    printf("  tick before: %u\n", (unsigned)get_tick_ms());
    sim_delay_ms(5);
    printf("  tick after 5ms: %u\n\n", (unsigned)get_tick_ms());

    printf("── 4. Deferred processing ──\n");
    exti_isr_button();
    dma_adc_isr();
    process_events();
    printf("\n");

    printf("EXERCISES:\n");
    printf("  1. Add overflow detection to the ring buffer. If the ISR tries\n");
    printf("     to push when full, set a volatile g_rx_overflow flag.\n");
    printf("  2. Extend the TX state machine to handle a maximum TX timeout:\n");
    printf("     if a byte is not sent within 1000 ticks, abort and set error.\n");
    printf("  3. Implement a 'super-loop' that sleeps (simulated) between events.\n");
    return 0;
}
