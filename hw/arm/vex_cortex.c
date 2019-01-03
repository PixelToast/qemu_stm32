/*
 * STM32F103VD based VEX Cortex Microcontroller
 *
 * Copyright (C) 2019 Andre Lipke
 *
 * Implementation based on
 * Olimex "STM-P103 Development Board Users Manual Rev. A, April 2008"
 *
 * Andre Beckus
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

#include "hw/arm/stm32.h"
#include "hw/arm/vex_cortex.h"
#include "hw/sysbus.h"
#include "hw/arm/arm.h"
#include "hw/devices.h"
#include "ui/console.h"
#include "sysemu/sysemu.h"
#include "hw/boards.h"

static void vex_cortex_init(MachineState *machine)
{
    const char* kernel_filename = machine->kernel_filename;

    qemu_irq *pic = stm32_init(393215, 65535, kernel_filename, 8000000, 32768);

    DeviceState *mgr = vex_mgr_create(pic);

    DeviceState *gpio_a = DEVICE(object_resolve_path("/machine/stm32/gpio[a]", NULL));
    DeviceState *gpio_c = DEVICE(object_resolve_path("/machine/stm32/gpio[c]", NULL));
    DeviceState *uart2 = DEVICE(object_resolve_path("/machine/stm32/uart[2]", NULL));
    DeviceState *uart1 = DEVICE(object_resolve_path("/machine/stm32/uart[1]", NULL));
    DeviceState *uart3 = DEVICE(object_resolve_path("/machine/stm32/uart[3]", NULL));
    assert(gpio_a);
    assert(gpio_c);
    assert(uart2);
    assert(uart1);
    assert(uart3);

    stm32_uart_connect(
        (Stm32Uart *)uart1,
        serial_hds[0],
        STM32_USART1_NO_REMAP);


 }

static QEMUMachine vex_cortex_machine = {
    .name = "vex-cortex",
    .desc = "VEX Cortex Microcontroller",
    .init = vex_cortex_init,
};


static void vex_cortex_machine_init(void)
{
    qemu_register_machine(&vex_cortex_machine);
}

machine_init(vex_cortex_machine_init);
