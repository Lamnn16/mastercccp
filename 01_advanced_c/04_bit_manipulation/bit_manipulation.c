/**
 * @file bit_manipulation.c
 * @brief Phase 1 — Bit Manipulation: The Language of Hardware
 *
 * In embedded C, every interaction with a peripheral is done through bit
 * manipulation of registers. This lesson builds the complete toolkit:
 *
 *   1. Set / Clear / Toggle / Read a single bit
 *   2. Read/Write multi-bit fields (MODER, AFR, CR1, etc.)
 *   3. Bitwise arithmetic tricks (detect power-of-2, count bits, reverse bits)
 *   4. Bit-banding (Cortex-M3/M4 feature for atomic bit access)
 *   5. CRC computation via XOR — used in all embedded protocols
 *
 * BUILD:  cmake --build build --target p1_bitops
 * RUN:    .\build\01_advanced_c\p1_bitops.exe
 */

#include "embedded_types.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. SINGLE-BIT OPERATIONS — Using macros from embedded_types.h
 * ═══════════════════════════════════════════════════════════════════════════ */
static void demo_single_bit(void)
{
    printf("── 1. Single-bit ops ──\n");

    uint32_t reg = 0x00000000U;

    BIT_SET(reg, 5);
    printf("  After SET   bit5: 0x%08X  (0b%08b should be ..100000)\n", reg, reg);

    BIT_SET(reg, 12);
    printf("  After SET  bit12: 0x%08X\n", reg);

    BIT_CLEAR(reg, 5);
    printf("  After CLR   bit5: 0x%08X\n", reg);

    BIT_TOGGLE(reg, 12);
    printf("  After TGL  bit12: 0x%08X  (should be 0)\n", reg);

    reg = 0xDEADBEEFU;
    printf("  BIT_READ(0xDEADBEEF, 0)  = %u\n", BIT_READ(reg, 0)); /* LSB */
    printf("  BIT_READ(0xDEADBEEF, 1)  = %u\n", BIT_READ(reg, 1));
    printf("  BIT_READ(0xDEADBEEF, 31) = %u\n", BIT_READ(reg, 31)); /* MSB */
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. MULTI-BIT FIELD MANIPULATION
 *
 * STM32 GPIO MODER register: 2 bits per pin
 *   00 = Input, 01 = Output, 10 = Alternate Function, 11 = Analog
 * AFR (Alternate Function Register): 4 bits per pin
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum
{
    GPIO_MODE_INPUT = 0U,
    GPIO_MODE_OUTPUT,
    GPIO_MODE_AF,
    GPIO_MODE_ANALOG,
} gpio_mode_t;

static void gpio_set_mode(volatile uint32_t *moder, uint8_t pin, gpio_mode_t mode)
{
    /* MODER has 2 bits per pin. pin 5 → bits [11:10] */
    FIELD_WRITE(*moder, (uint32_t)(pin * 2U), 2U, (uint32_t)mode);
}

static gpio_mode_t gpio_get_mode(uint32_t moder, uint8_t pin)
{
    return (gpio_mode_t)FIELD_READ(moder, (uint32_t)(pin * 2U), 2U);
}

static void gpio_set_af(volatile uint32_t afr[2], uint8_t pin, uint8_t af_num)
{
    /*
     * AFR[0] covers pins 0-7 (4 bits each, total 32 bits)
     * AFR[1] covers pins 8-15
     */
    uint8_t reg_idx = pin / 8U;
    uint8_t bit_offset = (pin % 8U) * 4U;
    FIELD_WRITE(afr[reg_idx], bit_offset, 4U, af_num);
}

static void demo_register_fields(void)
{
    printf("── 2. Multi-bit register field manipulation ──\n");

    volatile uint32_t moder = 0U;
    volatile uint32_t afr[2] = {0U, 0U};

    gpio_set_mode(&moder, 2, GPIO_MODE_AF);     /* PC2 → Alt Func */
    gpio_set_mode(&moder, 5, GPIO_MODE_OUTPUT); /* PC5 → Output (LED) */
    gpio_set_mode(&moder, 7, GPIO_MODE_INPUT);  /* PC7 → Input (button) */
    gpio_set_af(afr, 2, 11U);                   /* PC2 AF11 = ETH_MII_TXD0 */

    printf("  MODER  = 0x%08X\n", (uint32_t)moder);
    printf("  AFR[0] = 0x%08X\n", (uint32_t)afr[0]);
    printf("  pin2 mode = %u (2=AF)\n", (unsigned)gpio_get_mode((uint32_t)moder, 2));
    printf("  pin5 mode = %u (1=OUT)\n", (unsigned)gpio_get_mode((uint32_t)moder, 5));
    printf("  pin7 mode = %u (0=IN)\n\n", (unsigned)gpio_get_mode((uint32_t)moder, 7));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. BIT TRICKS — Useful patterns in embedded math and assertions
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Returns true if n is a power of 2 (used for buffer size checks) */
static bool is_power_of_2(uint32_t n)
{
    /* Classic trick: powers of 2 have exactly 1 bit set
     * n & (n-1) clears the lowest set bit — result is 0 for powers of 2 */
    return (n > 0U) && ((n & (n - 1U)) == 0U);
}

/** Round up to next power of 2 (useful for alignment) */
static uint32_t next_power_of_2(uint32_t n)
{
    if (n == 0U)
        return 1U;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n + 1U;
}

/** Count number of set bits (Hamming weight / popcount) */
static uint8_t popcount(uint32_t n)
{
    /* Brian Kernighan's algorithm */
    uint8_t count = 0;
    while (n)
    {
        n &= (n - 1U); /* clear lowest set bit */
        count++;
    }
    return count;
}

/** Reverse all 32 bits */
static uint32_t reverse_bits(uint32_t n)
{
    n = ((n >> 1) & 0x55555555U) | ((n & 0x55555555U) << 1);
    n = ((n >> 2) & 0x33333333U) | ((n & 0x33333333U) << 2);
    n = ((n >> 4) & 0x0F0F0F0FU) | ((n & 0x0F0F0F0FU) << 4);
    n = ((n >> 8) & 0x00FF00FFU) | ((n & 0x00FF00FFU) << 8);
    n = (n >> 16) | (n << 16);
    return n;
}

static void demo_bit_tricks(void)
{
    printf("── 3. Bit tricks ──\n");
    printf("  is_power_of_2(64)  = %s\n", is_power_of_2(64) ? "true" : "false");
    printf("  is_power_of_2(100) = %s\n", is_power_of_2(100) ? "true" : "false");
    printf("  next_pow2(100) = %u\n", next_power_of_2(100));
    printf("  next_pow2(128) = %u\n", next_power_of_2(128));
    printf("  popcount(0xFF00FF00) = %u\n", popcount(0xFF00FF00U));
    printf("  reverse(0x12345678) = 0x%08X\n", reverse_bits(0x12345678U));
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. CRC-8 via XOR TABLE
 *
 * CRC is used in UART framing (Modbus RTU), SPI sensors, I2C EEPROMs.
 * This implementation uses a lookup table for speed — common on MCUs.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* CRC-8/MAXIM (Dallas/Maxim 1-Wire) — polynomial x^8+x^5+x^4+1 = 0x31 */
static const uint8_t CRC8_TABLE[256] = {
    0x00,
    0x5e,
    0xbc,
    0xe2,
    0x61,
    0x3f,
    0xdd,
    0x83,
    0xc2,
    0x9c,
    0x7e,
    0x20,
    0xa3,
    0xfd,
    0x1f,
    0x41,
    0x9d,
    0xc3,
    0x21,
    0x7f,
    0xfc,
    0xa2,
    0x40,
    0x1e,
    0x5f,
    0x01,
    0xe3,
    0xbd,
    0x3e,
    0x60,
    0x82,
    0xdc,
    0x23,
    0x7d,
    0x9f,
    0xc1,
    0x42,
    0x1c,
    0xfe,
    0xa0,
    0xe1,
    0xbf,
    0x5d,
    0x03,
    0x80,
    0xde,
    0x3c,
    0x62,
    0xbe,
    0xe0,
    0x02,
    0x5c,
    0xdf,
    0x81,
    0x63,
    0x3d,
    0x7c,
    0x22,
    0xc0,
    0x9e,
    0x1d,
    0x43,
    0xa1,
    0xff,
    0x46,
    0x18,
    0xfa,
    0xa4,
    0x27,
    0x79,
    0x9b,
    0xc5,
    0x84,
    0xda,
    0x38,
    0x66,
    0xe5,
    0xbb,
    0x59,
    0x07,
    0xdb,
    0x85,
    0x67,
    0x39,
    0xba,
    0xe4,
    0x06,
    0x58,
    0x19,
    0x47,
    0xa5,
    0xfb,
    0x78,
    0x26,
    0xc4,
    0x9a,
    0x65,
    0x3b,
    0xd9,
    0x87,
    0x04,
    0x5a,
    0xb8,
    0xe6,
    0xa7,
    0xf9,
    0x1b,
    0x45,
    0xc6,
    0x98,
    0x7a,
    0x24,
    0xf8,
    0xa6,
    0x44,
    0x1a,
    0x99,
    0xc7,
    0x25,
    0x7b,
    0x3a,
    0x64,
    0x86,
    0xd8,
    0x5b,
    0x05,
    0xe7,
    0xb9,
    0x8c,
    0xd2,
    0x30,
    0x6e,
    0xed,
    0xb3,
    0x51,
    0x0f,
    0x4e,
    0x10,
    0xf2,
    0xac,
    0x2f,
    0x71,
    0x93,
    0xcd,
    0x11,
    0x4f,
    0xad,
    0xf3,
    0x70,
    0x2e,
    0xcc,
    0x92,
    0xd3,
    0x8d,
    0x6f,
    0x31,
    0xb2,
    0xec,
    0x0e,
    0x50,
    0xaf,
    0xf1,
    0x13,
    0x4d,
    0xce,
    0x90,
    0x72,
    0x2c,
    0x6d,
    0x33,
    0xd1,
    0x8f,
    0x0c,
    0x52,
    0xb0,
    0xee,
    0x32,
    0x6c,
    0x8e,
    0xd0,
    0x53,
    0x0d,
    0xef,
    0xb1,
    0xf0,
    0xae,
    0x4c,
    0x12,
    0x91,
    0xcf,
    0x2d,
    0x73,
    0xca,
    0x94,
    0x76,
    0x28,
    0xab,
    0xf5,
    0x17,
    0x49,
    0x08,
    0x56,
    0xb4,
    0xea,
    0x69,
    0x37,
    0xd5,
    0x8b,
    0x57,
    0x09,
    0xeb,
    0xb5,
    0x36,
    0x68,
    0x8a,
    0xd4,
    0x95,
    0xcb,
    0x29,
    0x77,
    0xf4,
    0xaa,
    0x48,
    0x16,
    0xe9,
    0xb7,
    0x55,
    0x0b,
    0x88,
    0xd6,
    0x34,
    0x6a,
    0x2b,
    0x75,
    0x97,
    0xc9,
    0x4a,
    0x14,
    0xf6,
    0xa8,
    0x74,
    0x2a,
    0xc8,
    0x96,
    0x15,
    0x4b,
    0xa9,
    0xf7,
    0xb6,
    0xe8,
    0x0a,
    0x54,
    0xd7,
    0x89,
    0x6b,
    0x35,
};

static uint8_t crc8_compute(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00U;
    for (size_t i = 0; i < len; i++)
    {
        crc = CRC8_TABLE[crc ^ data[i]];
    }
    return crc;
}

static void demo_crc8(void)
{
    printf("── 4. CRC-8 (MAXIM/Dallas 1-Wire) ──\n");

    uint8_t data[] = {0x01, 0x10, 0x00, 0x00, 0x01, 0x40};
    uint8_t crc = crc8_compute(data, sizeof(data));
    printf("  data: ");
    for (size_t i = 0; i < sizeof(data); i++)
        printf("%02X ", data[i]);
    printf("\n  CRC-8: 0x%02X\n", crc);
    printf("  (used in DS18B20 temperature sensor protocol)\n\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * EXERCISES
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * EXERCISE 1:
 *   Write gpio_toggle_pin(volatile uint32_t *odr, uint8_t pin) that toggles
 *   a GPIO output pin without a read-modify-write cycle.
 *   Hint: STM32 BSRR register — upper 16 bits reset, lower 16 bits set.
 *
 * EXERCISE 2:
 *   Implement a function that extracts a variable-width bitfield from a
 *   32-bit value given a start bit and width:
 *     uint32_t extract_field(uint32_t val, uint8_t start, uint8_t width);
 *   Test with MODER nibbles.
 *
 * EXERCISE 3:
 *   Implement CRC-16/CCITT (polynomial 0x1021, init 0xFFFF) without a table.
 *   This is used in UART Modbus RTU frames.
 */

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 1 — Lesson 4: Bit Manipulation       ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    demo_single_bit();
    demo_register_fields();
    demo_bit_tricks();
    demo_crc8();

    return 0;
}
