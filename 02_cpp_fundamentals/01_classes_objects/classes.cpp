/**
 * @file classes.cpp
 * @brief Phase 2 — C++ Classes: Your First Step from C to C++
 *
 * You know C structs well. A C++ class is a struct with:
 *   1. Member functions (methods) — no need to pass pointer to struct explicitly
 *   2. Access control (private/public/protected)
 *   3. Constructors and destructors — automatic initialization/cleanup
 *   4. Operator overloading — make types behave naturally
 *
 * C vs C++ comparison for embedded:
 *   C:   gpio_set_mode(&gpioa, 5, GPIO_MODE_OUTPUT);
 *   C++: gpioa.set_mode(5, GpioMode::Output);
 *
 * The generated machine code is IDENTICAL. C++ classes have zero overhead
 * over C structs when used correctly.
 *
 * BUILD:  cmake --build build --target p2_classes_objects
 * RUN:    .\build\02_cpp_fundamentals\p2_classes_objects.exe
 */

#include "embedded_types.h"
#include <cstdio>     // C++ version of stdio.h — same functions, cleaner include

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 1: FROM C STRUCT TO C++ CLASS — side by side
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── The C way (you already know this) ──────────────────────────────────── */
typedef struct {
    volatile uint32_t *base_addr;
} C_Gpio;

static void C_gpio_set_mode(C_Gpio *self, uint8_t pin, uint8_t mode)
{
    /* explicit "self" pointer, messy call site */
    uint32_t reg = *self->base_addr;
    reg = (reg & ~(3UL << (pin * 2U))) | ((uint32_t)mode << (pin * 2U));
    *self->base_addr = reg;
}

/* ── The C++ way ────────────────────────────────────────────────────────── */
/*
 * TERMINOLOGY:
 *   class vs struct: In C++, both are identical except default access level.
 *     struct members default to PUBLIC  (same as C)
 *     class  members default to PRIVATE (safer for hardware drivers)
 *
 * Use 'class' when you want to control access. Use 'struct' for plain data.
 */
class Gpio {
public:
    /* ── Scoped enum (C++11) — better than C enums ─────────────────────── */
    /*
     * enum class enforces scope: you MUST write GpioMode::Output
     * This prevents naming collisions (common problem in large embedded projects)
     */
    enum class Mode : uint8_t {
        Input  = 0,
        Output = 1,
        AF     = 2,
        Analog = 3,
    };

    enum class Speed : uint8_t {
        Low    = 0,
        Medium = 1,
        High   = 2,
        VHigh  = 3,
    };

    /* ── Constructor ────────────────────────────────────────────────────── */
    /*
     * Constructor runs automatically when the object is created.
     * The : base_addr_(addr) syntax is an INITIALIZER LIST — preferred over
     * assignment in the body because it initializes directly.
     *
     * For const and reference members, initializer list is THE ONLY WAY.
     */
    explicit Gpio(volatile uint32_t *addr) : base_addr_(addr), moder_(0U)
    {
        /* Constructor body — runs after initializer list */
        printf("  [Gpio] constructed at address %p\n", (void*)addr);
    }

    /* ── Member functions ───────────────────────────────────────────────── */
    void set_mode(uint8_t pin, Mode mode)
    {
        moder_ = (moder_ & ~(3UL << (pin * 2U)))
               | (static_cast<uint32_t>(mode) << (pin * 2U));
        if (base_addr_) { *base_addr_ = moder_; }
    }

    Mode get_mode(uint8_t pin) const   /* 'const' = does not modify object */
    {
        return static_cast<Mode>((moder_ >> (pin * 2U)) & 3U);
    }

    void set_pin(uint8_t pin)
    {
        if (base_addr_) {
            *(base_addr_ + 6U) = (1UL << pin);   /* simulate BSRR offset 0x18/4 = 6 */
        }
    }

    void clear_pin(uint8_t pin)
    {
        if (base_addr_) {
            *(base_addr_ + 6U) = (1UL << (pin + 16U));  /* BSRR reset bits */
        }
    }

    /* ── Destructor ─────────────────────────────────────────────────────── */
    /*
     * Runs automatically when the object goes out of scope.
     * For hardware drivers: release resources, set pins to safe state.
     */
    ~Gpio()
    {
        printf("  [Gpio] destroyed\n");
    }

private:
    /*
     * Private members: only THIS class's methods can access them.
     * This prevents external code from corrupting hardware state.
     *
     * Convention: trailing underscore for member variables (my_var_)
     * This avoids shadowing parameter names and makes members visible.
     */
    volatile uint32_t *base_addr_;
    uint32_t           moder_;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 2: OPERATOR OVERLOADING — make types feel natural
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Example: a fixed-point Q16.16 type for embedded DSP.
 * operator+ lets you write: result = a + b  instead of  q_add(a, b)
 */
class Q1616 {
public:
    explicit Q1616(float f) : raw_(static_cast<int32_t>(f * 65536.0f)) {}
    explicit Q1616(int32_t raw) : raw_(raw) {}

    float to_float() const { return static_cast<float>(raw_) / 65536.0f; }

    Q1616 operator+(const Q1616 &other) const { return Q1616(raw_ + other.raw_); }
    Q1616 operator-(const Q1616 &other) const { return Q1616(raw_ - other.raw_); }
    Q1616 operator*(const Q1616 &other) const {
        /* Q16.16 * Q16.16 → Q32.32 shift back to Q16.16 */
        return Q1616(static_cast<int32_t>(
            (static_cast<int64_t>(raw_) * other.raw_) >> 16));
    }

    /* Comparison operators */
    bool operator>(const Q1616 &other) const { return raw_ > other.raw_; }
    bool operator<(const Q1616 &other) const { return raw_ < other.raw_; }

private:
    int32_t raw_;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * PART 3: STRUCT FOR PLAIN DATA (no behavior needed)
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * In embedded C++, use struct for plain data aggregates (POD types).
 * POD types can be memcpy'd safely, used in DMA, etc.
 */
struct SensorReading {
    uint32_t timestamp_ms;
    int16_t  temperature_raw;   /* ADC counts */
    uint16_t pressure_raw;
    uint8_t  sensor_id;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * DEMO
 * ═══════════════════════════════════════════════════════════════════════════ */
static void demo_gpio_class(void)
{
    printf("── GPIO class demo ──\n");

    /* Simulated register memory */
    static volatile uint32_t fake_gpioa_regs[10] = {};

    {   /* scope: Gpio is destroyed at end of block */
        Gpio gpioa(fake_gpioa_regs);

        gpioa.set_mode(5, Gpio::Mode::Output);
        gpioa.set_mode(13, Gpio::Mode::AF);
        gpioa.set_pin(5);

        printf("  pin5  mode: %u (should be 1=Output)\n",
               static_cast<uint8_t>(gpioa.get_mode(5)));
        printf("  pin13 mode: %u (should be 2=AF)\n",
               static_cast<uint8_t>(gpioa.get_mode(13)));
        printf("  BSRR reg: 0x%08X\n", (uint32_t)fake_gpioa_regs[6]);
    }
    /* Gpio destructor ran here */
    printf("\n");
}

static void demo_fixed_point(void)
{
    printf("── Q16.16 fixed-point arithmetic ──\n");

    Q1616 a(3.14159f);
    Q1616 b(2.71828f);

    Q1616 sum  = a + b;
    Q1616 diff = a - b;
    Q1616 prod = a * b;

    printf("  pi  = %.5f\n", a.to_float());
    printf("  e   = %.5f\n", b.to_float());
    printf("  pi+e = %.5f (expect %.5f)\n", sum.to_float(),  3.14159f + 2.71828f);
    printf("  pi-e = %.5f (expect %.5f)\n", diff.to_float(), 3.14159f - 2.71828f);
    printf("  pi*e = %.5f (expect %.5f)\n", prod.to_float(), 3.14159f * 2.71828f);
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * EXERCISES
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * EXERCISE 1:
 *   Add a method Gpio::toggle(uint8_t pin) that reads the current ODR value
 *   and toggles the pin. Add 'volatile uint32_t odr_' as a private member.
 *
 * EXERCISE 2:
 *   Create a class Timer that wraps timer registers (PSC, ARR, CNT, CR1).
 *   Add methods: start(), stop(), reset(), get_count().
 *   Use the same pattern: private regs pointer, public interface.
 *
 * EXERCISE 3:
 *   Add operator<< and operator>> to Q1616 for left/right arithmetic shift.
 *   Implement Q1616 division using 64-bit intermediate.
 */

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 2 — Lesson 1: Classes & Objects      ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    demo_gpio_class();
    demo_fixed_point();
    return 0;
}
