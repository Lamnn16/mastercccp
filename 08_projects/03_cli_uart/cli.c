/**
 * @file cli.c
 * @brief Capstone 3 — UART CLI: Tokenizer, Parser, and Dispatcher
 *
 * Design:
 *   1. cli_feed_char() receives bytes one-at-a-time from UART (can be ISR)
 *   2. Backspace support, newline triggers line processing
 *   3. cli_process() tokenizes the line and dispatches to registered handler
 *   4. Generates "help" and "?" automatically from registered commands
 *
 * BUILD:  cmake --build build --target p8_cli
 */

#include "cli.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

/* ─── State ──────────────────────────────────────────────────────────────── */
static char s_line[CLI_MAX_LINE + 1U];
static uint8_t s_line_len = 0U;
static bool s_line_ready = false;

static cli_command_t s_cmds[CLI_MAX_COMMANDS];
static uint8_t s_cmd_count = 0U;

/* ─── Built-in: help ─────────────────────────────────────────────────────── */
static int cmd_help(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;
    printf("Available commands:\n");
    for (uint8_t i = 0U; i < s_cmd_count; i++)
    {
        printf("  %-16s %s\n", s_cmds[i].name, s_cmds[i].help);
    }
    return 0;
}

/* ─── API ────────────────────────────────────────────────────────────────── */
void cli_init(void)
{
    s_line_len = 0U;
    s_line_ready = false;
    s_cmd_count = 0U;
    static const cli_command_t help_cmd = {"help", "Show this help", cmd_help};
    cli_register(&help_cmd);
}

bool cli_register(const cli_command_t *cmd)
{
    if (s_cmd_count >= CLI_MAX_COMMANDS)
        return false;
    s_cmds[s_cmd_count++] = *cmd;
    return true;
}

/* ISR-safe: just accumulate bytes */
void cli_feed_char(char c)
{
    if (c == '\r' || c == '\n')
    {
        if (s_line_len > 0U)
        {
            s_line[s_line_len] = '\0';
            s_line_ready = true;
        }
    }
    else if (c == '\b' || c == 0x7FU)
    { /* backspace */
        if (s_line_len > 0U)
        {
            s_line_len--;
            printf("\b \b");
        }
    }
    else if (s_line_len < CLI_MAX_LINE)
    {
        s_line[s_line_len++] = c;
        putchar(c); /* local echo */
    }
}

/* Tokenize an in-place string — returns token count */
static int tokenize(char *line, const char *argv[], int max_args)
{
    int argc = 0;
    char *p = line;
    while (*p && argc < max_args)
    {
        while (*p && isspace((unsigned char)*p))
            p++;
        if (!*p)
            break;
        argv[argc++] = p;
        while (*p && !isspace((unsigned char)*p))
            p++;
        if (*p)
            *p++ = '\0';
    }
    return argc;
}

/* Call from main loop: dispatch if a line is ready */
void cli_process(void)
{
    if (!s_line_ready)
        return;
    s_line_ready = false;
    putchar('\n');

    const char *argv[CLI_MAX_ARGS];
    char line_copy[CLI_MAX_LINE + 1U];
    strncpy(line_copy, s_line, CLI_MAX_LINE);
    line_copy[CLI_MAX_LINE] = '\0';
    s_line_len = 0U;

    int argc = tokenize(line_copy, argv, CLI_MAX_ARGS);
    if (argc == 0)
    {
        printf("> ");
        fflush(stdout);
        return;
    }

    /* Search for command */
    for (uint8_t i = 0U; i < s_cmd_count; i++)
    {
        if (strcmp(argv[0], s_cmds[i].name) == 0)
        {
            int ret = s_cmds[i].fn(argc, argv);
            if (ret != 0)
                printf("Error: command returned %d\n", ret);
            printf("> ");
            fflush(stdout);
            return;
        }
    }
    printf("Unknown command: '%s' (type 'help' for list)\n", argv[0]);
    printf("> ");
    fflush(stdout);
}

void cli_print(const char *str)
{
    fputs(str, stdout);
    fflush(stdout);
}
