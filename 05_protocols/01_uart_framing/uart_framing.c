/**
 * @file uart_framing.c
 * @brief Phase 5 — UART Framing: Turning Byte Streams Into Reliable Messages
 *
 * Raw UART is a byte-by-byte stream with no inherent message boundaries.
 * Every real protocol adds a framing layer on top.
 *
 * This lesson implements three common framing approaches:
 *   1. Fixed-length frames (e.g., sensor data)
 *   2. Length-prefixed frames (e.g., custom binary protocol)
 *   3. Delimiter-based (e.g., Modbus ASCII, HDLC, COBS)
 *
 * BUILD:  cmake --build build --target p5_uart_framing
 */

#include "embedded_types.h"

/* ─── Frame format for all implementations ──────────────────────────────── */
/* [SOF=0xAA][SOF2=0x55][LEN=1][PAYLOAD=LEN][CRC16=2] */
#define SOF1 0xAAU
#define SOF2 0x55U

static uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFU;
    while (len--)
    {
        crc ^= (uint16_t)(*data++ << 8U);
        for (int i = 0; i < 8; i++)
        {
            crc = (crc & 0x8000U) ? ((uint16_t)(crc << 1U) ^ 0x1021U) : (uint16_t)(crc << 1U);
        }
    }
    return crc;
}

static size_t build_frame(uint8_t *out, size_t out_size,
                          const uint8_t *payload, uint8_t len)
{
    if (out_size < (size_t)(len + 5U))
        return 0U;
    out[0] = SOF1;
    out[1] = SOF2;
    out[2] = len;
    memcpy(&out[3], payload, len);
    uint16_t crc = crc16_ccitt(payload, len);
    out[3 + len] = (uint8_t)(crc >> 8U);
    out[3 + len + 1] = (uint8_t)(crc & 0xFFU);
    return (size_t)(len + 5U);
}

/* COBS encoding — removes 0x00 bytes from data, used in CLI-style framing */
static size_t cobs_encode(const uint8_t *in, size_t len, uint8_t *out, size_t out_size)
{
    if (out_size < len + 2U)
        return 0U;
    size_t code_pos = 0, write_pos = 1;
    uint8_t code = 1U;

    for (size_t i = 0; i < len; i++)
    {
        if (in[i] != 0x00U)
        {
            out[write_pos++] = in[i];
            if (++code == 0xFFU)
            {
                out[code_pos] = code;
                code_pos = write_pos++;
                code = 1U;
            }
        }
        else
        {
            out[code_pos] = code;
            code_pos = write_pos++;
            code = 1U;
        }
    }
    out[code_pos] = code;
    out[write_pos++] = 0x00U; /* frame delimiter */
    return write_pos;
}

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 5 — Lesson 1: UART Framing           ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    uint8_t payload[] = {0x01U, 0x02U, 0x03U, 0x04U};
    uint8_t frame[32];
    size_t flen = build_frame(frame, sizeof(frame), payload, sizeof(payload));

    printf("── Length-prefixed frame ──\n");
    printf("  payload: ");
    for (size_t i = 0; i < sizeof(payload); i++)
        printf("%02X ", payload[i]);
    printf("\n");
    printf("  frame  : ");
    for (size_t i = 0; i < flen; i++)
        printf("%02X ", frame[i]);
    printf("\n");
    printf("  CRC16  : 0x%04X\n\n", crc16_ccitt(payload, sizeof(payload)));

    printf("── COBS encoding ──\n");
    uint8_t raw[] = {0x01U, 0x00U, 0x02U, 0x00U, 0x03U};
    uint8_t cobs[32];
    size_t clen = cobs_encode(raw, sizeof(raw), cobs, sizeof(cobs));
    printf("  raw : ");
    for (size_t i = 0; i < sizeof(raw); i++)
        printf("%02X ", raw[i]);
    printf("\n");
    printf("  cobs: ");
    for (size_t i = 0; i < clen; i++)
        printf("%02X ", cobs[i]);
    printf("\n\n");

    printf("EXERCISES:\n");
    printf("  1. Write a frame parser using the state machine from Phase 4.\n");
    printf("  2. Implement COBS decode.\n");
    printf("  3. Implement Modbus RTU framing (3.5 char gap as frame delimiter).\n");
    return 0;
}
