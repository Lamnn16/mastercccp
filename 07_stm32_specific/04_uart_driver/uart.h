/**
 * @file uart.h
 * @brief Phase 7 — Interrupt-driven UART Driver for STM32F411
 *
 * This driver implements non-blocking TX+RX using ring buffers backed by
 * USART interrupts — the same pattern used inside every production firmware.
 *
 * TX: uart_write() copies bytes to the TX ring buffer and enables the
 *     TXE interrupt. The ISR drains the ring buffer one byte at a time.
 *
 * RX: RXNE interrupt fires when a byte arrives, ISR pushes it into the
 *     RX ring buffer. Application calls uart_read() to consume bytes.
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ─── USART register map ─────────────────────────────────────────────────── */
typedef struct
{
    volatile uint32_t SR;   /* Status register */
    volatile uint32_t DR;   /* Data register */
    volatile uint32_t BRR;  /* Baud rate register */
    volatile uint32_t CR1;  /* Control register 1 */
    volatile uint32_t CR2;  /* Control register 2 */
    volatile uint32_t CR3;  /* Control register 3 */
    volatile uint32_t GTPR; /* Guard time / prescaler */
} USART_TypeDef;

#define USART1 ((USART_TypeDef *)0x40011000UL)
#define USART2 ((USART_TypeDef *)0x40004400UL)
#define USART6 ((USART_TypeDef *)0x40011400UL)

/* SR bits */
#define USART_SR_TXE (1UL << 7)  /* TX data register empty */
#define USART_SR_TC (1UL << 6)   /* Transmission complete */
#define USART_SR_RXNE (1UL << 5) /* Read data register not empty */
#define USART_SR_ORE (1UL << 3)  /* Overrun error */

/* CR1 bits */
#define USART_CR1_UE (1UL << 13)   /* USART enable */
#define USART_CR1_TE (1UL << 3)    /* Transmitter enable */
#define USART_CR1_RE (1UL << 2)    /* Receiver enable */
#define USART_CR1_TXEIE (1UL << 7) /* TXE interrupt enable */
#define USART_CR1_RXNEIE(1UL << 5) /* RXNE interrupt enable */

/* ─── Driver configuration ───────────────────────────────────────────────── */
#define UART_TX_BUF_SIZE 64U /* must be power of 2 */
#define UART_RX_BUF_SIZE 64U

typedef struct
{
    USART_TypeDef *usart;
    uint32_t pclk_hz; /* peripheral clock feeding this USART */
    uint32_t baud;
} uart_config_t;

/* ─── API ────────────────────────────────────────────────────────────────── */
void uart_init(const uart_config_t *cfg);

/* Non-blocking write: copies up to len bytes from buf into TX ring buffer.
 * Returns number of bytes actually enqueued (may be < len if buffer full). */
size_t uart_write(const uint8_t *buf, size_t len);

/* Non-blocking read: pops up to len bytes from RX ring buffer into buf.
 * Returns number of bytes read (0 if no data available). */
size_t uart_read(uint8_t *buf, size_t len);

/* Blocking write: waits until all bytes are enqueued (spins) */
void uart_write_blocking(const uint8_t *buf, size_t len);

/* Blocking string write (null-terminated) */
void uart_print(const char *str);

/* Number of bytes available to read */
uint32_t uart_rx_available(void);

/* USART1 ISR — must be called from USART1_IRQHandler in startup file or user code */
void uart_irq_handler(void);
