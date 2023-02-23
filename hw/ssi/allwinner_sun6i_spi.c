/*
 * SPI drivers for allwinner sun6i/sun8i based SoCs
 *
 * Copyright (C) 2022 qianfan Zhao <qianfanguijin@163.com>
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

#include "qemu/osdep.h"
#include "hw/irq.h"
#include "hw/ssi/allwinner_sun6i_spi.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "trace.h"

#define SUN6I_AUTOSUSPEND_TIMEOUT               2000

#define SUN6I_FIFO_DEPTH                        128
#define SUN8I_FIFO_DEPTH                        64

#define SUN6I_GBL_CTL_REG                       0x04
#define SUN6I_GBL_CTL_BUS_ENABLE                BIT(0)
#define SUN6I_GBL_CTL_MASTER                    BIT(1)
#define SUN6I_GBL_CTL_TP                        BIT(7)
#define SUN6I_GBL_CTL_RST                       BIT(31)

#define SUN6I_TFR_CTL_REG                       0x08
#define SUN6I_TFR_CTL_CPHA                      BIT(0)
#define SUN6I_TFR_CTL_CPOL                      BIT(1)
#define SUN6I_TFR_CTL_SPOL                      BIT(2)
#define SUN6I_TFR_CTL_CS_MASK                   0x30
#define SUN6I_TFR_CTL_CS(cs)                    \
        (((cs) << 4) & SUN6I_TFR_CTL_CS_MASK)
#define SUN6I_TFR_CTL_CS_MANUAL                 BIT(6)
#define SUN6I_TFR_CTL_CS_LEVEL                  BIT(7)
#define SUN6I_TFR_CTL_DHB                       BIT(8)
#define SUN6I_TFR_CTL_FBS                       BIT(12)
#define SUN6I_TFR_CTL_XCH                       BIT(31)

#define SUN6I_INT_CTL_REG                       0x10
#define SUN6I_INT_CTL_RF_RDY                    BIT(0)
#define SUN6I_INT_CTL_RF_EMP                    BIT(1)
#define SUN6I_INT_CTL_RF_FULL                   BIT(2)
#define SUN6I_INT_CTL_TF_RDY                    BIT(4)
#define SUN6I_INT_CTL_TF_EMP                    BIT(5)
#define SUN6I_INT_CTL_TF_FULL                   BIT(6)
#define SUN6I_INT_CTL_RF_OVF                    BIT(8)
#define SUN6I_INT_CTL_RF_UDF                    BIT(9)
#define SUN6I_INT_CTL_TF_OVF                    BIT(10)
#define SUN6I_INT_CTL_TF_UDF                    BIT(11)
#define SUN6I_INT_CTL_TC                        BIT(12)

#define SUN6I_INT_STA_REG                       0x14

#define SUN6I_FIFO_CTL_REG                      0x18
#define SUN6I_FIFO_CTL_RF_RDY_TRIG_LEVEL_MASK   0xff
#define SUN6I_FIFO_CTL_RF_DRQ_EN                BIT(8)
#define SUN6I_FIFO_CTL_RF_RDY_TRIG_LEVEL_BITS   0
#define SUN6I_FIFO_CTL_RF_RST                   BIT(15)
#define SUN6I_FIFO_CTL_TF_ERQ_TRIG_LEVEL_MASK   0xff
#define SUN6I_FIFO_CTL_TF_ERQ_TRIG_LEVEL_BITS   16
#define SUN6I_FIFO_CTL_TF_DRQ_EN                BIT(24)
#define SUN6I_FIFO_CTL_TF_RST                   BIT(31)

#define SUN6I_FIFO_STA_REG                      0x1c
#define SUN6I_FIFO_STA_RF_CNT_MASK              GENMASK(7, 0)
#define SUN6I_FIFO_STA_TF_CNT_MASK              GENMASK(23, 16)

#define SUN6I_WAIT_CLK_REG                      0x20

#define SUN6I_CLK_CTL_REG                       0x24
#define SUN6I_CLK_CTL_CDR2_MASK                 0xff
#define SUN6I_CLK_CTL_CDR2(div)                 \
        (((div) & SUN6I_CLK_CTL_CDR2_MASK) << 0)
#define SUN6I_CLK_CTL_CDR1_MASK                 0xf
#define SUN6I_CLK_CTL_CDR1(div)                 \
        (((div) & SUN6I_CLK_CTL_CDR1_MASK) << 8)
#define SUN6I_CLK_CTL_DRS                       BIT(12)

#define SUN6I_MAX_XFER_SIZE                     0xffffff

#define SUN6I_BURST_CNT_REG                     0x30

#define SUN6I_XMIT_CNT_REG                      0x34

#define SUN6I_BURST_CTL_CNT_REG                 0x38

#define SUN6I_NDMA_MODE_CTL                     0x88

#define SUN6I_TXDATA_REG                        0x200
#define SUN6I_RXDATA_REG                        0x300

static const char *sun6i_spi_regname(hwaddr addr)
{
    switch (addr) {
    case SUN6I_GBL_CTL_REG:
        return "GCR";
    case SUN6I_TFR_CTL_REG:
        return "TCR";
    case SUN6I_INT_CTL_REG:
        return "IER";
    case SUN6I_INT_STA_REG:
        return "ISR";
    case SUN6I_FIFO_CTL_REG:
        return "FCR";
    case SUN6I_FIFO_STA_REG:
        return "FSR";
    case SUN6I_WAIT_CLK_REG:
        return "WCR";
    case SUN6I_CLK_CTL_REG:
        return "CCR";
    case SUN6I_BURST_CNT_REG:
        return "MBR";
    case SUN6I_XMIT_CNT_REG:
        return "MTC";
    case SUN6I_BURST_CTL_CNT_REG:
        return "BCC";
    case SUN6I_NDMA_MODE_CTL:
        return "DMA";
    case SUN6I_TXDATA_REG:
        return "TXD";
    case SUN6I_RXDATA_REG:
        return "RXD";
    }

    return "???";
}

static void txfifo_reset(AWSpiState *s)
{
    fifo32_reset(&s->tx_fifo);
}

static void rxfifo_reset(AWSpiState *s)
{
    fifo32_reset(&s->rx_fifo);
}

static void allwinner_sun6i_spi_reset(DeviceState *d)
{
    AWSpiState *s = AW_SPI_SUN6I(d);

    s->gcr = 0x80;
    s->tcr = 0x87;
    s->isr = 0x32;
    s->fcr = 0x00400001;
    s->ccr = 0x02;
    s->ndma_mode_ctl = 0xa5;

    rxfifo_reset(s);
    txfifo_reset(s);
}

static void allwinner_sun6i_spi_transfer(AWSpiState *s)
{
    uint32_t trace_tx[64], trace_rx[ARRAY_SIZE(trace_tx)];
    uint32_t burst = s->mbr & 0xffffff;

    if (!burst) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: master burst counter is 0\n", __func__);
        return;
    }

    if (fifo32_num_used(&s->tx_fifo) != burst) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: master burst counter(%d) != tx fifo counter(%d)\n",
                      __func__, burst, fifo32_num_used(&s->tx_fifo));
        burst = fifo32_num_used(&s->tx_fifo);
    }

    while (burst > 0) {
        char trace_string[ARRAY_SIZE(trace_tx) * 2 + 1] = { 0 };
        int n = burst;

        if (n > ARRAY_SIZE(trace_tx))
            n = ARRAY_SIZE(trace_tx);

        for (int i = 0; i < n; i++) {
            uint32_t tx = 0, rx = 0;

            if (!fifo32_is_empty(&s->tx_fifo))
                tx = fifo32_pop(&s->tx_fifo);

            rx = ssi_transfer(s->spi, tx);
            if (!fifo32_is_full(&s->rx_fifo))
                fifo32_push(&s->rx_fifo, rx);

            trace_tx[i] = tx;
            trace_rx[i] = rx;
        }

        if (trace_event_get_state_backends(TRACE_ALLWINNER_SUN6I_SPI_SEND)) {
            for (int i = 0; i < n; i++)
                snprintf(&trace_string[i * 2], 3, "%02x", trace_tx[i]);
            trace_allwinner_sun6i_spi_send(trace_string);
        }

        if (trace_event_get_state_backends(TRACE_ALLWINNER_SUN6I_SPI_RECV)) {
            for (int i = 0; i < n; i++)
                snprintf(&trace_string[i * 2], 3, "%02x", trace_rx[i]);
            trace_allwinner_sun6i_spi_send(trace_string);
        }

        burst -= n;
    }
}

static uint64_t spi_read(void *opaque, hwaddr addr, unsigned int size)
{
    AWSpiState *s = opaque;
    uint32_t ret = 0;

    switch (addr) {
    case SUN6I_GBL_CTL_REG:
        ret = s->gcr;
        break;
    case SUN6I_TFR_CTL_REG:
        ret = s->tcr;
        break;
    case SUN6I_INT_CTL_REG:
        ret = s->ier;
        break;
    case SUN6I_INT_STA_REG:
        ret = s->isr;
        break;
    case SUN6I_FIFO_CTL_REG:
        ret = s->fcr;
        break;
    case SUN6I_FIFO_STA_REG:
        ret |= (fifo32_num_used(&s->rx_fifo) <<  0);
        ret |= (fifo32_num_used(&s->tx_fifo) << 16);
        break;
    case SUN6I_WAIT_CLK_REG:
        ret = s->wcr;
        break;
    case SUN6I_CLK_CTL_REG:
        ret = s->ccr;
        break;
    case SUN6I_BURST_CNT_REG:
        ret = s->mbr;
        break;
    case SUN6I_XMIT_CNT_REG:
        ret = s->mtc;
        break;
    case SUN6I_BURST_CTL_CNT_REG:
        ret = s->bcc;
        break;
    case SUN6I_NDMA_MODE_CTL:
        ret = s->ndma_mode_ctl;
        break;
    case SUN6I_RXDATA_REG:
        if (fifo32_is_empty(&s->rx_fifo))
            ret = 0;
        else
            ret = fifo32_pop(&s->rx_fifo);
        break;
    case SUN6I_TXDATA_REG:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Can't read write-only register %s\n",
                      __func__, sun6i_spi_regname(addr));
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%" HWADDR_PRIx "\n", __func__, addr);
        break;
    }

    trace_allwinner_sun6i_spi_read(addr, sun6i_spi_regname(addr), ret);
    return ret;
}

static void spi_write(void *opaque, hwaddr addr,
                      uint64_t val64, unsigned int size)
{
    AWSpiState *s = opaque;
    uint32_t value = val64;

    trace_allwinner_sun6i_spi_write(addr, sun6i_spi_regname(addr), value);

    switch (addr) {
    case SUN6I_GBL_CTL_REG:
        s->gcr = value;
        break;
    case SUN6I_TFR_CTL_REG:
        if (value & SUN6I_TFR_CTL_XCH) {
            /* Write 1 to this bit will start the spi burst, and will
             * automatically clear after finishing the burst transfer
             * specified by SPI_BC
             */
            allwinner_sun6i_spi_transfer(s);
            value &= ~SUN6I_TFR_CTL_XCH;
        }
        s->tcr = value;
        break;
    case SUN6I_INT_CTL_REG:
        s->ier = value;
        break;
    case SUN6I_INT_STA_REG:
        s->isr = value;
        break;
    case SUN6I_FIFO_CTL_REG:
        /* TX_FIFO_RST and RX_FIFO_RST bit is WAC(Write-Automatic-Clear) */
        if (value & SUN6I_FIFO_CTL_TF_RST) {
            txfifo_reset(s);
            value &= ~SUN6I_FIFO_CTL_TF_RST;
        } else if (value & SUN6I_FIFO_CTL_RF_RST) {
            rxfifo_reset(s);
            value &= ~SUN6I_FIFO_CTL_RF_RST;
        }
        s->fcr = value;
        break;
    case SUN6I_WAIT_CLK_REG:
        s->wcr = value;
        break;
    case SUN6I_CLK_CTL_REG:
        s->ccr = value;
        break;
    case SUN6I_BURST_CNT_REG:
        s->mbr = value;
        break;
    case SUN6I_XMIT_CNT_REG:
        s->mtc = value;
        break;
    case SUN6I_BURST_CTL_CNT_REG:
        s->bcc = value;
        break;
    case SUN6I_NDMA_MODE_CTL:
        s->ndma_mode_ctl = value;
        break;
    case SUN6I_TXDATA_REG:
            fifo32_push(&s->tx_fifo, value);
        break;
    case SUN6I_RXDATA_REG:
    case SUN6I_FIFO_STA_REG:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Can't write read-only register %s\n",
                      __func__, sun6i_spi_regname(addr));
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%" HWADDR_PRIx "\n", __func__, addr);
        break;
    }
}

static const MemoryRegionOps spi_ops = {
    .read = spi_read,
    .write = spi_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4
    }
};

static void allwinner_sun6i_spi_realize(DeviceState *dev, Error **errp)
{
    AWSpiState *s = AW_SPI_SUN6I(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    s->spi = ssi_create_bus(dev, "spi");

    memory_region_init_io(&s->mmio, OBJECT(s), &spi_ops, s,
                          TYPE_AW_SPI_SUN6I, 0x400);
    sysbus_init_mmio(sbd, &s->mmio);
    sysbus_init_irq(sbd, &s->irq);

    fifo32_create(&s->tx_fifo, s->fifo_size);
    fifo32_create(&s->rx_fifo, s->fifo_size);
}

static const VMStateDescription vmstate_allwinner_sun6i_spi = {
    .name = TYPE_AW_SPI_SUN6I,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_FIFO32(tx_fifo, AWSpiState),
        VMSTATE_FIFO32(rx_fifo, AWSpiState),
        VMSTATE_UINT32(gcr, AWSpiState),
        VMSTATE_UINT32(tcr, AWSpiState),
        VMSTATE_UINT32(ier, AWSpiState),
        VMSTATE_UINT32(isr, AWSpiState),
        VMSTATE_UINT32(fcr, AWSpiState),
        VMSTATE_UINT32(wcr, AWSpiState),
        VMSTATE_UINT32(ccr, AWSpiState),
        VMSTATE_UINT32(mbr, AWSpiState),
        VMSTATE_UINT32(mtc, AWSpiState),
        VMSTATE_UINT32(bcc, AWSpiState),
        VMSTATE_UINT32(ndma_mode_ctl, AWSpiState),
        VMSTATE_END_OF_LIST()
    }
};

static void allwinner_sun6i_spi_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = allwinner_sun6i_spi_realize;
    dc->reset = allwinner_sun6i_spi_reset;
    dc->vmsd = &vmstate_allwinner_sun6i_spi;
}

static void allwinner_sun6i_spi_init(Object *obj)
{
    AWSpiState *s = AW_SPI_SUN6I(obj);

    s->fifo_size = SUN6I_FIFO_DEPTH;
}

static void allwinner_sun8i_spi_init(Object *obj)
{
    AWSpiState *s = AW_SPI_SUN6I(obj);

    s->fifo_size = SUN8I_FIFO_DEPTH;
}

static const TypeInfo allwinner_spi_infos[] = {
    {
        .name           = TYPE_AW_SPI_SUN6I,
        .parent         = TYPE_SYS_BUS_DEVICE,
        .instance_size  = sizeof(AWSpiState),
        .instance_init  = allwinner_sun6i_spi_init,
        .class_init     = allwinner_sun6i_spi_class_init,
    },     {
        .name           = TYPE_AW_SPI_SUN8I,
        .parent         = TYPE_SYS_BUS_DEVICE,
        .instance_size  = sizeof(AWSpiState),
        .instance_init  = allwinner_sun8i_spi_init,
        .class_init     = allwinner_sun6i_spi_class_init,
    }
};

DEFINE_TYPES(allwinner_spi_infos);
