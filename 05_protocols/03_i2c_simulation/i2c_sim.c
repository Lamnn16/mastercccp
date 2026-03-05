/**
 * @file i2c_sim.c
 * @brief Phase 5 — I2C Protocol: Bit-bang and Driver Patterns
 *
 * I2C is used for:
 *   EEPROMs, environmental sensors (BMP280, HDC1080, SHT31),
 *   RTCs (DS3231), IMUs (MPU6050), port expanders (PCF8574)
 *
 * This lesson covers:
 *   1. I2C electrical protocol (START, STOP, ACK/NAK, addresses)
 *   2. Bit-bang I2C — open-drain GPIO simulation
 *   3. Register write/read transactions
 *   4. ACK polling (EEPROM page write)
 *   5. STM32 HAL-style register driver patterns
 *
 * BUILD:  cmake --build build --target p5_i2c_sim
 */

#include "embedded_types.h"

/*  I2C address format (7-bit):
 *   [A6:A0] = device address  (0x00..0x7F)
 *   [0]     = R/W bit — 0=write, 1=read
 *   → 8-bit on-wire byte: (addr << 1) | rw
 */
#define I2C_WRITE 0U
#define I2C_READ 1U

/* ─── Simulated open-drain bus ───────────────────────────────────────────── */
static uint8_t g_sda_out = 1U, g_scl_out = 1U;
static uint8_t g_last_written_reg = 0U;
static uint8_t g_device_reg[256] = {0};

/* ACK simulation: return 0 (ACK) if address matches 0x68 (e.g. MPU6050) */
static uint8_t bus_read_sda_after_ack(uint8_t byte_sent)
{
    /* First byte is address+R/W; simulate ACK only from address 0x68 */
    if ((byte_sent >> 1U) == 0x68U || (byte_sent & 0xFEU) == 0xD0U)
        return 0U;
    if (byte_sent < 0xFEU)
        return 0U; /* also ACK data bytes */
    return 1U;     /* NAK */
}

static void i2c_set_sda(uint8_t high) { g_sda_out = high; }
static void i2c_set_scl(uint8_t high) { g_scl_out = high; }
static uint8_t i2c_read_sda(void) { return g_sda_out; }

/* ─── I2C primitives ─────────────────────────────────────────────────────── */
static void i2c_start(void)
{
    i2c_set_sda(1);
    i2c_set_scl(1);
    i2c_set_sda(0); /* SDA falls while SCL=1 → START */
    i2c_set_scl(0);
}

static void i2c_stop(void)
{
    i2c_set_sda(0);
    i2c_set_scl(1);
    i2c_set_sda(1); /* SDA rises while SCL=1 → STOP */
}

/* Write one byte, return ACK (0) or NAK (1) */
static uint8_t i2c_write_byte(uint8_t byte, bool verbose)
{
    if (verbose)
        printf("  I2C WRITE 0x%02X: ", byte);
    for (int bit = 7; bit >= 0; bit--)
    {
        i2c_set_sda((byte >> bit) & 1U);
        i2c_set_scl(1);
        i2c_set_scl(0);
    }
    /* Release SDA, clock in ACK */
    i2c_set_sda(1);
    i2c_set_scl(1);
    uint8_t ack = bus_read_sda_after_ack(byte);
    i2c_set_scl(0);
    if (verbose)
        printf("%s\n", ack == 0 ? "ACK" : "NAK");
    return ack;
}

/* Read one byte, send ACK (ack=0) or NAK (ack=1) */
static uint8_t i2c_read_byte(uint8_t ack, bool verbose)
{
    uint8_t data = 0U;
    i2c_set_sda(1);
    for (int bit = 7; bit >= 0; bit--)
    {
        i2c_set_scl(1);
        data |= (uint8_t)(i2c_read_sda() << bit);
        i2c_set_scl(0);
    }
    /* Populate simulated register read */
    data = g_device_reg[g_last_written_reg];
    i2c_set_sda(ack);
    i2c_set_scl(1);
    i2c_set_scl(0);
    if (verbose)
        printf("  I2C READ  0x%02X %s\n", data, ack == 0 ? "(ACK)" : "(NAK)");
    return data;
}

/* ─── High-level transactions ────────────────────────────────────────────── */

/* Write single register: [START][ADDR_W][REG][VAL][STOP] */
static bool i2c_reg_write(uint8_t dev_addr, uint8_t reg, uint8_t val)
{
    i2c_start();
    if (i2c_write_byte((uint8_t)(dev_addr << 1U) | I2C_WRITE, false))
        goto err;
    if (i2c_write_byte(reg, false))
        goto err;
    if (i2c_write_byte(val, false))
        goto err;
    i2c_stop();
    g_device_reg[reg] = val; /* simulate device storing value */
    return true;
err:
    i2c_stop();
    return false;
}

/* Read single register: [START][ADDR_W][REG][RS][ADDR_R][DATA][STOP] */
static bool i2c_reg_read(uint8_t dev_addr, uint8_t reg, uint8_t *out)
{
    i2c_start();
    if (i2c_write_byte((uint8_t)(dev_addr << 1U) | I2C_WRITE, false))
        goto err;
    if (i2c_write_byte(reg, false))
        goto err;
    i2c_start(); /* repeated START */
    if (i2c_write_byte((uint8_t)(dev_addr << 1U) | I2C_READ, false))
        goto err;
    g_last_written_reg = reg;
    *out = i2c_read_byte(1U, false); /* read + NAK (last byte) */
    i2c_stop();
    return true;
err:
    i2c_stop();
    return false;
}

/* Read multiple registers (burst read) for 3-axis sensors */
static bool i2c_burst_read(uint8_t dev_addr, uint8_t start_reg,
                           uint8_t *buf, uint8_t len)
{
    i2c_start();
    if (i2c_write_byte((uint8_t)(dev_addr << 1U) | I2C_WRITE, false))
        goto err;
    if (i2c_write_byte(start_reg, false))
        goto err;
    i2c_start();
    if (i2c_write_byte((uint8_t)(dev_addr << 1U) | I2C_READ, false))
        goto err;
    for (uint8_t i = 0U; i < len; i++)
    {
        g_last_written_reg = (uint8_t)(start_reg + i);
        bool last = (i == (len - 1U));
        buf[i] = i2c_read_byte(last ? 1U : 0U, false);
    }
    i2c_stop();
    return true;
err:
    i2c_stop();
    return false;
}

/* ─── EEPROM ACK polling ─────────────────────────────────────────────────── */
/* After a page write, EEPROM holds SDA low until write is complete.
 * STM32 I2C driver retries until ACK is received (ACK polling). */
static void eeprom_wait_ready(uint8_t dev_addr)
{
    int retries = 0;
    uint8_t nak = 1U;
    while (nak)
    {
        i2c_start();
        nak = i2c_write_byte((uint8_t)(dev_addr << 1U), false);
        i2c_stop();
        retries++;
        if (retries > 10)
        {
            break;
        } /* safety guard */
    }
    printf("  EEPROM ready after %d retries\n", retries - 1);
}

/* ─── MPU6050 demo ───────────────────────────────────────────────────────── */
#define MPU6050_ADDR 0x68U
#define MPU6050_WHO_AM_I 0x75U
#define MPU6050_PWR_MGMT1 0x6BU
#define MPU6050_ACCEL_XOUT 0x3BU

typedef struct
{
    int16_t ax, ay, az;
} accel_data_t;

static bool mpu6050_read_accel(accel_data_t *data)
{
    /* Preload simulated sensor values */
    g_device_reg[MPU6050_ACCEL_XOUT + 0] = 0x00U;
    g_device_reg[MPU6050_ACCEL_XOUT + 1] = 0xF0U; /* ax≈-4096 */
    g_device_reg[MPU6050_ACCEL_XOUT + 2] = 0x10U;
    g_device_reg[MPU6050_ACCEL_XOUT + 3] = 0x00U;
    g_device_reg[MPU6050_ACCEL_XOUT + 4] = 0x40U;
    g_device_reg[MPU6050_ACCEL_XOUT + 5] = 0x00U; /* az≈+16384 (1g) */

    uint8_t buf[6];
    if (!i2c_burst_read(MPU6050_ADDR, MPU6050_ACCEL_XOUT, buf, 6U))
        return false;
    data->ax = (int16_t)((buf[0] << 8U) | buf[1]);
    data->ay = (int16_t)((buf[2] << 8U) | buf[3]);
    data->az = (int16_t)((buf[4] << 8U) | buf[5]);
    return true;
}

int main(void)
{
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Phase 5 — Lesson 3: I2C Simulation         ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    printf("── Single register read ──\n");
    g_device_reg[MPU6050_WHO_AM_I] = 0x68U;
    uint8_t who = 0U;
    bool ok = i2c_reg_read(MPU6050_ADDR, MPU6050_WHO_AM_I, &who);
    printf("  WHO_AM_I = 0x%02X  (ok=%d, expected=0x68)\n\n", who, ok);

    printf("── Single register write ──\n");
    ok = i2c_reg_write(MPU6050_ADDR, MPU6050_PWR_MGMT1, 0x00U);
    printf("  PWR_MGMT1 write ok=%d, stored=0x%02X\n\n",
           ok, g_device_reg[MPU6050_PWR_MGMT1]);

    printf("── Burst read: accelerometer data ──\n");
    accel_data_t acc;
    ok = mpu6050_read_accel(&acc);
    printf("  accel: ax=%d  ay=%d  az=%d  (ok=%d)\n\n",
           acc.ax, acc.ay, acc.az, ok);

    printf("── EEPROM ACK polling demo ──\n");
    eeprom_wait_ready(0x50U);

    printf("\nEXERCISES:\n");
    printf("  1. Add i2c_reg_write_16() and i2c_reg_read_16() for 16-bit\n");
    printf("     register values used by BMP280.\n");
    printf("  2. Implement error recovery: after a NAK in the middle of a\n");
    printf("     transaction, generate STOP + optional repeated attempts.\n");
    printf("  3. Write an HDC1080 humidity/temperature driver using only\n");
    printf("     i2c_reg_write / i2c_burst_read.\n");
    return 0;
}
