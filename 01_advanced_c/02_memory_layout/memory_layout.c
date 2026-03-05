/**
 * @file memory_layout.c
 * @brief Phase 1 — Memory Layout: Stack, Heap, Data, BSS, Text
 *
 * One of the most critical skills in embedded development is knowing WHERE
 * your data lives. On an MCU with 256KB Flash and 64KB RAM you cannot afford
 * surprises. This lesson makes memory layout concrete and observable.
 *
 * Sections on ARM Cortex-M:
 *   .text   — compiled code + const data         → Flash (ROM)
 *   .rodata — string literals, const arrays       → Flash (ROM)
 *   .data   — initialized global/static variables → RAM (copied from Flash on boot)
 *   .bss    — zero-initialized globals/statics    → RAM (zeroed by startup.s)
 *   stack   — local variables, function frames    → RAM (grows toward lower addresses)
 *   heap    — dynamic allocation (malloc)         → RAM (we AVOID this on bare-metal)
 *
 * BUILD:  cmake --build build --target p1_memory_layout
 * RUN:    .\build\01_advanced_c\p1_memory_layout.exe
 */

#include "embedded_types.h"

/* ── .data section: initialized non-zero globals ───────────────────────── */
static uint32_t g_initialized = 0xDEADBEEFU; /* lives in .data */
static uint8_t g_version[4] = {1, 0, 0, 5};

/* ── .bss section: zero-initialized globals ─────────────────────────────── */
static uint32_t g_error_count;  /* implicit zero → .bss */
static uint8_t g_rx_buffer[64]; /* common pattern: RX buffer in .bss */

/* ── .rodata section: read-only constants ────────────────────────────────── */
/*
 * On MCU: stored in Flash, never copied to RAM.
 * On PC:  in read-only memory segment. Writing to it → segfault.
 */
static const char DEVICE_NAME[] = "STM32_MASTERCC";
static const uint8_t LOOKUP_TABLE[16] = {
    0x00, 0x0F, 0x1E, 0x11, 0x3C, 0x33, 0x22, 0x2D,
    0x78, 0x77, 0x66, 0x69, 0x44, 0x4B, 0x5A, 0x55};

/* ── Stack-allocated examples ─────────────────────────────────────────────── */
static void demo_stack(void)
{
    /*
     * STACK RULES for embedded:
     *   1. Never declare large arrays as local variables — stack is tiny (1-8 KB)
     *   2. Never return a pointer to a local variable — it's gone after return
     *   3. Watch out for deep recursion — each call frame consumes stack
     */
    uint8_t small_buf[16]; /* 16 bytes — fine */
    uint32_t temp = 0xABCD1234U;

    /* Initialize (local vars have garbage value until you write them) */
    for (int i = 0; i < 16; i++)
    {
        small_buf[i] = (uint8_t)i;
    }

    printf("── Stack demo ──\n");
    printf("  local uint32_t temp     @ %p = 0x%08X\n", (void *)&temp, temp);
    printf("  local uint8_t  buf[0]   @ %p\n", (void *)&small_buf[0]);
    printf("  local uint8_t  buf[15]  @ %p\n", (void *)&small_buf[15]);

    /*
     * Notice the addresses decrease from outer call frames to inner.
     * Stack grows DOWNWARD on ARM Cortex-M (as on x86-64).
     */
    printf("\n");
}

/* ── Dangerous anti-pattern: returning pointer to local ─────────────────── */
/*
 * This function is intentionally WRONG as a teaching example.
 * On a real MCU this causes hard-to-find corruption bugs.
 *
 * uint8_t* WRONG_do_not_use(void) {
 *     uint8_t local[4] = {1,2,3,4};   // lives on stack
 *     return local;                    // pointer is INVALID after return!
 * }
 */

/* ── Correct alternatives ─────────────────────────────────────────────────── */
/* Option A: return data via output parameter (preferred in C) */
static void get_version(uint8_t *out, size_t out_size)
{
    if (out_size >= sizeof(g_version))
    {
        memcpy(out, g_version, sizeof(g_version));
    }
}

/* Option B: return pointer to static buffer (ok if re-entrancy not needed) */
static const uint8_t *get_device_name(void)
{
    return (const uint8_t *)DEVICE_NAME; /* static storage — always valid */
}

static void demo_globals(void)
{
    printf("── Global sections demo ──\n");
    printf("  .data   g_initialized @ %p = 0x%08X\n",
           (void *)&g_initialized, g_initialized);
    printf("  .bss    g_error_count @ %p = %u (zero-init)\n",
           (void *)&g_error_count, (unsigned)g_error_count);
    printf("  .bss    g_rx_buffer   @ %p (64 bytes, zeroed)\n",
           (void *)g_rx_buffer);
    printf("  .rodata DEVICE_NAME   @ %p = \"%s\"\n",
           (void *)DEVICE_NAME, DEVICE_NAME);

    uint8_t ver[4];
    get_version(ver, sizeof(ver));
    printf("  version (via output ptr): %u.%u.%u.%u\n",
           ver[0], ver[1], ver[2], ver[3]);

    const uint8_t *name = get_device_name();
    printf("  device name (via static ptr): %s\n", (const char *)name);
    printf("\n");
}

/* ── struct packing and alignment ─────────────────────────────────────────── */
/*
 * The compiler adds padding to structs to satisfy alignment requirements.
 * On ARM Cortex-M, misaligned 32-bit accesses cause HardFault.
 * Know your struct sizes before putting them in DMA descriptors or packets.
 */
typedef struct
{
    uint8_t a; /* 1 byte */
    /* 3 bytes padding inserted here by compiler */
    uint32_t b; /* 4 bytes — must be 4-byte aligned */
    uint8_t c;  /* 1 byte */
    uint8_t d;  /* 1 byte */
    /* 2 bytes padding */
    uint16_t e; /* 2 bytes — must be 2-byte aligned */
} padded_t;     /* total: 12 bytes (not 9) */

typedef struct __attribute__((packed))
{
    uint8_t a;  /* 1 byte — at offset 0 */
    uint32_t b; /* 4 bytes — at offset 1 (MISALIGNED on ARM!) */
    uint8_t c;  /* 1 byte */
    uint8_t d;  /* 1 byte */
    uint16_t e; /* 2 bytes */
} packed_t;     /* total: 9 bytes, but b is misaligned → HardFault on M0! */
                /* Cortex-M3/M4 can handle misaligned but it's SLOWER */

static void demo_struct_layout(void)
{
    printf("── Struct alignment ──\n");
    printf("  sizeof(padded_t) = %zu (compiler added padding)\n", sizeof(padded_t));
    printf("  sizeof(packed_t) = %zu (no padding — misaligned on M0!)\n", sizeof(packed_t));

    padded_t p = {.a = 1, .b = 0x12345678U, .c = 3, .d = 4, .e = 0xABCDU};
    printf("  padded_t.b @ offset %zu\n", offsetof(padded_t, b));
    printf("  padded_t.e @ offset %zu\n", offsetof(padded_t, e));
    printf("  p.b = 0x%08X\n", p.b);
    printf("\n");

    /*
     * RULE: Use packed structs only for protocol frames that you memcpy into.
     * Never dereference a pointer to a packed struct member directly.
     * Instead, use memcpy to extract individual fields.
     */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * EXERCISES
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * EXERCISE 1:
 *   Define a struct that represents a UART frame:
 *     [ start_byte(1) | length(1) | payload(64) | crc16(2) ]
 *   Print sizeof() the struct. Is there padding? How would you eliminate it
 *   without using __attribute__((packed))?  (Hint: reorder fields.)
 *
 * EXERCISE 2:
 *   Write a function that measures stack depth by taking the address of a
 *   local variable at the start of main() and comparing it to the local
 *   variable address inside a deeply nested call.
 *
 * EXERCISE 3:
 *   Explain why this is dangerous in embedded firmware:
 *     static uint8_t *ptr;
 *     void init(void) { uint8_t buf[32]; ptr = buf; }
 *     void use(void)  { ptr[0] = 0xFF; }  // called after init returns
 */

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 1 — Lesson 2: Memory Layout          ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    demo_globals();
    demo_stack();
    demo_struct_layout();
    UNUSED(LOOKUP_TABLE); /* suppress unused warning for the lesson */
    UNUSED(get_device_name);

    return 0;
}
