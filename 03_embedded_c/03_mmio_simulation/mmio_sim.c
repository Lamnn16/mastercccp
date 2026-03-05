/**
 * @file mmio_sim.c
 * @brief Phase 3 — Memory-Mapped I/O Simulation
 *
 * On a real MCU, peripherals are accessed by reading/writing to specific
 * memory addresses. This lesson simulates this on the PC so you can
 * understand the mechanics without hardware.
 *
 * Real STM32F411 GPIOA base address: 0x40020000
 * We map our simulation struct to a local variable and pretend it's at
 * the hardware address.
 *
 * BUILD:  cmake --build build --target p3_mmio_sim
 */

#include "embedded_types.h"

/* Simulated GPIOA memory-mapped registers */
static volatile uint32_t sim_gpioa_mem[10]; /* 10 registers × 4 bytes = 40 bytes */

/* macros that simulate hardware addresses */
#define SIM_GPIOA_BASE ((uintptr_t)sim_gpioa_mem)
#define SIM_GPIOA_MODER REG32(SIM_GPIOA_BASE + 0x00U)
#define SIM_GPIOA_OTYPER REG32(SIM_GPIOA_BASE + 0x04U)
#define SIM_GPIOA_ODR REG32(SIM_GPIOA_BASE + 0x14U)
#define SIM_GPIOA_BSRR REG32(SIM_GPIOA_BASE + 0x18U)

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 3 — Lesson 3: MMIO Simulation        ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    /* Initialize */
    for (int i = 0; i < 10; i++)
        sim_gpioa_mem[i] = 0U;

    printf("GPIOA base (simulated): %p\n", (void *)SIM_GPIOA_BASE);
    printf("MODER  offset: 0x%02zX\n", (size_t)0x00U);
    printf("ODR    offset: 0x%02zX\n", (size_t)0x14U);
    printf("BSRR   offset: 0x%02zX\n\n", (size_t)0x18U);

    /* PA5 = output mode */
    SIM_GPIOA_MODER = (SIM_GPIOA_MODER & ~(3UL << 10U)) | (1UL << 10U);
    printf("MODER  = 0x%08X (PA5=Output)\n", (uint32_t)SIM_GPIOA_MODER);

    /* Set PA5 via BSRR */
    SIM_GPIOA_BSRR = (1UL << 5U);
    printf("BSRR   = 0x%08X\n", (uint32_t)SIM_GPIOA_BSRR);

    /* ODR tracks the state (in hardware, reading ODR would reflect the pin) */
    SIM_GPIOA_ODR |= (1UL << 5U); /* simulated hardware update */
    printf("ODR    = 0x%08X (PA5=1)\n\n", (uint32_t)SIM_GPIOA_ODR);

    printf("EXERCISES:\n");
    printf("  1. Map RCC_AHB1ENR at a simulated base and implement\n");
    printf("     rcc_enable_gpioa() that sets bit 0 of that register.\n");
    printf("  2. Create a full GPIO init + set + clear + toggle sequence\n");
    printf("     using only the BSRR/MODER registers.\n");
    printf("  3. Write a 'register dump' function that prints all 10\n");
    printf("     GPIOA registers with their offset and symbolic name.\n");

    return 0;
}
