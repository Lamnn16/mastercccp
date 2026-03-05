/**
 * @file gpio.h
 * @brief Phase 7 — GPIO Driver for STM32F411
 *
 * Production-quality GPIO driver using raw register access.
 * No STM32 HAL dependency — this IS the layer beneath HAL_GPIO_Init().
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

/* ─── GPIO port base addresses ──────────────────────────────────────────── */
#define GPIOA_BASE 0x40020000UL
#define GPIOB_BASE 0x40020400UL
#define GPIOC_BASE 0x40020800UL
#define GPIOD_BASE 0x40020C00UL
#define GPIOE_BASE 0x40021000UL
#define GPIOH_BASE 0x40021C00UL

typedef struct
{
    volatile uint32_t MODER;   /* Mode register */
    volatile uint32_t OTYPER;  /* Output type register */
    volatile uint32_t OSPEEDR; /* Output speed register */
    volatile uint32_t PUPDR;   /* Pull-up/pull-down register */
    volatile uint32_t IDR;     /* Input data register */
    volatile uint32_t ODR;     /* Output data register */
    volatile uint32_t BSRR;    /* Bit set/reset register */
    volatile uint32_t LCKR;    /* Configuration lock register */
    volatile uint32_t AFR[2];  /* Alternate function registers [0]=low [1]=high */
} GPIO_TypeDef;

#define GPIOA ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOB ((GPIO_TypeDef *)GPIOB_BASE)
#define GPIOC ((GPIO_TypeDef *)GPIOC_BASE)

/* ─── Pin descriptor ─────────────────────────────────────────────────────── */
typedef struct
{
    GPIO_TypeDef *port;
    uint8_t pin; /* 0..15 */
} gpio_pin_t;

/* ─── Enumerations ───────────────────────────────────────────────────────── */
typedef enum
{
    GPIO_MODE_INPUT = 0U,
    GPIO_MODE_OUTPUT = 1U,
    GPIO_MODE_AF = 2U,
    GPIO_MODE_ANALOG = 3U
} gpio_mode_t;

typedef enum
{
    GPIO_OTYPE_PUSH_PULL = 0U,
    GPIO_OTYPE_OPEN_DRAIN = 1U
} gpio_otype_t;

typedef enum
{
    GPIO_OSPEED_LOW = 0U,    /*  2 MHz */
    GPIO_OSPEED_MEDIUM = 1U, /* 25 MHz */
    GPIO_OSPEED_FAST = 2U,   /* 50 MHz */
    GPIO_OSPEED_HIGH = 3U,   /*100 MHz */
} gpio_ospeed_t;

typedef enum
{
    GPIO_PUPD_NONE = 0U,
    GPIO_PUPD_PULL_UP = 1U,
    GPIO_PUPD_PULL_DOWN = 2U
} gpio_pupd_t;

typedef struct
{
    gpio_mode_t mode;
    gpio_otype_t otype;
    gpio_ospeed_t speed;
    gpio_pupd_t pupd;
    uint8_t af;     /* alternate function number 0..15 (only for AF mode) */
    bool init_high; /* initial output state (only for OUTPUT mode) */
} gpio_config_t;

/* ─── API ────────────────────────────────────────────────────────────────── */
void gpio_init(GPIO_TypeDef *port, uint8_t pin, const gpio_config_t *cfg);
void gpio_set(GPIO_TypeDef *port, uint8_t pin);
void gpio_clear(GPIO_TypeDef *port, uint8_t pin);
void gpio_toggle(GPIO_TypeDef *port, uint8_t pin);
void gpio_write(GPIO_TypeDef *port, uint8_t pin, bool value);
bool gpio_read(GPIO_TypeDef *port, uint8_t pin);

/* Common shortcut for LED on PA5 (Nucleo LD2) */
#define LED_PORT GPIOA
#define LED_PIN 5U
#define BTN_PORT GPIOC
#define BTN_PIN 13U
