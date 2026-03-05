/**
 * @file dma_concepts.c
 * @brief Phase 3 — DMA: Moving Data Without the CPU
 *
 * DMA (Direct Memory Access) is critical for high-performance embedded:
 *   - ADC sampling at 1 MHz without CPU involvement
 *   - UART RX/TX of large buffers in background
 *   - SPI display refresh without blocking
 *
 * This lesson simulates DMA mechanics and its key software patterns.
 *
 * BUILD:  cmake --build build --target p3_dma
 */

#include "embedded_types.h"

/* ─── DMA Transfer descriptor (mirrors STM32 DMA_Stream registers) ───────── */
typedef struct
{
    volatile uint32_t CR;   /* control */
    volatile uint32_t NDTR; /* number of data items remaining */
    volatile uint32_t PAR;  /* peripheral address */
    volatile uint32_t M0AR; /* memory address 0 */
    volatile uint32_t M1AR; /* memory address 1 (double-buffer mode) */
} DMA_Stream_t;

/* DMA CR bits */
#define DMA_CR_EN (1UL << 0)      /* enable */
#define DMA_CR_TCIE (1UL << 4)    /* transfer complete interrupt enable */
#define DMA_CR_TEIE (1UL << 2)    /* transfer error interrupt enable */
#define DMA_CR_DIR_M2P (1UL << 6) /* memory to peripheral */
#define DMA_CR_DIR_P2M (0UL)      /* peripheral to memory (default) */
#define DMA_CR_MINC (1UL << 10)   /* memory increment mode */
#define DMA_CR_CIRC (1UL << 8)    /* circular mode */

static DMA_Stream_t sim_dma_stream = {0};

/* Buffer aligned for DMA */
static uint8_t dma_tx_buf[64] __attribute__((aligned(4)));
static uint8_t dma_rx_buf[64] __attribute__((aligned(4)));

/* DMA transfer complete callback */
static volatile bool g_dma_tc = false;

static void dma_tc_isr(void) /* Transfer Complete ISR */
{
    g_dma_tc = true;
    BIT_CLEAR(sim_dma_stream.CR, 0); /* DMA auto-disables on TC */
}

static void dma_start_tx(const uint8_t *src, volatile uint32_t *periph_dr,
                         uint16_t count)
{
    sim_dma_stream.M0AR = (uint32_t)(uintptr_t)src;
    sim_dma_stream.PAR = (uint32_t)(uintptr_t)periph_dr;
    sim_dma_stream.NDTR = count;
    sim_dma_stream.CR = DMA_CR_MINC | DMA_CR_DIR_M2P | DMA_CR_TCIE;
    sim_dma_stream.CR |= DMA_CR_EN;

    printf("  DMA TX started: src=%p len=%u\n", (void *)src, count);
}

/* Simulate DMA engine running */
static void sim_dma_run(void)
{
    while (sim_dma_stream.NDTR > 0 && BIT_IS_SET(sim_dma_stream.CR, 0))
    {
        sim_dma_stream.NDTR--;
    }
    dma_tc_isr();
}

/* Double-buffer ADC DMA (circular mode) */
static uint16_t adc_buf_a[64];
static uint16_t adc_buf_b[64];
static volatile bool g_use_buf_a = true;

static void adc_dma_half_complete_isr(void)
{
    /* Process first half while DMA fills second half */
    printf("  [DMA ADC] half-complete: processing buf_A[0..31]\n");
}
static void adc_dma_full_complete_isr(void)
{
    /* Process second half while DMA fills first half */
    printf("  [DMA ADC] full-complete: processing buf_A[32..63]\n");
    g_use_buf_a = !g_use_buf_a;
}

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 3 — Lesson 4: DMA Concepts           ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    /* Fill TX buffer */
    for (int i = 0; i < 8; i++)
        dma_tx_buf[i] = (uint8_t)(i + 1);

    volatile uint32_t fake_dr = 0U;
    printf("── DMA Transfer ──\n");
    dma_start_tx(dma_tx_buf, &fake_dr, 8);
    sim_dma_run();
    printf("  TC flag: %s  NDTR=%u\n\n", g_dma_tc ? "SET" : "clear",
           (unsigned)sim_dma_stream.NDTR);

    printf("── Double-buffer ADC DMA ──\n");
    adc_dma_half_complete_isr();
    adc_dma_full_complete_isr();
    printf("\n");

    printf("── Buffer alignment check ──\n");
    printf("  dma_tx_buf @ %p  (%%4=%lu)\n",
           (void *)dma_tx_buf, (unsigned long)(uintptr_t)dma_tx_buf % 4UL);
    printf("  dma_rx_buf @ %p  (%%4=%lu)\n\n",
           (void *)dma_rx_buf, (unsigned long)(uintptr_t)dma_rx_buf % 4UL);

    printf("EXERCISES:\n");
    printf("  1. Simulate a circular DMA: after TC, restart with NDTR=64.\n");
    printf("     Show 3 complete cycles with ping-pong buffer switching.\n");
    printf("  2. Add error handling: if DMA_CR_TEIE fires, log the error address.\n");
    printf("  3. Calculate: at 115200 baud, how long does sending 64 bytes take?\n");
    printf("     How many CPU cycles are freed by using DMA vs polled TX?\n");

    UNUSED(adc_buf_a);
    UNUSED(adc_buf_b);
    UNUSED(g_use_buf_a);
    return 0;
}
