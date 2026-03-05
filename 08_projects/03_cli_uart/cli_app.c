/**
 * @file cli_app.c
 * @brief Capstone 3 — CLI: Application Commands and Simulation
 *
 * Registers application commands and simulates a UART input session.
 * On real hardware, cli_feed_char() would be called from USART1_IRQHandler.
 *
 * BUILD:  cmake --build build --target p8_cli
 */

#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* ─── Simulated hardware state ───────────────────────────────────────────── */
static bool g_led_state = false;
static uint32_t g_uptime_s = 3600U; /* 1 hour */

/* ─── Command handlers ───────────────────────────────────────────────────── */

static int cmd_led(int argc, const char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: led <on|off|toggle>\n");
        return 1;
    }
    if (strcmp(argv[1], "on") == 0)
    {
        g_led_state = true;
    }
    else if (strcmp(argv[1], "off") == 0)
    {
        g_led_state = false;
    }
    else if (strcmp(argv[1], "toggle") == 0)
    {
        g_led_state = !g_led_state;
    }
    else
    {
        printf("Unknown argument: %s\n", argv[1]);
        return 1;
    }
    printf("LED is %s\n", g_led_state ? "ON" : "OFF");
    return 0;
}

static int cmd_status(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;
    printf("── System Status ──\n");
    printf("  Uptime:   %u s (%u h %u m %u s)\n",
           g_uptime_s,
           g_uptime_s / 3600U,
           (g_uptime_s % 3600U) / 60U,
           g_uptime_s % 60U);
    printf("  LED:      %s\n", g_led_state ? "ON" : "OFF");
    printf("  Heap:     n/a (bare-metal)\n");
    return 0;
}

static int cmd_echo(int argc, const char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        printf("%s", argv[i]);
        if (i < argc - 1)
            putchar(' ');
    }
    putchar('\n');
    return 0;
}

static int cmd_reboot(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;
    printf("Rebooting... (simulated)\n");
    /* On real hardware: NVIC_SystemReset() or watchdog trigger */
    return 0;
}

static int cmd_mem(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;
    /* On real hardware: read linker symbols _sdata, _edata, _ebss, etc. */
    printf("── Memory Map ──\n");
    printf("  Flash:  0x08000000 .. 0x08080000  (512 KB)\n");
    printf("  SRAM:   0x20000000 .. 0x20020000  (128 KB)\n");
    printf("  Stack:  0x20020000 (descending, 2 KB reserved)\n");
    printf("  .text:  ~8 KB (estimate)\n");
    return 0;
}

/* ─── Simulation: feed pre-canned commands one byte at a time ────────────── */
static void simulate_terminal(const char *session)
{
    printf("──── simulated terminal input ────\n");
    for (const char *p = session; *p; p++)
    {
        cli_feed_char(*p);
        if (*p == '\n')
            cli_process();
    }
}

int main(void)
{
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Capstone 3 — UART Command-Line Interface            ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    cli_init();

    /* Register application commands */
    static const cli_command_t cmds[] = {
        {"led", "led <on|off|toggle> — control LED", cmd_led},
        {"status", "Show system status", cmd_status},
        {"echo", "Echo arguments back", cmd_echo},
        {"reboot", "Trigger system reboot", cmd_reboot},
        {"mem", "Show memory map", cmd_mem},
    };
    for (uint8_t i = 0U; i < sizeof(cmds) / sizeof(cmds[0]); i++)
        cli_register(&cmds[i]);

    /* Simulate a terminal session */
    printf("> ");
    fflush(stdout);
    simulate_terminal(
        "help\n"
        "status\n"
        "led on\n"
        "led toggle\n"
        "echo hello embedded world\n"
        "mem\n"
        "badcmd arg1\n"
        "reboot\n");

    printf("\nEXERCISES:\n");
    printf("  1. Add command history (up-arrow) using an index into a circular\n");
    printf("     buffer of the last N command strings.\n");
    printf("  2. Add tab-completion: when TAB is received, print all commands\n");
    printf("     that start with the currently typed prefix.\n");
    printf("  3. Add 'gpio read PA5' and 'gpio write PA5 1' commands that call\n");
    printf("     the GPIO driver from Module 7.\n");
    return 0;
}
