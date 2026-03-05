/**
 * @file uart.c
 * @brief Phase 7 — Interrupt-driven UART Driver Implementation
 *
 * Key design points:
 *   - TX ring buffer drained by TXE ISR (zero CPU spin in main code)
 *   - RX ring buffer filled by RXNE ISR (zero bytes lost if app is slow)
 *   - Indices use full uint32_t, masked with (SIZE-1) for power-of-2 access
 *   - volatile on indices because ISR and main share them
 *   - No disable_irq needed for tail (single reader/writer per direction)
 */

#include "uart.h"
#include <string.h>
#include <stdint.h>

/* ─── Ring buffers ───────────────────────────────────────────────────────── */
#define TX_MASK (UART_TX_BUF_SIZE - 1U)
#define RX_MASK (UART_RX_BUF_SIZE - 1U)

static uint8_t s_tx_buf[UART_TX_BUF_SIZE];
static volatile uint32_t s_tx_head = 0U; /* ISR reads from head */
static volatile uint32_t s_tx_tail = 0U; /* main writes to tail */

static uint8_t s_rx_buf[UART_RX_BUF_SIZE];
static volatile uint32_t s_rx_head = 0U; /* ISR writes to head */
static volatile uint32_t s_rx_tail = 0U; /* main reads from tail */

static USART_TypeDef *s_usart = NULL;

/* ─── Ring buffer helpers ────────────────────────────────────────────────── */
static inline bool tx_empty(void) { return s_tx_head == s_tx_tail; }
static inline bool tx_full(void) { return (s_tx_tail - s_tx_head) >= UART_TX_BUF_SIZE; }
static inline bool rx_empty(void) { return s_rx_head == s_rx_tail; }
static inline bool rx_full(void) { return (s_rx_head - s_rx_tail) >= UART_RX_BUF_SIZE; }

/* ─── Init ───────────────────────────────────────────────────────────────── */
void uart_init(const uart_config_t *cfg)
{
    s_usart = cfg->usart;

    /* 1. Set baud rate
     *    BRR = fPCLK / baud  (for OVER8=0, 16x oversampling)
     *    Value is split: USARTDIV_mantissa in bits[15:4], fraction in bits[3:0]
     *    Integer divide gives the mantissa, fractional part = ((fPCLK % baud)*16)/baud */
    uint32_t div = (cfg->pclk_hz + cfg->baud / 2U) / cfg->baud; /* rounded */
    s_usart->BRR = div;

    /* 2. Enable UE, TE, RE, RXNEIE (TXE IE enabled on demand when we have data) */
    s_usart->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE |
                   USART_CR1_RXNEIE(1UL);
}

/* ─── Non-blocking write ─────────────────────────────────────────────────── */
size_t uart_write(const uint8_t *buf, size_t len)
{
    size_t written = 0U;
    while (written < len && !tx_full())
    {
        s_tx_buf[s_tx_tail & TX_MASK] = buf[written];
        s_tx_tail++;
        written++;
    }
    /* Enable TXE interrupt to drain the buffer */
    if (!tx_empty())
    {
        s_usart->CR1 |= USART_CR1_TXEIE;
    }
    return written;
}

/* ─── Non-blocking read ──────────────────────────────────────────────────── */
size_t uart_read(uint8_t *buf, size_t len)
{
    size_t read = 0U;
    while (read < len && !rx_empty())
    {
        buf[read] = s_rx_buf[s_rx_tail & RX_MASK];
        s_rx_tail++;
        read++;
    }
    return read;
}

/* ─── Blocking write ─────────────────────────────────────────────────────── */
void uart_write_blocking(const uint8_t *buf, size_t len)
{
    size_t sent = 0U;
    while (sent < len)
    {
        size_t n = uart_write(buf + sent, len - sent);
        sent += n;
        /* In real RTOS: task_yield() or semaphore wait instead of spin */
    }
    /* Wait for TX to physically complete (important before clock changes) */
    while (!(s_usart->SR & USART_SR_TC))
    {
    }
}

/* ─── String print ───────────────────────────────────────────────────────── */
void uart_print(const char *str)
{
    uart_write_blocking((const uint8_t *)str, strlen(str));
}

/* ─── RX available ───────────────────────────────────────────────────────── */
uint32_t uart_rx_available(void)
{
    return s_rx_head - s_rx_tail;
}

/* ─── ISR ────────────────────────────────────────────────────────────────── */
void uart_irq_handler(void)
{
    uint32_t sr = s_usart->SR;

    /* ── RX: byte received ── */
    if (sr & USART_SR_RXNE)
    {
        uint8_t byte = (uint8_t)(s_usart->DR & 0xFFU); /* clears RXNE */
        if (!rx_full())
        {
            s_rx_buf[s_rx_head & RX_MASK] = byte;
            s_rx_head++;
        }
        /* else: overrun — byte dropped. Track with a counter in production. */
    }

    /* ── TX: data register empty ── */
    if (sr & USART_SR_TXE)
    {
        if (!tx_empty())
        {
            s_usart->DR = s_tx_buf[s_tx_head & TX_MASK]; /* clears TXE */
            s_tx_head++;
        }
        else
        {
            /* Nothing left to send — disable TXE interrupt to avoid re-entry */
            s_usart->CR1 &= ~USART_CR1_TXEIE;
        }
    }

    /* ── Overrun error: clear by reading SR then DR ── */
    if (sr & USART_SR_ORE)
    {
        volatile uint32_t dummy = s_usart->DR;
        (void)dummy;
    }
}

/* Connect to vector table — weak override of startup Default_Handler */
void USART1_IRQHandler(void) __attribute__((weak, alias("uart_irq_handler")));
