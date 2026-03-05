/**
 * @file spi_sim.c
 * @brief Phase 5 — SPI Protocol: Bit-bang Implementation and Register Driver
 *
 * SPI (Serial Peripheral Interface) is used for:
 *   ADCs, DACs, displays (ST7789, ILI9341), SD cards, flash memory, IMUs
 *
 * This lesson covers:
 *   1. SPI timing and modes (CPOL/CPHA)
 *   2. Bit-bang SPI — implement in software on any GPIO
 *   3. Register-based SPI driver (uses hardware SPI peripheral)
 *   4. Multi-byte transfer with chip-select protocol
 *
 * BUILD:  cmake --build build --target p5_spi_sim
 */

#include "embedded_types.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * SPI MODES (CPOL and CPHA)
 *
 * CPOL=0: clock idle LOW    CPOL=1: clock idle HIGH
 * CPHA=0: sample on rising  CPHA=1: sample on falling (when CPOL=0)
 *
 * Most common SPI sensors use Mode 0 (CPOL=0, CPHA=0).
 * SD cards use Mode 0. STM32 dataflash: Mode 3.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum
{
    SPI_MODE0 = 0,
    SPI_MODE1,
    SPI_MODE2,
    SPI_MODE3
} spi_mode_t;

/* Simulated GPIO state */
static uint8_t g_clk = 0, g_mosi = 0, g_miso = 0, g_cs = 1;
static uint32_t g_miso_shift_reg = 0xA5U; /* simulated incoming byte */

static void gpio_write_clk(uint8_t v) { g_clk = v; }
static void gpio_write_mosi(uint8_t v) { g_mosi = v; }
static void gpio_write_cs(uint8_t v) { g_cs = v; }
static uint8_t gpio_read_miso(void)
{
    /* Simulate: return next bit of g_miso_shift_reg */
    uint8_t bit = (g_miso_shift_reg >> 7U) & 1U;
    g_miso_shift_reg <<= 1U;
    return bit;
}

/* ─── Bit-bang SPI Mode 0 ────────────────────────────────────────────────── */
static uint8_t spi_bb_transfer_byte(uint8_t tx_byte, bool verbose)
{
    uint8_t rx_byte = 0U;
    if (verbose)
        printf("  BB SPI: TX=0x%02X → ", tx_byte);

    for (int bit = 7; bit >= 0; bit--)
    {
        /* Setup MOSI before rising edge */
        gpio_write_mosi((tx_byte >> bit) & 1U);
        gpio_write_clk(0);
        /* Rising edge: sample MISO */
        gpio_write_clk(1);
        rx_byte |= (uint8_t)(gpio_read_miso() << bit);
    }
    gpio_write_clk(0);

    if (verbose)
        printf("RX=0x%02X\n", rx_byte);
    return rx_byte;
}

/* Read register from a SPI sensor (generic pattern) */
static uint8_t spi_read_reg(uint8_t reg_addr)
{
    gpio_write_cs(0);                              /* assert CS */
    spi_bb_transfer_byte(reg_addr | 0x80U, false); /* read command: bit7=1 */
    g_miso_shift_reg = 0x42U;                      /* sensor returns this value */
    uint8_t val = spi_bb_transfer_byte(0x00U, false);
    gpio_write_cs(1); /* deassert CS */
    return val;
}

/* ─── STM32-style hardware SPI driver (simulated registers) ─────────────── */
typedef struct
{
    volatile uint32_t CR1; /* Control register 1 */
    volatile uint32_t CR2; /* Control register 2 */
    volatile uint32_t SR;  /* Status register */
    volatile uint32_t DR;  /* Data register */
} SPI_TypeDef;

#define SPI_SR_TXE (1UL << 1)   /* TX buffer empty */
#define SPI_SR_RXNE (1UL << 0)  /* RX buffer not empty */
#define SPI_SR_BSY (1UL << 7)   /* SPI busy */
#define SPI_CR1_SPE (1UL << 6)  /* SPI enable */
#define SPI_CR1_MSTR (1UL << 2) /* Master mode */

static SPI_TypeDef sim_spi1 = {.SR = SPI_SR_TXE};

static uint8_t hw_spi_transfer(SPI_TypeDef *spi, uint8_t tx)
{
    while (!(spi->SR & SPI_SR_TXE))
    {
    } /* wait for TX empty */
    spi->DR = tx;
    sim_spi1.SR &= ~SPI_SR_TXE;
    sim_spi1.SR |= SPI_SR_RXNE; /* simulate receive */
    sim_spi1.DR = 0xA5U;        /* echo-back simulation */
    while (!(spi->SR & SPI_SR_RXNE))
    {
    } /* wait for RX data */
    sim_spi1.SR |= SPI_SR_TXE;
    sim_spi1.SR &= ~SPI_SR_RXNE;
    return (uint8_t)spi->DR;
}

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 5 — Lesson 2: SPI Simulation         ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    printf("── Bit-bang SPI Mode 0 ──\n");
    g_miso_shift_reg = 0xA5U;
    uint8_t rx = spi_bb_transfer_byte(0x3FU, true);
    printf("  expected RX=0xA5, got=0x%02X\n\n", rx);

    printf("── SPI register read (WHO_AM_I pattern) ──\n");
    uint8_t who_am_i = spi_read_reg(0x0FU);
    printf("  WHO_AM_I(0x0F) = 0x%02X  CS=%u\n\n", who_am_i, g_cs);

    printf("── Hardware SPI transfer ──\n");
    uint8_t hrx = hw_spi_transfer(&sim_spi1, 0x55U);
    printf("  HW SPI TX=0x55 RX=0x%02X\n\n", hrx);

    printf("EXERCISES:\n");
    printf("  1. Implement bit-bang SPI Mode 3 (CPOL=1, CPHA=1).\n");
    printf("  2. Write spi_write_burst(reg, buf, len) that transfers multiple\n");
    printf("     bytes with CS held low throughout the burst.\n");
    printf("  3. Implement an LIS3DH accelerometer driver (I2C/SPI) that reads\n");
    printf("     OUT_X_L/H, OUT_Y_L/H, OUT_Z_L/H and returns a 3D vector.\n");
    return 0;
}
