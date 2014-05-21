/**
 *******************************************************************************
 * @file    resource.c
 * @author  Olli Vanhoja
 * @brief   Zero Kernel user space code
 * @section LICENSE
 * Copyright (c) 2013, 2014 Olli Vanhoja <olli.vanhoja@cs.helsinki.fi>
 * Copyright (c) 2012, 2013, Ninjaware Oy, Olli Vanhoja <olli.vanhoja@ninjaware.fi>
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

/** @addtogroup Library_Functions
  * @{
  */

#include <hal/core.h>
#include <syscall.h>
#include <errno.h>
#include <sys/resource.h>

int getloadavg(double loadavg[3], int nelem)
{
    uint32_t loads[3];
    size_t i;

    if (nelem > 3)
        return -1;

    if(syscall(SYSCALL_SCHED_GET_LOADAVG, loads))
        return -1;

    /* TODO After float div support */
    for (i = 0; i < nelem; i++)
        loadavg[i] = (double)(loads[i]); // 100.0;

    return nelem;
}

int setpriority(int which, id_t who, int prio)
{
    switch (which) {
    case PRIO_THREAD:
        {
            struct _ds_set_priority ds = {
                .thread_id = who,
                .priority = prio
            };

        return (int)syscall(SYSCALL_SCHED_SETPRIORITY, &ds);
        }
    default:
        errno = EINVAL;
        return -1;
    }
}

int  getpriority(int which, id_t who)
{
    switch (which) {
    case PRIO_THREAD:
        return (int)syscall(SYSCALL_SCHED_GETPRIORITY, &who);
    default:
        errno = EINVAL;
        return -1;
    }
}

/**
  * @}
  */
