/*
 * AXP-221/221s PMU Emulation
 *
 * Copyright (C) 2023 qianfan Zhao <qianfanguijin@163.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/bitops.h"
#include "trace.h"
#include "hw/i2c/i2c.h"
#include "migration/vmstate.h"

#define TYPE_AXP221_PMU "axp221_pmu"

#define AXP221(obj) \
    OBJECT_CHECK(AXP221I2CState, (obj), TYPE_AXP221_PMU)

#define NR_REGS                         0xff

/* A simple I2C slave which returns values of ID or CNT register. */
typedef struct AXP221I2CState {
    /*< private >*/
    I2CSlave i2c;
    /*< public >*/
    uint8_t regs[NR_REGS];  /* peripheral registers */
    uint8_t ptr;            /* current register index */
    uint8_t count;          /* counter used for tx/rx */
} AXP221I2CState;

#define AXP221_PWR_STATUS_ACIN_PRESENT          BIT(7)
#define AXP221_PWR_STATUS_ACIN_AVAIL            BIT(6)
#define AXP221_PWR_STATUS_VBUS_PRESENT          BIT(5)
#define AXP221_PWR_STATUS_VBUS_USED             BIT(4)
#define AXP221_PWR_STATUS_BAT_CHARGING          BIT(2)
#define AXP221_PWR_STATUS_ACIN_VBUS_POWERED     BIT(1)

/* Reset all counters and load ID register */
static void axp221_reset_enter(Object *obj, ResetType type)
{
    AXP221I2CState *s = AXP221(obj);

    memset(s->regs, 0, NR_REGS);
    s->ptr = 0;
    s->count = 0;

    /* input power status register */
    s->regs[0x00] = AXP221_PWR_STATUS_ACIN_PRESENT
                    | AXP221_PWR_STATUS_ACIN_AVAIL
                    | AXP221_PWR_STATUS_ACIN_VBUS_POWERED;

    s->regs[0x01] = 0x00; /* no battery is connected */

    /* CHIPID register, no documented on datasheet, but it is checked in
     * u-boot spl. I had read it from AXP221s and got 0x06 value.
     * So leave 06h here.
     */
    s->regs[0x03] = 0x06;

    s->regs[0x10] = 0xbf;
    s->regs[0x13] = 0x01;
    s->regs[0x30] = 0x60;
    s->regs[0x31] = 0x03;
    s->regs[0x32] = 0x43;
    s->regs[0x33] = 0xc6;
    s->regs[0x34] = 0x45;
    s->regs[0x35] = 0x0e;
    s->regs[0x36] = 0x5d;
    s->regs[0x37] = 0x08;
    s->regs[0x38] = 0xa5;
    s->regs[0x39] = 0x1f;
    s->regs[0x3c] = 0xfc;
    s->regs[0x3d] = 0x16;
    s->regs[0x80] = 0x80;
    s->regs[0x82] = 0xe0;
    s->regs[0x84] = 0x32;
    s->regs[0x8f] = 0x01;

    s->regs[0x90] = 0x07;
    s->regs[0x91] = 0x1f;
    s->regs[0x92] = 0x07;
    s->regs[0x93] = 0x1f;

    s->regs[0x40] = 0xd8;
    s->regs[0x41] = 0xff;
    s->regs[0x42] = 0x03;
    s->regs[0x43] = 0x03;

    s->regs[0xb8] = 0xc0;
    s->regs[0xb9] = 0x64;
    s->regs[0xe6] = 0xa0;
}

/* Handle events from master. */
static int axp221_event(I2CSlave *i2c, enum i2c_event event)
{
    AXP221I2CState *s = AXP221(i2c);

    s->count = 0;

    return 0;
}

/* Called when master requests read */
static uint8_t axp221_rx(I2CSlave *i2c)
{
    AXP221I2CState *s = AXP221(i2c);
    uint8_t ret = 0xff;

    if (s->ptr < NR_REGS) {
        ret = s->regs[s->ptr];
        trace_axp221_rx(s->ptr, ret);
        s->ptr++;
    }

    return ret;
}

/*
 * Called when master sends write.
 * Update ptr with byte 0, then perform write with second byte.
 */
static int axp221_tx(I2CSlave *i2c, uint8_t data)
{
    AXP221I2CState *s = AXP221(i2c);

    if (s->count == 0) {
        /* Store register address */
        s->ptr = data;
        s->count++;
        trace_axp221_select(data);
    } else {
        trace_axp221_tx(s->ptr, data);
        s->regs[s->ptr++] = data;
    }

    return 0;
}

static const VMStateDescription vmstate_axp221 = {
    .name = TYPE_AXP221_PMU,
    .version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8_ARRAY(regs, AXP221I2CState, NR_REGS),
        VMSTATE_UINT8(count, AXP221I2CState),
        VMSTATE_UINT8(ptr, AXP221I2CState),
        VMSTATE_END_OF_LIST()
    }
};

static void axp221_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    I2CSlaveClass *isc = I2C_SLAVE_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);

    rc->phases.enter = axp221_reset_enter;
    dc->vmsd = &vmstate_axp221;
    isc->event = axp221_event;
    isc->recv = axp221_rx;
    isc->send = axp221_tx;
}

static const TypeInfo axp221_info = {
    .name = TYPE_AXP221_PMU,
    .parent = TYPE_I2C_SLAVE,
    .instance_size = sizeof(AXP221I2CState),
    .class_init = axp221_class_init
};

static void axp221_register_devices(void)
{
    type_register_static(&axp221_info);
}

type_init(axp221_register_devices);
