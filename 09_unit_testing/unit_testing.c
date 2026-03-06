/**
 * ╔══════════════════════════════════════════════════════════════════════════╗
 * ║  MODULE 09 — Unit Testing for Embedded C                                ║
 * ║  "If it isn't tested, it doesn't work."                                 ║
 * ╚══════════════════════════════════════════════════════════════════════════╝
 *
 * Written from the perspective of a 15+ year embedded software engineer.
 *
 * Hard truth: most embedded bugs are caught in field devices, not in CI.
 * The fix is disciplined off-target unit testing: run firmware logic on
 * your dev PC, fast, repeatable, no oscilloscope needed.
 *
 * Topics:
 *   1. Why unit testing is harder in embedded — and how to solve it
 *   2. Rolling a minimal test framework (no third-party deps)
 *   3. Hardware seams — mocking peripherals with function pointers
 *   4. Testing a real ring buffer (from capstone 1 — logic only, no hardware)
 *   5. Testing a finite state machine
 *   6. Testing integer / fixed-point math edge cases
 *   7. Test organisation: setup / teardown / suites
 *   8. Coverage traps: branch coverage, uint8_t overflow, off-by-one
 *
 * BUILD:  cmake --build build --target p9_unit_testing
 * RUN:    .\build\09_unit_testing\p9_unit_testing.exe
 *
 * Real-world frameworks to graduate to after this module:
 *   • Unity     (Throw The Switch) — de-facto standard for embedded C
 *   • CppUTest  — C++ capable, used on FreeRTOS itself
 *   • Google Test (gtest) — powerful but big; fine for host-side tests
 *   • Ceedling  — Unity + CMock + Ruby rake; full CI pipeline tool
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 1 — Why embedded unit testing is hard
 * ══════════════════════════════════════════════════════════════════════════
 *
 * Challenge 1: Hardware dependency
 *   Your code does: REG32(0x40020018) |= (1U << 5);
 *   On a PC this causes a segfault or silently reads 0.
 *   SOLUTION: "Seams" — inject hardware access through function pointers
 *             or HAL abstractions so tests can substitute fakes.
 *
 * Challenge 2: No heap / limited stack
 *   You can't use most test frameworks compiled for desktop.
 *   SOLUTION: Run tests OFF-TARGET on PC (recommended) or write a
 *             ROM-friendly framework (no dynamic allocation).
 *
 * Challenge 3: Timing-dependent code
 *   HAL_Delay(1000) blocks your test runner for a real second.
 *   SOLUTION: Abstract time — inject a "tick counter" so tests can
 *             advance time instantly without sleeping.
 *
 * Challenge 4: Global mutable state
 *   Many embedded drivers use file-scope statics.
 *   SOLUTION: Expose reset/init functions that tests call between runs.
 *             Or use dependency injection patterns.
 *
 * Challenge 5: Interrupt-driven logic
 *   You can't trigger a real UART IRQ on PC.
 *   SOLUTION: Extract the ISR *body* into a plain C function and call
 *             it directly from tests.
 */

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 2 — Minimal test framework (no deps)
 * ══════════════════════════════════════════════════════════════════════════
 *
 * A real framework like Unity is ~1000 lines of macros.
 * Understanding *why* it's built this way is the point of this section.
 *
 * Key design decisions:
 *   • __FILE__ / __LINE__ in failure message → pinpoints the exact assertion
 *   • Non-fatal assertions (TEST_EXPECT) continue after failure
 *   • Fatal assertions (TEST_ASSERT) abort the current test via longjmp
 *     (Unity does this; we simplify here with a flag + early return)
 *   • Each test function has setup() and teardown() called around it
 */

typedef struct
{
    int tests_run;
    int tests_passed;
    int tests_failed;
    bool current_test_failed;
    char current_test_name[64];
} TestRunner;

static TestRunner g_runner;

/* ── Assertion macros ─────────────────────────────────────────────────── */

#define TEST_ASSERT_EQ(expected, actual)                        \
    do                                                          \
    {                                                           \
        if ((expected) != (actual))                             \
        {                                                       \
            printf("  FAIL  %s:%d  expected=%lld  got=%lld\n",  \
                   __FILE__, __LINE__,                          \
                   (long long)(expected), (long long)(actual)); \
            g_runner.current_test_failed = true;                \
        }                                                       \
    } while (0)

#define TEST_ASSERT_TRUE(cond)                           \
    do                                                   \
    {                                                    \
        if (!(cond))                                     \
        {                                                \
            printf("  FAIL  %s:%d  expected TRUE: %s\n", \
                   __FILE__, __LINE__, #cond);           \
            g_runner.current_test_failed = true;         \
        }                                                \
    } while (0)

#define TEST_ASSERT_NULL(ptr)                                             \
    do                                                                    \
    {                                                                     \
        if ((ptr) != NULL)                                                \
        {                                                                 \
            printf("  FAIL  %s:%d  expected NULL\n", __FILE__, __LINE__); \
            g_runner.current_test_failed = true;                          \
        }                                                                 \
    } while (0)

#define TEST_ASSERT_NOT_NULL(ptr)                                             \
    do                                                                        \
    {                                                                         \
        if ((ptr) == NULL)                                                    \
        {                                                                     \
            printf("  FAIL  %s:%d  expected non-NULL\n", __FILE__, __LINE__); \
            g_runner.current_test_failed = true;                              \
        }                                                                     \
    } while (0)

#define TEST_ASSERT_MEM_EQ(expected, actual, len)                  \
    do                                                             \
    {                                                              \
        if (memcmp((expected), (actual), (len)) != 0)              \
        {                                                          \
            printf("  FAIL  %s:%d  memory mismatch (%zu bytes)\n", \
                   __FILE__, __LINE__, (size_t)(len));             \
            g_runner.current_test_failed = true;                   \
        }                                                          \
    } while (0)

/* ── Test lifecycle ───────────────────────────────────────────────────── */
typedef void (*test_fn_t)(void);

static void run_test(const char *name, test_fn_t setup,
                     test_fn_t test, test_fn_t teardown)
{
    snprintf(g_runner.current_test_name,
             sizeof(g_runner.current_test_name), "%s", name);
    g_runner.current_test_failed = false;
    g_runner.tests_run++;

    if (setup)
        setup();
    test();
    if (teardown)
        teardown();

    if (g_runner.current_test_failed)
    {
        printf("  [FAIL] %s\n", name);
        g_runner.tests_failed++;
    }
    else
    {
        printf("  [PASS] %s\n", name);
        g_runner.tests_passed++;
    }
}

#define RUN_TEST(fn) run_test(#fn, NULL, fn, NULL)
#define RUN_TEST_F(fn, s, t) run_test(#fn, s, fn, t)

static void print_results(const char *suite_name)
{
    printf("  ─────────────────────────────────────────────\n");
    printf("  Suite: %-30s  %d/%d passed\n",
           suite_name, g_runner.tests_passed, g_runner.tests_run);
    if (g_runner.tests_failed > 0)
    {
        printf("  *** %d FAILURE(S) ***\n", g_runner.tests_failed);
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 3 — Hardware seams: mock peripherals via function pointers
 * ══════════════════════════════════════════════════════════════════════════
 *
 * REAL embedded driver (gpio.c):
 *   void gpio_set(uint8_t port, uint8_t pin) {
 *       GPIO_TypeDef *gpio = gpio_port_to_reg(port);  // reads hardware!
 *       gpio->BSRR = (1U << pin);
 *   }
 *
 * TESTABLE driver — inject the register-write function:
 *   The driver calls g_hw.reg_write(addr, val) instead of *(volatile uint32_t*)addr = val
 *   Tests swap g_hw.reg_write with a spy that records calls.
 *
 * This is the "seam" pattern from "Working Effectively with Legacy Code"
 * by Michael Feathers — the single most important testing book for
 * embedded engineers.
 */

/* Simulated "hardware register file" — replaces real MMIO in tests */
#define MAX_REG_ADDR 32
static uint32_t s_fake_regs[MAX_REG_ADDR];
static uint32_t s_write_call_count = 0U;

static void fake_reg_write(uint32_t addr, uint32_t val)
{
    if (addr < MAX_REG_ADDR)
    {
        s_fake_regs[addr] = val;
        s_write_call_count++;
    }
}

static uint32_t fake_reg_read(uint32_t addr)
{
    return (addr < MAX_REG_ADDR) ? s_fake_regs[addr] : 0U;
}

/* The "driver" under test — uses injected HAL, not real MMIO */
typedef struct
{
    void (*reg_write)(uint32_t addr, uint32_t val);
    uint32_t (*reg_read)(uint32_t addr);
} Hal;

static const Hal g_fake_hal = {fake_reg_write, fake_reg_read};

/* GPIO register offsets (mirrors STM32 layout) */
#define GPIO_MODER_OFFSET 0U
#define GPIO_ODR_OFFSET 5U
#define GPIO_BSRR_OFFSET 6U

/* Driver function — works with any HAL (real or fake) */
static void gpio_set_output(const Hal *hal, uint32_t base, uint8_t pin)
{
    /* Set MODER bits [2n+1:2n] = 01 for output */
    uint32_t moder = hal->reg_read(base + GPIO_MODER_OFFSET);
    moder &= ~(3U << (pin * 2U));
    moder |= (1U << (pin * 2U));
    hal->reg_write(base + GPIO_MODER_OFFSET, moder);
}

static void gpio_write(const Hal *hal, uint32_t base, uint8_t pin, bool val)
{
    /* Use BSRR: high 16 bits = reset, low 16 bits = set */
    uint32_t bsrr = val ? (1U << pin) : (1U << (pin + 16U));
    hal->reg_write(base + GPIO_BSRR_OFFSET, bsrr);
}

/* ── Tests for gpio driver ─────────────────────────────────────────────── */
static void gpio_setup(void)
{
    memset(s_fake_regs, 0, sizeof(s_fake_regs));
    s_write_call_count = 0U;
}

static void test_gpio_set_output_sets_moder(void)
{
    gpio_set_output(&g_fake_hal, 0U, 5U);
    /* Pin 5: MODER bits [11:10] should be 0b01 = 1 */
    uint32_t moder = s_fake_regs[GPIO_MODER_OFFSET];
    uint32_t pin5_mode = (moder >> (5U * 2U)) & 3U;
    TEST_ASSERT_EQ(1U, pin5_mode);
    TEST_ASSERT_EQ(1U, s_write_call_count);
}

static void test_gpio_write_high_sets_bsrr_low_bit(void)
{
    gpio_write(&g_fake_hal, 0U, 3U, true);
    uint32_t bsrr = s_fake_regs[GPIO_BSRR_OFFSET];
    TEST_ASSERT_TRUE((bsrr & (1U << 3U)) != 0U); /* set bit 3 */
    TEST_ASSERT_TRUE((bsrr >> 16U) == 0U);       /* no reset bits */
}

static void test_gpio_write_low_sets_bsrr_high_bit(void)
{
    gpio_write(&g_fake_hal, 0U, 3U, false);
    uint32_t bsrr = s_fake_regs[GPIO_BSRR_OFFSET];
    TEST_ASSERT_TRUE((bsrr & (1U << (3U + 16U))) != 0U); /* reset bit 3 */
    TEST_ASSERT_TRUE((bsrr & 0xFFFFU) == 0U);            /* no set bits */
}

static void test_gpio_set_output_only_modifies_target_pin(void)
{
    /* Pre-set other pins as analog (0b11) */
    s_fake_regs[GPIO_MODER_OFFSET] = 0xFFFFFFFFU;
    gpio_set_output(&g_fake_hal, 0U, 2U);
    uint32_t moder = s_fake_regs[GPIO_MODER_OFFSET];
    /* Pin 2 should be 01, all other pins unchanged (11) */
    uint32_t pin2 = (moder >> 4U) & 3U;
    uint32_t pin1 = (moder >> 2U) & 3U;
    uint32_t pin3 = (moder >> 6U) & 3U;
    TEST_ASSERT_EQ(1U, pin2);
    TEST_ASSERT_EQ(3U, pin1); /* untouched */
    TEST_ASSERT_EQ(3U, pin3); /* untouched */
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 4 — Testing ring buffer logic (pure C, no hardware)
 * ══════════════════════════════════════════════════════════════════════════
 *
 * The ring buffer from Capstone 1 is ALREADY hardware-independent — pure
 * data structure logic. This is the ideal shape for a unit-testable module.
 *
 * Lesson: Keep data-structure logic in files that have NO hardware includes.
 *         Hardware drivers call into these, not the reverse.
 */
#define RB_SIZE 8U /* must be power of 2 */
#define RB_MASK (RB_SIZE - 1U)

typedef struct
{
    uint8_t buf[RB_SIZE];
    uint32_t head; /* next write index */
    uint32_t tail; /* next read index  */
} RingBuf;

static void rb_init(RingBuf *rb) { memset(rb, 0, sizeof(*rb)); }
static bool rb_full(const RingBuf *rb)
{
    return (rb->head - rb->tail) == RB_SIZE;
}
static bool rb_empty(const RingBuf *rb) { return rb->head == rb->tail; }
static bool rb_push(RingBuf *rb, uint8_t byte)
{
    if (rb_full(rb))
        return false;
    rb->buf[rb->head & RB_MASK] = byte;
    rb->head++;
    return true;
}
static bool rb_pop(RingBuf *rb, uint8_t *out)
{
    if (rb_empty(rb))
        return false;
    *out = rb->buf[rb->tail & RB_MASK];
    rb->tail++;
    return true;
}
static uint32_t rb_count(const RingBuf *rb) { return rb->head - rb->tail; }

static RingBuf s_rb;

static void rb_setup(void) { rb_init(&s_rb); }

static void test_rb_empty_on_init(void)
{
    TEST_ASSERT_TRUE(rb_empty(&s_rb));
    TEST_ASSERT_EQ(0U, rb_count(&s_rb));
}

static void test_rb_push_increases_count(void)
{
    TEST_ASSERT_TRUE(rb_push(&s_rb, 0xAA));
    TEST_ASSERT_EQ(1U, rb_count(&s_rb));
    TEST_ASSERT_TRUE(!rb_empty(&s_rb));
}

static void test_rb_pop_returns_correct_byte(void)
{
    rb_push(&s_rb, 0x42);
    uint8_t val = 0U;
    bool ok = rb_pop(&s_rb, &val);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQ(0x42U, val);
    TEST_ASSERT_TRUE(rb_empty(&s_rb));
}

static void test_rb_fifo_order(void)
{
    for (uint8_t i = 1; i <= 4; i++)
        rb_push(&s_rb, i);
    for (uint8_t i = 1; i <= 4; i++)
    {
        uint8_t v = 0U;
        rb_pop(&s_rb, &v);
        TEST_ASSERT_EQ(i, v);
    }
}

static void test_rb_full_rejects_push(void)
{
    /* Fill to capacity */
    for (uint8_t i = 0; i < RB_SIZE; i++)
        rb_push(&s_rb, i);
    TEST_ASSERT_TRUE(rb_full(&s_rb));
    /* One more push must fail */
    TEST_ASSERT_TRUE(!rb_push(&s_rb, 0xFF));
    TEST_ASSERT_EQ(RB_SIZE, rb_count(&s_rb)); /* count unchanged */
}

static void test_rb_wrap_around_preserves_data(void)
{
    /* Push 6, pop 6 (moves pointers), then push 4 more — wraps */
    for (uint8_t i = 0; i < 6U; i++)
        rb_push(&s_rb, i);
    for (uint8_t i = 0; i < 6U; i++)
    {
        uint8_t v = 0U;
        rb_pop(&s_rb, &v);
    }
    /* Now head=6, tail=6, buf offset has wrapped */
    for (uint8_t i = 10; i < 14U; i++)
        rb_push(&s_rb, i);
    for (uint8_t i = 10; i < 14U; i++)
    {
        uint8_t v = 0U;
        bool ok = rb_pop(&s_rb, &v);
        TEST_ASSERT_TRUE(ok);
        TEST_ASSERT_EQ(i, v);
    }
}

static void test_rb_pop_on_empty_returns_false(void)
{
    uint8_t v = 0xDEU;
    TEST_ASSERT_TRUE(!rb_pop(&s_rb, &v));
    TEST_ASSERT_EQ(0xDEU, v); /* output buffer must NOT be corrupted */
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 5 — Testing a finite state machine
 * ══════════════════════════════════════════════════════════════════════════
 *
 * FSMs are common in embedded: UART parsers, button debouncers, connection
 * managers, battery chargers. Testing them is about verifying TRANSITIONS
 * and OUTPUTS for every (state, input) pair — i.e., the state table.
 *
 * State machine under test: UART frame parser
 *  States:  IDLE → SOF → LENGTH → DATA → CRC → COMPLETE / ERROR
 *  Events:  byte received
 *  Output:  frame_ready flag, frame buffer filled
 *
 * The key lesson: the FSM function is a pure function of (state, input) —
 * no hardware needed. Feed it bytes in tests exactly as an ISR would.
 */
typedef enum
{
    PARSE_IDLE,
    PARSE_SOF,    /* received 0xAA, waiting for 0x55 */
    PARSE_LENGTH, /* waiting for length byte */
    PARSE_DATA,   /* collecting data[length] bytes */
    PARSE_CRC,    /* waiting for 8-bit XOR checksum */
    PARSE_COMPLETE,
    PARSE_ERROR,
} ParseState;

typedef struct
{
    ParseState state;
    uint8_t buf[64];
    uint8_t length;
    uint8_t rxed;
    uint8_t crc_accum;
    bool frame_ready;
} FrameParser;

static void parser_init(FrameParser *p) { memset(p, 0, sizeof(*p)); }

/* Returns true if state became COMPLETE this call */
static bool parser_feed(FrameParser *p, uint8_t byte)
{
    p->frame_ready = false;
    switch (p->state)
    {
    case PARSE_IDLE:
        if (byte == 0xAAU)
            p->state = PARSE_SOF;
        break;
    case PARSE_SOF:
        p->state = (byte == 0x55U) ? PARSE_LENGTH : PARSE_ERROR;
        break;
    case PARSE_LENGTH:
        p->length = byte;
        p->rxed = 0U;
        p->crc_accum = byte; /* CRC starts with length byte */
        p->state = (byte > 0U && byte <= 64U) ? PARSE_DATA : PARSE_ERROR;
        break;
    case PARSE_DATA:
        p->buf[p->rxed++] = byte;
        p->crc_accum ^= byte;
        if (p->rxed == p->length)
            p->state = PARSE_CRC;
        break;
    case PARSE_CRC:
        if (byte == p->crc_accum)
        {
            p->frame_ready = true;
            p->state = PARSE_COMPLETE;
        }
        else
        {
            p->state = PARSE_ERROR;
        }
        break;
    case PARSE_COMPLETE:
    case PARSE_ERROR:
        break;
    }
    return p->frame_ready;
}

static FrameParser s_parser;
static void parser_setup(void) { parser_init(&s_parser); }

static void test_parser_starts_idle(void)
{
    TEST_ASSERT_EQ(PARSE_IDLE, s_parser.state);
    TEST_ASSERT_TRUE(!s_parser.frame_ready);
}

static void test_parser_ignores_junk_before_sof(void)
{
    parser_feed(&s_parser, 0x00U);
    parser_feed(&s_parser, 0x12U);
    parser_feed(&s_parser, 0xFFU);
    TEST_ASSERT_EQ(PARSE_IDLE, s_parser.state);
}

static void test_parser_valid_frame(void)
{
    /* Build frame: AA 55 03 0x11 0x22 0x33 CRC */
    uint8_t crc = 0x03U ^ 0x11U ^ 0x22U ^ 0x33U;
    uint8_t frame[] = {0xAAU, 0x55U, 0x03U, 0x11U, 0x22U, 0x33U, crc};
    for (size_t i = 0; i < sizeof(frame); i++)
    {
        parser_feed(&s_parser, frame[i]);
    }
    TEST_ASSERT_EQ(PARSE_COMPLETE, s_parser.state);
    TEST_ASSERT_TRUE(s_parser.frame_ready);
    TEST_ASSERT_EQ(3U, s_parser.length);
    TEST_ASSERT_EQ(0x11U, s_parser.buf[0]);
    TEST_ASSERT_EQ(0x22U, s_parser.buf[1]);
    TEST_ASSERT_EQ(0x33U, s_parser.buf[2]);
}

static void test_parser_bad_crc_goes_error(void)
{
    uint8_t frame[] = {0xAAU, 0x55U, 0x01U, 0xABU, 0x00U /* bad CRC */};
    for (size_t i = 0; i < sizeof(frame); i++)
    {
        parser_feed(&s_parser, frame[i]);
    }
    TEST_ASSERT_EQ(PARSE_ERROR, s_parser.state);
    TEST_ASSERT_TRUE(!s_parser.frame_ready);
}

static void test_parser_bad_second_sof_byte_goes_error(void)
{
    parser_feed(&s_parser, 0xAAU);
    parser_feed(&s_parser, 0x00U); /* not 0x55 */
    TEST_ASSERT_EQ(PARSE_ERROR, s_parser.state);
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 6 — Integer / fixed-point edge-case tests
 * ══════════════════════════════════════════════════════════════════════════
 *
 * Embedded code is full of uint8_t/uint16_t arithmetic.
 * Classic bugs:
 *   • uint8_t sum = a + b;  overflows silently (wraps at 256)
 *   • int16_t x = (int16_t)(adc_raw - 2048);  sign-extension trap
 *   • Fixed-point multiply: (int32_t)(a * b) >> Q  — intermediate overflow
 *
 * Test these exhaustively. A missed overflow in battery voltage
 * calculation has caused real product recalls.
 */

/* Function under test: saturating add for uint8_t */
static uint8_t sat_add_u8(uint8_t a, uint8_t b)
{
    uint16_t sum = (uint16_t)a + (uint16_t)b;
    return (sum > 0xFFU) ? 0xFFU : (uint8_t)sum;
}

/* Fixed-point Q8 multiply: (a * b) >> 8 */
static int16_t q8_mul(int16_t a, int16_t b)
{
    return (int16_t)(((int32_t)a * (int32_t)b) >> 8);
}

static void test_sat_add_normal(void)
{
    TEST_ASSERT_EQ(5U, sat_add_u8(2U, 3U));
    TEST_ASSERT_EQ(255U, sat_add_u8(200U, 55U));
}

static void test_sat_add_overflow_saturates(void)
{
    TEST_ASSERT_EQ(255U, sat_add_u8(200U, 100U)); /* 300 → 255 */
    TEST_ASSERT_EQ(255U, sat_add_u8(255U, 255U)); /* max + max → 255 */
    TEST_ASSERT_EQ(255U, sat_add_u8(255U, 1U));   /* just over → 255 */
}

static void test_sat_add_zero_identity(void)
{
    TEST_ASSERT_EQ(42U, sat_add_u8(42U, 0U));
    TEST_ASSERT_EQ(0U, sat_add_u8(0U, 0U));
}

static void test_q8_mul_basic(void)
{
    /* 2.0 * 3.0 in Q8 = (512 * 768) >> 8 = 6.0 = 1536 Q8 */
    int16_t two = 2 * 256;   /* 2.0 in Q8 */
    int16_t three = 3 * 256; /* 3.0 in Q8 */
    TEST_ASSERT_EQ(6 * 256, q8_mul(two, three));
}

static void test_q8_mul_negative(void)
{
    int16_t neg_one = -256; /* -1.0 in Q8 */
    int16_t pos_two = 512;  /*  2.0 in Q8 */
    TEST_ASSERT_EQ(-512, q8_mul(neg_one, pos_two));
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 7 — Simulating an ISR body in unit tests
 * ══════════════════════════════════════════════════════════════════════════
 *
 * PATTERN: Never test the ISR vector directly.
 * Instead:
 *
 *   // In uart_isr.c (production code)
 *   void USART1_IRQHandler(void) {
 *       uart_rx_isr_body(&g_uart1_state);  // ← plain C function
 *   }
 *
 *   // In uart_isr.c
 *   void uart_rx_isr_body(UartState *s) {
 *       if (USART1->SR & RXNE) { rb_push(&s->rx_buf, USART1->DR); }
 *   }
 *
 * Tests call uart_rx_isr_body() directly with a fake UartState that
 * has a pre-populated DR register — no interrupt needed.
 *
 * Below is a concrete example with a UART state machine.
 */

typedef struct
{
    uint32_t fake_SR; /* status register — set RXNE bit to simulate receipt */
    uint8_t fake_DR;  /* data register */
    RingBuf rx_buf;
    uint32_t isr_call_count;
} UartState;

#define RXNE_BIT (1U << 5)

/* ISR body — extracted from the real ISR */
static void uart_rx_isr_body(UartState *s)
{
    s->isr_call_count++;
    if (s->fake_SR & RXNE_BIT)
    {
        rb_push(&s->rx_buf, s->fake_DR);
        s->fake_SR &= ~RXNE_BIT; /* HW auto-clears on read */
    }
}

static UartState s_uart;

static void uart_setup(void)
{
    memset(&s_uart, 0, sizeof(s_uart));
    rb_init(&s_uart.rx_buf);
}

static void test_isr_body_does_nothing_when_rxne_clear(void)
{
    s_uart.fake_SR = 0U; /* RXNE not set */
    s_uart.fake_DR = 0xAAU;
    uart_rx_isr_body(&s_uart);
    TEST_ASSERT_TRUE(rb_empty(&s_uart.rx_buf));
    TEST_ASSERT_EQ(1U, s_uart.isr_call_count);
}

static void test_isr_body_pushes_byte_when_rxne_set(void)
{
    s_uart.fake_SR = RXNE_BIT;
    s_uart.fake_DR = 0x42U;
    uart_rx_isr_body(&s_uart);
    TEST_ASSERT_EQ(1U, rb_count(&s_uart.rx_buf));
    uint8_t v = 0U;
    rb_pop(&s_uart.rx_buf, &v);
    TEST_ASSERT_EQ(0x42U, v);
    /* SR RXNE bit should be cleared after ISR */
    TEST_ASSERT_EQ(0U, s_uart.fake_SR & RXNE_BIT);
}

static void test_isr_body_multiple_bytes_in_order(void)
{
    uint8_t bytes[] = {0x01U, 0x02U, 0x03U};
    for (size_t i = 0; i < sizeof(bytes); i++)
    {
        s_uart.fake_SR = RXNE_BIT;
        s_uart.fake_DR = bytes[i];
        uart_rx_isr_body(&s_uart);
    }
    TEST_ASSERT_EQ(3U, rb_count(&s_uart.rx_buf));
    for (size_t i = 0; i < sizeof(bytes); i++)
    {
        uint8_t v = 0U;
        rb_pop(&s_uart.rx_buf, &v);
        TEST_ASSERT_EQ(bytes[i], v);
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 8 — Abstracting time for testing timeout logic
 * ══════════════════════════════════════════════════════════════════════════
 *
 * Production code:
 *   uint32_t timeout_ms = HAL_GetTick() + 1000;
 *   while (HAL_GetTick() < timeout_ms) { ... }
 *
 * This blocks the test for 1 real second!
 *
 * SOLUTION: inject a tick function pointer.
 *   In production: tick_fn = HAL_GetTick
 *   In tests:      tick_fn = &s_fake_tick_counter   (increment manually)
 */
static uint32_t s_fake_tick = 0U;
static uint32_t fake_get_tick(void) { return s_fake_tick; }

typedef uint32_t (*get_tick_fn)(void);

/* Returns true if 'ms' elapsed since 'start_tick' according to tick_fn */
static bool timeout_elapsed(get_tick_fn tick_fn,
                            uint32_t start_tick, uint32_t ms)
{
    return (tick_fn() - start_tick) >= ms;
}

static void tick_setup(void) { s_fake_tick = 0U; }

static void test_timeout_not_elapsed(void)
{
    s_fake_tick = 500U;
    TEST_ASSERT_TRUE(!timeout_elapsed(fake_get_tick, 0U, 1000U));
}

static void test_timeout_exactly_elapsed(void)
{
    s_fake_tick = 1000U;
    TEST_ASSERT_TRUE(timeout_elapsed(fake_get_tick, 0U, 1000U));
}

static void test_timeout_handles_tick_rollover(void)
{
    /* SysTick rolls over at UINT32_MAX — unsigned subtraction handles it */
    uint32_t start = 0xFFFFF000U;
    s_fake_tick = 0x00000100U;              /* rolled over */
    uint32_t elapsed = s_fake_tick - start; /* = 0x1100 = 4352 */
    TEST_ASSERT_TRUE(elapsed >= 4000U);
    TEST_ASSERT_TRUE(timeout_elapsed(fake_get_tick, start, 4000U));
}

/* ══════════════════════════════════════════════════════════════════════════
 * main — run all suites
 * ══════════════════════════════════════════════════════════════════════════ */
int main(void)
{
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  Module 09 — Unit Testing for Embedded C                    ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    /* ── Suite A: GPIO driver (hardware mock) ── */
    printf("── Suite A: GPIO driver with register mock ─────────────────────\n");
    g_runner = (TestRunner){0};
    RUN_TEST_F(test_gpio_set_output_sets_moder, gpio_setup, NULL);
    RUN_TEST_F(test_gpio_write_high_sets_bsrr_low_bit, gpio_setup, NULL);
    RUN_TEST_F(test_gpio_write_low_sets_bsrr_high_bit, gpio_setup, NULL);
    RUN_TEST_F(test_gpio_set_output_only_modifies_target_pin, gpio_setup, NULL);
    print_results("GPIO driver");

    /* ── Suite B: Ring buffer ── */
    printf("\n── Suite B: Ring buffer ─────────────────────────────────────────\n");
    g_runner = (TestRunner){0};
    RUN_TEST_F(test_rb_empty_on_init, rb_setup, NULL);
    RUN_TEST_F(test_rb_push_increases_count, rb_setup, NULL);
    RUN_TEST_F(test_rb_pop_returns_correct_byte, rb_setup, NULL);
    RUN_TEST_F(test_rb_fifo_order, rb_setup, NULL);
    RUN_TEST_F(test_rb_full_rejects_push, rb_setup, NULL);
    RUN_TEST_F(test_rb_wrap_around_preserves_data, rb_setup, NULL);
    RUN_TEST_F(test_rb_pop_on_empty_returns_false, rb_setup, NULL);
    print_results("Ring buffer");

    /* ── Suite C: Frame parser FSM ── */
    printf("\n── Suite C: UART frame parser FSM ──────────────────────────────\n");
    g_runner = (TestRunner){0};
    RUN_TEST_F(test_parser_starts_idle, parser_setup, NULL);
    RUN_TEST_F(test_parser_ignores_junk_before_sof, parser_setup, NULL);
    RUN_TEST_F(test_parser_valid_frame, parser_setup, NULL);
    RUN_TEST_F(test_parser_bad_crc_goes_error, parser_setup, NULL);
    RUN_TEST_F(test_parser_bad_second_sof_byte_goes_error, parser_setup, NULL);
    print_results("UART frame parser");

    /* ── Suite D: Integer math ── */
    printf("\n── Suite D: Integer / fixed-point edge cases ───────────────────\n");
    g_runner = (TestRunner){0};
    RUN_TEST(test_sat_add_normal);
    RUN_TEST(test_sat_add_overflow_saturates);
    RUN_TEST(test_sat_add_zero_identity);
    RUN_TEST(test_q8_mul_basic);
    RUN_TEST(test_q8_mul_negative);
    print_results("Integer math");

    /* ── Suite E: UART ISR body ── */
    printf("\n── Suite E: UART ISR body simulation ───────────────────────────\n");
    g_runner = (TestRunner){0};
    RUN_TEST_F(test_isr_body_does_nothing_when_rxne_clear, uart_setup, NULL);
    RUN_TEST_F(test_isr_body_pushes_byte_when_rxne_set, uart_setup, NULL);
    RUN_TEST_F(test_isr_body_multiple_bytes_in_order, uart_setup, NULL);
    print_results("UART ISR body");

    /* ── Suite F: Tick-injected timeout ── */
    printf("\n── Suite F: Time abstraction / timeout ─────────────────────────\n");
    g_runner = (TestRunner){0};
    RUN_TEST_F(test_timeout_not_elapsed, tick_setup, NULL);
    RUN_TEST_F(test_timeout_exactly_elapsed, tick_setup, NULL);
    RUN_TEST_F(test_timeout_handles_tick_rollover, tick_setup, NULL);
    print_results("Timeout logic");

    printf("\nEXERCISES:\n\n");
    printf("  1. [SEAM] The gpio_set_output() above only tests MODER.\n");
    printf("     Add gpio_read() that returns the IDR bit. Write 2 tests:\n");
    printf("     one where the fake register has the bit set, one where clear.\n\n");
    printf("  2. [COVERAGE] Add a test that verifies parser_feed() handles\n");
    printf("     a zero-length data field (length byte = 0) correctly.\n");
    printf("     Should it go to ERROR or COMPLETE? Decide and assert it.\n\n");
    printf("  3. [OVERFLOW] Write test_sat_add_boundary() that tests every value\n");
    printf("     from 250 to 255 added to 1…10. Use a loop with TEST_ASSERT_EQ.\n\n");
    printf("  4. [TIME MOCK] Write a function wait_for_flag() that spins on a\n");
    printf("     bool *flag until timeout. Inject the tick function. Write tests\n");
    printf("     for: flag set before timeout, flag never set (timeout fires),\n");
    printf("     and flag set exactly at the timeout tick.\n\n");
    printf("  5. [UNITY] Download Unity (single header+source) and rewrite\n");
    printf("     Suite B using Unity's TEST_ASSERT_EQUAL_UINT8 macros.\n");
    printf("     Compare the failure output format — much more descriptive.\n\n");

    return 0;
}
