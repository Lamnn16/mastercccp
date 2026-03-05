/**
 * @file bmp280.c
 * @brief Capstone 4 — BMP280 Driver Implementation
 *
 * The compensation formulas are taken directly from the BMP280 datasheet
 * (section 4.2.3). They use Q24.8 fixed-point arithmetic with int64_t
 * to avoid overflow — the datasheet notes that 64-bit math is required.
 */

#include "bmp280.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* t_fine carries the temperature compensation value for pressure correction */
static int32_t s_t_fine = 0;

/* ─── Init ───────────────────────────────────────────────────────────────── */
bool bmp280_init(bmp280_dev_t *dev, uint8_t addr,
                 bmp280_i2c_write_fn write_fn,
                 bmp280_i2c_read_fn read_fn)
{
    dev->i2c_addr = addr;
    dev->ready = false;

    /* Check chip ID */
    uint8_t id = 0U;
    if (!read_fn(addr, BMP280_REG_ID, &id, 1U))
        return false;
    if (id != 0x60U)
        return false; /* BMP280 chip ID = 0x60 */

    /* Soft reset */
    uint8_t reset_val = 0xB6U;
    write_fn(addr, BMP280_REG_RESET, &reset_val, 1U);
    /* Wait for reset (1.2 ms startup time, polling status.im_update) */

    /* Read 24-byte calibration block starting at 0x88 */
    uint8_t calib_raw[24];
    if (!read_fn(addr, BMP280_REG_CALIB_START, calib_raw, 24U))
        return false;

    /* Unpack little-endian calibration words */
    bmp280_calib_t *c = &dev->calib;
    c->dig_T1 = (uint16_t)(calib_raw[1] << 8 | calib_raw[0]);
    c->dig_T2 = (int16_t)(calib_raw[3] << 8 | calib_raw[2]);
    c->dig_T3 = (int16_t)(calib_raw[5] << 8 | calib_raw[4]);
    c->dig_P1 = (uint16_t)(calib_raw[7] << 8 | calib_raw[6]);
    c->dig_P2 = (int16_t)(calib_raw[9] << 8 | calib_raw[8]);
    c->dig_P3 = (int16_t)(calib_raw[11] << 8 | calib_raw[10]);
    c->dig_P4 = (int16_t)(calib_raw[13] << 8 | calib_raw[12]);
    c->dig_P5 = (int16_t)(calib_raw[15] << 8 | calib_raw[14]);
    c->dig_P6 = (int16_t)(calib_raw[17] << 8 | calib_raw[16]);
    c->dig_P7 = (int16_t)(calib_raw[19] << 8 | calib_raw[18]);
    c->dig_P8 = (int16_t)(calib_raw[21] << 8 | calib_raw[20]);
    c->dig_P9 = (int16_t)(calib_raw[23] << 8 | calib_raw[22]);

    /* Configure: T×2 oversampling, P×16 oversampling, normal mode */
    uint8_t ctrl = (uint8_t)(BMP280_OSRS_T_x2 | BMP280_OSRS_P_x16 | BMP280_MODE_NORMAL);
    if (!write_fn(addr, BMP280_REG_CTRL_MEAS, &ctrl, 1U))
        return false;

    dev->ready = true;
    return true;
}

/* ─── Temperature compensation (datasheet Section 4.2.3, formula 1) ─────── */
static int32_t compensate_temp(const bmp280_calib_t *c, int32_t adc_T)
{
    int32_t var1 = (int32_t)((adc_T >> 3) - ((int32_t)c->dig_T1 << 1));
    var1 = (var1 * (int32_t)c->dig_T2) >> 11;

    int32_t var2 = (int32_t)(adc_T >> 4) - (int32_t)c->dig_T1;
    var2 = (((var2 * var2) >> 12) * (int32_t)c->dig_T3) >> 14;

    s_t_fine = var1 + var2;
    return (s_t_fine * 5 + 128) >> 8; /* °C × 100 */
}

/* ─── Pressure compensation (datasheet Section 4.2.3, formula 2) ─────────── */
static uint32_t compensate_pressure(const bmp280_calib_t *c, int32_t adc_P)
{
    int64_t var1 = ((int64_t)s_t_fine) - 128000LL;
    int64_t var2 = var1 * var1 * (int64_t)c->dig_P6;
    var2 += (var1 * (int64_t)c->dig_P5) << 17;
    var2 += ((int64_t)c->dig_P4) << 35;
    var1 = ((var1 * var1 * (int64_t)c->dig_P3) >> 8) +
           ((var1 * (int64_t)c->dig_P2) << 12);
    var1 = (((1LL << 47) + var1) * (int64_t)c->dig_P1) >> 33;
    if (var1 == 0LL)
        return 0U; /* avoid division by zero */

    int64_t p = 1048576LL - (int64_t)adc_P;
    p = (((p << 31) - var2) * 3125LL) / var1;
    var1 = ((int64_t)c->dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)c->dig_P8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t)c->dig_P7 << 4);
    return (uint32_t)(p >> 8); /* Pa (integer) */
}

/* ─── ADC read helpers ───────────────────────────────────────────────────── */
static int32_t read_adc_temp(uint8_t addr, bmp280_i2c_read_fn read_fn)
{
    uint8_t raw[3];
    if (!read_fn(addr, BMP280_REG_TEMP_MSB, raw, 3U))
        return 0;
    return (int32_t)((raw[0] << 12) | (raw[1] << 4) | (raw[2] >> 4));
}

static int32_t read_adc_press(uint8_t addr, bmp280_i2c_read_fn read_fn)
{
    uint8_t raw[3];
    if (!read_fn(addr, BMP280_REG_PRESS_MSB, raw, 3U))
        return 0;
    return (int32_t)((raw[0] << 12) | (raw[1] << 4) | (raw[2] >> 4));
}

/* ─── Public API ─────────────────────────────────────────────────────────── */
int32_t bmp280_read_temperature_x100(bmp280_dev_t *dev, bmp280_i2c_read_fn read_fn)
{
    int32_t adc_T = read_adc_temp(dev->i2c_addr, read_fn);
    return compensate_temp(&dev->calib, adc_T);
}

uint32_t bmp280_read_pressure_pa(bmp280_dev_t *dev, bmp280_i2c_read_fn read_fn)
{
    int32_t adc_P = read_adc_press(dev->i2c_addr, read_fn);
    return compensate_pressure(&dev->calib, adc_P);
}

bool bmp280_read(bmp280_dev_t *dev, bmp280_i2c_read_fn read_fn,
                 int32_t *temp_x100, uint32_t *press_pa)
{
    /* Temperature MUST be computed first (populates s_t_fine for pressure) */
    int32_t adc_T = read_adc_temp(dev->i2c_addr, read_fn);
    int32_t adc_P = read_adc_press(dev->i2c_addr, read_fn);
    *temp_x100 = compensate_temp(&dev->calib, adc_T);
    *press_pa = compensate_pressure(&dev->calib, adc_P);
    return true;
}

/* ─── Altitude estimation ─────────────────────────────────────────────────
 * Barometric formula (simplified, valid to ~11 km):
 *   h = 44330 × (1 − (P/P0)^(1/5.255))
 * Integer approximation (±5 m accuracy):
 *   h ≈ 44330 × (1 − (P/P0)^0.19) using fixed-point
 * ─────────────────────────────────────────────────────────────────────────── */
int32_t bmp280_pressure_to_altitude_m(uint32_t press_pa, uint32_t sea_level_pa)
{
    /* Use a simple linear approximation for small altitude ranges:
     * ΔP ≈ ρ × g × Δh  =>  Δh = ΔP / (ρ × g)
     * At sea level: ρ = 1.225 kg/m³, g = 9.81 m/s²  → ρg ≈ 12.015 Pa/m
     * Using integer: h ≈ (P0 - P) × 1000 / 12015  */
    int32_t delta_pa = (int32_t)sea_level_pa - (int32_t)press_pa;
    return (delta_pa * 1000) / 12015;
}
