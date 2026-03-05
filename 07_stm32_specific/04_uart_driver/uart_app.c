/**
 * @file uart_app.c
 * @brief Phase 7 — GPIO + UART Driver: Main Application
 *
 * Ties together the GPIO driver and UART driver into a working application:
 *   - LED on PA5 blinks at 1 Hz using SysTick
 *   - Button on PC13 toggles the blink rate
 *   - USART2 (PA2=TX, PA3=RX) echoes received bytes and prints hello message
 *
 * Physical connections on Nucleo-F411RE:
 *   PA5  → LD2 LED (active HIGH)
 *   PC13 ← B1 user button (active LOW, internal pull-up not needed on Nucleo)
 *   PA2  → USART2_TX (routed to ST-Link VCP @ 115200)
 *   PA3  ← USART2_RX
 *
 * Build target: p7_gpio_uart
 */

#include "../03_gpio_driver/gpio.h"
#include "uart.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ─── RCC enable shortcuts (from clock_config.c concepts) ──────────────── */
#define RCC_AHB1ENR (*(volatile uint32_t *)0x40023830UL)
#define RCC_APB1ENR (*(volatile uint32_t *)0x40023840UL)
#define RCC_AHB1ENR_GPIOAEN (1UL << 0)
#define RCC_AHB1ENR_GPIOCEN (1UL << 2)
#define RCC_APB1ENR_USART2EN (1UL << 17)

/* ─── SysTick ───────────────────────────────────────────────────────────── */
#define SYSTICK_CTRL (*(volatile uint32_t *)0xE000E010UL)
#define SYSTICK_LOAD (*(volatile uint32_t *)0xE000E014UL)
#define SYSTICK_VAL (*(volatile uint32_t *)0xE000E018UL)

static volatile uint32_t g_ticks = 0U;

void SysTick_Handler(void)
{
    g_ticks++;
}

static void systick_init(uint32_t core_hz)
{
    SYSTICK_LOAD = (core_hz / 1000U) - 1U; /* 1 ms tick */
    SYSTICK_VAL = 0U;
    SYSTICK_CTRL = (1UL << 2) | (1UL << 1) | (1UL << 0); /* CLKSRC|TICKINT|ENABLE */
}

static void delay_ms(uint32_t ms)
{
    uint32_t start = g_ticks;
    while ((g_ticks - start) < ms)
    {
        __asm volatile("wfi");
    }
}

/* ─── Hardware init ─────────────────────────────────────────────────────── */
static void hw_init(void)
{
    /* Enable GPIO A and C clocks */
    RCC_AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;
    volatile uint32_t dummy = RCC_AHB1ENR;
    (void)dummy; /* read-back */

    /* PA5 = LED output, push-pull, low speed, no pull */
    gpio_config_t led_cfg = {
        .mode = GPIO_MODE_OUTPUT, .otype = GPIO_OTYPE_PUSH_PULL, .speed = GPIO_OSPEED_LOW, .pupd = GPIO_PUPD_NONE, .af = 0U, .init_high = false};
    gpio_init(LED_PORT, LED_PIN, &led_cfg);

    /* PC13 = button input, pull-up (active LOW) */
    gpio_config_t btn_cfg = {
        .mode = GPIO_MODE_INPUT, .otype = GPIO_OTYPE_PUSH_PULL, .speed = GPIO_OSPEED_LOW, .pupd = GPIO_PUPD_PULL_UP, .af = 0U, .init_high = false};
    gpio_init(BTN_PORT, BTN_PIN, &btn_cfg);

    /* PA2 = USART2_TX (AF7), PA3 = USART2_RX (AF7) */
    gpio_config_t uart_tx_cfg = {
        .mode = GPIO_MODE_AF, .otype = GPIO_OTYPE_PUSH_PULL, .speed = GPIO_OSPEED_FAST, .pupd = GPIO_PUPD_NONE, .af = 7U, .init_high = true};
    gpio_config_t uart_rx_cfg = {
        .mode = GPIO_MODE_AF, .otype = GPIO_OTYPE_PUSH_PULL, .speed = GPIO_OSPEED_FAST, .pupd = GPIO_PUPD_PULL_UP, .af = 7U, .init_high = true /* init_high ignored for inputs */
    };
    gpio_init(GPIOA, 2U, &uart_tx_cfg);
    gpio_init(GPIOA, 3U, &uart_rx_cfg);

    /* Enable USART2 clock and init driver */
    RCC_APB1ENR |= RCC_APB1ENR_USART2EN;
    dummy = RCC_APB1ENR;
    (void)dummy;

    uart_config_t ucfg = {
        .usart = USART2,
        .pclk_hz = 50000000UL, /* APB1 = 50 MHz at 100 MHz SYSCLK */
        .baud = 115200U};
    uart_init(&ucfg);
}

/* ─── Main ──────────────────────────────────────────────────────────────── */
int main(void)
{
    systick_init(100000000UL); /* 100 MHz core clock */
    hw_init();

    uart_print("\r\n=== STM32F411 GPIO + UART Demo ===\r\n");
    uart_print("Send any character to echo back.\r\nLED blinks on PA5.\r\n\r\n");

    uint32_t blink_ms = 500U;
    uint32_t last_blink = 0U;
    bool btn_debounce = false;
    uint32_t btn_time = 0U;

    while (1)
    {
        /* ── LED blink ── */
        if ((g_ticks - last_blink) >= blink_ms)
        {
            gpio_toggle(LED_PORT, LED_PIN);
            last_blink = g_ticks;
        }

        /* ── Button: toggle blink rate ── */
        bool btn_pressed = !gpio_read(BTN_PORT, BTN_PIN);
        if (btn_pressed && !btn_debounce)
        {
            btn_debounce = true;
            btn_time = g_ticks;
        }
        if (btn_debounce && (g_ticks - btn_time) > 50U)
        { /* 50ms debounce */
            if (!gpio_read(BTN_PORT, BTN_PIN))
            { /* still pressed */
                blink_ms = (blink_ms == 500U) ? 100U : 500U;
                uart_print("Button: blink rate changed\r\n");
            }
            btn_debounce = false;
        }

        /* ── UART echo ── */
        if (uart_rx_available() > 0U)
        {
            uint8_t byte = 0U;
            uart_read(&byte, 1U);
            uart_print("Echo: [");
            uart_write(&byte, 1U);
            uart_print("]\r\n");
        }

        /* ── WFI: sleep until next interrupt (SysTick or UART RXNE) ── */
        __asm volatile("wfi");
    }
}
