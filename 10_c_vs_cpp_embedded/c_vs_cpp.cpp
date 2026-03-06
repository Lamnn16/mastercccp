/**
 * ╔══════════════════════════════════════════════════════════════════════════╗
 * ║  MODULE 10 — C vs C++ in Embedded Systems                              ║
 * ║  "Choose your weapon — but know what each one costs"                   ║
 * ╚══════════════════════════════════════════════════════════════════════════╝
 *
 * Written from the perspective of a 15+ year embedded software engineer.
 *
 * The real question is NOT "which language is better".
 * The real question is: "What does each feature cost in ROM, RAM, and CPU
 * cycles — and is that trade-off worth making on this microcontroller?"
 *
 * Topics (each with C-equivalent side-by-side, assembly insights, and costs):
 *   1.  Name mangling & extern "C" — the linker bridge between C and C++
 *   2.  Namespaces          — zero cost, replaces fragile prefixes
 *   3.  Constructors/destructors — hidden costs in global objects (SFIOC)
 *   4.  RAII               — deterministic cleanup, embedded-safe
 *   5.  Templates          — compile-time generics vs C void* generics
 *   6.  constexpr          — compile-time computation vs C macros
 *   7.  virtual functions  — vtable, pointers to functions, ISR forbidden zones
 *   8.  exceptions & RTTI  — ALWAYS disabled in embedded (-fno-exceptions)
 *   9.  new / delete       — heap in embedded: when forbidden, when controlled
 *   10. Inline classes as hardware abstractions — the zero-overhead ideal
 *
 * BUILD:  cmake --build build --target p10_c_vs_cpp
 * RUN:    .\build\10_c_vs_cpp_embedded\p10_c_vs_cpp.exe
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <type_traits>   /* std::is_trivially_destructible */

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 1 — Name mangling & extern "C"
 * ══════════════════════════════════════════════════════════════════════════
 *
 * C++ "mangles" function names to encode type signatures.
 * void uart_write(const char *s, size_t n)
 * becomes something like: _Z10uart_writePKcm   (GCC ARM)
 *
 * C code calls uart_write by its original symbol name.
 * If your C++ driver is called from a C RTOS (FreeRTOS tasks are written in C),
 * the linker will fail unless you declare the C++ function with extern "C".
 *
 * RULE: Any function that must be callable from C, or that overrides a C
 *       callback (interrupt handler, FreeRTOS hook), MUST be extern "C".
 */

/* Simulated C-callable ISR handler — must use extern "C" in real projects:
 *
 *   extern "C" void USART1_IRQHandler(void) { ... }
 *
 * Without it, the startup assembler looking for USART1_IRQHandler won't
 * find _ZN4Uart14irq_handler_spEv — the linker silently uses the weak default.
 * Your ISR never fires. This is a real, hard-to-debug bug.
 */
namespace demo_mangling {
    // In a header shared with C:  extern "C" void uart_isr_hook(void);
    // In the C++ implementation:
    extern "C" void uart_isr_hook_demo(void)
    {
        printf("  [extern \"C\"] uart_isr_hook_demo() called — C-linkage preserved\n");
    }

    void cpp_only_function(int x)   /* mangled — NOT callable from C */
    {
        printf("  [mangled]   cpp_only_function(%d)\n", x);
    }
}

static void section1_name_mangling(void)
{
    printf("══ 1. Name mangling & extern \"C\" ════════════════════════════\n");
    printf("  C++ mangles function names to encode parameter types.\n");
    printf("  extern \"C\" tells the linker: use the unmangled C symbol name.\n");
    printf("  CRITICAL for: ISR handlers, FreeRTOS callbacks, HAL overrides.\n\n");
    uart_isr_hook_demo();
    demo_mangling::cpp_only_function(42);
    printf("\n  Rule: If a C++ function is registered in a C vector table or\n");
    printf("  passed as a C function pointer — declare it extern \"C\".\n\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 2 — Namespaces: zero-cost organisation
 * ══════════════════════════════════════════════════════════════════════════
 *
 * IN C:
 *   void dma_init(void);         // global — any file can collide
 *   void uart_dma_init(void);    // fragile prefix convention
 *
 * IN C++:
 *   namespace dma   { void init(void); }  // zero-cost, enforced
 *   namespace uart  { void init(void); }  // no collision possible
 *
 * Cost: ZERO bytes of ROM or RAM. Namespaces are a purely compile-time
 * mechanism. No generated code. The assembly output is identical.
 */
namespace gpio {
    inline void init(uint8_t pin) {
        printf("  gpio::init(%u)\n", pin);
    }
}
namespace spi {
    inline void init(uint8_t pin) {
        printf("  spi::init(%u) — same name, different namespace, no collision\n", pin);
    }
}

static void section2_namespaces(void)
{
    printf("══ 2. Namespaces — zero-cost name scoping ═══════════════════\n");
    printf("  In C: gpio_init(), spi_init() — convention, can conflict.\n");
    printf("  In C++: namespaces are enforced by the compiler, cost = 0.\n\n");
    gpio::init(5U);
    spi::init(3U);
    printf("\n  sizeof differences: none. Namespaces produce no ROM/RAM.\n\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 3 — Constructors / destructors: hidden costs (SFIOC)
 * ══════════════════════════════════════════════════════════════════════════
 *
 * C has no constructors. Initialization is explicit: uart_init(&my_uart);
 *
 * C++ runs constructors at startup, BEFORE main(), hidden in .init_array.
 * On STM32, this runs inside Reset_Handler after .data copy, .bss zero.
 *
 * STATIC INITIALIZATION ORDER FIASCO (SFIOC):
 *   If global object A's constructor calls global object B, and B hasn't
 *   been constructed yet — UNDEFINED BEHAVIOUR on startup. Crash before main.
 *   C has NO equivalent risk because there are no implicit constructors.
 *
 * RULE: Never have non-trivial constructors for global/static objects
 *       unless you are certain of initialization order (same TU = safe).
 *       Use singleton-with-local-static pattern to be safe.
 *
 * std::is_trivially_destructible<T>::value tells you if T is safe.
 */
struct PodGpio {     /* Trivial — safe as global in C++ */
    uint8_t port;
    uint8_t pin;
};

struct NontrivialGpio {  /* Non-trivial — risky as global */
    uint8_t port;
    uint8_t pin;
    NontrivialGpio(uint8_t p, uint8_t n) : port(p), pin(n) {
        printf("  NontrivialGpio ctor: P%c%u\n", (char)('A'+p), n);
    }
    ~NontrivialGpio() {
        printf("  NontrivialGpio dtor: P%c%u\n", (char)('A'+port), pin);
    }
};

/* SAFE: function-local static — constructed on first call, never SFIOC */
static NontrivialGpio &get_led_pin(void)
{
    static NontrivialGpio s_led(0, 5);   /* port A, pin 5 — constructed once */
    return s_led;
}

static void section3_constructors(void)
{
    printf("══ 3. Constructors / destructors & SFIOC ════════════════════\n");
    printf("  POD struct (trivially destructible): %s\n",
           std::is_trivially_destructible<PodGpio>::value ? "YES" : "NO");
    printf("  NontrivialGpio (has dtor):            %s\n\n",
           std::is_trivially_destructible<NontrivialGpio>::value ? "YES" : "NO");

    printf("  Accessing get_led_pin() for first time:\n");
    NontrivialGpio &led = get_led_pin();
    printf("  LED port=%u pin=%u\n\n", led.port, led.pin);

    printf("  SFIOC RULE: Never rely on global C++ object init order\n");
    printf("  across translation units. Use function-local statics\n");
    printf("  or explicit init() calls in main() instead.\n\n");

    printf("  C equivalent: no constructors, explicit uart_init(&ctx).\n");
    printf("  More verbose but ZERO hidden startup cost or ordering risk.\n\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 4 — RAII: deterministic cleanup (embedded-safe pattern)
 * ══════════════════════════════════════════════════════════════════════════
 *
 * RAII = constructor acquires resource, destructor releases.
 * On embedded: perfect for:
 *  - Critical sections (disable → enable IRQs)
 *  - SPI chip-select (CS low → CS high)
 *  - Mutex lock / unlock (FreeRTOS)
 *
 * In C you'd write:
 *   __disable_irq();
 *   do_work();            // if we early-return here, IRQs stay disabled!
 *   __enable_irq();
 *
 * In C++ RAII:
 *   { CriticalSection cs;  do_work(); }  // dtor re-enables unconditionally
 *
 * COST: The optimizer (O2) inlines the constructor and destructor —
 *       they become the same assembly as the C version.
 *       Zero overhead on ARM Cortex-M with -O2.
 */
class CriticalSection {
public:
    CriticalSection()  { printf("  [CriticalSection] __disable_irq()\n"); }
    ~CriticalSection() { printf("  [CriticalSection] __enable_irq()\n");  }

    /* Non-copyable — critical sections must not be copied */
    CriticalSection(const CriticalSection &) = delete;
    CriticalSection &operator=(const CriticalSection &) = delete;
};

class SpiChipSelect {
    uint8_t m_pin;
public:
    explicit SpiChipSelect(uint8_t pin) : m_pin(pin) {
        printf("  [SpiCS] CS pin %u LOW  (transfer start)\n", m_pin);
    }
    ~SpiChipSelect() {
        printf("  [SpiCS] CS pin %u HIGH (transfer end)\n",   m_pin);
    }
};

static void section4_raii(void)
{
    printf("══ 4. RAII — deterministic resource management ═══════════════\n");
    printf("  Writing 2 bytes to SPI with RAII CS control:\n");
    {
        SpiChipSelect cs(4U);           /* CS low */
        CriticalSection irq_guard;      /* IRQ disabled */
        printf("  spi_write(0xAB); spi_write(0xCD);\n");
        /* cs dtor  */
        /* irq_guard dtor — both called automatically even on early return */
    }
    printf("  Both released in reverse order. Try doing that safely in C!\n\n");

    printf("  C equivalent:\n");
    printf("    gpio_clear(4);  __disable_irq();\n");
    printf("    spi_write(0xAB);\n");
    printf("    __enable_irq();  gpio_set(4);  // MUST remember, can forget\n\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 5 — Templates: compile-time generics vs C void*
 * ══════════════════════════════════════════════════════════════════════════
 *
 * C generic containers use void* — no type safety, pointer cast everywhere.
 * C++ templates are expanded at compile time — strict typing, ZERO overhead.
 *
 * PRACTICAL COMPARISON: a type-safe ring buffer
 *
 * C version:
 *   void rb_push(RingBuf *rb, void *elem, size_t elem_size);
 *   void rb_pop (RingBuf *rb, void *out,  size_t elem_size);
 *   — caller must pass elem_size, easy to mismatch, UB lurks.
 *
 * C++ template:
 *   template<typename T, size_t N>
 *   class RingBuffer { T buf[N]; ... };
 *   — size_t N is baked in at compile time, no heap, no virtual, T is typed.
 *
 * COST: Each template instantiation IS separate code in ROM.
 *   RingBuffer<uint8_t,  8>  — one copy
 *   RingBuffer<uint32_t, 16> — separate copy
 * → Avoid over-instantiating templates on small devices.
 */
template<typename T, size_t N>
class RingBuffer {
    static_assert((N & (N-1)) == 0, "N must be a power of 2");
    T        m_buf[N];
    uint32_t m_head = 0U;
    uint32_t m_tail = 0U;
public:
    bool push(T val) {
        if (full()) return false;
        m_buf[m_head & (N-1U)] = val;
        m_head++;
        return true;
    }
    bool pop(T &out) {
        if (empty()) return false;
        out = m_buf[m_tail & (N-1U)];
        m_tail++;
        return true;
    }
    bool   empty() const { return m_head == m_tail; }
    bool   full()  const { return (m_head - m_tail) == N; }
    size_t count() const { return m_head - m_tail; }
};

static void section5_templates(void)
{
    printf("══ 5. Templates — compile-time generics ═════════════════════\n");

    RingBuffer<uint8_t,  8> byte_buf;
    RingBuffer<uint32_t, 4> word_buf;

    byte_buf.push(0xAAU); byte_buf.push(0xBBU);
    word_buf.push(0xDEADBEEFU);

    uint8_t  b = 0U; byte_buf.pop(b);
    uint32_t w = 0U; word_buf.pop(w);

    printf("  byte_buf popped: 0x%02X  (type-safe: uint8_t)\n", b);
    printf("  word_buf popped: 0x%08X (type-safe: uint32_t)\n", w);
    printf("\n  sizeof(byte_buf) = %zu bytes  (8 × uint8_t + 2 × uint32_t)\n",
           sizeof(byte_buf));
    printf("  sizeof(word_buf) = %zu bytes  (4 × uint32_t + 2 × uint32_t)\n\n",
           sizeof(word_buf));
    printf("  C equivalent: void* + elem_size — compiles to same size buffer\n");
    printf("  but loses all type safety and requires casteverywhere.\n\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 6 — constexpr: compile-time computation vs C macros
 * ══════════════════════════════════════════════════════════════════════════
 *
 * C macros:
 *   #define BAUD_DIV(clk, baud) ((clk) / (baud))
 *   No type safety. Expands everywhere. Cannot be debugged.
 *
 * C++ constexpr:
 *   constexpr uint32_t baud_div(uint32_t clk, uint32_t baud) { return clk/baud; }
 *   Fully typed. Evaluated at compile time. Debuggable. Appears in symbols.
 *
 * COST: ZERO — constexpr values are constants baked into the binary.
 * On arm-none-eabi-objdump you see the literal value, not a call.
 */
constexpr uint32_t SYSCLK_HZ       = 100'000'000U;
constexpr uint32_t baud_div(uint32_t clk, uint32_t baud) { return clk / baud; }
constexpr uint32_t USART_BRR_9600  = baud_div(SYSCLK_HZ, 9600U);
constexpr uint32_t USART_BRR_115200= baud_div(SYSCLK_HZ, 115200U);
constexpr size_t   kib_to_bytes(size_t k) { return k * 1024U; }
constexpr size_t   SRAM_SIZE_BYTES = kib_to_bytes(128U);

static void section6_constexpr(void)
{
    printf("══ 6. constexpr — compile-time computation ══════════════════\n");
    printf("  SYSCLK    = %u Hz\n",   SYSCLK_HZ);
    printf("  BRR 9600  = %u  (= 100MHz/9600 — computed at compile time)\n",
           USART_BRR_9600);
    printf("  BRR 115200= %u\n",  USART_BRR_115200);
    printf("  SRAM      = %zu bytes  (%zu KiB)\n\n",
           SRAM_SIZE_BYTES, SRAM_SIZE_BYTES / 1024U);

    /* C equivalent: #define BAUD_DIV(c,b) ((c)/(b))  — no type check */
    printf("  C equivalent: #define BAUD_DIV(c,b) ((c)/(b))\n");
    printf("  Risk: BAUD_DIV(clk, 0) — division by zero, no compile error.\n");
    printf("  constexpr catches this at compile time: 'not a constant expr'.\n\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 7 — virtual functions: vtable, cost, and ISR forbidden zones
 * ══════════════════════════════════════════════════════════════════════════
 *
 * virtual functions implement runtime polymorphism via a vtable:
 *   Every object gets a hidden vptr (4 bytes on 32-bit) pointing to its vtable.
 *   A virtual call is: load vptr, load fn-ptr from table, indirect call.
 *   On Cortex-M4 this is ~3 extra instructions vs a direct call.
 *
 * WHEN TO USE:
 *   ✓ HAL abstraction (UartBase, SpiBase) — one virtual call per transfer, fine
 *   ✓ Plugin / polymorphic driver tables
 *
 * WHEN NOT TO USE:
 *   ✗ Inside ISRs — predictability matters; indirect branch can delay ISR
 *   ✗ In tight DSP loops — direct call every sample is faster
 *   ✗ Safety-critical code — some DO-178C / IEC 61508 guidelines forbid RTTI
 *   ✗ On Cortex-M0 with tiny ROM and every byte counts
 *
 * C equivalent: function pointer table (vtable by hand)
 *   struct UartOps { void (*write)(uint8_t *d, size_t n); };
 *   — explicit, same cost, less syntax sugar, what C++ generates for you.
 */
class SensorBase {
public:
    virtual ~SensorBase() = default;
    virtual float read_temperature() = 0;  /* pure virtual */
    virtual const char *name() const = 0;
};

class Bmp280Sensor : public SensorBase {
public:
    float read_temperature() override { return 25.08f; }
    const char *name() const override { return "BMP280"; }
};

class MockSensor : public SensorBase {
    float m_val;
public:
    explicit MockSensor(float v) : m_val(v) {}
    float read_temperature() override { return m_val; }
    const char *name() const override { return "MOCK"; }
};

static void print_sensor(SensorBase &s)
{
    printf("  %-10s  temp = %.2f°C   vptr cost = %zu bytes/object\n",
           s.name(), (double)s.read_temperature(), sizeof(void*));
}

static void section7_virtual_functions(void)
{
    printf("══ 7. virtual functions — vtable cost & ISR rules ═══════════\n");

    Bmp280Sensor bmp;
    MockSensor   mock(99.0f);

    printf("  sizeof(Bmp280Sensor)       = %zu bytes  (data + vptr)\n", sizeof(bmp));
    printf("  sizeof(MockSensor)         = %zu bytes\n\n", sizeof(mock));

    print_sensor(bmp);
    print_sensor(mock);

    printf("\n  C equivalent vtable (function pointer struct):\n");
    printf("    struct SensorOps { float (*read_temp)(void *ctx); };\n");
    printf("    — identical assembly cost as C++ virtual, but manual.\n\n");

    printf("  ISR RULE: No virtual calls inside interrupt handlers.\n");
    printf("  DSP RULE: No virtual calls inside inner DSP/control loops.\n\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 8 — Exceptions & RTTI: ALWAYS disabled in embedded
 * ══════════════════════════════════════════════════════════════════════════
 *
 * Exceptions (-fno-exceptions must be passed):
 *   throw/catch generates a large exception-handling table (.ARM.extab)
 *   that can add 10-50% to firmware ROM. Unacceptable on 64KB Flash.
 *   Stack unwinding code pulls in ~20KB of libgcc. Non-deterministic timing.
 *
 * RTTI (-fno-rtti must be passed):
 *   dynamic_cast<> and typeid() require type-info tables in ROM.
 *   Typically +10-30 bytes per polymorphic class. Adds up fast.
 *
 * Flags added in root CMakeLists:
 *   add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
 *
 * REPLACEMENT for exceptions in embedded:
 *   Return status codes (status_t from embedded_types.h)
 *   std::optional<T> (C++17, header only, zero overhead with optimization)
 *   Panic/assert macros for truly unrecoverable errors
 */
enum class Status : int8_t {
    OK        =  0,
    ERR_BUSY  = -1,
    ERR_TIMEOUT = -2,
};

/* C++ without exceptions — error via return code (same as C) */
static Status uart_write_noexcept(const uint8_t *data, size_t len, bool fifo_full)
{
    if (fifo_full) return Status::ERR_BUSY;
    printf("  uart_write: %zu bytes sent\n", len);
    (void)data;
    return Status::OK;
}

static void section8_exceptions_rtti(void)
{
    printf("══ 8. Exceptions & RTTI — disabled in embedded ══════════════\n");
    printf("  Flags: -fno-exceptions  -fno-rtti\n");
    printf("  Without them: exception tables add 10-50%% to firmware size.\n\n");

    uint8_t buf[] = {0x01U, 0x02U};
    Status s1 = uart_write_noexcept(buf, sizeof(buf), false);
    Status s2 = uart_write_noexcept(buf, sizeof(buf), true);
    printf("  Status OK:   %s\n", (s1 == Status::OK)       ? "OK"       : "FAIL");
    printf("  Status BUSY: %s\n", (s2 == Status::ERR_BUSY) ? "ERR_BUSY" : "FAIL");
    printf("\n  NO exceptions used — error handled via return code (C style).\n\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 9 — new / delete: heap in embedded
 * ══════════════════════════════════════════════════════════════════════════
 *
 * NEVER use new/delete in:
 *   • Interrupt service routines (malloc is not reentrant)
 *   • Deeply embedded bare-metal with no heap (linker heap = 0)
 *   • Safety-critical code (MISRA C++ Rule 18-4-1: no dynamic memory)
 *   • Any context where fragmentation causes non-determinism
 *
 * SOMETIMES acceptable:
 *   • Linux-based embedded (Raspberry Pi, i.MX)
 *   • Startup-only allocation (allocate once in main, never free)
 *   • Placement new — construct object in PRE-ALLOCATED buffer (safe!)
 *
 * PLACEMENT NEW — the embedded-safe compromise:
 *   uint8_t buf[sizeof(MyDriver)] __attribute__((aligned(4)));
 *   MyDriver *d = new (buf) MyDriver(args);   // no heap involved
 *   d->~MyDriver();                           // explicit destructor call
 *
 * This is exactly what FreeRTOS does internally for task stacks.
 */
class UartDriver {
public:
    uint32_t baud;
    explicit UartDriver(uint32_t b) : baud(b) {
        printf("  UartDriver::ctor baud=%u\n", baud);
    }
    ~UartDriver() {
        printf("  UartDriver::dtor baud=%u\n", baud);
    }
};

static void section9_new_delete(void)
{
    printf("══ 9. new / delete — placement new for bare-metal ══════════\n");
    printf("  RULE: No new/delete in ISRs, no free() in RTOS tasks.\n\n");

    /* ── Placement new in a static buffer — ZERO heap ── */
    alignas(UartDriver) static uint8_t s_uart_buf[sizeof(UartDriver)];
    UartDriver *uart = new (s_uart_buf) UartDriver(115200U);
    printf("  Object lives at: %p  (inside static array, no heap)\n",
           static_cast<void*>(uart));
    printf("  Baud rate: %u\n", uart->baud);
    uart->~UartDriver();   /* explicit dtor call — required for placement new */

    printf("\n  Buffer reuse after dtor:\n");
    UartDriver *uart2 = new (s_uart_buf) UartDriver(9600U);
    uart2->~UartDriver();
    printf("\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * SECTION 10 — Zero-overhead inline class: practical register abstraction
 * ══════════════════════════════════════════════════════════════════════════
 *
 * The ideal: write C++ that compiles to IDENTICAL assembly as hand-written C.
 *
 * C version:
 *   #define GPIOA_BASE 0x40020000
 *   #define GPIOA_BSRR ((volatile uint32_t *)(GPIOA_BASE + 0x18))
 *   *GPIOA_BSRR = (1U << 5);  // set PA5
 *
 * C++ version (should produce identical .text):
 *   GpioReg gpioa{0x40020000};
 *   gpioa.set(5);
 *
 * Verify with: arm-none-eabi-objdump -d firmware.elf | grep -A5 "set"
 * You should see a single STR instruction — zero function call overhead.
 */
struct GpioReg {
    uintptr_t base;

    void set   (uint8_t pin) const {
        volatile uint32_t *bsrr =
            reinterpret_cast<volatile uint32_t *>(base + 0x18U);
        *bsrr = (1U << pin);
    }
    void clear (uint8_t pin) const {
        volatile uint32_t *bsrr =
            reinterpret_cast<volatile uint32_t *>(base + 0x18U);
        *bsrr = (1U << (pin + 16U));
    }
    void toggle(uint8_t pin) const {
        volatile uint32_t *odr =
            reinterpret_cast<volatile uint32_t *>(base + 0x14U);
        *odr ^= (1U << pin);
    }
};

static void section10_zero_overhead_abstraction(void)
{
    printf("══ 10. Zero-overhead class — same assembly as C macros ══════\n");
    printf("  C:   *((volatile uint32_t *)(0x40020018)) = (1 << 5);\n");
    printf("  C++: GpioReg gpioa{0x40020000}; gpioa.set(5);\n\n");
    printf("  With -O2/-Os the compiler inlines everything.\n");
    printf("  The generated assembly is a single STR instruction each.\n");
    printf("  sizeof(GpioReg) = %zu bytes  (just the base address)\n\n",
           sizeof(GpioReg));
    printf("  This is the C++ Abstraction Principle:\n");
    printf("  'What you don't use, you don't pay for. What you do use,\n");
    printf("   you couldn't hand-code any better.'  — Bjarne Stroustrup\n\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * QUICK REFERENCE CHEAT SHEET
 * ══════════════════════════════════════════════════════════════════════════ */
static void print_cheatsheet(void)
{
    printf("── CHEAT SHEET: C vs C++ in Embedded ───────────────────────────\n");
    printf(" Feature             C approach            C++ approach          Cost\n");
    printf(" ─────────────────────────────────────────────────────────────────────\n");
    printf(" Scoping             prefixes (gpio_)      namespaces            ZERO\n");
    printf(" Generic code        void* + cast          templates             ZERO†\n");
    printf(" Const expressions   #define macros        constexpr             ZERO\n");
    printf(" Init/cleanup        explicit init/deinit  constructors/RAII     ZERO§\n");
    printf(" Polymorphism        fn-ptr struct (manual)virtual functions     +4B vptr\n");
    printf(" Error handling      return codes          return codes (noexcept)ZERO\n");
    printf(" Hardware mapping    macros + cast         inline class + cast   ZERO\n");
    printf(" Exceptions          N/A                   DISABLED (-fno-*)     N/A\n");
    printf(" RTTI                N/A                   DISABLED (-fno-rtti)  N/A\n");
    printf(" Heap allocation     malloc/free           placement new         ZERO¶\n");
    printf("\n");
    printf(" † each template instantiation is separate code — avoid explosion\n");
    printf(" § SFIOC risk with non-trivial global constructors — see §3\n");
    printf(" ¶ regular new/delete → avoid in ISRs and safety-critical code\n\n");
}

int main(void)
{
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║  Module 10 — C vs C++ in Embedded Systems                  ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    section1_name_mangling();
    section2_namespaces();
    section3_constructors();
    section4_raii();
    section5_templates();
    section6_constexpr();
    section7_virtual_functions();
    section8_exceptions_rtti();
    section9_new_delete();
    section10_zero_overhead_abstraction();
    print_cheatsheet();

    printf("══════════════════════════════════════════════════════════════\n");
    printf("EXERCISES:\n\n");
    printf("  1. [MANGLING] Write a C file that declares 'extern void my_func(int);'\n");
    printf("     and calls it. Write a C++ file implementing my_func. Does it\n");
    printf("     link without extern \"C\"? Add it and confirm it links.\n\n");
    printf("  2. [TEMPLATES] Instantiate RingBuffer<SensorReading, 16> where\n");
    printf("     SensorReading is a struct {float temp; uint32_t ts;}.\n");
    printf("     Compare sizeof against the C version using void* + elem_size.\n\n");
    printf("  3. [RAII] Write SpiTransaction that takes a SpiChipSelect + a\n");
    printf("     CriticalSection as members. Verify the nesting order matches\n");
    printf("     LIFO (last acquired = first released) by reading dtor output.\n\n");
    printf("  4. [VIRTUAL COST] Create a base Sensor class with one virtual method.\n");
    printf("     Compare sizeof(ConcreteA) vs a C struct with a function-pointer\n");
    printf("     field. They should be equal. Verify with printf.\n\n");
    printf("  5. [PLACEMENT NEW] Implement a static pool allocator:\n");
    printf("     template<typename T, size_t N> class Pool {\n");
    printf("         alignas(T) uint8_t m_storage[sizeof(T)*N];\n");
    printf("         ...\n");
    printf("     };\n");
    printf("     with allocate(Args...) using placement new and free() that\n");
    printf("     calls the destructor. No heap involved.\n\n");

    return 0;
}
