/*
 * Allwinner R40 SRAM controller emulation
 *
 * Copyright (C) 2023 qianfan Zhao <qianfanguijin@163.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "hw/sysbus.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"
#include "hw/misc/allwinner-r40-sramc.h"
#include "trace.h"

#define SUN8I_R40_SRAMC_SOFT_ENTRY_REG0     0xbc

uint32_t sun8i_r40_sramc_soft_entry_reg0;

static uint64_t allwinner_r40_sramc_read(void *opaque, hwaddr offset,
                                         unsigned size)
{
    uint64_t val = 0;

    switch (offset) {
    case SUN8I_R40_SRAMC_SOFT_ENTRY_REG0:
        val = sun8i_r40_sramc_soft_entry_reg0;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: out-of-bounds offset 0x%04x\n",
                      __func__, (uint32_t)offset);
        return 0;
    }

    trace_allwinner_r40_sramc_read(offset, val);

    return val;
}

static void allwinner_r40_sramc_write(void *opaque, hwaddr offset,
                                      uint64_t val, unsigned size)
{
    trace_allwinner_r40_sramc_write(offset, val);

    switch (offset) {
    case SUN8I_R40_SRAMC_SOFT_ENTRY_REG0:
        sun8i_r40_sramc_soft_entry_reg0 = val;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: out-of-bounds offset 0x%04x\n",
                      __func__, (uint32_t)offset);
        break;
    }
}

static const MemoryRegionOps allwinner_r40_sramc_ops = {
    .read = allwinner_r40_sramc_read,
    .write = allwinner_r40_sramc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
    .impl.min_access_size = 4,
};

static void allwinner_r40_sramc_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    AwR40SRAMCState *s = AW_R40_SRAMC(obj);

    /* Memory mapping */
    memory_region_init_io(&s->iomem, OBJECT(s), &allwinner_r40_sramc_ops, s,
                           TYPE_AW_R40_SRAMC, 1 * KiB);
    sysbus_init_mmio(sbd, &s->iomem);
}

static const TypeInfo allwinner_r40_sramc_info = {
    .name          = TYPE_AW_R40_SRAMC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_init = allwinner_r40_sramc_init,
    .instance_size = sizeof(AwR40SRAMCState),
};

static void allwinner_r40_sramc_register(void)
{
    type_register_static(&allwinner_r40_sramc_info);
}

type_init(allwinner_r40_sramc_register)
