/*
 * Allwinner SUN6i/SUN8i SPI
 *
 * Copyright (c) 2023 qianfan Zhao <qianfanguijin@163.com>
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

#ifndef ALLWINNER_SUN6I_SPI_H
#define ALLWINNER_SUN6I_SPI_H

#include "hw/sysbus.h"
#include "hw/ssi/ssi.h"
#include "qemu/fifo32.h"
#include "qom/object.h"

#define TYPE_AW_SPI_SUN6I    "allwinner-spi.sun6i"
#define TYPE_AW_SPI_SUN8I    "allwinner-spi.sun8i"
OBJECT_DECLARE_SIMPLE_TYPE(AWSpiState, AW_SPI_SUN6I)

struct AWSpiState {
    SysBusDevice parent_obj;

    MemoryRegion mmio;

    SSIBus *spi;
    qemu_irq irq;

    Fifo32 rx_fifo;
    Fifo32 tx_fifo;
    uint32_t fifo_size;

    uint32_t gcr;
    uint32_t tcr;
    uint32_t ier;
    uint32_t isr;
    uint32_t fcr;
    uint32_t wcr;
    uint32_t ccr;
    uint32_t mbr;
    uint32_t mtc;
    uint32_t bcc;
    uint32_t ndma_mode_ctl;
};

#endif /* ALLWINNER_SUN6I_SPI_H */
