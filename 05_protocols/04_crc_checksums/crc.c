/**
 * @file crc.c
 * @brief Phase 5 — CRC Checksums: Theory to Hardware Acceleration
 *
 * CRC is the standard integrity check in embedded communications:
 *   UART frames, SPI flash reads, firmware updates, BLE packets,
 *   CANbus messages, and filesystem metadata all use CRC variants.
 *
 * This lesson covers:
 *   1. CRC theory — polynomial division over GF(2)
 *   2. CRC-8/MAXIM (Dallas 1-Wire), CRC-16/CCITT, CRC-32/ISO-HDLC
 *   3. Bitwise vs table-driven implementations (speed/flash tradeoff)
 *   4. STM32 CRC hardware peripheral simulation
 *   5. Testing with verified reference vectors
 *
 * BUILD:  cmake --build build --target p5_crc
 */

#include "embedded_types.h"

/* ─── Reference test data ────────────────────────────────────────────────── */
static const uint8_t k_data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
#define DATA_LEN (sizeof(k_data))

/* ═══════════════════════════════════════════════════════════════════════════
 * CRC-8/MAXIM (poly=0x31, init=0x00, refin=true, refout=true, xorout=0x00)
 * Used by Dallas/Maxim 1-Wire devices (DS18B20 temperature sensor).
 * Reference result for "123456789" → 0xA1
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Reflect bits in a byte (bit-reverse) */
static uint8_t reflect8(uint8_t b)
{
    uint8_t r = 0U;
    for (int i = 0; i < 8; i++)
    {
        r |= (uint8_t)(((b >> i) & 1U) << (7 - i));
    }
    return r;
}

/* Bitwise CRC-8/MAXIM — educational, no lookup table */
static uint8_t crc8_maxim_bitwise(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00U;
    for (size_t i = 0U; i < len; i++)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
        {
            if (crc & 1U)
                crc = (uint8_t)((crc >> 1U) ^ 0x8CU); /* reflected poly */
            else
                crc >>= 1U;
        }
    }
    return crc;
}

/* Pre-computed CRC-8/MAXIM table */
static uint8_t s_crc8_table[256];
static bool s_crc8_inited = false;

static void crc8_table_init(void)
{
    for (uint32_t b = 0U; b < 256U; b++)
    {
        uint8_t crc = (uint8_t)b;
        for (int bit = 0; bit < 8; bit++)
        {
            crc = (crc & 1U) ? (uint8_t)((crc >> 1U) ^ 0x8CU) : (uint8_t)(crc >> 1U);
        }
        s_crc8_table[b] = crc;
    }
    s_crc8_inited = true;
}

static uint8_t crc8_maxim_table(const uint8_t *data, size_t len)
{
    if (!s_crc8_inited)
        crc8_table_init();
    uint8_t crc = 0x00U;
    for (size_t i = 0U; i < len; i++)
    {
        crc = s_crc8_table[crc ^ data[i]];
    }
    return crc;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CRC-16/CCITT (poly=0x1021, init=0xFFFF, refin=false, refout=false)
 * Used by: XMODEM, SD card response, NASA CCSDS
 * Reference result for "123456789" → 0x29B1
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint16_t crc16_ccitt_bitwise(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFU;
    for (size_t i = 0U; i < len; i++)
    {
        crc ^= (uint16_t)((uint16_t)data[i] << 8U);
        for (int bit = 0; bit < 8; bit++)
        {
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1U) ^ 0x1021U)
                                  : (uint16_t)(crc << 1U);
        }
    }
    return crc;
}

static uint16_t s_crc16_table[256];
static bool s_crc16_inited = false;

static void crc16_table_init(void)
{
    for (uint32_t b = 0U; b < 256U; b++)
    {
        uint16_t crc = (uint16_t)(b << 8U);
        for (int bit = 0; bit < 8; bit++)
        {
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1U) ^ 0x1021U)
                                  : (uint16_t)(crc << 1U);
        }
        s_crc16_table[b] = crc;
    }
    s_crc16_inited = true;
}

static uint16_t crc16_ccitt_table(const uint8_t *data, size_t len)
{
    if (!s_crc16_inited)
        crc16_table_init();
    uint16_t crc = 0xFFFFU;
    for (size_t i = 0U; i < len; i++)
    {
        uint8_t idx = (uint8_t)((crc >> 8U) ^ data[i]);
        crc = (uint16_t)((crc << 8U) ^ s_crc16_table[idx]);
    }
    return crc;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CRC-32/ISO-HDLC (poly=0x04C11DB7, init=0xFFFFFFFF, refin=true,
 *                  refout=true, xorout=0xFFFFFFFF)
 * Used by: Ethernet, ZIP, PNG, STM32 Flash option bytes
 * Reference result for "123456789" → 0xCBF43926
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint32_t s_crc32_table[256];
static bool s_crc32_inited = false;

static void crc32_table_init(void)
{
    for (uint32_t b = 0U; b < 256U; b++)
    {
        uint32_t crc = b;
        for (int bit = 0; bit < 8; bit++)
        {
            crc = (crc & 1U) ? (crc >> 1U) ^ 0xEDB88320UL : (crc >> 1U);
        }
        s_crc32_table[b] = crc;
    }
    s_crc32_inited = true;
}

static uint32_t crc32_iso(const uint8_t *data, size_t len)
{
    if (!s_crc32_inited)
        crc32_table_init();
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0U; i < len; i++)
    {
        uint8_t idx = (uint8_t)(crc ^ data[i]);
        crc = (crc >> 8U) ^ s_crc32_table[idx];
    }
    return crc ^ 0xFFFFFFFFUL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * STM32 CRC PERIPHERAL SIMULATION
 *
 * The STM32 has a hardware CRC unit that computes CRC-32/MPEG-2
 * (poly=0x04C11DB7, init=0xFFFFFFFF, non-reflected, non-inverted).
 * It processes 32-bit words and is useful for firmware image checks.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct
{
    volatile uint32_t DR;  /* Data register */
    volatile uint32_t IDR; /* Independent data register (8-bit scratch) */
    volatile uint32_t CR;  /* Control register (bit0=reset) */
} CRC_TypeDef;

#define CRC_CR_RESET (1UL << 0)

static CRC_TypeDef sim_crc = {0};
static uint32_t crc_hw_state = 0xFFFFFFFFUL;

/* Feed one 32-bit word to the simulated CRC hardware */
static void hw_crc_write(uint32_t word)
{
    for (int bit = 31; bit >= 0; bit--)
    {
        uint32_t top = crc_hw_state & 0x80000000UL;
        crc_hw_state <<= 1U;
        if (top ^ (((uint32_t)word >> bit) & 1UL) << 31U)
            crc_hw_state ^= 0x04C11DB7UL;
    }
    sim_crc.DR = crc_hw_state;
}

static uint32_t hw_crc_compute(const uint32_t *words, size_t word_count)
{
    sim_crc.CR = CRC_CR_RESET;
    crc_hw_state = 0xFFFFFFFFUL;
    for (size_t i = 0U; i < word_count; i++)
    {
        hw_crc_write(words[i]);
    }
    return sim_crc.DR;
}

/* ─── Firmware CRC verification pattern ─────────────────────────────────── */
static uint32_t compute_firmware_crc(void)
{
    /* Simulated firmware image in Flash (4-byte aligned words) */
    static const uint32_t fake_flash[] = {
        0xDEADBEEFUL, 0xCAFEBABEUL, 0x12345678UL, 0xAABBCCDDUL};
    return hw_crc_compute(fake_flash, sizeof(fake_flash) / sizeof(fake_flash[0]));
}

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 5 — Lesson 4: CRC Checksums          ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    printf("Test vector: \"123456789\" (%zu bytes)\n\n", DATA_LEN);

    /* CRC-8/MAXIM */
    uint8_t c8_bw = crc8_maxim_bitwise(k_data, DATA_LEN);
    uint8_t c8_tb = crc8_maxim_table(k_data, DATA_LEN);
    printf("CRC-8/MAXIM:    bitwise=0x%02X  table=0x%02X  ref=0xA1  %s\n",
           c8_bw, c8_tb, (c8_bw == 0xA1U) ? "PASS" : "FAIL");

    /* CRC-16/CCITT */
    uint16_t c16_bw = crc16_ccitt_bitwise(k_data, DATA_LEN);
    uint16_t c16_tb = crc16_ccitt_table(k_data, DATA_LEN);
    printf("CRC-16/CCITT:   bitwise=0x%04X  table=0x%04X  ref=0x29B1  %s\n",
           c16_bw, c16_tb, (c16_bw == 0x29B1U) ? "PASS" : "FAIL");

    /* CRC-32 */
    uint32_t c32 = crc32_iso(k_data, DATA_LEN);
    printf("CRC-32/ISO:     0x%08X  ref=0xCBF43926  %s\n",
           c32, (c32 == 0xCBF43926UL) ? "PASS" : "FAIL");

    /* STM32 HW CRC peripheral */
    uint32_t fw_crc = compute_firmware_crc();
    printf("\nSTM32 HW CRC of fake Flash image: 0x%08X\n", fw_crc);
    printf("(Store this value in firmware descriptor for boot verification)\n");

    /* CRC as a hash: demonstrate incremental update */
    printf("\n── Incremental CRC-16 update ──\n");
    const uint8_t part1[] = {'1', '2', '3', '4'};
    const uint8_t part2[] = {'5', '6', '7', '8', '9'};
    /* CRC-16 is composable: crc(A+B) = crc_update(crc_update(0, A), B) */
    if (!s_crc16_inited)
        crc16_table_init();
    uint16_t accum = 0xFFFFU;
    /* Process part1 */
    for (size_t i = 0U; i < sizeof(part1); i++)
    {
        uint8_t idx = (uint8_t)((accum >> 8U) ^ part1[i]);
        accum = (uint16_t)((accum << 8U) ^ s_crc16_table[idx]);
    }
    /* Process part2 (continue from previous state) */
    for (size_t i = 0U; i < sizeof(part2); i++)
    {
        uint8_t idx = (uint8_t)((accum >> 8U) ^ part2[i]);
        accum = (uint16_t)((accum << 8U) ^ s_crc16_table[idx]);
    }
    printf("  Incremental result=0x%04X  full=0x%04X  match=%s\n",
           accum, c16_tb, (accum == c16_tb) ? "YES" : "NO");

    printf("\nEXERCISES:\n");
    printf("  1. Compute CRC-8 of a DS18B20 ROM code (8 bytes, last byte=CRC)\n");
    printf("     and verify it matches. All valid ROM codes satisfy CRC=0.\n");
    printf("  2. Write crc32_of_file() that reads a binary file in 1KB chunks\n");
    printf("     and computes CRC-32 incrementally.\n");
    printf("  3. Implement a CRC-16/MODBUS (poly=0x8005, reflected) used in\n");
    printf("     industrial RTU frames.\n");
    return 0;
}
