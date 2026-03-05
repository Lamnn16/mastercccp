/**
 * @file function_pointers.c
 * @brief Phase 1 — Function Pointers: Callbacks, Dispatch Tables, and Drivers
 *
 * Function pointers are the gateway to OOP-style patterns in C. They appear
 * throughout embedded firmware:
 *
 *   1. Interrupt Vector Table (IVT) — array of function pointers at 0x00000000
 *   2. HAL callbacks (UART complete, SPI complete, DMA complete, etc.)
 *   3. Driver "vtable" — a struct of function pointers (C analog to C++ vtable)
 *   4. Command dispatch table — parse command byte, index into handler table
 *   5. State machine transition table
 *
 * BUILD:  cmake --build build --target p1_function_ptrs
 * RUN:    .\build\01_advanced_c\p1_function_ptrs.exe
 */

#include "embedded_types.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. CALLBACK PATTERN (HAL-style)
 *
 * STM32 HAL uses this everywhere:
 *   HAL_UART_RegisterCallback(&huart1, HAL_UART_TX_COMPLETE_CB_ID, my_cb);
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Callback type: takes a pointer to driver context, returns nothing */
typedef void (*uart_cb_t)(void *context);

typedef struct
{
    volatile bool tx_busy;
    volatile bool rx_ready;
    uart_cb_t tx_complete_cb;
    uart_cb_t rx_complete_cb;
    void *user_context; /* passed to callbacks unchanged */
} uart_driver_t;

static void uart_register_tx_callback(uart_driver_t *drv, uart_cb_t cb, void *ctx)
{
    drv->tx_complete_cb = cb;
    drv->user_context = ctx;
}

/* Call from ISR or DMA complete interrupt */
static void uart_sim_tx_complete(uart_driver_t *drv)
{
    drv->tx_busy = false;
    if (drv->tx_complete_cb != NULL)
    {
        drv->tx_complete_cb(drv->user_context); /* fire callback */
    }
}

/* User's callback */
static void my_tx_done(void *ctx)
{
    const char *msg = (const char *)ctx;
    printf("  [cb] TX complete! message was: \"%s\"\n", msg);
}

static void demo_callback(void)
{
    printf("── 1. HAL-style callback ──\n");

    uart_driver_t uart1 = {.tx_busy = true};
    const char *my_msg = "Hello STM32";

    uart_register_tx_callback(&uart1, my_tx_done, (void *)my_msg);
    uart_sim_tx_complete(&uart1); /* simulate ISR firing */
    printf("  tx_busy after complete: %s\n\n", uart1.tx_busy ? "true" : "false");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. DRIVER VTABLE — C equivalent of C++ virtual functions
 *
 * This is how you write a hardware-agnostic driver layer in C.
 * The same API works for UART, USB, SPI, etc. by swapping the vtable.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Abstract "stream" interface — any byte stream peripheral */
typedef struct stream_ops_s
{
    status_t (*write)(const uint8_t *buf, size_t len);
    status_t (*read)(uint8_t *buf, size_t len);
    status_t (*flush)(void);
} stream_ops_t;

/* ── UART backend ──────────────────────────────────────────────────────── */
static status_t uart_write(const uint8_t *buf, size_t len)
{
    printf("  [UART] writing %zu bytes: ", len);
    for (size_t i = 0; i < len; i++)
        printf("0x%02X ", buf[i]);
    printf("\n");
    return STATUS_OK;
}
static status_t uart_read(uint8_t *buf, size_t len)
{
    UNUSED(buf);
    UNUSED(len);
    return STATUS_OK;
}
static status_t uart_flush(void)
{
    printf("  [UART] flush\n");
    return STATUS_OK;
}

static const stream_ops_t UART_OPS = {
    .write = uart_write,
    .read = uart_read,
    .flush = uart_flush,
};

/* ── SPI backend ──────────────────────────────────────────────────────── */
static status_t spi_write(const uint8_t *buf, size_t len)
{
    printf("  [SPI]  writing %zu bytes: ", len);
    for (size_t i = 0; i < len; i++)
        printf("0x%02X ", buf[i]);
    printf("\n");
    return STATUS_OK;
}
static status_t spi_read(uint8_t *buf, size_t len)
{
    UNUSED(buf);
    UNUSED(len);
    return STATUS_OK;
}
static status_t spi_flush(void)
{
    printf("  [SPI]  flush\n");
    return STATUS_OK;
}

static const stream_ops_t SPI_OPS = {
    .write = spi_write,
    .read = spi_read,
    .flush = spi_flush,
};

/* Stream handle — holds pointer to vtable */
typedef struct
{
    const stream_ops_t *ops;
} stream_t;

static void stream_write(stream_t *s, const uint8_t *buf, size_t len)
{
    if (s && s->ops && s->ops->write)
    {
        s->ops->write(buf, len);
    }
}

static void demo_vtable(void)
{
    printf("── 2. Driver vtable (polymorphism in C) ──\n");

    stream_t uart_stream = {.ops = &UART_OPS};
    stream_t spi_stream = {.ops = &SPI_OPS};

    uint8_t data[] = {0xAA, 0x55, 0xDE, 0xAD};

    stream_write(&uart_stream, data, sizeof(data));
    stream_write(&spi_stream, data, sizeof(data));
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. COMMAND DISPATCH TABLE — parse a command byte, call the handler
 *
 * Used in CLI parsers, protocol decoders, and Modbus implementations.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef void (*cmd_handler_t)(const uint8_t *payload, uint8_t len);

static void cmd_ping(const uint8_t *p, uint8_t len)
{
    UNUSED(p);
    UNUSED(len);
    printf("  CMD PING → PONG\n");
}
static void cmd_read_sensor(const uint8_t *p, uint8_t len)
{
    UNUSED(p);
    UNUSED(len);
    printf("  CMD READ_SENSOR → temp=23.4°C\n");
}
static void cmd_set_led(const uint8_t *p, uint8_t len)
{
    UNUSED(len);
    printf("  CMD SET_LED → LED %s\n", p[0] ? "ON" : "OFF");
}
static void cmd_reset(const uint8_t *p, uint8_t len)
{
    UNUSED(p);
    UNUSED(len);
    printf("  CMD RESET → rebooting...\n");
}

#define CMD_PING 0x01U
#define CMD_READ_SENSOR 0x02U
#define CMD_SET_LED 0x05U
#define CMD_RESET 0x0FU
#define CMD_TABLE_SIZE 16U

typedef struct
{
    uint8_t cmd;
    cmd_handler_t handler;
} cmd_entry_t;

static const cmd_entry_t CMD_TABLE[] = {
    {CMD_PING, cmd_ping},
    {CMD_READ_SENSOR, cmd_read_sensor},
    {CMD_SET_LED, cmd_set_led},
    {CMD_RESET, cmd_reset},
};

static void dispatch_command(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    for (size_t i = 0; i < ARRAY_SIZE(CMD_TABLE); i++)
    {
        if (CMD_TABLE[i].cmd == cmd)
        {
            CMD_TABLE[i].handler(payload, len);
            return;
        }
    }
    printf("  Unknown command: 0x%02X\n", cmd);
}

static void demo_dispatch_table(void)
{
    printf("── 3. Command dispatch table ──\n");

    uint8_t led_on_payload[1] = {1U};
    dispatch_command(CMD_PING, NULL, 0);
    dispatch_command(CMD_READ_SENSOR, NULL, 0);
    dispatch_command(CMD_SET_LED, led_on_payload, 1);
    dispatch_command(0xAA, NULL, 0); /* unknown */
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. STATE MACHINE WITH FUNCTION POINTER TABLE
 *
 * Each state is a function. Transitions are array lookups.
 * This is FASTER than a switch/case and much cleaner for complex FSMs.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum
{
    STATE_IDLE,
    STATE_RX,
    STATE_PROCESS,
    STATE_TX,
    STATE_ERROR,
    STATE_COUNT
} fsm_state_t;

typedef fsm_state_t (*state_fn_t)(void);

static fsm_state_t state_idle(void)
{
    printf("  [FSM] IDLE    → waiting for SOF\n");
    return STATE_RX;
}
static fsm_state_t state_rx(void)
{
    printf("  [FSM] RX      → received frame\n");
    return STATE_PROCESS;
}
static fsm_state_t state_process(void)
{
    printf("  [FSM] PROCESS → building response\n");
    return STATE_TX;
}
static fsm_state_t state_tx(void)
{
    printf("  [FSM] TX      → sent response\n");
    return STATE_IDLE;
}
static fsm_state_t state_error(void)
{
    printf("  [FSM] ERROR   → recovering\n");
    return STATE_IDLE;
}

static const state_fn_t FSM_TABLE[STATE_COUNT] = {
    [STATE_IDLE] = state_idle,
    [STATE_RX] = state_rx,
    [STATE_PROCESS] = state_process,
    [STATE_TX] = state_tx,
    [STATE_ERROR] = state_error,
};

static void demo_fsm(void)
{
    printf("── 4. State machine via function pointer table ──\n");

    fsm_state_t state = STATE_IDLE;
    for (int step = 0; step < 4; step++)
    {
        state = FSM_TABLE[state]();
    }
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * EXERCISES
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * EXERCISE 1:
 *   Extend the FSM to handle a timeout event. Add a state STATE_TIMEOUT that
 *   transitions to STATE_ERROR. Modify state_rx() to sometimes return
 *   STATE_TIMEOUT instead of STATE_PROCESS.
 *
 * EXERCISE 2:
 *   Rewrite the stream vtable to include a third backend: a "null" stream that
 *   silently discards all writes (useful for disabling logging without #if).
 *
 * EXERCISE 3:
 *   Write a sorted command table and implement binary search dispatch.
 *   Measure the improvement in worst-case lookup time for 256-entry tables.
 */

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 1 — Lesson 5: Function Pointers      ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    demo_callback();
    demo_vtable();
    demo_dispatch_table();
    demo_fsm();

    return 0;
}
