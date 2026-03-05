/**
 * @file embedded_types.h
 * @brief Portable fixed-width types and common macros for all exercises.
 *
 * On a host PC, <stdint.h> provides these. On STM32, the HAL or CMSIS provides
 * them. Using these types everywhere keeps your code portable.
 *
 * RULE: Never use plain `int` for hardware registers or protocol fields.
 *       Always use uint8_t, uint16_t, uint32_t etc.
 */

#ifndef EMBEDDED_TYPES_H
#define EMBEDDED_TYPES_H

#include <stdint.h>  /* uint8_t, uint16_t, uint32_t, int32_t ... */
#include <stddef.h>  /* size_t, NULL                             */
#include <stdbool.h> /* bool, true, false  (C99+)                */
#include <stdio.h>   /* printf — for PC simulation only          */
#include <string.h>  /* memcpy, memset                           */
#include <assert.h>  /* assert() — disabled in NDEBUG builds     */

/* ── Bit manipulation macros ──────────────────────────────────────────────── */
/* These are the bread and butter of embedded C. Know them cold.               */

/** Set bit n in register reg */
#define BIT_SET(reg, n) ((reg) |= (1UL << (n)))

/** Clear bit n in register reg */
#define BIT_CLEAR(reg, n) ((reg) &= ~(1UL << (n)))

/** Toggle bit n in register reg */
#define BIT_TOGGLE(reg, n) ((reg) ^= (1UL << (n)))

/** Read bit n from register reg (returns 0 or 1) */
#define BIT_READ(reg, n) (((reg) >> (n)) & 1UL)

/** Check if bit n is set (returns true/false) */
#define BIT_IS_SET(reg, n) (((reg) & (1UL << (n))) != 0U)

/** Write value v into a bitfield: bits [pos+width-1 : pos] */
#define FIELD_WRITE(reg, pos, width, v) \
    ((reg) = ((reg) & ~(((1UL << (width)) - 1UL) << (pos))) | (((uint32_t)(v) & ((1UL << (width)) - 1UL)) << (pos)))

/** Read a bitfield: bits [pos+width-1 : pos] */
#define FIELD_READ(reg, pos, width) \
    (((reg) >> (pos)) & ((1UL << (width)) - 1UL))

/* ── Register access macros (simulates CMSIS style) ─────────────────────── */
/** Cast an address to a memory-mapped 32-bit register pointer */
#define REG32(addr) (*(volatile uint32_t *)(addr))
#define REG16(addr) (*(volatile uint16_t *)(addr))
#define REG8(addr) (*(volatile uint8_t *)(addr))

/* ── Array utilities ─────────────────────────────────────────────────────── */
/** Number of elements in a stack-allocated array (do NOT use on pointers!) */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* ── Compiler hints ──────────────────────────────────────────────────────── */
#define UNUSED(x) ((void)(x))
#define INLINE __attribute__((always_inline)) inline
#define NOINLINE __attribute__((noinline))
#define PACKED __attribute__((packed))
#define ALIGNED(n) __attribute__((aligned(n)))
#define WEAK __attribute__((weak))
#define SECTION(s) __attribute__((section(s)))

/* ── Return status codes ─────────────────────────────────────────────────── */
typedef enum
{
    STATUS_OK = 0,
    STATUS_ERROR = 1,
    STATUS_TIMEOUT = 2,
    STATUS_BUSY = 3,
    STATUS_INVALID = 4,
} status_t;

#endif /* EMBEDDED_TYPES_H */
