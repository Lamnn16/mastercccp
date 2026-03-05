/**
 * @file preprocessor.c
 * @brief Phase 1 — Preprocessor: Macros, Conditional Compilation, X-Macros
 *
 * The C preprocessor is heavily used in embedded firmware for:
 *   1. Hardware abstraction (pin definitions, register addresses)
 *   2. Compile-time configuration (board selection, debug switches)
 *   3. X-macros — generate repetitive code from a single source of truth
 *   4. Static assertions — catch errors at compile time, not runtime
 *   5. Stringification and token pasting
 *
 * BUILD:  cmake --build build --target p1_preprocessor
 * RUN:    .\build\01_advanced_c\p1_preprocessor.exe
 */

#include "embedded_types.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. HARDWARE PIN DEFINITIONS
 *
 * Always use macros (or enums) for pin/port definitions, never magic numbers.
 * This is the only portable way to swap boards by changing one header file.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Imagine this comes from a board-specific header: board_nucleo_f411.h */
#define LED_PORT GPIOA
#define LED_PIN 5U /* PA5 = LD2 on Nucleo-F411RE */
#define BUTTON_PORT GPIOC
#define BUTTON_PIN 13U /* PC13 = B1 (blue button) */
#define UART_TX_PORT GPIOA
#define UART_TX_PIN 2U /* PA2 = USART2_TX */

/* Derived macros from pin numbers */
#define LED_PIN_MASK (1UL << LED_PIN)
#define BUTTON_PIN_MASK (1UL << BUTTON_PIN)

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. COMPILE-TIME CONFIGURATION
 *
 * Use #define + #ifdef to create build variants without runtime overhead.
 * In CMake, pass -DDEBUG_LEVEL=2 to set this externally.
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 1 /* default: info only */
#endif

/* Logging macros that compile to NOTHING in release builds */
#if DEBUG_LEVEL >= 2
#define LOG_DBG(fmt, ...) printf("[DBG] " fmt "\n", ##__VA_ARGS__)
#else
#define LOG_DBG(fmt, ...) /* stripped from release */
#endif

#if DEBUG_LEVEL >= 1
#define LOG_INF(fmt, ...) printf("[INF] " fmt "\n", ##__VA_ARGS__)
#else
#define LOG_INF(fmt, ...)
#endif

#define LOG_ERR(fmt, ...) printf("[ERR] " fmt " (%s:%d)\n", ##__VA_ARGS__, __FILE__, __LINE__)

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. X-MACROS — single source of truth, no sync bugs
 *
 * Problem: you have an enum of events AND a string table for debugging.
 *          They must always be in sync. X-macros enforce this.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Define all events once — this is the ONLY place to add/remove events */
#define FOREACH_EVENT(X)                  \
    X(EVENT_IDLE, "idle")                 \
    X(EVENT_BUTTON_PRESS, "button_press") \
    X(EVENT_UART_RX, "uart_rx")           \
    X(EVENT_TIMER_TICK, "timer_tick")     \
    X(EVENT_ERROR, "error")

/* Generate the enum */
typedef enum
{
#define X_ENUM(name, str) name,
    FOREACH_EVENT(X_ENUM)
#undef X_ENUM
        EVENT_COUNT
} event_t;

/* Generate the string table */
static const char *const EVENT_NAMES[EVENT_COUNT] = {
#define X_STR(name, str) [name] = str,
    FOREACH_EVENT(X_STR)
#undef X_STR
};

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. STATIC ASSERTIONS — catch bugs at compile time
 *
 * Much better than runtime checks for things you know at compile time.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Verify struct sizes match protocol spec */
typedef struct
{
    uint8_t cmd;
    uint8_t len;
    uint8_t data[8];
    uint16_t crc;
} __attribute__((packed)) protocol_frame_t;

_Static_assert(sizeof(protocol_frame_t) == 12,
               "protocol_frame_t size mismatch — protocol violation!");

/* Verify enum didn't grow beyond a uint8_t (for compact storage) */
_Static_assert(EVENT_COUNT <= 255,
               "Too many events — event_t won't fit in uint8_t!");

/* ═══════════════════════════════════════════════════════════════════════════
 * 5. STRINGIFY AND TOKEN PASTING
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Token pasting: creates GPIOA, GPIOB, etc. at compile time */
#define GPIO_PORT(x) GPIO##x

/* Stringification */
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

/* Usage: log the build configuration */
#define FIRMWARE_VERSION_MAJOR 1
#define FIRMWARE_VERSION_MINOR 2
#define FIRMWARE_VERSION_PATCH 3
#define FW_VER_STRING                \
    TOSTRING(FIRMWARE_VERSION_MAJOR) \
    "." TOSTRING(FIRMWARE_VERSION_MINOR) "." TOSTRING(FIRMWARE_VERSION_PATCH)

static void demo_preprocessor(void)
{
    printf("── Compile-time configuration ──\n");
    printf("  DEBUG_LEVEL = %d\n", DEBUG_LEVEL);
    printf("  FW version  = " FW_VER_STRING "\n");

    LOG_INF("System initialized");
    LOG_DBG("Debug detail: LED_PIN=%u BUTTON_PIN=%u", LED_PIN, BUTTON_PIN);
    LOG_ERR("Simulated error code %d", -1);
    printf("\n");

    printf("── X-Macro event table ──\n");
    for (int e = 0; e < (int)EVENT_COUNT; e++)
    {
        printf("  EVENT[%d] = \"%s\"\n", e, EVENT_NAMES[e]);
    }
    printf("\n");

    printf("── Static assertion passed ──\n");
    printf("  sizeof(protocol_frame_t) = %zu (must be 12)\n", sizeof(protocol_frame_t));
    printf("  EVENT_COUNT = %u (must be < 256)\n\n", (unsigned)EVENT_COUNT);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * EXERCISES
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * EXERCISE 1:
 *   Add a new event EVENT_SPI_RX to the X-macro table. Observe that both the
 *   enum and the string table update automatically. Add nothing else.
 *
 * EXERCISE 2:
 *   Write a ASSERT_FIELD_AT(struct_type, field, expected_offset) macro that
 *   uses _Static_assert and offsetof to verify a struct field is at the
 *   expected offset. Test it on protocol_frame_t.
 *
 * EXERCISE 3:
 *   Write a MIN(a,b) and MAX(a,b) macro that is safe against double evaluation
 *   (i.e., MIN(x++, y++) should not increment x or y twice).
 *   Compare to the inline function approach — which is better for embedded?
 */

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 1 — Lesson 6: Preprocessor & Macros  ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    demo_preprocessor();
    return 0;
}
