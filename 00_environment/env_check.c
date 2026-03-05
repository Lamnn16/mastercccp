/**
 * @file env_check.c
 * @brief Phase 0 — Toolchain Verification
 *
 * Run this first. If it compiles and prints correctly, your environment is
 * ready for all PC-native exercises (Phases 1–6, 8).
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Verify that fixed-width types are the correct size ─────────────────── */
_Static_assert(sizeof(uint8_t) == 1, "uint8_t  must be 1 byte");
_Static_assert(sizeof(uint16_t) == 2, "uint16_t must be 2 bytes");
_Static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4 bytes");
_Static_assert(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");

int main(void)
{
    printf("=== Mastercc Environment Check ===\n\n");

    /* ── Integer sizes (will differ between host PC and ARM Cortex-M) ────── */
    printf("Type sizes on this host:\n");
    printf("  char      = %zu byte(s)\n", sizeof(char));
    printf("  short     = %zu byte(s)\n", sizeof(short));
    printf("  int       = %zu byte(s)\n", sizeof(int));
    printf("  long      = %zu byte(s)\n", sizeof(long));
    printf("  long long = %zu byte(s)\n", sizeof(long long));
    printf("  pointer   = %zu byte(s)\n", sizeof(void *));
    printf("\n");

    /*
     * IMPORTANT: On a 32-bit ARM Cortex-M:
     *   sizeof(long) == 4  (same as int)
     *   sizeof(void*)== 4  (not 8 like on x86-64)
     *
     * This is why you ALWAYS use uint32_t instead of "unsigned long" when
     * writing portable embedded code.
     */

    /* ── Endianness check ────────────────────────────────────────────────── */
    /*
     * ARM Cortex-M is little-endian by default.
     * x86-64 (your PC) is also little-endian.
     * So this check should show the same result on both.
     */
    uint32_t val = 0x01020304U;
    uint8_t *bytes = (uint8_t *)&val;
    printf("Endianness check (0x01020304 stored as bytes):\n");
    printf("  byte[0]=0x%02X  byte[1]=0x%02X  byte[2]=0x%02X  byte[3]=0x%02X\n",
           bytes[0], bytes[1], bytes[2], bytes[3]);
    if (bytes[0] == 0x04)
    {
        printf("  -> Little-endian (same as ARM Cortex-M default)\n");
    }
    else
    {
        printf("  -> Big-endian\n");
    }
    printf("\n");

    /* ── Compiler info ───────────────────────────────────────────────────── */
    printf("Compiler info:\n");
#if defined(__GNUC__)
    printf("  GCC %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif
    printf("  __STDC_VERSION__ = %ldL\n", (long)__STDC_VERSION__);
    printf("\n");

    printf("All checks passed. Your environment is ready.\n");
    printf("Next step: open 01_advanced_c/ and start Phase 1.\n");
    return 0;
}
