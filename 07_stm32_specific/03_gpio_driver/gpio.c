/**
 * @file gpio.c
 * @brief Phase 7 — GPIO Driver Implementation
 *
 * Every write uses the BSRR (Bit Set/Reset Register) for atomic operations.
 * BSRR[15:0]  = set pins     (write 1 to set output HIGH)
 * BSRR[31:16] = reset pins   (write 1 to set output LOW)
 * This means GPIO output changes are inherently atomic on Cortex-M — no
 * disable-IRQ guard needed for pin writes, unlike RMW on ODR.
 */

#include "gpio.h"
#include <stdint.h>

void gpio_init(GPIO_TypeDef *port, uint8_t pin, const gpio_config_t *cfg)
{
    uint32_t mask2 = 3UL << (pin * 2U); /* 2-bit field mask */
    uint32_t mask1 = 1UL << pin;        /* 1-bit field mask */

    /* ── Mode ── */
    port->MODER = (port->MODER & ~mask2) | ((uint32_t)cfg->mode << (pin * 2U));

    /* ── Output type ── */
    if (cfg->mode == GPIO_MODE_OUTPUT || cfg->mode == GPIO_MODE_AF)
    {
        port->OTYPER = (port->OTYPER & ~mask1) |
                       ((uint32_t)cfg->otype << pin);
    }

    /* ── Output speed ── */
    port->OSPEEDR = (port->OSPEEDR & ~mask2) |
                    ((uint32_t)cfg->speed << (pin * 2U));

    /* ── Pull-up / pull-down ── */
    port->PUPDR = (port->PUPDR & ~mask2) | ((uint32_t)cfg->pupd << (pin * 2U));

    /* ── Alternate function ── */
    if (cfg->mode == GPIO_MODE_AF)
    {
        uint8_t reg = pin >> 3U;         /* AFR[0] pins 0-7, AFR[1] pins 8-15 */
        uint8_t shift = (pin & 7U) * 4U; /* 4 bits per pin */
        uint32_t afmask = 0xFUL << shift;
        port->AFR[reg] = (port->AFR[reg] & ~afmask) |
                         ((uint32_t)cfg->af << shift);
    }

    /* ── Initial output state ── */
    if (cfg->mode == GPIO_MODE_OUTPUT)
    {
        if (cfg->init_high)
            port->BSRR = mask1;
        else
            port->BSRR = mask1 << 16U;
    }
}

void gpio_set(GPIO_TypeDef *port, uint8_t pin)
{
    port->BSRR = 1UL << pin;
}

void gpio_clear(GPIO_TypeDef *port, uint8_t pin)
{
    port->BSRR = 1UL << (pin + 16U);
}

void gpio_toggle(GPIO_TypeDef *port, uint8_t pin)
{
    /* Read ODR, write BSRR for atomic toggle */
    uint32_t odr = port->ODR;
    uint32_t bit = 1UL << pin;
    if (odr & bit)
        port->BSRR = bit << 16U; /* was high → clear */
    else
        port->BSRR = bit; /* was low  → set */
}

void gpio_write(GPIO_TypeDef *port, uint8_t pin, bool value)
{
    if (value)
        gpio_set(port, pin);
    else
        gpio_clear(port, pin);
}

bool gpio_read(GPIO_TypeDef *port, uint8_t pin)
{
    return (bool)((port->IDR >> pin) & 1UL);
}
