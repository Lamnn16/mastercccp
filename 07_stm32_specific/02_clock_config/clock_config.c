/**
 * @file clock_config.c
 * @brief Phase 7 — STM32F411: Clock Tree Configuration
 *
 * Clocking is the foundation of every STM32 firmware.
 * Wrong clock setup = wrong UART baud, wrong timer period, wrong ADC sample rate.
 *
 * STM32F411RE clock tree:
 *
 *   HSI (16 MHz RC) ──┐
 *   HSE (8–26 MHz) ───┤──→ PLL ──→ SYSCLK ──→ AHB ──→ APB1/APB2
 *                     └─────────────────────→ (bypass PLL)
 *
 * Target: 100 MHz from HSE=8 MHz (Nucleo-64 crystal)
 *   PLL_M = 8  (VCO input = 8/8 = 1 MHz)
 *   PLL_N = 200 (VCO output = 200 MHz)
 *   PLL_P = 2  (SYSCLK = 200/2 = 100 MHz)
 *   PLL_Q = 4  (USB/SDIO = 200/4 = 50 MHz — must be 48 MHz for USB)
 *
 * APB1 prescaler = /2 → PCLK1 = 50 MHz (TIM2..5 clock = 100 MHz × 2 = wait, no:
 *   TIM2..5 clock = PCLK1 × 2 when APB1 prescaler ≠ 1 → 100 MHz)
 * APB2 prescaler = /1 → PCLK2 = 100 MHz (USART1, TIM1, ADC)
 *
 * Flash latency: at 100 MHz + 3.3V → WS=3 wait states required.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

/* ─── RCC register map ────────────────────────────────────────────────────── */
typedef struct
{
    volatile uint32_t CR;
    volatile uint32_t PLLCFGR;
    volatile uint32_t CFGR;
    volatile uint32_t CIR;
    volatile uint32_t AHB1RSTR;
    volatile uint32_t AHB2RSTR;
    uint32_t RESERVED0[2];
    volatile uint32_t APB1RSTR;
    volatile uint32_t APB2RSTR;
    uint32_t RESERVED1[2];
    volatile uint32_t AHB1ENR;
    volatile uint32_t AHB2ENR;
    uint32_t RESERVED2[2];
    volatile uint32_t APB1ENR;
    volatile uint32_t APB2ENR;
    uint32_t RESERVED3[2];
    volatile uint32_t AHB1LPENR;
    volatile uint32_t AHB2LPENR;
    uint32_t RESERVED4[2];
    volatile uint32_t APB1LPENR;
    volatile uint32_t APB2LPENR;
    uint32_t RESERVED5[2];
    volatile uint32_t BDCR;
    volatile uint32_t CSR;
    uint32_t RESERVED6[2];
    volatile uint32_t SSCGR;
    volatile uint32_t PLLI2SCFGR;
    uint32_t RESERVED7;
    volatile uint32_t DCKCFGR;
} RCC_TypeDef;

#define RCC ((RCC_TypeDef *)0x40023800UL)

/* CR bits */
#define RCC_CR_HSEON (1UL << 16)
#define RCC_CR_HSERDY (1UL << 17)
#define RCC_CR_PLLON (1UL << 24)
#define RCC_CR_PLLRDY (1UL << 25)
#define RCC_CR_HSION (1UL << 0)
#define RCC_CR_HSIRDY (1UL << 1)

/* CFGR bits */
#define RCC_CFGR_SW_PLL (0x2UL << 0)
#define RCC_CFGR_SWS_PLL (0x2UL << 2)
#define RCC_CFGR_SWS_Msk (0x3UL << 2)
/* AHB prescaler = /1 */
#define RCC_CFGR_HPRE_1 (0x0UL << 4)
/* APB1 prescaler = /2 */
#define RCC_CFGR_PPRE1_2 (0x4UL << 10)
/* APB2 prescaler = /1 */
#define RCC_CFGR_PPRE2_1 (0x0UL << 13)

/* PLLCFGR bits */
#define RCC_PLLCFGR_PLLSRC_HSE (1UL << 22)

/* Flash interface */
typedef struct
{
    volatile uint32_t ACR;
    volatile uint32_t KEYR;
    volatile uint32_t OPTKEYR;
    volatile uint32_t SR;
    volatile uint32_t CR;
    volatile uint32_t OPTCR;
} FLASH_TypeDef;

#define FLASH ((FLASH_TypeDef *)0x40023C00UL)
#define FLASH_ACR_LATENCY_3WS (3UL << 0)
#define FLASH_ACR_PRFTEN (1UL << 8)
#define FLASH_ACR_ICEN (1UL << 9)
#define FLASH_ACR_DCEN (1UL << 10)

/* ─── Clock configuration ────────────────────────────────────────────────── */
/* PLL parameters for 100 MHz from 8 MHz HSE */
#define PLL_M 8U
#define PLL_N 200U
#define PLL_P 2U /* 0=div2, 1=div4, 2=div6, 3=div8 in register */
#define PLL_Q 4U

static bool system_clock_config(void)
{
    /* ── Step 1: Enable HSE and wait for it to stabilise ── */
    RCC->CR |= RCC_CR_HSEON;
    uint32_t timeout = 50000U;
    while (!(RCC->CR & RCC_CR_HSERDY) && --timeout)
    {
    }
    if (!timeout)
        return false; /* HSE failed to start — fallback to HSI */

    /* ── Step 2: Set Flash latency before increasing frequency ── */
    FLASH->ACR = FLASH_ACR_LATENCY_3WS | FLASH_ACR_PRFTEN |
                 FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    /* ── Step 3: Configure PLL ── */
    RCC->PLLCFGR = (PLL_M << 0) |
                   (PLL_N << 6) |
                   ((PLL_P / 2U - 1U) << 16) | /* P: 0=÷2, 1=÷4 */
                   RCC_PLLCFGR_PLLSRC_HSE |
                   (PLL_Q << 24);

    /* ── Step 4: Set bus prescalers BEFORE enabling PLL ── */
    RCC->CFGR = RCC_CFGR_HPRE_1 | RCC_CFGR_PPRE1_2 | RCC_CFGR_PPRE2_1;

    /* ── Step 5: Enable PLL ── */
    RCC->CR |= RCC_CR_PLLON;
    timeout = 50000U;
    while (!(RCC->CR & RCC_CR_PLLRDY) && --timeout)
    {
    }
    if (!timeout)
        return false;

    /* ── Step 6: Switch SYSCLK to PLL ── */
    RCC->CFGR = (RCC->CFGR & ~0x3UL) | RCC_CFGR_SW_PLL;
    timeout = 50000U;
    while (((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_PLL) && --timeout)
    {
    }
    if (!timeout)
        return false;

    /* ── Step 7: Disable HSI (saves ~2 mA) ── */
    RCC->CR &= ~RCC_CR_HSION;

    return true;
}

/* ─── Verify clock frequencies at runtime (using SysTick measurement) ─────── */
static void print_clock_tree(void)
{
    /* In a real project, read CFGR prescaler fields and calculate dynamically.
     * Here we print the expected values for our configuration. */
    printf("Clock tree (100 MHz from 8 MHz HSE via PLL):\n");
    printf("  HSE      %2u MHz\n", 8U);
    printf("  PLL_M    %2u  → VCO input = %u MHz\n", PLL_M, 8U / PLL_M);
    printf("  PLL_N    %3u → VCO output = %u MHz\n", PLL_N, (8U / PLL_M) * PLL_N);
    printf("  PLL_P    %2u  → SYSCLK    = %u MHz\n", PLL_P, (8U / PLL_M) * PLL_N / PLL_P);
    printf("  AHB      /1  → HCLK      = %u MHz  (CPU, DMA, Flash)\n",
           (8U / PLL_M) * PLL_N / PLL_P);
    printf("  APB1     /2  → PCLK1     = %u MHz  (SPI2/3, I2C, TIM2-5)\n",
           (8U / PLL_M) * PLL_N / PLL_P / 2U);
    printf("  APB2     /1  → PCLK2     = %u MHz  (SPI1, USART1/6, ADC, TIM1)\n",
           (8U / PLL_M) * PLL_N / PLL_P);
    printf("  TIM2-5 clock = PCLK1 × 2 = %u MHz (prescaler ≠ /1)\n\n",
           (8U / PLL_M) * PLL_N / PLL_P);
}

/* ─── Peripheral clock enable helpers ───────────────────────────────────── */
#define RCC_AHB1ENR_GPIOAEN (1UL << 0)
#define RCC_AHB1ENR_GPIOBEN (1UL << 1)
#define RCC_AHB1ENR_GPIOCEN (1UL << 2)
#define RCC_APB2ENR_USART1EN (1UL << 4)
#define RCC_APB1ENR_TIM2EN (1UL << 0)
#define RCC_APB2ENR_SPI1EN (1UL << 12)

static void enable_peripheral_clocks(void)
{
    /* Enable GPIOA, GPIOB, GPIOC */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
    /* Enable USART1 (on APB2), TIM2 (APB1), SPI1 (APB2) */
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_SPI1EN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    /* Read back to guarantee clock is running before accessing peripheral */
    volatile uint32_t dummy = RCC->AHB1ENR;
    (void)dummy;
}

int main(void)
{
    /* Note: on real hardware, system_clock_config() must be called first.
     * Here we just demonstrate and print the expected configuration. */
    printf("=== Phase 7 — STM32F411 Clock Configuration ===\n\n");
    print_clock_tree();

    printf("Flash latency at 100 MHz (3.3V): 3 wait states\n");
    printf("  FLASH_ACR = 0x%03X (LATENCY=3, PRFTEN, ICEN, DCEN)\n\n",
           FLASH_ACR_LATENCY_3WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN);

    printf("PLL register value:\n");
    uint32_t pllcfgr = (PLL_M << 0) | (PLL_N << 6) |
                       ((PLL_P / 2U - 1U) << 16) |
                       RCC_PLLCFGR_PLLSRC_HSE | (PLL_Q << 24);
    printf("  RCC_PLLCFGR = 0x%08lX\n\n", (unsigned long)pllcfgr);

    printf("Critical notes:\n");
    printf("  1. Always set Flash latency BEFORE increasing SYSCLK.\n");
    printf("     Failure → CPU reads stale instructions = hard fault.\n");
    printf("  2. Always set APB prescalers BEFORE switching SYSCLK source.\n");
    printf("  3. Wait for PLLRDY before switching to PLL as SYSCLK source.\n");
    printf("  4. USART BRR = PCLK / (16 × baud) — uses PCLK2 for USART1.\n");
    printf("     At 100 MHz, BRR for 115200 = %u\n",
           100000000U / (16U * 115200U));

    printf("\nEXERCISES:\n");
    printf("  1. Calculate the correct BRR value for 9600 baud on USART2\n");
    printf("     (on APB1, PCLK1=50 MHz). Verify with actual hardware.\n");
    printf("  2. Modify the PLL to target 84 MHz (PLL_N=168, PLL_P=2, PLL_M=4\n");
    printf("     for a 8 MHz HSE). What Flash latency does this require?\n");
    printf("  3. Why must you read-back RCC_AHB1ENR after enabling a clock?\n");
    printf("     Hint: look up 'AHB bus write latency' in the STM32 errata.\n");
    return 0;
}
