/* ============================================================================
 * STM32F411RE Linker Script
 *
 * STM32F411RE memory map:
 *   Flash: 512 KB  @ 0x0800_0000
 *   SRAM:  128 KB  @ 0x2000_0000
 *
 * Sections layout (lowest address first in Flash):
 *   .isr_vector — interrupt vector table (must be at start of Flash)
 *   .text       — code
 *   .rodata     — constants, string literals
 *   .ARM.exidx  — exception index table (can omit with -fno-exceptions)
 *   .data       — initialised globals (LMA in Flash, VMA in SRAM)
 *   .bss        — zero-initialised globals (VMA in SRAM, no Flash storage)
 *   ._user_heap — heap (grows upward from after .bss)
 *   ._user_stack— stack (grows downward from top of SRAM)
 * ============================================================================ */

ENTRY(Reset_Handler)

MEMORY {
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

_estack = ORIGIN(SRAM) + LENGTH(SRAM);   /* top of stack */

_Min_Heap_Size  = 0x400;   /* 1 KB heap minimum  */
_Min_Stack_Size = 0x800;   /* 2 KB stack minimum */

SECTIONS {

    /* Vector table — MUST start at 0x08000000 (or VTOR offset) */
    .isr_vector : {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    /* Program code and constant data */
    .text : {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    /* Read-only data (constants) */
    .rodata : {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    /* ARM exception unwinding (unused with -fno-exceptions but harmless) */
    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
    .ARM : {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } >FLASH

    /* Pre-init and init arrays (C++ constructors) */
    .preinit_array : {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } >FLASH

    .init_array : {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } >FLASH

    .fini_array : {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } >FLASH

    /* ── RAM sections ── */

    /* Initialised data: stored in Flash (LMA), copied to SRAM at startup */
    _sidata = LOADADDR(.data);   /* Flash load address */

    .data : {
        . = ALIGN(4);
        _sdata = .;        /* SRAM start address */
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;        /* SRAM end address */
    } >SRAM AT>FLASH

    /* Zero-initialised data: no Flash storage, zeroed by startup code */
    .bss : {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >SRAM

    /* Heap */
    ._user_heap : {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + _Min_Heap_Size;
        . = ALIGN(8);
    } >SRAM

    /* Stack — verify minimum size fits */
    ._user_stack : {
        . = ALIGN(8);
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >SRAM

    /* Assert total SRAM usage does not exceed available SRAM */
    ASSERT(._user_stack <= ORIGIN(SRAM) + LENGTH(SRAM),
           "Not enough SRAM for heap + stack!")
}
