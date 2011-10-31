/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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

#ifndef HARDWARE_PROFILE_PIC18F46J50_PIM_H
#define HARDWARE_PROFILE_PIC18F46J50_PIM_H

/* USB stack hardware selection options */

//#define USE_SELF_POWER_SENSE_IO
#define tris_self_power		TRISCbits.TRISC2    // Input
#define self_power		1

//#define USE_USB_BUS_SENSE_IO
#define tris_usb_bus_sense	TRISCbits.TRISC2    // Input
#define USB_BUS_SENSE		1

//#define PROGRAMMABLE_WITH_USB_HID_BOOTLOADER

#define DEMO_BOARD		PIC18F46J50_PIM
#define PIC18F46J50_PIM
#define CLOCK_FREQ		48000000

#endif