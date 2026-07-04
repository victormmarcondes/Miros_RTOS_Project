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
  * Dev_t é usado diretamente como endereço I2C de 8 bits (ex: 0x52).
  * Defina o sensor no seu .h como:
  *
  *     static Dev_t sensor = 0x52;  // 7-bit addr 0x29, shifted: 0x52
  *
  * O hi2c1 é obtido via BSP (stm32g4xx_nucleo_bus.c), inicializado
  * com BSP_I2C1_Init() antes de usar o sensor.
  *
  ******************************************************************************
  */

#include "platform.h"
#include "stm32g4xx_hal.h"

/* ── Configuração ────────────────────────────────────────────────────────── */

/* hi2c1 declarado em stm32g4xx_nucleo_bus.c */
extern I2C_HandleTypeDef hi2c1;
#define I2C_HANDLE      hi2c1

/* Timeout HAL em ms */
#define I2C_TIMEOUT_MS  100U

/* ── Helpers internos ────────────────────────────────────────────────────── */

static uint8_t platform_write(uint16_t i2c_addr,
                              uint16_t reg,
                              const uint8_t *pData,
                              uint16_t len)
{
    /* Monta pacote: [reg_MSB, reg_LSB, data...] */
    uint8_t buf[6];

    if (len > 4U) { return 255U; }

    buf[0] = (uint8_t)(reg >> 8);
    buf[1] = (uint8_t)(reg & 0xFF);

    for (uint16_t i = 0; i < len; i++)
    {
        buf[2U + i] = pData[i];
    }

    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(
        &I2C_HANDLE,
        i2c_addr,
        buf,
        (uint16_t)(2U + len),
        I2C_TIMEOUT_MS);

    return (ret == HAL_OK) ? 0U : 255U;
}

static uint8_t platform_read(uint16_t i2c_addr,
                             uint16_t reg,
                             uint8_t *pData,
                             uint16_t len)
{
    uint8_t addr[2];
    addr[0] = (uint8_t)(reg >> 8);
    addr[1] = (uint8_t)(reg & 0xFF);

    /* Envia endereço do registrador */
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(
        &I2C_HANDLE,
        i2c_addr,
        addr,
        2U,
        I2C_TIMEOUT_MS);

    if (ret != HAL_OK) { return 255U; }

    /* Lê os bytes */
    ret = HAL_I2C_Master_Receive(
        &I2C_HANDLE,
        i2c_addr,
        pData,
        len,
        I2C_TIMEOUT_MS);

    return (ret == HAL_OK) ? 0U : 255U;
}

/* ── API pública ─────────────────────────────────────────────────────────── */

uint8_t VL53L4CD_RdDWord(Dev_t dev, uint16_t RegisterAdress, uint32_t *value)
{
    uint8_t buf[4];
    uint8_t status = platform_read((uint16_t)dev, RegisterAdress, buf, 4U);

    if (status == 0U)
    {
        /* Sensor envia big-endian → reconstrói no host (little-endian) */
        *value = ((uint32_t)buf[0] << 24)
               | ((uint32_t)buf[1] << 16)
               | ((uint32_t)buf[2] <<  8)
               |  (uint32_t)buf[3];
    }

    return status;
}

uint8_t VL53L4CD_RdWord(Dev_t dev, uint16_t RegisterAdress, uint16_t *value)
{
    uint8_t buf[2];
    uint8_t status = platform_read((uint16_t)dev, RegisterAdress, buf, 2U);

    if (status == 0U)
    {
        *value = ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
    }

    return status;
}

uint8_t VL53L4CD_RdByte(Dev_t dev, uint16_t RegisterAdress, uint8_t *value)
{
    uint8_t buf[1];
    uint8_t status = platform_read((uint16_t)dev, RegisterAdress, buf, 1U);

    if (status == 0U)
    {
        *value = buf[0];
    }

    return status;
}

uint8_t VL53L4CD_WrByte(Dev_t dev, uint16_t RegisterAdress, uint8_t value)
{
    uint8_t buf[1] = { value };
    return platform_write((uint16_t)dev, RegisterAdress, buf, 1U);
}

uint8_t VL53L4CD_WrWord(Dev_t dev, uint16_t RegisterAdress, uint16_t value)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)(value & 0xFF);

    return platform_write((uint16_t)dev, RegisterAdress, buf, 2U);
}

uint8_t VL53L4CD_WrDWord(Dev_t dev, uint16_t RegisterAdress, uint32_t value)
{
    uint8_t buf[4];
    buf[0] = (uint8_t)(value >> 24);
    buf[1] = (uint8_t)(value >> 16);
    buf[2] = (uint8_t)(value >>  8);
    buf[3] = (uint8_t)(value & 0xFF);

    return platform_write((uint16_t)dev, RegisterAdress, buf, 4U);
}

uint8_t VL53L4CD_WaitMs(Dev_t dev, uint32_t TimeMs)
{
    (void)dev;
    HAL_Delay(TimeMs);
    return 0U;
}