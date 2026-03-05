/**
 * @file bmp280_app.c
 * @brief Capstone 4 — BMP280 Sensor Driver: Simulation and Verification
 *
 * Demonstrates the full driver lifecycle with simulated I2C callbacks:
 *   1. Init: chip ID check, calibration read, mode set
 *   2. Read temperature and pressure with compensation
 *   3. Compute altitude from pressure
 *   4. Verify results against known reference values from BMP280 datasheet
 *
 * The datasheet provides reference values in Appendix 8.2:
 *   adc_T = 519888,  compensation → 25.08°C
 *   adc_P = 415148,  compensation → 100653 Pa (1006.53 hPa)
 *
 * BUILD:  cmake --build build --target p8_sensor_driver
 */

#include "bmp280.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* ─── Simulated I2C callbacks ────────────────────────────────────────────── */
/* We pre-load the register map with known datasheet reference values */

static uint8_t s_reg_map[256];
static bool s_i2c_ready = false;

static void populate_sim_registers(void)
{
    memset(s_reg_map, 0, sizeof(s_reg_map));

    /* Chip ID */
    s_reg_map[BMP280_REG_ID] = 0x60U;

    /* Reference calibration values (from datasheet Appendix 8.1) */
    uint16_t T1 = 27504U;
    int16_t T2 = 26435, T3 = -1000;
    uint16_t P1 = 36477U;
    int16_t P2 = -10685, P3 = 3024, P4 = 2855, P5 = 140;
    int16_t P6 = -7, P7 = 15500, P8 = -14600, P9 = 6000;

    uint8_t *c = &s_reg_map[BMP280_REG_CALIB_START];
    c[0] = (uint8_t)(T1);
    c[1] = (uint8_t)(T1 >> 8);
    c[2] = (uint8_t)(T2);
    c[3] = (uint8_t)(T2 >> 8);
    c[4] = (uint8_t)(T3);
    c[5] = (uint8_t)(T3 >> 8);
    c[6] = (uint8_t)(P1);
    c[7] = (uint8_t)(P1 >> 8);
    c[8] = (uint8_t)(P2);
    c[9] = (uint8_t)(P2 >> 8);
    c[10] = (uint8_t)(P3);
    c[11] = (uint8_t)(P3 >> 8);
    c[12] = (uint8_t)(P4);
    c[13] = (uint8_t)(P4 >> 8);
    c[14] = (uint8_t)(P5);
    c[15] = (uint8_t)(P5 >> 8);
    c[16] = (uint8_t)(P6);
    c[17] = (uint8_t)(P6 >> 8);
    c[18] = (uint8_t)(P7);
    c[19] = (uint8_t)(P7 >> 8);
    c[20] = (uint8_t)(P8);
    c[21] = (uint8_t)(P8 >> 8);
    c[22] = (uint8_t)(P9);
    c[23] = (uint8_t)(P9 >> 8);

    /* Reference ADC values from datasheet Appendix 8.2:
     *   adc_T = 519888 = 0x7EE30  (20-bit, left-justified in 24-bit reg)
     *   adc_P = 415148 = 0x656AC
     * The register stores 20-bit value left-justified in 3 bytes:
     *   raw = adc << 4 (bits 23..4 of the 24-bit reg)
     */
    uint32_t adc_t = 519888UL; /* datasheet reference */
    uint32_t adc_p = 415148UL;
    uint32_t temp_raw = adc_t << 4;
    uint32_t press_raw = adc_p << 4;

    s_reg_map[BMP280_REG_TEMP_MSB] = (uint8_t)(temp_raw >> 16);
    s_reg_map[BMP280_REG_TEMP_MSB + 1] = (uint8_t)(temp_raw >> 8);
    s_reg_map[BMP280_REG_TEMP_MSB + 2] = (uint8_t)(temp_raw);
    s_reg_map[BMP280_REG_PRESS_MSB] = (uint8_t)(press_raw >> 16);
    s_reg_map[BMP280_REG_PRESS_MSB + 1] = (uint8_t)(press_raw >> 8);
    s_reg_map[BMP280_REG_PRESS_MSB + 2] = (uint8_t)(press_raw);

    s_i2c_ready = true;
}

static bool sim_i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, uint8_t len)
{
    (void)addr;
    for (uint8_t i = 0U; i < len; i++)
        s_reg_map[reg + i] = data[i];
    return true;
}

static bool sim_i2c_read(uint8_t addr, uint8_t reg, uint8_t *data, uint8_t len)
{
    (void)addr;
    if (!s_i2c_ready)
        return false;
    for (uint8_t i = 0U; i < len; i++)
        data[i] = s_reg_map[reg + i];
    return true;
}

int main(void)
{
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  Capstone 4 — BMP280 Sensor Driver                  ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    populate_sim_registers();

    bmp280_dev_t dev;
    bool ok = bmp280_init(&dev, BMP280_I2C_ADDR_0, sim_i2c_write, sim_i2c_read);
    printf("Init: %s  (chip_id=0x60)\n\n", ok ? "OK" : "FAILED");

    /* Read using reference ADC values */
    int32_t temp_x100 = 0;
    uint32_t press_pa = 0U;
    bmp280_read(&dev, sim_i2c_read, &temp_x100, &press_pa);

    printf("── Compensation results ──\n");
    printf("  Temperature: %d.%02d°C  (datasheet ref: 25.08°C)  %s\n",
           temp_x100 / 100, temp_x100 % 100,
           (temp_x100 >= 2500 && temp_x100 <= 2520) ? "PASS" : "CHECK");

    printf("  Pressure:    %u Pa  = %u.%02u hPa\n",
           press_pa, press_pa / 100U, press_pa % 100U);
    printf("  Datasheet ref: 100653 Pa (1006.53 hPa)  %s\n\n",
           (press_pa >= 100000U && press_pa <= 101000U) ? "PASS" : "CHECK");

    /* Altitude */
    int32_t alt_m = bmp280_pressure_to_altitude_m(press_pa, 101325U);
    printf("── Altitude estimate ──\n");
    printf("  P = %u Pa, P0 = 101325 Pa → altitude ≈ %d m\n\n", press_pa, alt_m);

    /* Multiple readings simulation */
    printf("── Simulated weather readings (vary adc_T) ──\n");
    int32_t adc_ts[] = {500000, 519888, 540000, 560000};
    for (int i = 0; i < 4; i++)
    {
        uint32_t raw = (uint32_t)((uint32_t)adc_ts[i] << 4);
        s_reg_map[BMP280_REG_TEMP_MSB] = (uint8_t)(raw >> 16);
        s_reg_map[BMP280_REG_TEMP_MSB + 1] = (uint8_t)(raw >> 8);
        s_reg_map[BMP280_REG_TEMP_MSB + 2] = (uint8_t)(raw);
        int32_t t = 0;
        uint32_t p = 0U;
        bmp280_read(&dev, sim_i2c_read, &t, &p);
        printf("  adc_T=%-7d → %d.%02d°C  %u Pa\n",
               adc_ts[i], t / 100, t % 100, p);
    }

    printf("\nEXERCISES:\n");
    printf("  1. Add HDC1080 humidity + temperature driver following the same\n");
    printf("     HAL callback pattern (i2c_write/read function pointers).\n");
    printf("  2. Use the barometric formula (not the linear approx used here)\n");
    printf("     to compute altitude more accurately above 500 m.\n");
    printf("  3. Wire up bmp280_init() to the real I2C driver from Module 7.\n");
    printf("     You only need to replace the callback functions.\n");
    return 0;
}
