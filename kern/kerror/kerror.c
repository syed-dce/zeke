/**
 *******************************************************************************
 * @file    kerror.c
 * @author  Olli Vanhoja
 * @brief   Kernel error logging.
 * @section LICENSE
 * Copyright (c) 2014 Olli Vanhoja <olli.vanhoja@cs.helsinki.fi>
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

#define KERNEL_INTERNAL
#include <kinit.h>
#include <kstring.h>
#include <libkern.h>
#include <fs/fs.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <hal/uart.h>
#include <kerror.h>

const char _kernel_panic_msg[19] = "Oops, Kernel panic";

static size_t kerror_fdwrite(vnode_t * file, const off_t * offset,
        const void * buf, size_t count);

vnode_ops_t kerror_vops = {
    .write = kerror_fdwrite
};

vnode_t kerror_vnode = {
    .vnode_num = 0,
    .refcount = 0,
    .len = SIZE_MAX,
    .vnode_ops = &kerror_vops
};

SET_DECLARE(klogger_set, struct kerror_klogger);

static void nolog_puts(const char * str);

void (*kputs)(const char *) = &kerror_buf_puts; /* Boot value */
static size_t curr_klogger_id = KERROR_BUF; /* Boot value */

static int klogger_change(size_t new_id, size_t old_id);

void kerror_init(void) __attribute__((constructor));
void kerror_init(void)
{
    SUBSYS_INIT();

    klogger_change(configDEF_KLOGGER, curr_klogger_id);

    SUBSYS_INITFINI("Kerror logger OK");
}

/**
 * Kernel fake fd write function to print kerror messages from usr mode threads.
 */
static size_t kerror_fdwrite(vnode_t * file, const off_t * offset,
        const void * buf, size_t count)
{
    kputs(buf);
    return count;
}

void kerror_print_macro(char level, const char * where, const char * msg)
{
    char buf[configKERROR_MAXLEN];

    ksprintf(buf, sizeof(buf), "%c:%s%s\n", level, where, msg);
    kputs(buf);
}

static void nolog_puts(const char * str)
{
}

static struct kerror_klogger * get_klogger(size_t id)
{
    struct kerror_klogger * logger;

    SET_FOREACH(klogger, klogger_set) {
        if (klogger->id == id)
            return klogger;
    }

    return 0;
}

static int klogger_change(size_t new_id, size_t old_id)
{
    struct kerror_klogger * new = get_klogger(new_id);
    struct kerror_klogger * old = get_klogger(old_id);

    if (new == 0 || old == 0)
        return EINVAL;

    if (new->init)
        new->init();

    kputs = klogger_arr[new_klogger].puts;

    if (old->flush)
        old->flush();

    curr_klogger_id = new_id;

    return 0;
}

/**
 * sysctl function to read current klogger and change it.
 */
static int sysctl_kern_klogger(SYSCTL_HANDLER_ARGS)
{
    int error;
    size_t new_klogger = curr_klogger_id;
    size_t old_klogger = curr_klogger_id;

    error = sysctl_handle_int(oidp, &new_klogger, sizeof(new_klogger), req);
    if (!error && req->newptr) {
        error = klogger_change(new_klogger, old_klogger);
    }

    return error;
}

SYSCTL_PROC(_kern, OID_AUTO, klogger, CTLTYPE_INT | CTLFLAG_RW,
        NULL, 0, sysctl_kern_klogger, "I", "Kernel logger type.");

