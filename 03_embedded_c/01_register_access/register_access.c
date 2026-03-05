/**
 * @file register_access.c
 * @brief Phase 3 — Register Access Patterns for STM32 Peripherals
 *
 * This lesson covers the exact patterns used with STM32 CMSIS and HAL:
 *   1. CMSIS-style register structs (how STM32 headers are organized)
 *   2. Read-Modify-Write (RMW) — the most common register operation
 *   3. Write-only registers (BSRR, ICSR) — no need to read
 *   4. Status polling vs interrupt-driven — when to use each
 *   5. Register "shadow copies" — when you can't read back hardware
 *   6. Peripheral initialization sequence — order matters
 *
 * BUILD:  cmake --build build --target p3_registers
 * RUN:    .\build\03_embedded_c\p3_registers.exe
 */

#include "embedded_types.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * SIMULATED STM32F4 REGISTER BLOCKS
 *
 * These match the REAL STM32F4xx CMSIS header layout (stm32f4xx.h).
 * On a real MCU you'd just #include "stm32f4xx.h" and use GPIOA, RCC, etc.
 * We simulate them here to run on PC.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* RCC (Reset and Clock Control) — simplified */
typedef struct
{
    volatile uint32_t CR;      /* 0x00: Clock control */
    volatile uint32_t PLLCFGR; /* 0x04: PLL configuration */
    volatile uint32_t CFGR;    /* 0x08: Clock configuration */
    volatile uint32_t CIR;     /* 0x0C: Clock interrupt */
    volatile uint32_t _reserved[4];
    volatile uint32_t AHB1ENR; /* 0x30: AHB1 peripheral clock enable */
    volatile uint32_t AHB2ENR; /* 0x34: AHB2 peripheral clock enable */
    volatile uint32_t _reserved2[2];
    volatile uint32_t APB1ENR; /* 0x40: APB1 peripheral clock enable */
    volatile uint32_t APB2ENR; /* 0x44: APB2 peripheral clock enable */
} RCC_TypeDef;

/* GPIO register block */
typedef struct
{
    volatile uint32_t MODER;   /* 0x00: Mode */
    volatile uint32_t OTYPER;  /* 0x04: Output type */
    volatile uint32_t OSPEEDR; /* 0x08: Output speed */
    volatile uint32_t PUPDR;   /* 0x0C: Pull-up/pull-down */
    volatile uint32_t IDR;     /* 0x10: Input data */
    volatile uint32_t ODR;     /* 0x14: Output data */
    volatile uint32_t BSRR;    /* 0x18: Bit set/reset (WRITE-ONLY) */
    volatile uint32_t LCKR;    /* 0x1C: Configuration lock */
    volatile uint32_t AFR[2];  /* 0x20-0x24: Alternate function */
} GPIO_TypeDef;

/* USART register block (simplified) */
typedef struct
{
    volatile uint32_t SR;  /* 0x00: Status */
    volatile uint32_t DR;  /* 0x04: Data */
    volatile uint32_t BRR; /* 0x08: Baud rate */
    volatile uint32_t CR1; /* 0x0C: Control 1 */
    volatile uint32_t CR2; /* 0x10: Control 2 */
    volatile uint32_t CR3; /* 0x14: Control 3 */
} USART_TypeDef;

/* Bit definitions (subset) */
#define RCC_AHB1ENR_GPIOAEN (1UL << 0)
#define RCC_APB2ENR_USART1EN (1UL << 4)

#define USART_SR_TXE (1UL << 7)  /* TX Empty — ready to write */
#define USART_SR_TC (1UL << 6)   /* Transmission Complete */
#define USART_SR_RXNE (1UL << 5) /* RX Not Empty  */
#define USART_CR1_UE (1UL << 13) /* USART Enable */
#define USART_CR1_TE (1UL << 3)  /* TX Enable */
#define USART_CR1_RE (1UL << 2)  /* RX Enable */

/* Simulated peripheral instances */
static RCC_TypeDef sim_rcc = {0};
static GPIO_TypeDef sim_gpioa = {0};
static USART_TypeDef sim_uart1 = {.SR = USART_SR_TXE}; /* TXE high initially */

/* ── Convenience pointers (mirrors CMSIS style) ─────────────────────────── */
static RCC_TypeDef *const RCC = &sim_rcc;
static GPIO_TypeDef *const GPIOA = &sim_gpioa;
static USART_TypeDef *const USART1 = &sim_uart1;

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. READ-MODIFY-WRITE (RMW)
 *
 * Most register configs use RMW: read the current value, change only the
 * bits you care about, write back. Never clobber other fields.
 * ═══════════════════════════════════════════════════════════════════════════ */
static void gpio_set_moder(GPIO_TypeDef *gpio, uint8_t pin, uint8_t mode)
{
    /* RMW: clear 2 bits, set new mode */
    uint32_t tmp = gpio->MODER;
    tmp = (tmp & ~(3UL << (pin * 2U))) | ((uint32_t)mode << (pin * 2U));
    gpio->MODER = tmp;
}

static void gpio_set_speed(GPIO_TypeDef *gpio, uint8_t pin, uint8_t speed)
{
    uint32_t tmp = gpio->OSPEEDR;
    tmp = (tmp & ~(3UL << (pin * 2U))) | ((uint32_t)speed << (pin * 2U));
    gpio->OSPEEDR = tmp;
}

static void gpio_set_afr(GPIO_TypeDef *gpio, uint8_t pin, uint8_t af)
{
    /* AFR[0] for pins 0-7, AFR[1] for pins 8-15 */
    uint8_t idx = pin >> 3U;              /* pin/8 */
    uint8_t offset = (pin & 0x07U) << 2U; /* (pin%8)*4 */
    uint32_t tmp = gpio->AFR[idx];
    tmp = (tmp & ~(0x0FUL << offset)) | ((uint32_t)af << offset);
    gpio->AFR[idx] = tmp;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. PERIPHERAL INITIALIZATION SEQUENCE
 *
 * On STM32, ALWAYS:
 *   Step 1: Enable peripheral clock in RCC
 *   Step 2: Configure GPIO alternate function
 *   Step 3: Configure the peripheral itself
 *   Step 4: Enable the peripheral
 *
 * Doing these out of order → HardFault (accessing disabled peripheral)
 * ═══════════════════════════════════════════════════════════════════════════ */
static void uart1_init(uint32_t brr_value)
{
    printf("  Step 1: Enable GPIOA clock (for TX/RX pins)\n");
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    printf("  Step 2: Enable USART1 clock\n");
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    printf("  Step 3: Configure PA9 (TX) and PA10 (RX) as AF7\n");
    gpio_set_moder(GPIOA, 9, 2U); /* AF mode */
    gpio_set_moder(GPIOA, 10, 2U);
    gpio_set_speed(GPIOA, 9, 2U); /* High speed */
    gpio_set_speed(GPIOA, 10, 2U);
    gpio_set_afr(GPIOA, 9, 7U); /* AF7 = USART1 */
    gpio_set_afr(GPIOA, 10, 7U);

    printf("  Step 4: Configure USART1\n");
    USART1->CR1 = 0U;                          /* reset first */
    USART1->BRR = brr_value;                   /* baud rate */
    USART1->CR2 = 0U;                          /* 1 stop bit */
    USART1->CR3 = 0U;                          /* no HW flow control */
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE; /* enable TX and RX */
    USART1->CR1 |= USART_CR1_UE;               /* enable UART last */
    printf("  USART1 enabled. CR1=0x%08X  BRR=0x%08X\n",
           (uint32_t)USART1->CR1, (uint32_t)USART1->BRR);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3 . WRITE-ONLY REGISTERS (BSRR pattern)
 *
 * GPIO BSRR is write-only on STM32:
 *   Bits [15:0]  → SET the corresponding output pin
 *   Bits [31:16] → RESET (clear) the corresponding output pin
 *
 * This allows atomic set+clear of multiple pins in one write — perfect for:
 *   - SPI CS toggle
 *   - Bit-banging
 *   - Simultaneous LED update
 * ═══════════════════════════════════════════════════════════════════════════ */
static void gpio_set_pin_fast(GPIO_TypeDef *gpio, uint8_t pin)
{
    gpio->BSRR = (1UL << pin); /* set: lower 16 bits */
}
static void gpio_clear_pin_fast(GPIO_TypeDef *gpio, uint8_t pin)
{
    gpio->BSRR = (1UL << (pin + 16U)); /* reset: upper 16 bits */
}
static void gpio_atomic_set_clear(GPIO_TypeDef *gpio, uint16_t set_mask, uint16_t clear_mask)
{
    /* One atomic write: set some pins AND clear others simultaneously */
    gpio->BSRR = ((uint32_t)clear_mask << 16U) | set_mask;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. POLLING VS INTERRUPT
 * ═══════════════════════════════════════════════════════════════════════════ */
static void uart_send_byte_polled(USART_TypeDef *uart, uint8_t byte)
{
    /* Poll TXE: wait until TX buffer empty */
    while (!(uart->SR & USART_SR_TXE))
    {
        /* CPU is stuck here — wastes cycles but simplest implementation */
    }
    uart->DR = byte;
    printf("    [polled] sent 0x%02X ('%c')\n", byte, (char)byte);
}

/* With interrupts, you'd do:
 *   1. Enable TXEIE in CR1
 *   2. Write first byte to DR
 *   3. USART IRQ fires when TXE — write next byte in ISR
 *   4. When last byte sent, disable TXEIE
 * See 02_interrupt_patterns/ for the full pattern.
 */

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 3 — Lesson 1: Register Access        ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    printf("── 1. USART1 Initialization Sequence ──\n");
    uart1_init(0x0045U); /* BRR for 115200 @ 100 MHz PCLK2 */
    printf("\n");

    printf("── 2. GPIO fast pin operations (BSRR) ──\n");
    gpio_set_pin_fast(GPIOA, 5);
    printf("  BSRR=0x%08X  (after set PA5)\n", (uint32_t)GPIOA->BSRR);
    gpio_clear_pin_fast(GPIOA, 5);
    printf("  BSRR=0x%08X  (after clear PA5)\n", (uint32_t)GPIOA->BSRR);
    gpio_atomic_set_clear(GPIOA, 0x0001U, 0x0020U); /* set PA0, clear PA5 */
    printf("  BSRR=0x%08X  (atomic set PA0, clear PA5)\n\n", (uint32_t)GPIOA->BSRR);

    printf("── 3. Polled UART TX ──\n");
    const char *msg = "Hi!";
    for (const char *p = msg; *p; p++)
    {
        uart_send_byte_polled(USART1, (uint8_t)*p);
    }
    printf("\n");

    printf("EXERCISES:\n");
    printf("  1. Write gpio_config_pin() that takes a config struct and sets\n");
    printf("     MODER, OTYPER, OSPEEDR, PUPDR, AFR all at once.\n");
    printf("  2. Add shadow ODR: since BSRR is write-only, track the expected\n");
    printf("     output state in a static uint16_t shadow variable.\n");
    printf("  3. Write uart_send_string_polled() using DR/TXE polling,\n");
    printf("     then write uart_send_string_timeout() that aborts after N polls.\n");
    return 0;
}
