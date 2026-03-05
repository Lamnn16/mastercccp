/**
 * @file namespaces.cpp
 * @brief Phase 2 — Namespaces: Organizing Embedded Firmware Code
 *
 * Namespaces prevent name collisions in large embedded projects where
 * multiple drivers, libraries, and board-specific files exist.
 *
 * Real scenario: STM32 HAL, FreeRTOS, your drivers, third-party libs —
 * all in the same project. Namespaces mean no more:
 *   gpio_set_mode()  vs  hal_gpio_set_mode()  vs  board_gpio_set_mode()
 *
 * In C:    prefix everything manually (fragile, inconsistent)
 * In C++:  use namespaces (enforced, zero runtime overhead)
 *
 * BUILD:  cmake --build build --target p2_namespaces
 * RUN:    .\build\02_cpp_fundamentals\p2_namespaces.exe
 */

#include "embedded_types.h"
#include <cstdio>

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. BASIC NAMESPACE — group related hardware abstractions
 * ═══════════════════════════════════════════════════════════════════════════ */
namespace hal {

    namespace gpio {
        enum class Mode : uint8_t { Input=0, Output=1, AF=2, Analog=3 };
        enum class Pull : uint8_t { None=0, Up=1, Down=2 };

        struct Pin {
            uint8_t port;   /* 0=A, 1=B, ... */
            uint8_t num;
        };

        void configure(Pin pin, Mode mode, Pull pull)
        {
            printf("  [hal::gpio] configure P%c%u mode=%u pull=%u\n",
                   (char)('A' + pin.port), pin.num,
                   static_cast<uint8_t>(mode),
                   static_cast<uint8_t>(pull));
        }

        void set(Pin pin)
        {
            printf("  [hal::gpio] set P%c%u HIGH\n", (char)('A'+pin.port), pin.num);
        }

        void clear(Pin pin)
        {
            printf("  [hal::gpio] set P%c%u LOW\n", (char)('A'+pin.port), pin.num);
        }
    }  /* namespace gpio */

    namespace uart {
        struct Config {
            uint32_t baud;
            uint8_t  data_bits;
            uint8_t  stop_bits;
        };

        void init(uint8_t instance, Config cfg)
        {
            printf("  [hal::uart] UART%u baud=%u %u%u\n",
                   instance, cfg.baud, cfg.data_bits, cfg.stop_bits);
        }

        void send_byte(uint8_t instance, uint8_t byte)
        {
            printf("  [hal::uart] UART%u TX: 0x%02X\n", instance, byte);
        }
    }  /* namespace uart */

}  /* namespace hal */

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. INLINE NAMESPACE — versioned APIs
 *
 * inline namespace lets you upgrade APIs while maintaining backward compat.
 * The current version is accessed without version prefix.
 * ═══════════════════════════════════════════════════════════════════════════ */
namespace drivers {
    inline namespace v2 {               /* current version — default accessed */
        struct SensorData {
            int16_t temp_raw;
            uint16_t pressure_raw;
            uint16_t humidity_raw;      /* added in v2 */
        };
        void read_sensor(SensorData &out)
        {
            out = { 2250, 100000U, 6000U };
            printf("  [drivers::v2] sensor read: T=%d P=%u H=%u\n",
                   out.temp_raw, out.pressure_raw, out.humidity_raw);
        }
    }

    namespace v1 {                      /* old version still accessible */
        struct SensorData { int16_t temp_raw; uint16_t pressure_raw; };
        void read_sensor(SensorData &out)
        {
            out = { 2250, 100000U };
            printf("  [drivers::v1] sensor read: T=%d P=%u\n",
                   out.temp_raw, out.pressure_raw);
        }
    }
}  /* namespace drivers */

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. ANONYMOUS NAMESPACE — file-internal linkage (replaces static in C++)
 *
 * In C, you use 'static' at file scope to limit visibility.
 * In C++, prefer anonymous namespace — same effect, clearer intent.
 * ═══════════════════════════════════════════════════════════════════════════ */
namespace {
    /* This is internal to this translation unit only */
    constexpr uint32_t INTERNAL_MAGIC = 0xDEADBEEFU;

    void internal_helper(void)
    {
        printf("  [anon ns] internal magic: 0x%08X\n", INTERNAL_MAGIC);
    }
}

/* ─── DEMO ──────────────────────────────────────────────────────────── */
static void demo_namespaces(void)
{
    printf("── Basic namespaces ──\n");

    /* Fully qualified */
    hal::gpio::Pin led_pin = { 0, 5 };   /* PA5 */
    hal::gpio::configure(led_pin, hal::gpio::Mode::Output, hal::gpio::Pull::None);
    hal::gpio::set(led_pin);

    /* Using declaration for a specific name */
    using hal::uart::init;
    using hal::uart::Config;
    init(2, Config{ 115200U, 8, 1 });
    hal::uart::send_byte(2, 0x55U);
    printf("\n");

    printf("── Versioned namespace ──\n");
    drivers::SensorData data{};            /* gets v2::SensorData (inline) */
    drivers::read_sensor(data);            /* gets v2::read_sensor  */

    drivers::v1::SensorData legacy_data{};
    drivers::v1::read_sensor(legacy_data); /* explicit v1 */
    printf("\n");

    printf("── Anonymous namespace ──\n");
    internal_helper();
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * EXERCISES
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * EXERCISE 1:
 *   Create a namespace 'board' that contains pin definitions for a
 *   Nucleo-F411RE board: LED (PA5), BUTTON (PC13), UART2_TX (PA2), UART2_RX (PA3).
 *   Each should be a constexpr hal::gpio::Pin.
 *
 * EXERCISE 2:
 *   Write a namespace 'util' with: min(), max(), abs(), clamp() function
 *   templates. Use them in place of the bare functions from templates.cpp.
 *
 * EXERCISE 3:
 *   Two libraries define 'Error': lib_a::Error and lib_b::Error.
 *   Write a resolve() function that takes both and returns a combined
 *   uint32_t status code packed as [lib_b:16 | lib_a:16].
 */

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 2 — Lesson 5: Namespaces             ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    demo_namespaces();
    return 0;
}
