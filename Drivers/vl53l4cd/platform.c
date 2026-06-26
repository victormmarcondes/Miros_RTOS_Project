
/**
  ******************************************************************************
  * @file    platform.c
  * @brief   VL53L4CD platform I2C implementation for STM32 Nucleo G474RE (HAL)
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  *
  * CONFIGURATION:
  *   - Adjust I2C_HANDLE to match the I2C instance used (hi2c1, hi2c2, etc.)
  *   - Adjust VL53L4CD_I2C_ADDR if XSHUT/address pin is configured differently
  *   - I2C timeout is set via I2C_TIMEOUT_MS (default: 100 ms)
  *
  * The VL53L4CD uses 16-bit register addresses and big-endian data.
  * This implementation is for a LITTLE-ENDIAN platform (Cortex-M4),
  * so byte swapping is performed where needed.
  ******************************************************************************
  */

#include "platform.h"
#include "stm32g4xx_hal.h"   /* Adjust to your STM32 family if different */

/* ── User configuration ─────────────────────────────────────────────────── */

/* I2C handle declared in main.c (or wherever CubeMX placed it) */
extern I2C_HandleTypeDef hi2c1;
#define I2C_HANDLE      hi2c1

/* Default 8-bit I2C address (7-bit = 0x29, shifted left = 0x52) */
#define VL53L4CD_I2C_ADDR   (0x29 << 1)   /* 0x52 */

/* HAL timeout in milliseconds */
#define I2C_TIMEOUT_MS      100U

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/**
 * @brief  Write a 16-bit register address followed by a data buffer over I2C.
 *         The VL53L4CD register address is big-endian (MSB first).
 *
 * @param  RegisterAdress  16-bit register address
 * @param  pData           Pointer to data to write (after address)
 * @param  dataLen         Number of data bytes to write
 * @return 0 on success, non-zero on error
 */
static uint8_t platform_write(uint16_t RegisterAdress,
                              const uint8_t *pData,
                              uint16_t dataLen)
{
    /* Build the transmit buffer: [addr_MSB, addr_LSB, data...] */
    uint8_t buf[6]; /* 2 bytes addr + up to 4 bytes data */

    if (dataLen > 4U)
    {
        return 255U; /* Safety guard – all calls stay within 4 data bytes */
    }

    buf[0] = (uint8_t)(RegisterAdress >> 8);   /* Address MSB */
    buf[1] = (uint8_t)(RegisterAdress & 0xFF); /* Address LSB */

    for (uint16_t i = 0; i < dataLen; i++)
    {
        buf[2U + i] = pData[i];
    }

    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(
        &I2C_HANDLE,
        VL53L4CD_I2C_ADDR,
        buf,
        (uint16_t)(2U + dataLen),
        I2C_TIMEOUT_MS);

    return (ret == HAL_OK) ? 0U : 255U;
}

/**
 * @brief  Write the 16-bit register address, then read a data buffer over I2C.
 *
 * @param  RegisterAdress  16-bit register address
 * @param  pData           Buffer to store received data
 * @param  dataLen         Number of bytes to read
 * @return 0 on success, non-zero on error
 */
static uint8_t platform_read(uint16_t RegisterAdress,
                             uint8_t *pData,
                             uint16_t dataLen)
{
    uint8_t addr[2];
    addr[0] = (uint8_t)(RegisterAdress >> 8);
    addr[1] = (uint8_t)(RegisterAdress & 0xFF);

    /* Send register address (no STOP between write and read) */
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(
        &I2C_HANDLE,
        VL53L4CD_I2C_ADDR,
        addr,
        2U,
        I2C_TIMEOUT_MS);

    if (ret != HAL_OK)
    {
        return 255U;
    }

    /* Read the requested bytes */
    ret = HAL_I2C_Master_Receive(
        &I2C_HANDLE,
        VL53L4CD_I2C_ADDR,
        pData,
        dataLen,
        I2C_TIMEOUT_MS);

    return (ret == HAL_OK) ? 0U : 255U;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

uint8_t VL53L4CD_RdDWord(Dev_t dev, uint16_t RegisterAdress, uint32_t *value)
{
    (void)dev; /* Unused – single device on bus */

    uint8_t buf[4];
    uint8_t status = platform_read(RegisterAdress, buf, 4U);

    if (status == 0U)
    {
        /* Sensor sends big-endian; reconstruct as host uint32_t */
        *value = ((uint32_t)buf[0] << 24)
               | ((uint32_t)buf[1] << 16)
               | ((uint32_t)buf[2] <<  8)
               |  (uint32_t)buf[3];
    }

    return status;
}

uint8_t VL53L4CD_RdWord(Dev_t dev, uint16_t RegisterAdress, uint16_t *value)
{
    (void)dev;

    uint8_t buf[2];
    uint8_t status = platform_read(RegisterAdress, buf, 2U);

    if (status == 0U)
    {
        *value = ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
    }

    return status;
}

uint8_t VL53L4CD_RdByte(Dev_t dev, uint16_t RegisterAdress, uint8_t *value)
{
    (void)dev;

    uint8_t buf[1];
    uint8_t status = platform_read(RegisterAdress, buf, 1U);

    if (status == 0U)
    {
        *value = buf[0];
    }

    return status;
}

uint8_t VL53L4CD_WrByte(Dev_t dev, uint16_t RegisterAdress, uint8_t value)
{
    (void)dev;

    uint8_t buf[1] = { value };
    return platform_write(RegisterAdress, buf, 1U);
}

uint8_t VL53L4CD_WrWord(Dev_t dev, uint16_t RegisterAdress, uint16_t value)
{
    (void)dev;

    /* Big-endian on the wire */
    uint8_t buf[2];
    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)(value & 0xFF);

    return platform_write(RegisterAdress, buf, 2U);
}

uint8_t VL53L4CD_WrDWord(Dev_t dev, uint16_t RegisterAdress, uint32_t value)
{
    (void)dev;

    uint8_t buf[4];
    buf[0] = (uint8_t)(value >> 24);
    buf[1] = (uint8_t)(value >> 16);
    buf[2] = (uint8_t)(value >>  8);
    buf[3] = (uint8_t)(value & 0xFF);

    return platform_write(RegisterAdress, buf, 4U);
}

uint8_t VL53L4CD_WaitMs(Dev_t dev, uint32_t TimeMs)
{
    (void)dev;

    HAL_Delay(TimeMs);
    return 0U;
}
