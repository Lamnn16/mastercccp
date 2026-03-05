/**
 * @file constexpr_demo.cpp
 * @brief Phase 2 — constexpr: Computation at Compile Time
 *
 * constexpr = "compute this at compile time, not at runtime"
 *
 * Why this is a game changer for embedded:
 *   - Baud rate register calculations → precomputed, no floating-point at runtime
 *   - CRC tables → computed at compile time, stored in Flash
 *   - Frequency dividers, timer reload values → exact integer math at compile time
 *   - Type-safe, checked constants (unlike #define macros)
 *
 * constexpr GUARANTEES that if the input is a compile-time constant, the
 * result is also computed at compile time — zero runtime cost.
 *
 * BUILD:  cmake --build build --target p2_constexpr
 * RUN:    .\build\02_cpp_fundamentals\p2_constexpr.exe
 */

#include "embedded_types.h"
#include <cstdio>
#include <array>

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 1: constexpr VARIABLES AND FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── Clock tree constants (STM32F411 @ 100 MHz) ─────────────────────────── */
namespace clk {
    constexpr uint32_t HSI_HZ       = 16'000'000U;   /* C++14: digit separators */
    constexpr uint32_t HSE_HZ       = 25'000'000U;
    constexpr uint32_t PLL_N        = 200U;           /* VCO multiplier */
    constexpr uint32_t PLL_M        = 25U;            /* VCO divider */
    constexpr uint32_t PLL_P        = 2U;             /* SYSCLK divider */
    constexpr uint32_t SYSCLK_HZ    = (HSE_HZ / PLL_M) * PLL_N / PLL_P;  /* 100 MHz */
    constexpr uint32_t APB1_DIV     = 2U;
    constexpr uint32_t APB2_DIV     = 1U;
    constexpr uint32_t PCLK1_HZ     = SYSCLK_HZ / APB1_DIV;  /* 50 MHz */
    constexpr uint32_t PCLK2_HZ     = SYSCLK_HZ / APB2_DIV;  /* 100 MHz */
}

/* Verify at compile time that clock config is valid */
static_assert(clk::SYSCLK_HZ == 100'000'000U, "SYSCLK must be 100 MHz");
static_assert(clk::PCLK1_HZ  ==  50'000'000U, "PCLK1 must be 50 MHz");

/* ── USART BRR (Baud Rate Register) computation ─────────────────────────── */
/*
 * STM32 USART_BRR = (PCLK / (16 * baud_rate))  scaled by 16 for fractions
 * This is the EXACT formula — computed by the compiler, not at runtime.
 */
constexpr uint32_t usart_brr(uint32_t pclk_hz, uint32_t baud)
{
    return (pclk_hz + (baud / 2U)) / baud;   /* round to nearest */
}

constexpr uint32_t BRR_115200 = usart_brr(clk::PCLK2_HZ, 115200U);
constexpr uint32_t BRR_9600   = usart_brr(clk::PCLK2_HZ,   9600U);

/* ── Timer ARR (Auto-Reload Register) for period control ────────────────── */
/*
 * Timer period: ARR = (PCLK / (prescaler+1)) / frequency - 1
 * For 1 kHz PWM on TIM1 (PCLK2=100MHz), prescaler=99:
 *   ARR = 100,000,000 / (99+1) / 1000 - 1 = 999
 */
constexpr uint32_t timer_arr(uint32_t pclk_hz, uint32_t prescaler,
                              uint32_t freq_hz)
{
    return (pclk_hz / (prescaler + 1U) / freq_hz) - 1U;
}

constexpr uint32_t TIM_ARR_1KHZ  = timer_arr(clk::PCLK2_HZ, 99U,  1000U);
constexpr uint32_t TIM_ARR_10KHZ = timer_arr(clk::PCLK2_HZ,  9U, 10000U);

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 2: constexpr CRC TABLE — computed at compile time, lives in Flash
 *
 * Without constexpr: table computed at startup (runtime cost each boot)
 * With constexpr:    table embedded in .rodata at compile time (zero runtime cost)
 * ═══════════════════════════════════════════════════════════════════════════ */

constexpr uint8_t crc8_entry(uint8_t byte)
{
    uint8_t crc = byte;
    for (int i = 0; i < 8; i++) {
        crc = (crc & 0x80U) ? ((crc << 1U) ^ 0x07U) : (crc << 1U);
    }
    return crc;
}

/* Generate all 256 table entries at compile time */
template <size_t... I>
constexpr std::array<uint8_t, 256> make_crc8_table_impl(
    std::index_sequence<I...>)
{
    return {{ crc8_entry(static_cast<uint8_t>(I))... }};
}

constexpr auto CRC8_TABLE_CT = make_crc8_table_impl(
    std::make_index_sequence<256>{});

/* Runtime CRC using compile-time table */
uint8_t crc8_ct(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00U;
    for (size_t i = 0; i < len; i++) {
        crc = CRC8_TABLE_CT[crc ^ data[i]];
    }
    return crc;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 3: if constexpr — platform-specific code without preprocessor
 *
 * if constexpr evaluated at compile time — the false branch is NEVER compiled.
 * This replaces many #ifdef USE_DMA / #else patterns in embedded code.
 * ═══════════════════════════════════════════════════════════════════════════ */

enum class DmaSupport { Yes, No };

template <DmaSupport Dma>
void transmit_bytes(const uint8_t *buf, size_t len)
{
    if constexpr (Dma == DmaSupport::Yes) {
        printf("  [DMA ]  transmitting %zu bytes via DMA channel\n", len);
        /* DMA-specific code here — not compiled when Dma==No */
    } else {
        printf("  [Busy]  transmitting %zu bytes via busy-wait\n", len);
        /* Boring loop here — not compiled when Dma==Yes */
    }
    (void)buf;
}

/* ─── DEMOS ──────────────────────────────────────────────────────────── */

static void demo_clock_constants(void)
{
    printf("── Compile-time clock constants ──\n");
    printf("  SYSCLK  = %u Hz (%.1f MHz)\n", clk::SYSCLK_HZ,
           clk::SYSCLK_HZ / 1000000.0f);
    printf("  PCLK1   = %u Hz\n", clk::PCLK1_HZ);
    printf("  PCLK2   = %u Hz\n", clk::PCLK2_HZ);
    printf("  BRR@115200 = %u  (write to USART_BRR)\n", BRR_115200);
    printf("  BRR@9600   = %u\n", BRR_9600);
    printf("  TIM ARR@1kHz   = %u  (write to TIM_ARR)\n", TIM_ARR_1KHZ);
    printf("  TIM ARR@10kHz  = %u\n\n", TIM_ARR_10KHZ);
}

static void demo_constexpr_crc(void)
{
    printf("── constexpr CRC-8 table (computed at compile time) ──\n");
    printf("  table[0]=0x%02X  table[1]=0x%02X  table[255]=0x%02X\n",
           CRC8_TABLE_CT[0], CRC8_TABLE_CT[1], CRC8_TABLE_CT[255]);

    uint8_t data[] = { 0x01, 0x10, 0x00, 0x00, 0x01, 0x40 };
    printf("  CRC8 of test packet: 0x%02X\n\n", crc8_ct(data, sizeof(data)));
}

static void demo_if_constexpr(void)
{
    printf("── if constexpr (zero-overhead platform selection) ──\n");
    uint8_t frame[] = { 0xAA, 0x55 };
    transmit_bytes<DmaSupport::Yes>(frame, sizeof(frame));
    transmit_bytes<DmaSupport::No> (frame, sizeof(frame));
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * EXERCISES
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * EXERCISE 1:
 *   Write a constexpr function spi_prescaler(uint32_t pclk, uint32_t target_hz)
 *   that returns the SPI_CR1.BR[2:0] value (0..7) for the largest SPI clock
 *   that is ≤ target_hz.  SPI clock = PCLK / 2^(BR+1).
 *
 * EXERCISE 2:
 *   Create a constexpr I2C timing config struct for STM32 I2C (I2C_TIMINGR).
 *   Fields: PRESC, SCLDEL, SDADEL, SCLH, SCLL.
 *   Compute them for 400 kHz Fast Mode given PCLK1 = 50 MHz.
 *
 * EXERCISE 3:
 *   Write a constexpr function is_valid_prescaler(uint32_t n) that returns
 *   true if n is one of {1,2,4,8,16,32,64,128,256,512} (RCC HCLK prescalers).
 *   Use it in a static_assert.
 */

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 2 — Lesson 4: constexpr              ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    demo_clock_constants();
    demo_constexpr_crc();
    demo_if_constexpr();
    return 0;
}
