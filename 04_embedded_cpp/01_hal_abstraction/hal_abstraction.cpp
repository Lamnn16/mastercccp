/**
 * @file hal_abstraction.cpp
 * @brief Phase 4 — HAL Abstraction: Building a Hardware-Independent Layer in C++
 *
 * A well-designed HAL (Hardware Abstraction Layer) lets you:
 *   1. Test application logic on PC without hardware
 *   2. Port between STM32 families by changing only the HAL implementation
 *   3. Unit-test drivers with mock hardware
 *
 * Architecture:
 *   Application layer → HAL interface (abstract C++ interface)
 *                            ↓
 *              STM32 HAL impl  |  PC Simulator impl
 *
 * BUILD:  cmake --build build --target p4_hal_abstraction
 */

#include "embedded_types.h"
#include <cstdio>
#include <cstring>

/* ═══════════════════════════════════════════════════════════════════════════
 * HAL INTERFACES — pure abstract, no hardware dependency
 * ═══════════════════════════════════════════════════════════════════════════ */
namespace hal {

class IGpio {
public:
    enum class Direction { Input, Output };
    virtual void     set_direction(Direction d) = 0;
    virtual void     write(bool high) = 0;
    virtual bool     read() const = 0;
    virtual void     toggle() = 0;
    virtual ~IGpio() = default;
};

class IUart {
public:
    virtual status_t send(const uint8_t *buf, size_t len) = 0;
    virtual status_t recv(uint8_t *buf, size_t len, size_t *received) = 0;
    virtual bool     tx_ready() const = 0;
    virtual ~IUart() = default;
};

class ITimer {
public:
    virtual void     start()    = 0;
    virtual void     stop()     = 0;
    virtual uint32_t get_ms()   = 0;
    virtual void     delay_ms(uint32_t ms) = 0;
    virtual ~ITimer() = default;
};

}  /* namespace hal */

/* ═══════════════════════════════════════════════════════════════════════════
 * PC SIMULATOR IMPLEMENTATIONS — no hardware needed
 * ═══════════════════════════════════════════════════════════════════════════ */
namespace sim {

class SimGpio : public hal::IGpio {
public:
    explicit SimGpio(const char *name) : name_(name), state_(false), dir_(Direction::Output) {}

    void set_direction(Direction d) override
    {
        dir_ = d;
        printf("  [SimGpio:%-8s] direction=%s\n", name_,
               d == Direction::Output ? "OUT" : "IN");
    }
    void write(bool high) override
    {
        state_ = high;
        printf("  [SimGpio:%-8s] = %s\n", name_, high ? "HIGH" : "LOW");
    }
    bool read() const override { return state_; }
    void toggle() override { write(!state_); }

private:
    const char *name_;
    bool        state_;
    Direction   dir_;
};

class SimUart : public hal::IUart {
public:
    explicit SimUart(uint8_t id) : id_(id) {}

    status_t send(const uint8_t *buf, size_t len) override
    {
        printf("  [SimUART%u] TX: ", id_);
        for (size_t i = 0; i < len; i++) {
            if (buf[i] >= 32 && buf[i] < 127) printf("%c", (char)buf[i]);
            else                               printf("[%02X]", buf[i]);
        }
        printf("\n");
        return STATUS_OK;
    }

    status_t recv(uint8_t *buf, size_t len, size_t *received) override
    {
        /* Simulate receiving "OK\r\n" */
        const char *fake = "OK\r\n";
        size_t n = strlen(fake) < len ? strlen(fake) : len;
        memcpy(buf, fake, n);
        if (received) *received = n;
        return STATUS_OK;
    }

    bool tx_ready() const override { return true; }

private:
    uint8_t id_;
};

class SimTimer : public hal::ITimer {
public:
    SimTimer() : ticks_(0), running_(false) {}
    void     start() override    { running_ = true; printf("  [SimTimer] started\n"); }
    void     stop() override     { running_ = false; }
    uint32_t get_ms() override   { return ticks_; }
    void     delay_ms(uint32_t ms) override
    {
        printf("  [SimTimer] delay %u ms (fast sim)\n", (unsigned)ms);
        ticks_ += ms;
    }

private:
    uint32_t ticks_;
    bool     running_;
};

}  /* namespace sim */

/* ═══════════════════════════════════════════════════════════════════════════
 * APPLICATION LAYER — talks only to HAL interfaces
 *
 * This code will work IDENTICALLY on STM32 or PC.
 * ═══════════════════════════════════════════════════════════════════════════ */
class LedBlinker {
public:
    LedBlinker(hal::IGpio &led, hal::ITimer &timer, uint32_t period_ms)
        : led_(led), timer_(timer), period_ms_(period_ms) {}

    void update()
    {
        if (timer_.get_ms() - last_toggle_ >= period_ms_) {
            led_.toggle();
            last_toggle_ = timer_.get_ms();
        }
    }

    void blink_n(uint32_t n)
    {
        printf("  Blinking %u times, period=%u ms:\n", (unsigned)n, (unsigned)period_ms_);
        for (uint32_t i = 0; i < n * 2U; i++) {
            timer_.delay_ms(period_ms_ / 2U);
            led_.toggle();
        }
    }

private:
    hal::IGpio  &led_;
    hal::ITimer &timer_;
    uint32_t     period_ms_;
    uint32_t     last_toggle_ = 0U;
};

class AtModem {
public:
    AtModem(hal::IUart &uart, hal::ITimer &timer)
        : uart_(uart), timer_(timer) {}

    bool send_cmd(const char *cmd, char *resp, size_t resp_len)
    {
        printf("  [AT] → %s\n", cmd);
        uart_.send(reinterpret_cast<const uint8_t*>(cmd), strlen(cmd));

        size_t received = 0;
        uart_.recv(reinterpret_cast<uint8_t*>(resp), resp_len - 1, &received);
        resp[received] = '\0';

        printf("  [AT] ← %s\n", resp);
        return strstr(resp, "OK") != nullptr;
    }

private:
    hal::IUart  &uart_;
    hal::ITimer &timer_;
};

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 4 — Lesson 1: HAL Abstraction        ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    sim::SimGpio  led("LED_PA5");
    sim::SimTimer timer;
    sim::SimUart  modem_uart(2);

    led.set_direction(hal::IGpio::Direction::Output);
    timer.start();

    printf("── LED Blinker (hardware-independent) ──\n");
    LedBlinker blinker(led, timer, 500U);
    blinker.blink_n(3);
    printf("\n");

    printf("── AT Modem (hardware-independent) ──\n");
    AtModem modem(modem_uart, timer);
    char resp[32];
    modem.send_cmd("AT\r\n", resp, sizeof(resp));
    modem.send_cmd("AT+CGMI\r\n", resp, sizeof(resp));
    printf("\n");

    printf("EXERCISES:\n");
    printf("  1. Add a SimSpi class implementing an ISpi interface with:\n");
    printf("     transfer(tx, rx, len), cs_assert(), cs_deassert().\n");
    printf("  2. Write an EEPROM driver class that takes ISpi& in its ctor.\n");
    printf("     It should work with SimSpi AND a real STM32 SPI backend.\n");
    printf("  3. Write a mock GPIO that records all writes for testing.\n");
    return 0;
}
