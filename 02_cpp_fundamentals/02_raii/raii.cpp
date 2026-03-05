/**
 * @file raii.cpp
 * @brief Phase 2 — RAII: The MOST Important C++ Concept for Embedded
 *
 * RAII = Resource Acquisition Is Initialization
 *
 * The idea: tie the lifetime of a resource to the lifetime of an object.
 * When the object is created  → acquire the resource (enable clock, take mutex)
 * When the object is destroyed → release the resource (disable clock, give mutex)
 *
 * WHY THIS MATTERS FOR EMBEDDED:
 *   - Critical sections (disable/re-enable interrupts) are ALWAYS paired
 *   - Peripheral clocks are ALWAYS disabled when driver is released
 *   - Chip-select pins are ALWAYS deasserted after SPI transactions
 *   - Mutexes are ALWAYS released, even if an early return happens
 *
 * Without RAII: you forget to re-enable interrupts → system freezes
 * With RAII: impossible to forget → the destructor handles it
 *
 * BUILD:  cmake --build build --target p2_raii
 * RUN:    .\build\02_cpp_fundamentals\p2_raii.exe
 */

#include "embedded_types.h"
#include <cstdio>

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 1: CRITICAL SECTION — RAII interrupt guard
 *
 * Problem in C:
 *   void process_data(void) {
 *       __disable_irq();
 *       if (condition) { __enable_irq(); return; }  // must remember!
 *       do_work();
 *       __enable_irq();   // what if do_work() throws / early-returns?
 *   }
 *
 * Solution in C++: CriticalSection object
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Simulated interrupt state (on real MCU: read/write PRIMASK register) */
static bool g_irq_enabled = true;

static void sim_disable_irq(void) { g_irq_enabled = false; printf("    [HW] IRQ DISABLED\n"); }
static void sim_enable_irq(void)  { g_irq_enabled = true;  printf("    [HW] IRQ ENABLED\n");  }

class CriticalSection {
public:
    CriticalSection()  { sim_disable_irq(); }   /* constructor: disable IRQ */
    ~CriticalSection() { sim_enable_irq();  }   /* destructor:  always re-enable */

    /* ── Rule of Five ──────────────────────────────────────────────────── */
    /*
     * For RAII types: prevent copying and moving.
     * If you copy a CriticalSection, who owns the re-enable? It's undefined.
     * Delete copy/move so the compiler catches misuse at compile time.
     */
    CriticalSection(const CriticalSection&)            = delete;
    CriticalSection& operator=(const CriticalSection&) = delete;
    CriticalSection(CriticalSection&&)                 = delete;
    CriticalSection& operator=(CriticalSection&&)      = delete;
};

static void update_shared_data(bool early_exit)
{
    printf("  Entering update_shared_data(early_exit=%s):\n",
           early_exit ? "true" : "false");

    CriticalSection guard;    /* interrupts disabled HERE */

    if (early_exit) {
        printf("    Early return — guard destructor fires automatically!\n");
        return;   /* guard.~CriticalSection() called HERE → IRQ re-enabled */
    }

    printf("    Doing critical work...\n");
    /* guard.~CriticalSection() called at end of scope → IRQ re-enabled */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 2: CHIP SELECT GUARD — SPI transactions
 *
 * SPI protocol: assert CS low before transaction, deassert after.
 * Forgetting to deassert CS leaves the device in a bad state.
 * ═══════════════════════════════════════════════════════════════════════════ */

class SpiChipSelect {
public:
    /*
     * Constructor takes a pin number and asserts CS (drives low).
     * 'explicit' prevents accidental implicit conversions like:
     *   SpiChipSelect cs = 5;  // ambiguous? not allowed with explicit
     */
    explicit SpiChipSelect(uint8_t pin) : pin_(pin)
    {
        printf("    [SPI] CS pin %u LOW (transaction start)\n", pin_);
        /* Real: HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET); */
    }

    ~SpiChipSelect()
    {
        printf("    [SPI] CS pin %u HIGH (transaction end)\n", pin_);
        /* Real: HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET); */
    }

    SpiChipSelect(const SpiChipSelect&)            = delete;
    SpiChipSelect& operator=(const SpiChipSelect&) = delete;

private:
    uint8_t pin_;
};

static uint8_t spi_read_register(uint8_t cs_pin, uint8_t reg_addr)
{
    SpiChipSelect cs(cs_pin);         /* CS low */
    printf("    [SPI] sending reg addr 0x%02X, receiving data\n", reg_addr);
    uint8_t data = 0x42U;             /* simulated read */
    /* cs destructor fires here → CS high, even on early return */
    return data;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 3: SCOPED CLOCK ENABLE — peripheral power gating
 *
 * STM32 peripherals must have their RCC clock enabled before use.
 * Forgetting to enable → HardFault. Forgetting to disable → wastes power.
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint32_t g_rcc_ahb1en = 0U;   /* simulated RCC->AHB1ENR */

class PeripheralClock {
public:
    /* bit_mask: which bit in RCC_AHB1ENR (e.g., bit 0 = GPIOA, bit 1 = GPIOB) */
    explicit PeripheralClock(uint32_t bit_mask, uint32_t *rcc_reg)
        : mask_(bit_mask), rcc_(rcc_reg)
    {
        *rcc_ |= mask_;
        printf("    [RCC] clock enabled  (RCC_AHB1ENR=0x%08X)\n", *rcc_);
    }

    ~PeripheralClock()
    {
        *rcc_ &= ~mask_;
        printf("    [RCC] clock disabled (RCC_AHB1ENR=0x%08X)\n", *rcc_);
    }

    PeripheralClock(const PeripheralClock&)            = delete;
    PeripheralClock& operator=(const PeripheralClock&) = delete;

private:
    uint32_t  mask_;
    uint32_t *rcc_;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 4: UNIQUE OWNERSHIP — stack-allocated buffer manager
 *
 * On MCU we avoid dynamic memory (heap). But we still need "unique owner"
 * semantics. Example: DMA transfer buffer — only ONE driver can own it.
 *
 * This shows the CONCEPT behind std::unique_ptr (without dynamic allocation).
 * ═══════════════════════════════════════════════════════════════════════════ */

template <size_t N>
class OwnedBuffer {
public:
    OwnedBuffer() : valid_(true)
    {
        static_assert(N > 0, "Buffer size must be > 0");
        for (size_t i = 0; i < N; i++) buf_[i] = 0U;
    }

    uint8_t *get()       { return valid_ ? buf_ : nullptr; }
    size_t   size() const { return N; }

    /* Move constructor: transfer ownership */
    OwnedBuffer(OwnedBuffer &&other) noexcept
        : valid_(other.valid_)
    {
        for (size_t i = 0; i < N; i++) buf_[i] = other.buf_[i];
        other.valid_ = false;   /* other no longer owns the buffer */
    }

    /* No copies */
    OwnedBuffer(const OwnedBuffer&)            = delete;
    OwnedBuffer& operator=(const OwnedBuffer&) = delete;

private:
    uint8_t buf_[N];
    bool    valid_;
};

/* ─── DEMO ────────────────────────────────────────────────────────────── */

static void demo_critical_section(void)
{
    printf("── RAII Critical Section ──\n");
    update_shared_data(false);
    printf("  State: IRQ enabled = %s\n\n", g_irq_enabled ? "YES (correct)" : "NO (bug!)");

    update_shared_data(true);
    printf("  State: IRQ enabled = %s\n\n", g_irq_enabled ? "YES (correct)" : "NO (bug!)");
}

static void demo_spi_cs(void)
{
    printf("── RAII SPI Chip Select ──\n");
    uint8_t val = spi_read_register(4, 0x0DU);   /* CS=pin4, WHO_AM_I reg */
    printf("  read value: 0x%02X\n\n", val);
}

static void demo_clock(void)
{
    printf("── RAII Peripheral Clock ──\n");
    {
        PeripheralClock gpioa_clk(1U << 0U, &g_rcc_ahb1en);  /* GPIOA = bit 0 */
        printf("    Using GPIOA...\n");
    }
    printf("  After scope: AHB1ENR = 0x%08X (should be 0)\n\n", g_rcc_ahb1en);
}

static void demo_owned_buffer(void)
{
    printf("── RAII Owned Buffer ──\n");
    OwnedBuffer<32> tx_buf;
    if (tx_buf.get()) {
        tx_buf.get()[0] = 0xAA;
        tx_buf.get()[1] = 0x55;
        printf("  buf[0]=0x%02X  buf[1]=0x%02X  size=%zu\n",
               tx_buf.get()[0], tx_buf.get()[1], tx_buf.size());
    }

    /* Move: rx_buf now owns the data, tx_buf is invalid */
    OwnedBuffer<32> rx_buf = static_cast<OwnedBuffer<32>&&>(tx_buf);
    printf("  After move: tx_buf.get()=%s  rx_buf.get()=%s\n\n",
           tx_buf.get() ? "valid(bug!)" : "null(correct)",
           rx_buf.get() ? "valid(correct)" : "null(bug!)");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * EXERCISES
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * EXERCISE 1:
 *   Write a class MutexLock that wraps a simulated RTOS mutex.
 *   The mutex is just a bool. Constructor: while(!mutex) {} (spin).
 *   Destructor: mutex = true.
 *   Test with two nested scopes — show the inner scope waits.
 *
 * EXERCISE 2:
 *   Write a class UartTransaction that:
 *     - Constructor: sets baud rate, enables clock, configures pins
 *     - Destructor: disables UART clock, resets pins to input
 *   Use simulated register variables.
 *
 * EXERCISE 3:
 *   Why is this dangerous?
 *     void send_data(void) {
 *         CriticalSection *cs = new CriticalSection();
 *         do_work();
 *         delete cs;  // what if do_work() returns early?
 *     }
 *   Fix it without using the heap.
 */

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 2 — Lesson 2: RAII                   ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    demo_critical_section();
    demo_spi_cs();
    demo_clock();
    demo_owned_buffer();
    return 0;
}
