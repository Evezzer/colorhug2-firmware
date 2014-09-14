/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2014 Richard Hughes <richard@hughsie.com>
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

#include <i2c.h>

#include "ColorHug.h"

#include "ch-mcdc04.h"

#define MCDC04_SLAVE_ADDRESS_READ		0b11101001
#define MCDC04_SLAVE_ADDRESS_WRITE		0b11101000

typedef enum {
	MCDC04_ADDR_OSR			= 0x00,	/* wo */
	MCDC04_ADDR_AGEN		= 0x02,	/* ro */
	MCDC04_ADDR_CREGL		= 0x06,	/* rw */
	MCDC04_ADDR_CREGH		= 0x07,	/* rw */
	MCDC04_ADDR_OPTREG		= 0x08,	/* rw */
	MCDC04_ADDR_BREAK		= 0x09,	/* rw */
	MCDC04_ADDR_EDGES		= 0x0a	/* rw */
} Mcdc04Addr;

/**
 * CHugMcdc04Init:
 **/
void
CHugMcdc04Init (CHugMcdc04Context *ctx)
{
	ctx->tint = CH_MCDC04_TINT_512;
	ctx->iref = CH_MCDC04_IREF_20;
	ctx->div = CH_MCDC04_DIV_DISABLE;
}

/**
 * CHugMcdc04SetTINT:
 **/
void
CHugMcdc04SetTINT (CHugMcdc04Context *ctx, CHugMcdc04Tint tint)
{
	ctx->tint = tint;
}

/**
 * CHugMcdc04SetIREF:
 **/
void
CHugMcdc04SetIREF (CHugMcdc04Context *ctx, CHugMcdc04Iref iref)
{
	ctx->iref = iref;
}

/**
 * CHugMcdc04SetIREF:
 **/
void
CHugMcdc04SetDIV (CHugMcdc04Context *ctx, CHugMcdc04Div div)
{
	ctx->div = div;
}

/**
 * CHugMcdc04WriteConfig:
 **/
uint8_t
CHugMcdc04WriteConfig (CHugMcdc04Context *ctx)
{
	uint8_t tmp;
	uint8_t rc;

	/* start */
	StartI2C1();

	/* send slave address */
	rc = WriteI2C1(MCDC04_SLAVE_ADDRESS_WRITE);
	if (rc != 0x00) {
		rc = CH_ERROR_I2C_SLAVE_ADDRESS;
		goto out;
	}

	/* send address pointer */
	rc = WriteI2C1(MCDC04_ADDR_OSR);
	if (rc != 0x00) {
		rc = CH_ERROR_I2C_SLAVE_CONFIG;
		goto out;
	}

	/* go to configuration mode */
	rc = WriteI2C1(0b00000010);
	if (rc != 0x00) {
		rc = CH_ERROR_I2C_SLAVE_CONFIG;
		goto out;
	}

	/* restart */
	RestartI2C1();

	/* send slave address */
	rc = WriteI2C1(MCDC04_SLAVE_ADDRESS_WRITE);
	if (rc != 0x00) {
		rc = CH_ERROR_I2C_SLAVE_ADDRESS;
		goto out;
	}

	/* set address pointer */
	rc = WriteI2C1(MCDC04_ADDR_CREGL);
	if (rc != 0x00) {
		rc = CH_ERROR_I2C_SLAVE_CONFIG;
		goto out;
	}

	/* send CREGL */
	tmp = 0b10000000;		/* DIR: 	anodes to input pins */
	tmp |= ctx->iref << 4;		/* R: 		ADC Reference Current */
	tmp |= ctx->tint;		/* T: 		Integration Time */
	WriteI2C1(tmp);

	/* send CREGH */
	tmp = 0b10000000;		/* ENTM:	disable access to OUTINT */
	tmp |= 0b0100000;		/* SB:		Standby Enable */
	tmp |= 0b0001000;		/* MODE:	CMD measurement mode */
	if (ctx->div != CH_MCDC04_DIV_DISABLE) {
		tmp |= ctx->div << 1;	/* DIV:		Divide higher bits */
		tmp |= 0b00000001;	/* ENDIV:	Enable divider */
	}
	WriteI2C1(tmp);

	/* send OPTREG */
	WriteI2C1(0x00);		/* ZERO:	Offset value */

	/* send BREAK */
	WriteI2C1(0x20);		/* BREAK:	1us to 255us */

	/* send EDGES */
	WriteI2C1(0x01); 		/* EDGES:	Only valid in SYND mode */
out:
	StopI2C1();
	return rc;
}

/**
 * CHugMcdc04TakeReadings:
 **/
uint8_t
CHugMcdc04TakeReadings (CHugMcdc04Context *ctx,
			CHugPackedFloat *x,
			CHugPackedFloat *y,
			CHugPackedFloat *z)
{
	uint16_t tmp;
	uint8_t rc;
	uint8_t tmp_lsb = 4;
	uint8_t tmp_msb = 2;
	uint32_t i;

	/* ensure the READY pin is high */
	if (PORTAbits.RA0 == 0)
		return CH_ERROR_OVERFLOW_SENSOR;

	/* start */
	StartI2C1();

	/* send slave address */
	rc = WriteI2C1(MCDC04_SLAVE_ADDRESS_WRITE);
	if (rc != 0x00) {
		rc = CH_ERROR_I2C_SLAVE_ADDRESS;
		goto out;
	}

	/* send address pointer */
	rc = WriteI2C1(MCDC04_ADDR_OSR);
	if (rc != 0x00) {
		rc = CH_ERROR_I2C_SLAVE_CONFIG;
		goto out;
	}

	/* start measurement */
	rc = WriteI2C1(0b10000011);
	if (rc != 0x00) {
		rc = CH_ERROR_I2C_SLAVE_CONFIG;
		goto out;
	}

	/* stop */
	StopI2C1();

	/* wait for READY pin */
	for (i = 0x80000; i > 0; i--) {
		if (PORTAbits.RA0)
			break;
	}
	if (i == 0x0) {
		rc = CH_ERROR_OVERFLOW_SENSOR;
		goto out;
	}

	/* start */
	StartI2C1();

	/* send slave address */
	rc = WriteI2C1(MCDC04_SLAVE_ADDRESS_WRITE);
	if (rc != 0x00) {
		rc = CH_ERROR_I2C_SLAVE_ADDRESS;
		goto out;
	}

	/* send address pointer */
	rc = WriteI2C1(MCDC04_ADDR_OSR);
	if (rc != 0x00) {
		rc = CH_ERROR_I2C_SLAVE_CONFIG;
		goto out;
	}

	/* reset the bus */
	RestartI2C1();

	/* send slave address */
	rc = WriteI2C1(MCDC04_SLAVE_ADDRESS_READ);
	if (rc != 0x00) {
		rc = CH_ERROR_I2C_SLAVE_ADDRESS;
		goto out;
	}

	/* read the XYZ color */
	x->raw = ReadI2C1();
	AckI2C1();
	x->raw |= ((uint16_t) ReadI2C1()) << 8;
	AckI2C1();
	y->raw = ReadI2C1();
	AckI2C1();
	y->raw |= ((uint16_t) ReadI2C1()) << 8;
	AckI2C1();
	z->raw = ReadI2C1();
	AckI2C1();
	z->raw |= ((uint16_t) ReadI2C1()) << 8;
	NotAckI2C1();
out:
	StopI2C1();
	return rc;
}
