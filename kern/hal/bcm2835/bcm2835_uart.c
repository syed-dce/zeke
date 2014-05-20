/**
 *******************************************************************************
 * @file    uart.c
 * @author  Olli Vanhoja
 * @brief   UART source code for BCM2835.
 * @section LICENSE
 * Copyright (c) 2013, 2014 Olli Vanhoja <olli.vanhoja@cs.helsinki.fi>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *******************************************************************************
 */

/** @addtogroup HAL
* @{
*/

/** @addtogroup BCM2835
* @{
*/

#include <kinit.h>
#include <kerror.h>
#include "bcm2835_mmio.h"
#include "bcm2835_gpio.h"
#include <hal/uart.h>

#define UART0_BASE      0x20201000
#define UART0_DR        (UART0_BASE + 0x00)
#define UART0_RSRECR    (UART0_BASE + 0x04)
#define UART0_FR        (UART0_BASE + 0x18)
#define UART0_ILPR      (UART0_BASE + 0x20)
#define UART0_IBRD      (UART0_BASE + 0x24)
#define UART0_FBRD      (UART0_BASE + 0x28)
#define UART0_LCRH      (UART0_BASE + 0x2C)
#define UART0_CR        (UART0_BASE + 0x30)
#define UART0_IFLS      (UART0_BASE + 0x34)
#define UART0_IMSC      (UART0_BASE + 0x38)
#define UART0_RIS       (UART0_BASE + 0x3C)
#define UART0_MIS       (UART0_BASE + 0x40)
#define UART0_ICR       (UART0_BASE + 0x44)
#define UART0_DMACR     (UART0_BASE + 0x48)
#define UART0_ITCR      (UART0_BASE + 0x80)
#define UART0_ITIP      (UART0_BASE + 0x84)
#define UART0_ITOP      (UART0_BASE + 0x88)
#define UART0_TDR       (UART0_BASE + 0x8C)

static void bcm2835_uart_init(const uart_port_init_t * conf);
static void set_baudrate(unsigned int baud_rate);
static void set_lcrh(const uart_port_init_t * conf);
void bcm2835_uart_uputc(uint8_t byte);
int bcm2835_uart_ugetc(void);

void bcm2835_uart_register(void)
{
    SUBSYS_INIT();
    SUBSYS_DEP(arm_interrupt_preinit);

    uart_port_t port = {
        .init = bcm2835_uart_init,
        .uputc = bcm2835_uart_uputc,
        .ugetc = bcm2835_uart_ugetc
    };

    uart_register_port(&port);

    SUBSYS_INITFINI("BCM2835 UART Registered");
}
HW_PREINIT_ENTRY(bcm2835_uart_register);

void bcm2835_uart_init(const uart_port_init_t * conf)
{
    mmio_start();

    /* Disable UART0. */
    mmio_write(UART0_CR, 0x00000000);

    /* Setup the GPIO pin 14 & 15. */

    /* Disable pull up/down for all GPIO pins & delay for 150 cycles. */
    mmio_write(GPIO_GPPUD, 0x00000000);
    bcm2835_gpio_delay(150);

    /* Disable pull up/down for pin 14, 15 and delay for 150 cycles. */
    mmio_write(GPIO_PUDCLK0, (1 << 14) | (1 << 15));
    bcm2835_gpio_delay(150);

    /* Write 0 to GPPUDCLK0 to make it take effect.
     * (only affects to pins 14 & 15) */
    mmio_write(GPIO_PUDCLK0, 0x00000000);

    /* Clear pending interrupts. */
    mmio_write(UART0_ICR, 0x7FF);

    /* Set baud rate */
    set_baudrate(conf->baud_rate);

    /* Configure UART */
    set_lcrh(conf);

    /* Mask all interrupts. */
    /*mmio_write(UART0_IMSC, (1 << 1) | (1 << 4) | (1 << 5) |
            (1 << 6) | (1 << 7) | (1 << 8) |
            (1 << 9) | (1 << 10));*/

    /* Enable UART0, receive & transfer part of UART.*/
    mmio_write(UART0_CR, (1 << 0) | (1 << 8) | (1 << 9));
}


static void set_baudrate(unsigned int baud_rate)
{
    /* Integer & fractional part of baud rate.
     * divider = UART_CLOCK / (16 * 115200)
     * fraction = (divider mod 1 * 64) + 0.5
     * UART_CLOCK = 3000000
     */
    uint32_t tmp = 3000000/(16 * ((uint32_t)baud_rate >> 6));
    uint32_t divider = tmp >> 6;
    uint32_t fraction = tmp - (divider << 6);

    mmio_write(UART0_IBRD, divider);
    mmio_write(UART0_FBRD, fraction);
}

static void set_lcrh(const uart_port_init_t * conf)
{
    uint32_t tmp = 0;
    istate_t s_entry;

    s_entry = get_interrupt_state();
    disable_interrupt();

    /* Enable FIFOs */
    tmp |= 0x1 << 4;

    switch (conf->data_bits) {
        case UART_DATABITS_5:
            /* NOP */
            break;
        case UART_DATABITS_6:
            tmp |= 0x1 << 5;
            break;
        case UART_DATABITS_7:
            tmp |= 0x2 << 5;
            break;
        case UART_DATABITS_8:
            tmp |= 0x3 << 5;
            break;
    }

    switch (conf->parity) {
        case UART_PARITY_NO:
            /* NOP */
            break;
        case UART_PARITY_EVEN:
            tmp |= 0x3 << 1;
            break;
        case UART_PARITY_ODD:
            tmp |= 0x1 << 1;
            break;
    }

    mmio_write(UART0_LCRH, tmp);
    mmio_end();
    set_interrupt_state(s_entry);
}

void bcm2835_uart_uputc(uint8_t byte)
{
    istate_t s_entry;

    s_entry = get_interrupt_state();
    disable_interrupt();
    mmio_start();

    /* Wait for UART to become ready to transmit. */
    while (mmio_read(UART0_FR) & (1 << 5));
    mmio_write(UART0_DR, byte);

    mmio_end();
    set_interrupt_state(s_entry);
}

int bcm2835_uart_ugetc()
{
    int byte = -1;
    istate_t s_entry;

    s_entry = get_interrupt_state();
    disable_interrupt();
    mmio_start();

    /* Check that receive FIFO/register is not empty. */
    if(!(mmio_read(UART0_FR) & 0x10)) {
        /* TODO check errors (parity) */
        byte = mmio_read(UART0_DR);
    }

    mmio_end();
    set_interrupt_state(s_entry);

    return byte;
}

/**
* @}
*/

/**
* @}
*/