/**
 * @file bmp280.h
 * @brief Capstone 4 — BMP280 Barometric Pressure Sensor Driver API
 *
 * BMP280 communicates over I2C or SPI, contains:
 *   - 20-bit pressure ADC    → hPa (resolution: 0.18 Pa)
 *   - 16-bit temperature ADC → °C  (resolution: 0.01°C)
 *   - Internal 88-byte calibration ROM
 *
 * Datasheet compensation formulas use Q24.8 fixed-point arithmetic.
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

/* I2C address: SDO pin = LOW → 0x76, HIGH → 0x77 */
#define BMP280_I2C_ADDR_0 0x76U
#define BMP280_I2C_ADDR_1 0x77U

/* Registers */
#define BMP280_REG_ID 0xD0U    /* should read 0x60 */
#define BMP280_REG_RESET 0xE0U /* write 0xB6 to soft-reset */
#define BMP280_REG_STATUS 0xF3U
#define BMP280_REG_CTRL_MEAS 0xF4U
#define BMP280_REG_CONFIG 0xF5U
#define BMP280_REG_PRESS_MSB 0xF7U   /* 3 bytes: msb, lsb, xlsb */
#define BMP280_REG_TEMP_MSB 0xFAU    /* 3 bytes: msb, lsb, xlsb */
#define BMP280_REG_CALIB_START 0x88U /* 24 bytes of calibration */

/* ctrl_meas register fields */
#define BMP280_OSRS_T_x2 (2U << 5)
#define BMP280_OSRS_P_x16 (5U << 2)
#define BMP280_MODE_NORMAL (3U << 0)

/* Calibration data (from datasheet section 4.2.2) */
typedef struct
{
    uint16_t dig_T1;
    int16_t dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
} bmp280_calib_t;

typedef struct
{
    uint8_t i2c_addr;
    bmp280_calib_t calib;
    bool ready;
} bmp280_dev_t;

/* Platform I2C callbacks (you provide these for your HAL) */
typedef bool (*bmp280_i2c_write_fn)(uint8_t addr, uint8_t reg, const uint8_t *data, uint8_t len);
typedef bool (*bmp280_i2c_read_fn)(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len);

bool bmp280_init(bmp280_dev_t *dev, uint8_t addr,
                 bmp280_i2c_write_fn write_fn,
                 bmp280_i2c_read_fn read_fn);

/* Returns temperature in °C × 100 (e.g. 2350 = 23.50°C) */
int32_t bmp280_read_temperature_x100(bmp280_dev_t *dev,
                                     bmp280_i2c_read_fn read_fn);

/* Returns pressure in Pa (e.g. 101325 = 1013.25 hPa) */
uint32_t bmp280_read_pressure_pa(bmp280_dev_t *dev,
                                 bmp280_i2c_read_fn read_fn);

/* Combined read (reuses temperature for pressure compensation) */
bool bmp280_read(bmp280_dev_t *dev, bmp280_i2c_read_fn read_fn,
                 int32_t *temp_x100, uint32_t *press_pa);

/* Altitude from pressure using barometric formula */
int32_t bmp280_pressure_to_altitude_m(uint32_t press_pa, uint32_t sea_level_pa);
