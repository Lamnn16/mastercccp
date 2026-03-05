/**
 * @file inheritance.cpp
 * @brief Phase 2 — Inheritance & Polymorphism: The Embedded-Safe Subset
 *
 * C++ inheritance is a powerful tool BUT dangerous when misused in embedded:
 *
 * NEVER do on bare-metal MCU:
 *   - virtual functions in interrupt handlers
 *   - deep inheritance hierarchies (>2 levels is a code smell)
 *   - dynamic_cast (requires RTTI — disabled by -fno-rtti)
 *   - Multiple inheritance with virtual base classes (vtable overhead)
 *
 * SAFE and USEFUL on MCU:
 *   - Non-virtual base classes for shared state/methods
 *   - Single virtual dispatch for driver polymorphism (ONE vtable is OK)
 *   - Pure abstract interfaces (all-virtual) for HAL design
 *   - CRTP (Curiously Recurring Template Pattern) for zero-overhead polymorphism
 *
 * BUILD:  cmake --build build --target p2_inheritance
 * RUN:    .\build\02_cpp_fundamentals\p2_inheritance.exe
 */

#include "embedded_types.h"
#include <cstdio>

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 1: PURE ABSTRACT INTERFACE (the right way to do HAL in C++)
 *
 * This is the primary use case for virtual in embedded: define a contract
 * that multiple hardware backends must satisfy.
 * ═══════════════════════════════════════════════════════════════════════════ */

class IDigitalOutput {
public:
    virtual void set_high()   = 0;   /* pure virtual — MUST be overridden */
    virtual void set_low()    = 0;
    virtual void toggle()     = 0;
    virtual bool read_state() const = 0;

    /* Virtual destructor — REQUIRED when deleting through base pointer */
    virtual ~IDigitalOutput() = default;
};

/* Concrete implementations */
class GpioOutput : public IDigitalOutput {
public:
    explicit GpioOutput(uint8_t port, uint8_t pin)
        : port_(port), pin_(pin), state_(false) {}

    void set_high() override { state_ = true;
        printf("    [GPIO] P%c%u = HIGH\n", (char)('A'+port_), pin_); }
    void set_low()  override { state_ = false;
        printf("    [GPIO] P%c%u = LOW\n",  (char)('A'+port_), pin_); }
    void toggle()   override { state_ ? set_low() : set_high(); }
    bool read_state() const override { return state_; }

private:
    uint8_t port_, pin_;
    bool    state_;
};

class SimOutput : public IDigitalOutput {
public:
    explicit SimOutput(const char *label) : label_(label), state_(false) {}

    void set_high() override { state_ = true;
        printf("    [SIM] %-10s = 1\n", label_); }
    void set_low()  override { state_ = false;
        printf("    [SIM] %-10s = 0\n", label_); }
    void toggle()   override { state_ ? set_low() : set_high(); }
    bool read_state() const override { return state_; }

private:
    const char *label_;
    bool        state_;
};

/* Application layer — knows nothing about GPIO, only the interface */
static void blink_three_times(IDigitalOutput &led)
{
    printf("  blink_three_times:\n");
    for (int i = 0; i < 3; i++) {
        led.set_high();
        led.set_low();
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 2: NON-VIRTUAL BASE CLASS for shared logic
 *
 * Use inheritance for CODE REUSE when there's no polymorphism needed.
 * Base class holds common state and utility functions.
 * No vtable overhead — this generates exactly the same code as C structs.
 * ═══════════════════════════════════════════════════════════════════════════ */

class UartBase {
public:
    uint32_t tx_count() const { return tx_count_; }
    uint32_t rx_count() const { return rx_count_; }

protected:
    explicit UartBase(uint8_t instance)
        : instance_(instance), tx_count_(0), rx_count_(0) {}

    void record_tx(size_t len) { tx_count_ += static_cast<uint32_t>(len); }
    void record_rx(size_t len) { rx_count_ += static_cast<uint32_t>(len); }

    uint8_t instance_;

private:
    uint32_t tx_count_;
    uint32_t rx_count_;
};

class UartPolled : public UartBase {
public:
    explicit UartPolled(uint8_t inst) : UartBase(inst) {}

    void send(const uint8_t *buf, size_t len)
    {
        printf("    [UART%u polled] TX %zu bytes\n", instance_, len);
        record_tx(len);
    }
};

class UartDma : public UartBase {
public:
    explicit UartDma(uint8_t inst) : UartBase(inst) {}

    void send(const uint8_t *buf, size_t len)
    {
        printf("    [UART%u DMA   ] TX %zu bytes via DMA\n", instance_, len);
        record_tx(len);
        (void)buf;
    }
};

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 3: CRTP — Compile-Time Polymorphism (ZERO overhead, no vtable)
 *
 * CRTP = Curiously Recurring Template Pattern
 * The base class takes the derived class as a template parameter.
 * Virtual dispatch is resolved at compile time, not runtime.
 * ═══════════════════════════════════════════════════════════════════════════ */

template <typename Derived>
class SensorBase {
public:
    /* 'observe' the derived class through the base interface */
    float read_celsius()
    {
        int32_t raw = static_cast<Derived*>(this)->read_raw();
        return raw_to_celsius(raw);
    }

    void print_reading()
    {
        printf("    [%s] temp = %.2f °C\n",
               static_cast<Derived*>(this)->name(),
               read_celsius());
    }

private:
    /* Common conversion — shared by all sensors */
    static float raw_to_celsius(int32_t raw)
    {
        return static_cast<float>(raw) / 100.0f;
    }
};

class Ds18b20 : public SensorBase<Ds18b20> {
public:
    int32_t read_raw()  { return 2375; }    /* 23.75 °C simulated */
    const char *name()  { return "DS18B20"; }
};

class Lm75 : public SensorBase<Lm75> {
public:
    int32_t read_raw()  { return 2512; }    /* 25.12 °C simulated */
    const char *name()  { return "LM75";   }
};

/* ─── DEMOS ──────────────────────────────────────────────────────────── */
static void demo_interface(void)
{
    printf("── Abstract interface (virtual) ──\n");
    GpioOutput hw_led(0, 5);       /* actual GPIO PA5 */
    SimOutput  sim_led("TEST_LED");

    blink_three_times(hw_led);     /* same code, different backend */
    blink_three_times(sim_led);
    printf("  gpio state after blinks: %s\n\n",
           hw_led.read_state() ? "HIGH(bug)" : "LOW(correct)");
}

static void demo_base_class(void)
{
    printf("── Non-virtual base class (code reuse) ──\n");

    uint8_t frame[] = { 0x01, 0x02, 0x03, 0x04 };
    UartPolled up(1);
    UartDma    ud(2);

    up.send(frame, 4);  ud.send(frame, 4);
    up.send(frame, 2);

    printf("  UART1 tx_count=%u  UART2 tx_count=%u\n\n",
           (unsigned)up.tx_count(), (unsigned)ud.tx_count());
}

static void demo_crtp(void)
{
    printf("── CRTP (zero-overhead compile-time polymorphism) ──\n");
    Ds18b20 ds; ds.print_reading();
    Lm75    lm; lm.print_reading();
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * EXERCISES
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * EXERCISE 1:
 *   Create an IStream abstract interface with:
 *     virtual status_t write(const uint8_t *buf, size_t len) = 0;
 *     virtual status_t read(uint8_t *buf, size_t len, size_t *bytes_read) = 0;
 *     virtual bool     ready() const = 0;
 *   Implement UartStream and SpiStream concrete classes.
 *
 * EXERCISE 2:
 *   Add a threshold alert to SensorBase via CRTP:
 *     void check_alert(float max_temp) {
 *         if (read_celsius() > max_temp) alert_handler();
 *     }
 *   Derived classes implement alert_handler() with their own behavior.
 *
 * EXERCISE 3:
 *   Measure vtable overhead: create 1000 IDigitalOutput calls through
 *   a base pointer vs 1000 direct GpioOutput calls. Print timing.
 *   Discuss: when is the overhead acceptable in embedded?
 */

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 2 — Lesson 6: Inheritance            ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    demo_interface();
    demo_base_class();
    demo_crtp();
    return 0;
}
