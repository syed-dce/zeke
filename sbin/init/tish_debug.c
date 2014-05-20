/**
 *******************************************************************************
 * @file    tish_debug.c
 * @author  Olli Vanhoja
 * @brief   Various debug tools for tish/Zeke.
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

#include <sys/types.h>
#include <kstring.h>
#include <unistd.h>
#include <libkern.h>
#include <errno.h>
#include <pthread.h>
#include <kernel.h>
#include "tish.h"

static void create_debug_thread(void);
static void * test_thread(void * arg);
static void thread_stat(void);

/* TODO Remove */
#define fprintf(stream, str) write(2, str, strlenn(str, MAX_LEN) + 1)
#define puts(str) fprintf(stderr, str)

static char invalid_arg[] = "Invalid argument\n";

void tish_debug(char ** args)
{
    char * arg = kstrtok(0, DELIMS, args);

    /* Thread debug commands */
    if (!strcmp(arg, "thread")) {
        arg = kstrtok(0, DELIMS, args);
        if (!strcmp(arg, "create")) {
            create_debug_thread();
        } else {
            puts(invalid_arg);
        }
    /* Process debug commands */
    } else if (!strcmp(arg, "proc")) {
        arg = kstrtok(0, DELIMS, args);
        if (!strcmp(arg, "fork")) {
            pid_t pid = fork();
            if (pid == -1) {
                puts("fork() failed\n");
            } else if (pid == 0) {
                puts("Hello from the child process\n");
                while(1)
                    msleep(500);
            } else {
                puts("original\n");
            }
        } else {
            puts(invalid_arg);
        }
    /* Data Abort Commands */
    } else if (!strcmp(arg, "dab")) {
        arg = kstrtok(0, DELIMS, args);
        if (!strcmp(arg, "fatal")) {
            puts("Trying fatal DAB\n");
            int * x = (void *)0xfffffff;
            *x = 1;
        } else {
            puts(invalid_arg);
        }
    } else {
        puts("Invalid subcommand\n");
        errno = EINVAL;
    }
}

static void create_debug_thread(void)
{
    static pthread_t test_tid;
    char buf[80];
    char * newstack;

    errno = 0;
    if ((newstack = sbrk(1024)) == (void *)-1) {
        puts("Failed to create a stack\n");
        return;
    }

    pthread_attr_t attr = {
        .tpriority  = 0,
        .stackAddr  = newstack,
        .stackSize  = 1024
    };

    errno = 0;
    if (pthread_create(&test_tid, &attr, test_thread, 0)) {
        puts("Thread creation failed\n");
        return;
    }
    ksprintf(buf, sizeof(buf), "Thread created with id: %u\n", test_tid);
    puts(buf);
}

static void * test_thread(void * arg)
{
    while(1) {
        sleep(10);
        thread_stat();
    }
}

static void thread_stat(void)
{
    /* Print thread id & cpu mode */
    char buf[80];
    uint32_t mode;
    pthread_t id = pthread_self();

    __asm__ volatile ("mrs     %0, cpsr" : "=r" (mode));
    ksprintf(buf, sizeof(buf), "My id: %u, my mode: %x\n", id, mode);
    puts(buf);
}