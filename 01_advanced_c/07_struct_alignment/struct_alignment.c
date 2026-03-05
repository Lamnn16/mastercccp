/**
 * @file struct_alignment.c
 * @brief Phase 1 — Struct Alignment, Padding, and DMA-Safe Layouts
 *
 * This lesson covers everything you need to know about struct memory layout
 * for embedded firmware:
 *
 *   1. Natural alignment rules (why padding happens)
 *   2. How to eliminate padding by reordering fields
 *   3. __attribute__((packed)) — when to use and when NOT to
 *   4. Cache-line and DMA alignment requirements
 *   5. Bit-fields — compact but platform-dependent
 *
 * BUILD:  cmake --build build --target p1_struct_alignment
 * RUN:    .\build\01_advanced_c\p1_struct_alignment.exe
 */

#include "embedded_types.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. NATURAL ALIGNMENT RULES
 *
 * ARM Cortex-M alignment rules:
 *   uint8_t  → align to 1-byte boundary (any address)
 *   uint16_t → align to 2-byte boundary (even address)
 *   uint32_t → align to 4-byte boundary (address divisible by 4)
 *   uint64_t → align to 4-byte boundary (ARM) or 8-byte (desktop x86-64)
 *
 * The compiler adds PADDING bytes before fields to satisfy their alignment.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct
{
    uint8_t a;  /* offset 0, size 1 */
                /* 1 byte padding → offset 1 */
    uint16_t b; /* offset 2, size 2 (aligned to 2) */
    uint8_t c;  /* offset 4, size 1 */
                /* 3 bytes padding → offsets 5,6,7 */
    uint32_t d; /* offset 8, size 4 (aligned to 4) */
    uint8_t e;  /* offset 12, size 1 */
                /* 3 bytes tail padding → total = 16 */
} bad_layout_t; /* wastes 7 bytes of padding in a 16-byte struct! */

typedef struct
{
    uint32_t d;    /* offset 0, size 4 — biggest first */
    uint16_t b;    /* offset 4, size 2 */
    uint8_t a;     /* offset 6, size 1 */
    uint8_t c;     /* offset 7, size 1 */
    uint8_t e;     /* offset 8, size 1 */
                   /* 3 bytes tail padding → total = 12 */
} better_layout_t; /* saves 4 bytes just by reordering */

typedef struct
{
    uint32_t d;      /* offset 0 */
    uint16_t b;      /* offset 4 */
    uint8_t a;       /* offset 6 */
    uint8_t c;       /* offset 7 */
    uint8_t e;       /* offset 8 */
    uint8_t _pad[3]; /* explicit padding → total = 12, documented */
} explicit_pad_t;

static void demo_padding(void)
{
    printf("── 1. Struct padding ──\n");
    printf("  bad_layout_t    : %2zu bytes (%zu wasted in padding)\n",
           sizeof(bad_layout_t),
           sizeof(bad_layout_t) - (1 + 2 + 1 + 4 + 1));
    printf("  better_layout_t : %2zu bytes (%zu wasted)\n",
           sizeof(better_layout_t),
           sizeof(better_layout_t) - (4 + 2 + 1 + 1 + 1));

    printf("  bad_layout offsets: a=%zu b=%zu c=%zu d=%zu e=%zu\n",
           offsetof(bad_layout_t, a),
           offsetof(bad_layout_t, b),
           offsetof(bad_layout_t, c),
           offsetof(bad_layout_t, d),
           offsetof(bad_layout_t, e));
    printf("  better  offsets: d=%zu b=%zu a=%zu c=%zu e=%zu\n\n",
           offsetof(better_layout_t, d),
           offsetof(better_layout_t, b),
           offsetof(better_layout_t, a),
           offsetof(better_layout_t, c),
           offsetof(better_layout_t, e));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. BIT-FIELDS — compact register description
 *
 * Bit-fields are platform-dependent but useful for clearly documenting
 * register layouts. NEVER use bit-fields for protocol frames — use masks.
 *
 * On STM32 CMSIS headers, registers are often documented WITH bit-field
 * structs alongside their #define masks.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* UART Status Register (simulated, based on STM32 USART_SR) */
typedef union
{
    uint32_t raw;
    struct
    {
        uint32_t PE : 1;    /* bit 0: Parity Error */
        uint32_t FE : 1;    /* bit 1: Framing Error */
        uint32_t NF : 1;    /* bit 2: Noise Flag */
        uint32_t ORE : 1;   /* bit 3: Overrun Error */
        uint32_t IDLE : 1;  /* bit 4: IDLE line detected */
        uint32_t RXNE : 1;  /* bit 5: RX Not Empty (data ready) */
        uint32_t TC : 1;    /* bit 6: Transmission Complete */
        uint32_t TXE : 1;   /* bit 7: TX Empty (ready to write) */
        uint32_t LBD : 1;   /* bit 8: LIN Break Detection */
        uint32_t CTS : 1;   /* bit 9: CTS flag */
        uint32_t RSVD : 22; /* bits 31:10 reserved */
    } bits;
} usart_sr_t;

static void demo_bitfields(void)
{
    printf("── 2. Bit-fields for register documentation ──\n");

    usart_sr_t sr;
    sr.raw = 0U;
    sr.bits.RXNE = 1; /* simulate incoming data */
    sr.bits.TXE = 1;  /* TX buffer empty */

    printf("  SR.raw  = 0x%08X\n", sr.raw);
    printf("  SR.RXNE = %u (data ready)\n", sr.bits.RXNE);
    printf("  SR.TXE  = %u (tx empty)\n", sr.bits.TXE);

    /* Verify the bits are where we expect them */
    printf("  Bit 5 (RXNE) via mask: %u\n", (sr.raw >> 5) & 1U);
    printf("  Bit 7 (TXE)  via mask: %u\n\n", (sr.raw >> 7) & 1U);

    /*
     * NOTE: Bit-field order (MSB or LSB first) is IMPLEMENTATION-DEFINED in C.
     * On GCC/ARM, bit 0 is the LSB of the first field. Always verify with .raw.
     */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. DMA ALIGNMENT
 *
 * On STM32 with DMA, the source and destination buffers must be:
 *   - 4-byte aligned for 32-bit DMA transfers
 *   - May need to be in specific RAM regions (SRAM1, SRAM2, etc.)
 *
 * Use __attribute__((aligned(4))) to force alignment.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Buffer for DMA ADC transfer — MUST be 4-byte aligned */
static uint16_t adc_dma_buffer[16] __attribute__((aligned(4)));

/* Buffer in a specific linker section (e.g., DTCM RAM on STM32H7) */
/* static uint8_t dtcm_buffer[64] __attribute__((section(".dtcmram"))); */

static void demo_dma_alignment(void)
{
    printf("── 3. DMA alignment ──\n");
    printf("  adc_dma_buffer address: %p\n", (void *)adc_dma_buffer);
    printf("  address %% 4 = %lu (must be 0 for 32-bit DMA)\n",
           (unsigned long)(uintptr_t)adc_dma_buffer % 4UL);
    printf("  sizeof buffer = %zu bytes (%zu uint16_t samples)\n\n",
           sizeof(adc_dma_buffer), ARRAY_SIZE(adc_dma_buffer));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * EXERCISES
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * EXERCISE 1:
 *   Create a struct for a BMP280 sensor calibration block (26 bytes):
 *   [T1:u16][T2:s16][T3:s16][P1:u16][P2:s16]...[P9:s16][_:u8][H1:u8]
 *   Use __attribute__((packed)) since this is a raw register readout via I2C.
 *   Verify sizeof == 26.
 *
 * EXERCISE 2:
 *   Write a macro PRINT_STRUCT_INFO(T) that prints the name, size, and
 *   alignment of a struct type T using sizeof and _Alignof.
 *
 * EXERCISE 3:
 *   Create a CAN frame struct matching the CAN 2.0A frame layout:
 *   [ID:11bit][RTR:1bit][DLC:4bit][DATA:8bytes]
 *   Use bit-fields and verify .raw matches expected bit positions.
 */

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 1 — Lesson 7: Struct Alignment       ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    demo_padding();
    demo_bitfields();
    demo_dma_alignment();
    UNUSED(explicit_pad_t);
    return 0;
}
