/**
 * @file cli.h
 * @brief Capstone 3 — UART Command-Line Interface API
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

#define CLI_MAX_LINE 80U
#define CLI_MAX_ARGS 8U
#define CLI_MAX_COMMANDS 16U

typedef int (*cli_cmd_fn_t)(int argc, const char *argv[]);

typedef struct
{
    const char *name;
    const char *help;
    cli_cmd_fn_t fn;
} cli_command_t;

void cli_init(void);
bool cli_register(const cli_command_t *cmd);
void cli_feed_char(char c); /* call from UART RX ISR or polling */
void cli_process(void);     /* call from main loop */
void cli_print(const char *str);
