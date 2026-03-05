/**
 * @file pointers.c
 * @brief Phase 1 — Pointers: Everything You Need for Embedded C
 *
 * As an experienced C developer you know basic pointers. This lesson focuses
 * on the patterns that appear *constantly* in embedded code and that trip up
 * even experienced developers:
 *
 *   1. const-correctness (input buffers vs output buffers)
 *   2. Pointer to array vs pointer to first element
 *   3. Pointer arithmetic and hardware register arrays
 *   4. restrict — telling the optimizer buffers don't alias
 *   5. void * and type-punning safely
 *   6. Pointer to function (used for callbacks, vtables, ISR tables)
 *
 * BUILD:  cmake --build build --target p1_pointers
 * RUN:    .\build\01_advanced_c\p1_pointers.exe
 */

#include "embedded_types.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. CONST-CORRECTNESS
 *
 * Embedded code passes buffers everywhere. Getting const right matters:
 *  - It expresses intent (this buffer is read-only)
 *  - It enables the compiler to place data in Flash (ROM) on MCU
 *  - It prevents accidental writes to hardware registers
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Read a register (output via pointer, no modification of input).
 *
 *   const uint32_t *reg  — we won't modify the register value
 *   uint32_t *out        — we WILL write into this caller buffer
 */
static void read_register(const volatile uint32_t *reg, uint32_t *out)
{
    *out = *reg; /* safe: reads volatile, writes non-volatile out */
}

/*
 * Write a register.
 *   volatile uint32_t *reg — hardware register (must be volatile)
 *   uint32_t value         — value is passed by copy, not pointer
 */
static void write_register(volatile uint32_t *reg, uint32_t value)
{
    *reg = value;
}

/* Lookup table stored in "ROM" — const forces it to .rodata section on MCU */
static const uint8_t crc_table[8] = {0x00, 0x07, 0x0E, 0x09,
                                     0x1C, 0x1B, 0x12, 0x15};

static void demo_const_correctness(void)
{
    printf("── 1. const-correctness ──\n");

    /* Simulated memory-mapped register */
    volatile uint32_t fake_reg = 0xDEADBEEFU;
    uint32_t val = 0;

    read_register(&fake_reg, &val);
    printf("  Read  reg: 0x%08X\n", val);

    write_register(&fake_reg, 0xCAFEBABEU);
    printf("  Write reg: 0x%08X, reg now: 0x%08X\n", 0xCAFEBABEU, (uint32_t)fake_reg);

    /* const pointer to const data — typical for ROM lookup table access */
    const uint8_t *const p = crc_table; /* p cannot be moved, *p cannot be changed */
    printf("  crc_table[3] via const ptr: 0x%02X\n", p[3]);
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. POINTER TO ARRAY vs POINTER TO FIRST ELEMENT
 *
 * This confusion causes real bugs in embedded code when passing arrays.
 * ═══════════════════════════════════════════════════════════════════════════ */
static void demo_array_pointers(void)
{
    printf("── 2. Pointer-to-array vs pointer-to-element ──\n");

    uint8_t buf[4] = {0xAA, 0xBB, 0xCC, 0xDD};

    uint8_t *p_elem = buf;      /* pointer to first uint8_t element */
    uint8_t (*p_arr)[4] = &buf; /* pointer to the whole 4-byte array */

    printf("  p_elem   points to: 0x%02X,  p_elem+1  -> 0x%02X (steps 1 byte)\n",
           *p_elem, *(p_elem + 1));

    printf("  *p_arr[0]= 0x%02X,  *(p_arr+1) would jump %zu bytes\n",
           (*p_arr)[0], sizeof(*p_arr));

    /*
     * KEY INSIGHT:
     *   p_elem + 1  advances by sizeof(uint8_t) = 1 byte
     *   p_arr  + 1  advances by sizeof(uint8_t[4]) = 4 bytes
     *
     * In embedded code, p_arr is useful for iterating over arrays-of-arrays,
     * e.g., a 2D frame buffer or a DMA descriptor table.
     */
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. POINTER ARITHMETIC ON REGISTERS
 *
 * GPIO on STM32F4: 11 ports (GPIOA..GPIOK) each at a fixed base address.
 * We can address them as an array of register structs.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Minimal simulation of a GPIO register block */
typedef struct
{
    volatile uint32_t MODER;   /* offset 0x00 — pin mode */
    volatile uint32_t OTYPER;  /* offset 0x04 — output type */
    volatile uint32_t OSPEEDR; /* offset 0x08 — output speed */
    volatile uint32_t PUPDR;   /* offset 0x0C — pull-up/pull-down */
    volatile uint32_t IDR;     /* offset 0x10 — input data register */
    volatile uint32_t ODR;     /* offset 0x14 — output data register */
    volatile uint32_t BSRR;    /* offset 0x18 — bit set/reset register */
    volatile uint32_t LCKR;    /* offset 0x1C — lock register */
    volatile uint32_t AFR[2];  /* offset 0x20/0x24 — alternate function */
} GPIO_TypeDef;

/* On a real STM32F411:
 *   GPIOA base = 0x40020000
 *   GPIOB base = 0x40020400  (stride = 0x400 = sizeof(GPIO_TypeDef))
 *
 * Here we just simulate it in normal memory.
 */
static GPIO_TypeDef simulated_gpio[3]; /* GPIOA, GPIOB, GPIOC */

static void set_pin_output(GPIO_TypeDef *gpio, uint8_t pin)
{
    /* MODER[2n+1:2n] = 01 for output mode */
    uint32_t mode = gpio->MODER;
    mode &= ~(3UL << (pin * 2U)); /* clear 2 bits */
    mode |= (1UL << (pin * 2U));  /* set to "01" = output */
    gpio->MODER = mode;
}

static void set_pin_high(GPIO_TypeDef *gpio, uint8_t pin)
{
    /* Write to BSRR lower half to set, upper half to reset */
    gpio->BSRR = (1UL << pin);
}

static void demo_register_pointer_arithmetic(void)
{
    printf("── 3. Register pointer arithmetic ──\n");

    /* Access GPIO "B" by adding 1 stride to pointer to "A" */
    GPIO_TypeDef *gpioa = &simulated_gpio[0];
    GPIO_TypeDef *gpiob = gpioa + 1; /* advances by sizeof(GPIO_TypeDef) */

    set_pin_output(gpioa, 5); /* PA5 = output (LED on Nucleo boards) */
    set_pin_high(gpioa, 5);

    printf("  GPIOA->MODER  = 0x%08X (bit5 mode = output)\n", (uint32_t)gpioa->MODER);
    printf("  GPIOA->BSRR   = 0x%08X (bit5 set)\n", (uint32_t)gpioa->BSRR);
    printf("  GPIOB->MODER  = 0x%08X (untouched)\n", (uint32_t)gpiob->MODER);
    printf("  stride = %zu bytes (should equal register block size)\n",
           (size_t)((uintptr_t)gpiob - (uintptr_t)gpioa));
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. restrict — CRITICAL FOR EFFICIENT EMBEDDED CODE
 *
 * The `restrict` qualifier tells the compiler that two pointers do NOT alias
 * (point to the same memory). This enables SIMD/unroll optimizations that are
 * critical for DSP and image processing on Cortex-M.
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * Without restrict: compiler must reload src on each iteration because
 * it cannot know if dst and src overlap.
 *
 * With restrict: compiler can vectorize freely (NEON/DSP instructions).
 */
static void fast_memcpy(uint8_t *restrict dst,
                        const uint8_t *restrict src,
                        size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        dst[i] = src[i];
    }
}

static void demo_restrict(void)
{
    printf("── 4. restrict qualifier ──\n");

    uint8_t src[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t dst[8] = {0};

    fast_memcpy(dst, src, 8);

    printf("  src: ");
    for (int i = 0; i < 8; i++)
        printf("%d ", src[i]);
    printf("\n");
    printf("  dst: ");
    for (int i = 0; i < 8; i++)
        printf("%d ", dst[i]);
    printf("\n\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 5. TYPE-PUNNING with union (the safe, defined-behavior way in C99)
 *
 * Never cast float* to uint32_t* in C — that is undefined behavior.
 * Use a union instead. This appears in IEEE754 float manipulation,
 * protocol serialization, and CAN frame packing.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef union
{
    float f;
    uint32_t u;
    uint8_t bytes[4];
} float_bits_t;

static void demo_type_punning(void)
{
    printf("── 5. Type-punning via union ──\n");

    float_bits_t x;
    x.f = 3.14159f;

    printf("  3.14159f as hex : 0x%08X\n", x.u);
    printf("  bytes (LE)      : %02X %02X %02X %02X\n",
           x.bytes[0], x.bytes[1], x.bytes[2], x.bytes[3]);

    /*
     * This is how you'd pack a float into a CAN frame or serialize it over
     * UART: copy x.bytes[] into the TX buffer directly.
     */
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 6. FUNCTION POINTERS — previewed here, expanded in 05_function_pointers/
 *
 * Used for: interrupt vector tables, HAL callbacks, command dispatch tables,
 * state machine transitions, driver polymorphism without C++.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef void (*irq_handler_t)(void); /* type alias for clarity */

static void uart_irq_handler(void) { printf("  [ISR] UART interrupt fired\n"); }
static void tim_irq_handler(void) { printf("  [ISR] Timer interrupt fired\n"); }

static void demo_function_pointers(void)
{
    printf("── 6. Function pointers (ISR dispatch table) ──\n");

    /* Simulated interrupt vector table — on real STM32 this lives at 0x00000000 */
    irq_handler_t vector_table[2];
    vector_table[0] = uart_irq_handler;
    vector_table[1] = tim_irq_handler;

    /* Dispatcher — in real firmware this is called by the CPU on interrupt */
    for (int irq = 0; irq < 2; irq++)
    {
        printf("  Dispatching IRQ %d: ", irq);
        vector_table[irq](); /* call through function pointer */
    }
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * EXERCISES  (implement these yourself before checking _solution files)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * EXERCISE 1:
 *   Write a function  read_modify_write(volatile uint32_t *reg, uint32_t mask, uint32_t val)
 *   that atomically modifies only the bits indicated by `mask` in `*reg`,
 *   setting those bits to `val`. Used for GPIO MODER, AFR, etc.
 *
 * EXERCISE 2:
 *   Given: uint8_t packet[6] = { 0xAA, 0x01, 0x02, 0x00, 0x00, 0xBB };
 *   Use a union to interpret bytes[2..5] as a uint32_t without memcpy.
 *   Print the value. (Hint: struct + union combo.)
 *
 * EXERCISE 3:
 *   Write a swap(void *a, void *b, size_t size) using only uint8_t * casts.
 *   This is the generic swap used in qsort callbacks.
 */

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 1 — Lesson 1: Pointers for Embedded  ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    demo_const_correctness();
    demo_array_pointers();
    demo_register_pointer_arithmetic();
    demo_restrict();
    demo_type_punning();
    demo_function_pointers();

    printf("── EXERCISES ──\n");
    printf("  See the EXERCISE comments at the bottom of pointers.c\n");
    printf("  Implement them in this file, then rebuild and verify output.\n");

    return 0;
}
