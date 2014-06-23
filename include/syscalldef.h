/**
 *******************************************************************************
 * @file    syscalldef.h
 * @author  Olli Vanhoja
 * @brief   Types and definitions for syscalls.
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

#pragma once
#ifndef SYSCALLDEF_H
#define SYSCALLDEF_H

#include <kernel.h>

/** Argument struct for SYSCALL_SCHED_THREAD_CREATE */
struct _ds_pthread_create {
    pthread_t * thread;     /*!< Returned thread id. */
    start_routine start;    /*!< Thread start routine. */
    pthread_attr_t * def;   /*!< Thread def attributes. */
    void * argument;        /*!< Thread parameter(s) pointer. */
    void (*del_thread)(void *); /*!< Thread exit function. */
};

/** Argument struct for SYSCALL_SCHED_THREAD_SETPRIORITY */
struct _ds_set_priority {
    pthread_t thread_id;    /*!< Thread id */
    osPriority priority;    /*!< Thread priority */
};

/** Argument struct for SYSCALL_SCHED_SIGNAL_SET
 *  and KERNEL_SYSCALL_SCHED_SIGNAL_CLEAR */
struct ds_signal {
    pthread_t thread_id;   /*!< Thread id */
    int32_t signal;         /*!< Thread signals to set */
};

/** Argument struct for SYSCALL_SCHED_SIGNAL_WAIT */
struct _ds_signal_wait_t {
    int32_t signals;        /*!< Thread signal(s) to wait */
    uint32_t millisec;      /*!< Timeout in ms */
};

/** Argument struct for SYSCALL_SEMAPHORE_WAIT */
struct _ds_semaphore_wait {
    uint32_t * s;           /*!< Pointer to the semaphore */
    uint32_t millisec;      /*!< Timeout in ms */
};

struct _sysctl_args {
    int * name;
    unsigned int namelen;
    void * old;
    size_t * oldlenp;
    void * new;
    size_t newlen;
};

/** Arguments struct for SYSCALL_FS_WRITE */
struct _fs_write_args {
    int fildes;
    void * buf;
    size_t nbyte;
    off_t offset;
};

struct _fs_mount_args {
    const char * source;
    size_t source_len;
    const char * target;
    size_t target_len;
    const char fsname[8];
    uint32_t mode;
    const char * parm;
    size_t parm_len;
};

/** Arguments struct for SYSCALL_PROC_GETBREAK */
struct _ds_getbreak {
    void * start;
    void * stop;
};

#endif /* SYSCALLDEF_H */
