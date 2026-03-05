/**
 * @file driver_patterns.cpp
 * @brief Phase 4 — Driver Design Patterns: The Embedded C++ Toolkit
 *
 * Real-world embedded drivers use these patterns:
 *   1. Singleton — one peripheral instance, globally accessible
 *   2. Builder/Init struct — configure before constructing
 *   3. Command Queue — decouple caller from hardware timing
 *   4. Observer on status changes
 *
 * BUILD:  cmake --build build --target p4_driver_patterns
 */

#include "embedded_types.h"
#include <cstdio>
#include <array>

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. BUILDER PATTERN — Configuration via init struct
 *
 * Better than long constructor argument lists.
 * The init struct can be const, placed in Flash on MCU.
 * ═══════════════════════════════════════════════════════════════════════════ */
struct UartConfig {
    uint32_t baud       = 115200U;
    uint8_t  data_bits  = 8U;
    uint8_t  stop_bits  = 1U;
    bool     parity     = false;
    bool     hw_flow    = false;
};

class UartDriver {
public:
    explicit UartDriver(uint8_t instance, const UartConfig &cfg)
        : instance_(instance), cfg_(cfg)
    {
        printf("  [UART%u] init: baud=%u %u%c%u flow=%s\n",
               instance_, cfg_.baud, cfg_.data_bits,
               cfg_.parity ? 'E' : 'N', cfg_.stop_bits,
               cfg_.hw_flow ? "RTS/CTS" : "none");
    }

    status_t send(const char *str)
    {
        printf("  [UART%u] TX: \"%s\"\n", instance_, str);
        return STATUS_OK;
    }

private:
    uint8_t    instance_;
    UartConfig cfg_;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. COMMAND QUEUE — decouple app logic from peripheral timing
 *
 * App enqueues: "set PA5 high after 10ms"
 * Timer ISR dequeues and executes when time arrives.
 * Zero coupling between application and timing mechanism.
 * ═══════════════════════════════════════════════════════════════════════════ */
struct GpioCommand {
    enum class Type : uint8_t { None, SetHigh, SetLow, Toggle };
    Type     type     = Type::None;
    uint8_t  pin      = 0U;
    uint32_t delay_ms = 0U;
};

template <size_t N>
class CommandQueue {
    static_assert((N & (N-1)) == 0, "N must be power of 2");
public:
    bool push(const GpioCommand &cmd)
    {
        size_t next = (head_ + 1U) & (N - 1U);
        if (next == tail_) return false;
        buf_[head_] = cmd;
        head_ = next;
        return true;
    }
    bool pop(GpioCommand &out)
    {
        if (head_ == tail_) return false;
        out  = buf_[tail_];
        tail_ = (tail_ + 1U) & (N - 1U);
        return true;
    }
    bool empty() const { return head_ == tail_; }

private:
    std::array<GpioCommand, N> buf_;
    size_t head_ = 0, tail_ = 0;
};

static void demo_driver_patterns(void)
{
    printf("── Builder pattern (UartConfig) ──\n");
    const UartConfig rs485_cfg = { .baud=9600, .data_bits=8, .stop_bits=1, .parity=true };
    UartDriver modbus_uart(3, rs485_cfg);
    modbus_uart.send("Modbus RTU frame");
    printf("\n");

    printf("── Command queue (deferred GPIO) ──\n");
    CommandQueue<8> q;
    q.push({GpioCommand::Type::SetHigh, 5, 10});
    q.push({GpioCommand::Type::SetLow,  5, 510});
    q.push({GpioCommand::Type::Toggle,  13, 0});

    constexpr const char *type_names[] = {"None","SetHigh","SetLow","Toggle"};
    GpioCommand cmd;
    while (q.pop(cmd)) {
        printf("  Exec: pin %u → %s (delay=%u ms)\n",
               cmd.pin, type_names[static_cast<uint8_t>(cmd.type)], (unsigned)cmd.delay_ms);
    }
    printf("\n");
}

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 4 — Lesson 2: Driver Patterns        ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");
    demo_driver_patterns();

    printf("EXERCISES:\n");
    printf("  1. Add a Singleton<T> template wrapper that holds an instance\n");
    printf("     as a static local variable (Meyers singleton — thread-safe in C++11).\n");
    printf("  2. Extend CommandQueue with a time-sorted pop: next() returns\n");
    printf("     the command with the smallest delay_ms.\n");
    printf("  3. Write a GpioExpander driver (e.g., MCP23017 over I2C) using\n");
    printf("     the command queue to batch register writes.\n");
    return 0;
}
