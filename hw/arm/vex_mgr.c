/*-
 * Copyright (c) 2013
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/*
 * QEMU DMA controller device model
 */
#include "hw/sysbus.h"
#include "hw/arm/stm32.h"
#include "hw/arm/vex_cortex.h"

//#define DEBUG_STM32_DMA
#ifdef DEBUG_VEX_MGR

// NOTE: The usleep() helps the MacOS stdout from freezing when we have a lot of print out
#define DPRINTF(fmt, ...)                                       \
	do { printf("VEX_MGR: " fmt , ## __VA_ARGS__); \
		 usleep(1000); \
	} while (0)
#else
#define DPRINTF(fmt, ...)
#endif

typedef struct {
    uint8_t axis[6];
    uint8_t button56;
    uint8_t button78;
} __attribute__((packed)) vex_sv_joystick;

typedef struct {
    uint8_t gameStatus;
    uint8_t mainBattery;
    uint8_t backupBattery;
    vex_sv_joystick joystick[2];
} __attribute__((packed)) vex_sv_status;

typedef struct vex_mgr {
	SysBusDevice busdev;
	MemoryRegion iomem;
	struct QEMUTimer *circular_timer;

	uint32_t cr;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t r1;
    uint32_t r2;

    bool svInitialized;
    bool svFirstSync;
    char robotName[8];
    vex_sv_status svStatus;

    qemu_irq *pic;
} vex_mgr;

static void vex_mgr_stream_circular_timer(void *opaque) {
    vex_mgr *s = (vex_mgr *)opaque;

    uint64_t curr_time = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    timer_mod(s->circular_timer, curr_time + 10000000);

    if (qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) > 1000 && !s->svInitialized) {
        printf("[QEMU] Poking SPI1 interrupt\n");
        s->svInitialized = true;
        s->r1 = 0;
        s->r2 = 0;
        qemu_irq_pulse(s->pic[STM32_SPI1_IRQ]);
    } else if (s->svInitialized) {
        s->r1 = 1;
        s->r2 = 0;
        qemu_irq_pulse(s->pic[STM32_SPI1_IRQ]);
    }
}

/* Register read. */
static uint64_t
vex_mgr_read(void *arg, hwaddr addr, unsigned int size)
{
	vex_mgr *s = (vex_mgr *)arg;

    if (size != 4 || (addr & 3) != 0) return 0;

    if (addr == 0) {
        return s->cr;
    } else if (addr == 4) {
        return s->a1;
    } else if (addr == 8) {
        return s->a2;
    } else if (addr == 12) {
        return s->a3;
    } else if (addr == 16) {
        return s->r1;
    } else if (addr == 20) {
        return s->r2;
    }

    return 0;
}

void writeWord(hwaddr addr, uint32_t value) {
    value = tswap32(value);
    cpu_physical_memory_write(addr, &value, 4);
}

/* Register write. */
static void
vex_mgr_write(void *arg, hwaddr addr, uint64_t data, unsigned int size)
{
    uint64_t curr_time = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    vex_mgr *s = (vex_mgr *)arg;

    if (size != 4 || (addr & 3) != 0) return;

    if (addr == 0) {
        s->cr = data;
        s->r1 = 0;
        s->r2 = 0;

        int mod = data >> 16;
        int fun = data & 0xFFFF;

        if (mod == 0) { // Serial
            if (fun == 0) { // init
                printf("[QEMU] Set UART%d to %d/%d\n", s->a1, s->a2, s->a3);
            } else if (fun == 1) { // shutdown
                printf("[QEMU] Shut down UART%d\n", s->a1);
            } else if (fun == 2) { // putc
                printf("%c", s->a2);
            }
        } else if (mod == 3) { // GPIO
            if (fun == 0) { // ADCInit
                printf("[QEMU] Initializing ADC\n");
            } else if (fun == 1) { // SetDir
                const char* mode = "unknown";
                if (s->a2 == 0) mode = "INPUT_ANALOG";
                if (s->a2 == 1) mode = "OUTPUT";
                if (s->a2 == 4) mode = "INPUT_FLOATING";
                if (s->a2 == 5) mode = "OUTPUT_OD";
                if (s->a2 == 9) mode = "AFO";
                if (s->a2 == 10) mode = "INPUT";

                printf("[QEMU] Set mode of pin %d to %s (%d)\n", s->a1, mode, s->a2);
            } else if (fun == 2) { // GetInput
            } else if (fun == 3) { // GetOutput
            } else if (fun == 4) { // SetOutput
                printf("[QEMU] Wrote %d to pin %d\n", s->a2, s->a1);
            } else if (fun == 5) { // SetInterupt
                printf("[QEMU] Set interrupt mask of pin %d to %d\n", s->a1, s->a2);
            }
        } else if (mod == 4) { // Motor
          if (fun == 0) { // get
              s->r1 = 127;
          } else if (fun == 1) { // set
              printf("[QEMU] Setting motor %d to %d\n", s->a1, s->a2);
          } else if (fun == 2) {
              printf("[QEMU] Stopping all motors\n");
          }
        } else if (mod == 5) { // Comp
            if (fun == 0) { // init
                printf("[QEMU] Starting supervisor clock\n");
                timer_mod(s->circular_timer, curr_time + 1000000);
            } else if (fun == 1) { // enableStandalone
                printf("[QEMU] Enabling standalone mode\n");
            } else if (fun == 2) { // setName
                cpu_physical_memory_read(s->a1, &s->robotName, 4);
                if (!s->svFirstSync) printf("[QEMU] Set robot name to '%c%c%c%c%c%c%c%c'\n", s->robotName[0], s->robotName[1], s->robotName[2], s->robotName[3], s->robotName[4], s->robotName[5], s->robotName[6], s->robotName[7]);
            } else if (fun == 3) { // getStatus
                if (!s->svFirstSync) printf("[QEMU] Dumping status to %08x\n", s->a1);
                cpu_physical_memory_write(s->a1, &s->svStatus, sizeof(vex_sv_status));
                s->svFirstSync = true;
            }
        } else if (mod == 6) { // System
            if (fun == 0) { // Exit
                exit(s->a1);
            } else if (fun == 1) { // Break

            }
        } else {
            printf("[QEMU] Unknown %d.%d(%d,%d,%d)\n", mod, fun, s->a1, s->a2, s->a3);
        }
    } else if (addr == 4) {
        s->a1 = data;
    } else if (addr == 8) {
        s->a2 = data;
    } else if (addr == 12) {
        s->a3 = data;
    } else return;
}

static const MemoryRegionOps vex_mgr_ops = {
	.read = vex_mgr_read,
	.write = vex_mgr_write,
	.endianness = DEVICE_NATIVE_ENDIAN,
	.impl = {
		.min_access_size = 1,
		.max_access_size = 4,
	}
};

DeviceState *
vex_mgr_create(qemu_irq *pic)
{
    DeviceState *dev;

    dev = qdev_create(NULL, "vex_mgr");
    vex_mgr *s = OBJECT_CHECK(vex_mgr, (dev), "vex_mgr");

    qdev_init_nofail(dev);
    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, 0x40021400);
    s->pic = pic;

    return dev;
}

static int
vex_mgr_init(SysBusDevice *dev)
{
	vex_mgr *s = OBJECT_CHECK(vex_mgr, (dev), "vex_mgr");

	memory_region_init_io(&s->iomem, OBJECT(s), &vex_mgr_ops, s, "mgr", 0xC00);
	sysbus_init_mmio(dev, &s->iomem);

	s->circular_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, (QEMUTimerCB *)vex_mgr_stream_circular_timer, s);
	s->svInitialized = false;
	s->svFirstSync = false;

	return 0;
}

static void
vex_mgr_reset(DeviceState *ds)
{
	vex_mgr *s = OBJECT_CHECK(vex_mgr, (ds), "vex_mgr");
    (void)s;
}

static Property vex_mgr_properties[] = {
	DEFINE_PROP_END_OF_LIST(),
	DEFINE_PROP_END_OF_LIST(),
};

static void
vex_mgr_class_init(ObjectClass *klass, void *data)
{
	DeviceClass *dc = DEVICE_CLASS(klass);
	SysBusDeviceClass *sc = SYS_BUS_DEVICE_CLASS(klass);
	sc->init = vex_mgr_init;
	dc->reset = vex_mgr_reset;
	dc->props = vex_mgr_properties;
}

static const TypeInfo
vex_mgr_info = {
	.name          = "vex_mgr",
	.parent        = TYPE_SYS_BUS_DEVICE,
	.instance_size = sizeof(vex_mgr),
	.class_init    = vex_mgr_class_init,
};

static void
vex_mgr_register_types(void)
{
	type_register_static(&vex_mgr_info);
}

type_init(vex_mgr_register_types)
