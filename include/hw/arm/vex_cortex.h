/*
 * STM32 Microcontroller
 *
 * Copyright (C) 2010 Andre Beckus
 * Copyright (C) 2014 Andrew Hankins
 *
 * Implementation based on ST Microelectronics "RM0008 Reference Manual Rev 10"
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef VEX_CORTEX_H
#define VEX_CORTEX_H

#include "qemu/timer.h"
#include "hw/arm/arm.h"
#include "qemu-common.h"
#include "hw/sysbus.h"
#include "qemu/log.h"


DeviceState *
vex_mgr_create(qemu_irq *pic);

#endif /* VEX_CORTEX_H */
