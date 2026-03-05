/**
 * @file observer.cpp
 * @brief Phase 4 — Observer Pattern: Decoupled Event Systems on MCU
 *
 * The Observer pattern (also called Publish/Subscribe or Event Bus) lets
 * components communicate without direct dependencies.
 *
 * Example: Button ISR fires → ButtonEvent published →
 *           LED subscriber toggles, Logger subscriber prints, Modem wakes up
 *
 * No component knows about the others. Adding a new reaction to a button
 * press requires ZERO changes to existing code — just register a new observer.
 *
 * BUILD:  cmake --build build --target p4_observer
 */

#include "embedded_types.h"
#include <cstdio>
#include <array>

/* ═══════════════════════════════════════════════════════════════════════════
 * EVENT TYPES
 * ═══════════════════════════════════════════════════════════════════════════ */
enum class EventId : uint8_t {
    BUTTON_PRESS,
    BUTTON_RELEASE,
    TEMPERATURE_ALARM,
    UART_FRAME_RECEIVED,
    COUNT,
};

struct Event {
    EventId  id;
    uint32_t data;     /* event-specific payload (cast as needed) */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * OBSERVER INTERFACE
 * ═══════════════════════════════════════════════════════════════════════════ */
class IObserver {
public:
    virtual void on_event(const Event &e) = 0;
    virtual ~IObserver() = default;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * EVENT BUS — static array, no heap
 * ═══════════════════════════════════════════════════════════════════════════ */
template <size_t MAX_OBSERVERS>
class EventBus {
public:
    bool subscribe(EventId id, IObserver *obs)
    {
        for (size_t i = 0; i < MAX_OBSERVERS; i++) {
            if (slots_[i].observer == nullptr) {
                slots_[i] = { id, obs };
                return true;
            }
        }
        return false;   /* bus full */
    }

    void unsubscribe(IObserver *obs)
    {
        for (auto &s : slots_) {
            if (s.observer == obs) { s = {}; }
        }
    }

    void publish(const Event &e)
    {
        for (const auto &s : slots_) {
            if (s.observer && s.event_id == e.id) {
                s.observer->on_event(e);
            }
        }
    }

private:
    struct Slot { EventId event_id = EventId::COUNT; IObserver *observer = nullptr; };
    std::array<Slot, MAX_OBSERVERS> slots_;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * CONCRETE OBSERVERS (components that react to events)
 * ═══════════════════════════════════════════════════════════════════════════ */
class LedObserver : public IObserver {
public:
    void on_event(const Event &e) override
    {
        if (e.id == EventId::BUTTON_PRESS) {
            state_ = !state_;
            printf("    [LED] toggled → %s\n", state_ ? "ON" : "OFF");
        }
    }
private: bool state_ = false;
};

class LogObserver : public IObserver {
public:
    void on_event(const Event &e) override
    {
        switch (e.id) {
        case EventId::BUTTON_PRESS:   printf("    [LOG] Button pressed\n"); break;
        case EventId::BUTTON_RELEASE: printf("    [LOG] Button released\n"); break;
        case EventId::TEMPERATURE_ALARM:
            printf("    [LOG] TEMP ALARM! val=%u (raw)\n", (unsigned)e.data);
            break;
        default: break;
        }
    }
};

class AlarmObserver : public IObserver {
public:
    void on_event(const Event &e) override
    {
        if (e.id == EventId::TEMPERATURE_ALARM) {
            printf("    [ALARM] activating buzzer! temp=%u\n", (unsigned)e.data);
        }
    }
};

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 4 — Lesson 4: Observer Pattern       ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    EventBus<8> bus;

    LedObserver   led;
    LogObserver   logger;
    AlarmObserver alarm;

    bus.subscribe(EventId::BUTTON_PRESS,       &led);
    bus.subscribe(EventId::BUTTON_PRESS,       &logger);
    bus.subscribe(EventId::BUTTON_RELEASE,     &logger);
    bus.subscribe(EventId::TEMPERATURE_ALARM,  &logger);
    bus.subscribe(EventId::TEMPERATURE_ALARM,  &alarm);

    printf("── Button events ──\n");
    bus.publish({ EventId::BUTTON_PRESS,   0U });
    bus.publish({ EventId::BUTTON_RELEASE, 0U });
    bus.publish({ EventId::BUTTON_PRESS,   0U });
    printf("\n");

    printf("── Temperature alarm ──\n");
    bus.publish({ EventId::TEMPERATURE_ALARM, 8500U }); /* 85.00 °C * 100 */
    printf("\n");

    printf("EXERCISES:\n");
    printf("  1. Add priority to observers: higher priority gets called first.\n");
    printf("     Use a uint8_t priority field in Slot, sort on subscribe.\n");
    printf("  2. Make the event bus ISR-safe by using a RingBuffer<Event,16>\n");
    printf("     for deferred publish. ISR calls enqueue(), main calls dispatch().\n");
    printf("  3. Add a once() subscription that auto-unregisters after 1 event.\n");
    return 0;
}
