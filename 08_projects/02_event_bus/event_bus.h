/**
 * @file event_bus.h
 * @brief Capstone 2 — Event Bus API
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Event IDs (bit flags — can combine with OR) */
typedef uint32_t event_id_t;
#define EVT_NONE 0x00000000UL
#define EVT_BUTTON_PRESSED 0x00000001UL
#define EVT_SENSOR_READY 0x00000002UL
#define EVT_UART_RX 0x00000004UL
#define EVT_TIMER_EXPIRED 0x00000008UL
#define EVT_ERROR 0x00000010UL
#define EVT_ALL 0xFFFFFFFFUL

typedef void (*event_handler_t)(event_id_t event, uint32_t data);

bool event_subscribe(event_id_t mask, event_handler_t handler, const char *name);
bool event_post(event_id_t event, uint32_t data);
bool event_dispatch_one(void);
void event_dispatch_all(void);
uint32_t event_queue_depth(void);
void event_tick(void);
