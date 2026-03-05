/**
 * @file event_bus_app.c
 * @brief Capstone 2 — Event Bus: Application Demo
 *
 * Demonstrates the full event-driven application pattern:
 *   - Subscribers register handlers for specific events
 *   - "ISR" posts events to the queue
 *   - Main loop calls event_dispatch_all() — clean, simple main loop
 *
 * BUILD:  cmake --build build --target p8_event_bus
 */

#include "event_bus.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/* ─── Event handlers (could be in separate modules) ─────────────────────── */

static void led_handler(event_id_t event, uint32_t data)
{
    if (event == EVT_BUTTON_PRESSED)
        printf("  [LED]    Button pressed (data=%u), toggling LED\n", data);
    if (event == EVT_TIMER_EXPIRED)
        printf("  [LED]    Timer expired, blink\n");
}

static void sensor_handler(event_id_t event, uint32_t data)
{
    if (event == EVT_SENSOR_READY)
    {
        int32_t temp = (int32_t)data;
        printf("  [SENSOR] Data ready: %d.%d°C\n", temp / 10, temp % 10);
        /* Post a derived event if temperature is high */
        if (temp > 350)
            event_post(EVT_ERROR, 0x01U);
    }
}

static void comms_handler(event_id_t event, uint32_t data)
{
    if (event == EVT_UART_RX)
        printf("  [COMMS]  UART byte received: 0x%02X ('%c')\n",
               data & 0xFFU, (data >= 0x20U && data < 0x7FU) ? (char)data : '.');
    if (event == EVT_SENSOR_READY)
        printf("  [COMMS]  Sending sensor data over UART\n");
}

static void error_handler(event_id_t event, uint32_t data)
{
    (void)event;
    printf("  [ERROR]  Code=0x%02X — logging and asserting LED\n", data);
}

/* ─── Simulated ISRs ─────────────────────────────────────────────────────── */

static void simulate_isr_sequence(void)
{
    printf("  [SIM] Button press ISR\n");
    event_post(EVT_BUTTON_PRESSED, 1U);
    event_tick();

    printf("  [SIM] UART RX ISR: byte=0x48 ('H')\n");
    event_post(EVT_UART_RX, 0x48U);
    event_tick();

    printf("  [SIM] ADC conversion complete ISR: temp=295 (29.5°C)\n");
    event_post(EVT_SENSOR_READY, 295U);
    event_tick();

    printf("  [SIM] Timer ISR\n");
    event_post(EVT_TIMER_EXPIRED, 0U);
    event_tick();

    printf("  [SIM] ADC ISR: temp=372 (37.2°C — high!)\n");
    event_post(EVT_SENSOR_READY, 372U);
    event_tick();
}

int main(void)
{
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Capstone 2 — Interrupt-Safe Event Bus              ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    /* ── Subscribe to events ── */
    event_subscribe(EVT_BUTTON_PRESSED | EVT_TIMER_EXPIRED, led_handler, "LED");
    event_subscribe(EVT_SENSOR_READY, sensor_handler, "SENSOR");
    event_subscribe(EVT_UART_RX | EVT_SENSOR_READY, comms_handler, "COMMS");
    event_subscribe(EVT_ERROR, error_handler, "ERROR");

    printf("── Simulating ISR sequence ──\n");
    simulate_isr_sequence();

    printf("\nQueue depth before dispatch: %u\n\n", event_queue_depth());

    /* ── Main loop drains the queue ── */
    printf("── Dispatching events ──\n");
    event_dispatch_all();

    printf("\nQueue depth after dispatch: %u\n", event_queue_depth());

    printf("\nEXERCISES:\n");
    printf("  1. Add EVT_SYSTEM_INIT and post it before the loop. Show each\n");
    printf("     module can safely initialize from its event handler.\n");
    printf("  2. Add a priority field to the event queue. High-priority events\n");
    printf("     (e.g. EVT_ERROR) should always be dispatched before others.\n");
    printf("  3. Profile how many events can be posted and dispatched per\n");
    printf("     second on STM32F411 at 100 MHz. What is the bottleneck?\n");
    return 0;
}
