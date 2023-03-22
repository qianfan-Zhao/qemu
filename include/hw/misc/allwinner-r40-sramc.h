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

#ifndef HW_MISC_ALLWINNER_R40_SRAMC_H
#define HW_MISC_ALLWINNER_R40_SRAMC_H

#include "qom/object.h"
#include "hw/sysbus.h"
#include "qemu/uuid.h"

/**
 * Object model
 * @{
 */

#define TYPE_AW_R40_SRAMC   "allwinner-r40-sramc"
OBJECT_DECLARE_SIMPLE_TYPE(AwR40SRAMCState, AW_R40_SRAMC)

/** @} */

/**
 * Allwinner R40 SRAMC object instance state
 */
struct AwR40SRAMCState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/

    /** Maps I/O registers in physical memory */
    MemoryRegion iomem;
};

extern uint32_t sun8i_r40_sramc_soft_entry_reg0;

#endif /* HW_MISC_ALLWINNER_R40_SRAMC_H */
