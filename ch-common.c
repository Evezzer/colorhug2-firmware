/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2013 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ColorHug.h"

#include "ch-common.h"
#include "ch-flash.h"
#include "ch-mcdc04.h"
#include "ch-sram.h"
#include "ch-temp.h"

/**
 * CHugFatalError:
 **/
void
CHugFatalError (ChError error)
{
	uint8_t i;

	/* turn off watchdog */
	WDTCONbits.SWDTEN = 0;
	TRISE = 0x3c;

	while (1) {
		for (i = 0; i < error; i++) {
			PORTE = CH_STATUS_LED_RED;
			Delay10KTCYx(0xff);
			PORTE = 0x00;
			Delay10KTCYx(0xff);
		}
		Delay10KTCYx(0xff);
		Delay10KTCYx(0xff);
	}
}

/**
 * CHugCalculateChecksum:
 **/
uint8_t
CHugCalculateChecksum(uint8_t *data, uint8_t length)
{
	int i;
	uint8_t checksum = 0xff;
	for (i = 0; i < length; i++)
		checksum ^= data[i];
	return checksum;
}

/**
 * CHugSelfTestSram:
 **/
static uint8_t
CHugSelfTestSram(void)
{
	uint8_t sram_dma[4];
	uint8_t sram_tmp;

	/* check SRAM */
	CHugSramWriteByte(0x0000, 0xde);
	CHugSramWriteByte(0x0001, 0xad);
	CHugSramWriteByte(0x0002, 0xbe);
	CHugSramWriteByte(0x0003, 0xef);
	sram_tmp = CHugSramReadByte(0x0000);
	if (sram_tmp != 0xde)
		return CH_ERROR_SRAM_FAILED;
	sram_tmp = CHugSramReadByte(0x0001);
	if (sram_tmp != 0xad)
		return CH_ERROR_SRAM_FAILED;

	/* test DMA to and from SRAM */
	sram_dma[0] = 0xde;
	sram_dma[1] = 0xad;
	sram_dma[2] = 0xbe;
	sram_dma[3] = 0xef;
	CHugSramDmaFromCpu(sram_dma, 0x0010, 3);
	CHugSramDmaWait();
	sram_dma[0] = 0x00;
	sram_dma[1] = 0x00;
	sram_dma[2] = 0x00;
	sram_dma[3] = 0x00;
	CHugSramDmaToCpu(0x0010, sram_dma, 3);
	CHugSramDmaWait();

	if (sram_dma[0] != 0xde &&
	    sram_dma[1] != 0xad &&
	    sram_dma[2] != 0xbe &&
	    sram_dma[3] != 0xef)
		return CH_ERROR_SRAM_FAILED;

	/* success */
	return CH_ERROR_NONE;
}

/**
 * CHugSelfTestSensor:
 **/
static uint8_t
CHugSelfTestSensor(void)
{
	CHugMcdc04Context ctx;
	CHugPackedFloat xyz[3];
	uint8_t rc;

	/* setup MCDC04 */
	CHugMcdc04Init(&ctx);
	CHugMcdc04SetTINT(&ctx, CH_MCDC04_TINT_128);
	CHugMcdc04SetIREF(&ctx, CH_MCDC04_IREF_20);
	CHugMcdc04SetDIV(&ctx, CH_MCDC04_DIV_DISABLE);
	rc = CHugMcdc04WriteConfig(&ctx);
	if (rc != CH_ERROR_NONE)
		return rc;

	/* measure red, green and blue */
	rc = CHugMcdc04TakeReadings(&ctx,
				    &xyz[CH_COLOR_OFFSET_RED],
				    &xyz[CH_COLOR_OFFSET_GREEN],
				    &xyz[CH_COLOR_OFFSET_BLUE]);
	if (rc != CH_ERROR_NONE)
		return rc;

	/* are all the results invalid? */
	if (xyz[CH_COLOR_OFFSET_RED].raw < 0x20)
		return CH_ERROR_SELF_TEST_RED;
	if (xyz[CH_COLOR_OFFSET_GREEN].raw < 0x20)
		return CH_ERROR_SELF_TEST_GREEN;
	if (xyz[CH_COLOR_OFFSET_BLUE].raw < 0x20)
		return CH_ERROR_SELF_TEST_BLUE;

	/* are any results skewed */
	rc = CHugMcdc04TakeReadingsAuto(&ctx,
					&xyz[CH_COLOR_OFFSET_RED],
					&xyz[CH_COLOR_OFFSET_GREEN],
					&xyz[CH_COLOR_OFFSET_BLUE]);
	if (rc != CH_ERROR_NONE)
		return rc;
	if (4 * xyz[CH_COLOR_OFFSET_RED].raw < xyz[CH_COLOR_OFFSET_GREEN].raw)
		return CH_ERROR_SELF_TEST_RED;
	if (4 * xyz[CH_COLOR_OFFSET_GREEN].raw < xyz[CH_COLOR_OFFSET_GREEN].raw)
		return CH_ERROR_SELF_TEST_GREEN;
	if (4 * xyz[CH_COLOR_OFFSET_BLUE].raw < xyz[CH_COLOR_OFFSET_GREEN].raw)
		return CH_ERROR_SELF_TEST_BLUE;

	return CH_ERROR_NONE;
}

/**
 * CHugSelfTestI2C:
 **/
static uint8_t
CHugSelfTestI2C(void)
{
	uint8_t i = 0x00;

	/* test the i2c bus */
	SSP1CON2bits.SEN = 1;
	while (SSP1CON2bits.SEN) {
		if (++i == 0)
			return CH_ERROR_SELF_TEST_I2C;
	}
	PIR1bits.SSP1IF = 0;
	SSP1BUF = 0x00;
	while (!PIR1bits.SSP1IF) {
		if (++i == 0)
			return CH_ERROR_SELF_TEST_I2C;
	}
	SSP1CON2bits.PEN = 1;
	while (SSP1CON2bits.PEN) {
		if (++i == 0)
			return CH_ERROR_SELF_TEST_I2C;
	}
	return CH_ERROR_NONE;
}

/**
 * CHugSelfTestFlash:
 **/
static uint8_t
CHugSelfTestFlash(void)
{
	uint8_t tmp[3] = { 'C', 'H', '\0' };

	CHugFlashErase(CH_EEPROM_ADDR_OWNER, 3);
	CHugFlashWrite(CH_EEPROM_ADDR_OWNER, 3, tmp);
	tmp[0] = 'H';
	tmp[1] = 'C';
	tmp[2] = '\0';
	CHugFlashRead(CH_EEPROM_ADDR_OWNER, 3, tmp);
	if (tmp[0] != 'C' && tmp[1] != 'H' && tmp[1] != '\0')
		return CH_ERROR_SELF_TEST_EEPROM;

	return CH_ERROR_NONE;
}

/**
 * CHugSelfTest:
 **/
uint8_t
CHugSelfTest(void)
{
	CHugPackedFloat xyz[3];
	CHugPackedFloat tmp;
	uint8_t rc;

	/* check SRAM */
	rc = CHugSelfTestSram();
	if (rc != CH_ERROR_NONE)
		goto out;

	/* check I2C */
	rc = CHugSelfTestI2C();
	if (rc != CH_ERROR_NONE)
		goto out;

	/* get the sensor temperature */
	rc = CHugTempGetAmbient(&tmp);
	if (rc != CH_ERROR_NONE)
		goto out;
	if (tmp.offset > 50 || tmp.offset < 10) {
		rc = CH_ERROR_SELF_TEST_TEMPERATURE;
		goto out;
	}

	/* test the flash reading and writing */
	rc = CHugSelfTestFlash();
	if (rc != CH_ERROR_NONE)
		goto out;

	/* test the sensor */
	rc = CHugSelfTestSensor();
	if (rc != CH_ERROR_NONE)
		goto out;

	/* success */
	rc = CH_ERROR_NONE;
out:
	return rc;
}
