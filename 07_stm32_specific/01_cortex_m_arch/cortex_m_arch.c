/**
 * @file cortex_m_arch.c
 * @brief Phase 7 — ARM Cortex-M4 Architecture Deep Dive
 *
 * Covers the core CPU concepts every STM32 developer must understand:
 *   1. Register map: R0-R15, PSR, MSP/PSP, CONTROL
 *   2. NVIC: priority levels, preemption, subpriority, pending/active bits
 *   3. Exception model: fault escalation, fault status registers
 *   4. Entering/leaving critical sections: PRIMASK, BASEPRI
 *   5. FPU: lazy stacking, context save requirements
 *   6. SysTick: 24-bit countdown timer for OS tick
 *   7. Power: WFI, WFE, sleep-on-exit
 *
 * NOTE: This file targets real STM32F411 hardware or QEMU.
 *       Use 07_stm32_specific CMakeLists.txt to cross-compile.
 */

#include <stdint.h>
#include <stdio.h>

/* ─── Core peripherals (CMSIS-style, manually defined) ──────────────────── */

/* SysTick (0xE000E010) */
typedef struct
{
    volatile uint32_t CTRL;
    volatile uint32_t LOAD;
    volatile uint32_t VAL;
    volatile uint32_t CALIB;
} SysTick_Type;

#define SysTick ((SysTick_Type *)0xE000E010UL)
#define SYSTICK_CTRL_ENABLE (1UL << 0)
#define SYSTICK_CTRL_TICKINT (1UL << 1)
#define SYSTICK_CTRL_CLKSOURCE (1UL << 2) /* 1=AHB, 0=AHB/8 */

/* NVIC (0xE000E100) */
typedef struct
{
    volatile uint32_t ISER[8]; /* Interrupt Set Enable Registers */
    uint32_t RESERVED0[24];
    volatile uint32_t ICER[8]; /* Interrupt Clear Enable Registers */
    uint32_t RESERVED1[24];
    volatile uint32_t ISPR[8]; /* Interrupt Set Pending Registers */
    uint32_t RESERVED2[24];
    volatile uint32_t ICPR[8]; /* Interrupt Clear Pending Registers */
    uint32_t RESERVED3[24];
    volatile uint32_t IABR[8]; /* Interrupt Active Bit Registers (read-only) */
    uint32_t RESERVED4[56];
    volatile uint8_t IP[240]; /* Interrupt Priority Registers (8-bit each) */
} NVIC_Type;

#define NVIC ((NVIC_Type *)0xE000E100UL)

/* SCB (0xE000ED00) */
typedef struct
{
    volatile uint32_t CPUID;
    volatile uint32_t ICSR;
    volatile uint32_t VTOR;
    volatile uint32_t AIRCR; /* Application Interrupt and Reset Control */
    volatile uint32_t SCR;
    volatile uint32_t CCR;
    volatile uint8_t SHPR[12]; /* System Handler Priority Registers */
    volatile uint32_t SHCSR;
    volatile uint32_t CFSR; /* Configurable Fault Status Register */
    volatile uint32_t HFSR; /* Hard Fault Status Register */
    uint32_t RESERVED;
    volatile uint32_t MMFAR; /* MemManage Fault Address Register */
    volatile uint32_t BFAR;  /* Bus Fault Address Register */
} SCB_Type;

#define SCB ((SCB_Type *)0xE000ED00UL)

#define SCB_AIRCR_VECTKEYSTAT 0x05FA0000UL
#define SCB_AIRCR_PRIGROUP_Pos 8U
#define SCB_CFSR_IBUSERR (1UL << 8)
#define SCB_CFSR_PRECISERR (1UL << 9)
#define SCB_CFSR_MEMFAULT (1UL << 0)

/* IRQ numbers (STM32F411) */
#define USART1_IRQn 37
#define TIM2_IRQn 28

/* ── Critical section macros ─────────────────────────────────────────────── */
/* PRIMASK: when set to 1, blocks all interrupts except NMI+HardFault */
static inline void __disable_irq(void)
{
    __asm volatile("cpsid i" : : : "memory");
}
static inline void __enable_irq(void)
{
    __asm volatile("cpsie i" : : : "memory");
}

/* BASEPRI: blocks interrupts with priority LOWER than (numerically >=) threshold.
 * Example: BASEPRI=0x80 blocks pri 128-255, allows 0-127 (higher priority ISRs). */
static inline void set_basepri(uint32_t pri)
{
    __asm volatile("MSR basepri, %0" : : "r"(pri) : "memory");
}

/* Save and restore interrupt state without assuming current state */
static inline uint32_t enter_critical(void)
{
    uint32_t primask;
    __asm volatile("MRS %0, primask" : "=r"(primask));
    __disable_irq();
    return primask;
}
static inline void exit_critical(uint32_t primask_save)
{
    __asm volatile("MSR primask, %0" : : "r"(primask_save) : "memory");
}

/* ── SysTick setup ───────────────────────────────────────────────────────── */
static volatile uint32_t g_systick_count = 0U;

void SysTick_Handler(void)
{
    g_systick_count++;
}

static void systick_init(uint32_t core_clock_hz, uint32_t tick_hz)
{
    uint32_t reload = (core_clock_hz / tick_hz) - 1U;
    SysTick->LOAD = reload & 0x00FFFFFFUL;
    SysTick->VAL = 0U;
    SysTick->CTRL = SYSTICK_CTRL_ENABLE | SYSTICK_CTRL_TICKINT | SYSTICK_CTRL_CLKSOURCE;
}

/* ── NVIC helpers ────────────────────────────────────────────────────────── */
static void nvic_enable_irq(int irqn)
{
    NVIC->ISER[(uint32_t)irqn >> 5U] = 1UL << ((uint32_t)irqn & 31U);
}
static void nvic_set_priority(int irqn, uint8_t pre_pri, uint8_t sub_pri)
{
    /* STM32F411 uses 4 bits of priority, configured with PRIGROUP=4 (4 bits pre, 0 sub)
     * Priority register value = preemption_priority << (8 - __NVIC_PRIO_BITS) */
    (void)sub_pri;
    NVIC->IP[(uint32_t)irqn] = (uint8_t)(pre_pri << 4U);
}

/* ── Fault handler demo ──────────────────────────────────────────────────── */
/* In a real HardFault_Handler you would read these registers to diagnose crash */
static void decode_fault_status(void)
{
    uint32_t cfsr = SCB->CFSR;
    printf("CFSR=0x%08lX\n", (unsigned long)cfsr);
    if (cfsr & SCB_CFSR_MEMFAULT)
        printf("  MemFault: invalid memory access\n");
    if (cfsr & SCB_CFSR_IBUSERR)
        printf("  BusFault: prefetch abort\n");
    if (cfsr & SCB_CFSR_PRECISERR)
        printf("  BusFault: precise data access fault\n");
    /* MMFAR holds the faulting address when MMFSVALID bit is set */
}

/* ── Sleep modes ─────────────────────────────────────────────────────────── */
static inline void __wfi(void)
{
    __asm volatile("wfi" : : : "memory");
}
/* In idle task or main loop: call __wfi() to stop the CPU clock until the
 * next interrupt. Saves ~90% power vs spinning. SysTick wakes it every 1ms. */

int main(void)
{
    /* On real hardware these printf calls would go over SWO/ITM or UART */
    printf("=== Phase 7 — Cortex-M4 Architecture ===\n\n");

    printf("── NVIC priority groups ──\n");
    printf("  STM32F411 uses 4 priority bits → 16 preemption levels (0=highest)\n");
    printf("  Configure via AIRCR.PRIGROUP before enabling any IRQ.\n\n");

    printf("── SysTick configuration ──\n");
    printf("  Core clock = 100 MHz, tick rate = 1 kHz\n");
    printf("  LOAD = %lu (0x%05lX)\n",
           (unsigned long)(100000000UL / 1000UL - 1UL),
           (unsigned long)(100000000UL / 1000UL - 1UL));
    printf("  Calls SysTick_Handler every 1 ms → OS time base\n\n");

    printf("── Critical section patterns ──\n");
    printf("  Option A: PRIMASK — disable ALL maskable IRQs (simple, blunt)\n");
    printf("  Option B: BASEPRI — disable only low-priority IRQs (fine-grained)\n");
    printf("  Always save/restore state instead of blindly enable after section.\n\n");

    printf("── WFI (Wait For Interrupt) ──\n");
    printf("  Idle task: while(1) { __wfi(); }  /* CPU clock halted until IRQ */\n");
    printf("  Combined with sleep-on-exit (SCR.SLEEPONEXIT): CPU sleeps when\n");
    printf("  returning from every ISR, only wakes when new IRQ arrives.\n\n");

    printf("── FPU context saving ──\n");
    printf("  Cortex-M4 FPU uses lazy stacking: FPU registers only saved on stack\n");
    printf("  if a second interrupt occurs. Saves ~100 cycles in typical ISRs.\n");
    printf("  FPCCR.LSPEN=1 enables lazy stacking (default after SystemInit).\n\n");

    decode_fault_status(); /* demonstrates CFSR decode */

    printf("EXERCISES:\n");
    printf("  1. What is the HardFault priority? Can you lower it?\n");
    printf("     Why is it fixed at -1 (above configurable range)?\n");
    printf("  2. Write a real HardFault_Handler in assembly that saves SP then\n");
    printf("     calls a C function to decode the stacked exception frame.\n");
    printf("  3. Configure NVIC PRIGROUP=4 (no subpriority) and set USART1\n");
    printf("     preemption priority=6, TIM2 priority=5. Which can preempt which?\n");
    return 0;
}
