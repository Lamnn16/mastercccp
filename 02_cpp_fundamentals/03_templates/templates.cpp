/**
 * @file templates.cpp
 * @brief Phase 2 — Templates: Type-Safe Generic Programming for Embedded
 *
 * Templates are a compile-time code generation mechanism. They produce
 * the SAME machine code as hand-written specialized functions, but you
 * write it once and the compiler generates all variants.
 *
 * Key uses in embedded firmware:
 *   1. Type-safe ring buffers (RingBuffer<uint8_t, 64>)
 *   2. Type-safe register access (Reg<uint32_t, 0x40020000>)
 *   3. Generic algorithms that work on any numeric sensor type
 *   4. Static (compile-time) polymorphism via policy classes
 *
 * EMBEDDED RULE: Templates never cause heap allocation by themselves.
 *                All sizes are compile-time constants → zero overhead.
 *
 * BUILD:  cmake --build build --target p2_templates
 * RUN:    .\build\02_cpp_fundamentals\p2_templates.exe
 */

#include "embedded_types.h"
#include <cstdio>
#include <cstring>   /* memcpy */
#include <array>     /* std::array — stack-only, zero-overhead wrapper */

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 1: FUNCTION TEMPLATES
 *
 * Write once, works for uint8_t, uint16_t, float, etc.
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Template syntax:  template <typename T>
 * T is replaced by the actual type at compile time.
 * The compiler generates one copy of the function per unique type used.
 */
template <typename T>
T clamp(T value, T lo, T hi)
{
    /*
     * No < / > ambiguity issues with typed T.
     * Compare: the C macro  #define CLAMP(v,lo,hi) ...  has double-eval bugs.
     */
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

template <typename T>
void swap(T &a, T &b)
{
    T tmp = a;
    a = b;
    b = tmp;
}

/* ── Running average (useful for sensor smoothing on ADC data) ──────────── */
template <typename T, size_t N>
class RunningAverage {
public:
    static_assert(N > 0 && N <= 256, "Window size must be 1..256");

    RunningAverage() : head_(0), count_(0), sum_(0) {}

    void add(T value)
    {
        if (count_ == N) {
            sum_ -= buf_[head_];   /* remove oldest */
        } else {
            count_++;
        }
        buf_[head_] = value;
        sum_ += value;
        head_ = (head_ + 1U) % N;
    }

    T average() const
    {
        if (count_ == 0) return T(0);
        return static_cast<T>(sum_ / static_cast<T>(count_));
    }

    size_t count() const { return count_; }

private:
    T      buf_[N];
    size_t head_;
    size_t count_;
    T      sum_;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 2: CLASS TEMPLATE — Type-safe Ring Buffer
 *
 * A ring buffer (circular buffer) is fundamental in embedded firmware:
 *   - UART RX/TX buffers
 *   - ADC sample queues
 *   - CAN frame FIFOs
 *   - Log message queues
 *
 * Template parameters:
 *   T     = element type  (uint8_t for UART, uint32_t for ADC)
 *   N     = capacity      (compile-time constant — no heap allocation)
 *
 * This is a power-of-2 ring buffer for fast modulo via bitmask.
 * ═══════════════════════════════════════════════════════════════════════════ */
template <typename T, size_t N>
class RingBuffer {
    /* Enforce power-of-2 at compile time — enables fast modulo */
    static_assert((N & (N - 1)) == 0, "RingBuffer size must be a power of 2");
    static_assert(N >= 2,             "RingBuffer size must be >= 2");

    static constexpr size_t MASK = N - 1U;

public:
    RingBuffer() : head_(0), tail_(0) {}

    bool push(const T &item)
    {
        if (full()) return false;
        buf_[head_ & MASK] = item;
        head_++;
        return true;
    }

    bool pop(T &out)
    {
        if (empty()) return false;
        out = buf_[tail_ & MASK];
        tail_++;
        return true;
    }

    bool    empty() const { return head_ == tail_; }
    bool    full()  const { return (head_ - tail_) == N; }
    size_t  size()  const { return head_ - tail_; }
    size_t  capacity() const { return N; }

private:
    T      buf_[N];
    size_t head_;   /* write index (monotonically increasing) */
    size_t tail_;   /* read  index (monotonically increasing) */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 3: POLICY-BASED DESIGN (static polymorphism, zero overhead)
 *
 * Instead of virtual functions (which require vtable + heap in some cases),
 * use templates to select behavior at compile time.
 *
 * Example: UART driver that works with DMA *or* interrupt mode,
 * selected at compile time by the policy class.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Policy: interrupt-mode UART (simulated) */
struct InterruptMode {
    static void start_tx(const uint8_t *data, size_t len)
    {
        printf("    [IRQ mode] TX %zu bytes: ", len);
        for (size_t i = 0; i < len; i++) printf("%02X ", data[i]);
        printf("\n");
    }
};

/* Policy: DMA-mode UART (simulated) */
struct DmaMode {
    static void start_tx(const uint8_t *data, size_t len)
    {
        printf("    [DMA mode] TX %zu bytes (DMA channel loaded)\n", len);
        (void)data;
    }
};

template <typename TxPolicy>
class Uart {
public:
    void send(const uint8_t *buf, size_t len)
    {
        TxPolicy::start_tx(buf, len);   /* resolved at compile time — no vtable */
    }
};

/* ─── DEMOS ──────────────────────────────────────────────────────────── */

static void demo_function_templates(void)
{
    printf("── Function templates ──\n");

    int16_t adc_raw = clamp<int16_t>(-2000, -1000, 1000);
    printf("  clamp(-2000, -1000, 1000) = %d\n", adc_raw);

    float duty = clamp(1.5f, 0.0f, 1.0f);
    printf("  clamp(1.5f, 0, 1) = %.2f\n", duty);

    uint32_t a = 100, b = 200;
    swap(a, b);
    printf("  after swap: a=%u b=%u\n\n", (unsigned)a, (unsigned)b);
}

static void demo_running_average(void)
{
    printf("── Running average (window=4) ──\n");

    RunningAverage<int16_t, 4> avg;
    int16_t values[] = { 100, 200, 300, 400, 500, 150 };

    for (size_t i = 0; i < ARRAY_SIZE(values); i++) {
        avg.add(values[i]);
        printf("  add(%4d) → avg=%d  (n=%zu)\n",
               values[i], (int)avg.average(), avg.count());
    }
    printf("\n");
}

static void demo_ring_buffer(void)
{
    printf("── RingBuffer<uint8_t, 8> ──\n");

    RingBuffer<uint8_t, 8> rx_buf;

    /* Simulate UART ISR pushing bytes */
    uint8_t incoming[] = { 0xAA, 0x01, 0x0C, 0x00, 0xFF };
    for (size_t i = 0; i < ARRAY_SIZE(incoming); i++) {
        rx_buf.push(incoming[i]);
    }
    printf("  size=%zu / capacity=%zu\n", rx_buf.size(), rx_buf.capacity());

    /* Main loop draining the buffer */
    uint8_t byte;
    printf("  draining: ");
    while (rx_buf.pop(byte)) { printf("0x%02X ", byte); }
    printf("\n  empty=%s\n\n", rx_buf.empty() ? "true" : "false");
}

static void demo_policy_design(void)
{
    printf("── Policy-based UART (compile-time selection) ──\n");

    Uart<InterruptMode> uart_irq;
    Uart<DmaMode>       uart_dma;

    uint8_t frame[] = { 0x01, 0x10, 0xAB, 0xCD };
    uart_irq.send(frame, sizeof(frame));
    uart_dma.send(frame, sizeof(frame));
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * EXERCISES
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * EXERCISE 1:
 *   Add a peek() method to RingBuffer that reads the oldest element
 *   WITHOUT removing it. Useful for framing: peek at length field first.
 *
 * EXERCISE 2:
 *   Create a StaticQueue<T, N> (FIFO) that is interrupt-safe:
 *   push() disables IRQ, inserts item, re-enables IRQ.
 *   pop() does the same. Use your CriticalSection from the RAII lesson.
 *
 * EXERCISE 3:
 *   Write a template function map_value<T>(T val, T in_lo, T in_hi,
 *                                          T out_lo, T out_hi)
 *   that maps a value from one range to another (like Arduino map()).
 *   Handle both integer and float types. Verify with: map(512, 0, 1023, 0, 100).
 */

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 2 — Lesson 3: Templates              ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    demo_function_templates();
    demo_running_average();
    demo_ring_buffer();
    demo_policy_design();
    return 0;
}
