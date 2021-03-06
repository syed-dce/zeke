/*
 * Copyright (c) 2014, 2015 Olli Vanhoja <olli.vanhoja@cs.helsinki.fi>
 * Copyright (c) 1983, 1993
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <string.h>
#include <unistd.h>
#include <signal.h>

const char * const sys_siglist[] = {
    "None",
    "Hangup",
    "Interrupt",
    "Quit",
    "Illegal instruction",
    "Trace/BPT trap",
    "Abort trap",
    "Child exited",
    "Floating point exception",
    "Killed",
    "Bus error",
    "Segmentation fault",
    "Continued",
    "Broken pipe",
    "Alarm clock",
    "Terminated",
    "Suspended (signal)",
    "Suspended",
    "Stopped (tty input)",
    "Stopped (tty output)",
    "User defined signal 1",
    "User defined signal 2",
    "Bad system call",
    "Urgent I/O condition",
    "Information request",
    "Power failure",
    "Child thread exited",
    "Thread cancelled",
};

void psignal(int signum, const char * message)
{
    const char * c;
    if ((size_t)signum < sizeof(sys_siglist)) {
        c = sys_siglist[signum];
    } else {
        c = "Unknown signal";
    }

    if (message != NULL && *message != '\0') {
        write(STDERR_FILENO, message, strlen(message));
        write(STDERR_FILENO, ": ", 2);
    }
    write(STDERR_FILENO, c, strlen(c));
    write(STDERR_FILENO, "\n", 1);
}
