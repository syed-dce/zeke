/**
 *******************************************************************************
 * @file    fs.c
 * @author  Olli Vanhoja
 * @brief   Virtual file system, nofs.
 * @section LICENSE
 * Copyright (c) 2015 Olli Vanhoja <olli.vanhoja@cs.helsinki.fi>
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

#include <errno.h>
#include <proc.h>
#include <fs/fs.h>

const vnode_ops_t nofs_vnode_ops = {
    .lock = fs_enotsup_lock,
    .release = fs_enotsup_release,
    .read = fs_enotsup_read,
    .write = fs_enotsup_write,
    .ioctl = fs_enotsup_ioctl,
    .file_opened = fs_enotsup_file_opened,
    .file_closed = fs_enotsup_file_closed,
    .create = fs_enotsup_create,
    .mknod = fs_enotsup_mknod,
    .lookup = fs_enotsup_lookup,
    .link = fs_enotsup_link,
    .unlink = fs_enotsup_unlink,
    .mkdir = fs_enotsup_mkdir,
    .rmdir = fs_enotsup_rmdir,
    .readdir = fs_enotsup_readdir,
    .stat = fs_enotsup_stat,
    .utimes = fs_enotsup_utimes,
    .chmod = fs_enotsup_chmod,
    .chflags = fs_enotsup_chflags,
    .chown = fs_enotsup_chown,
};

/* Not sup vnops */
int fs_enotsup_lock(file_t * file)
{
    return -ENOTSUP;
}

int fs_enotsup_release(file_t * file)
{
    return -ENOTSUP;
}

ssize_t fs_enotsup_read(file_t * file, void * buf, size_t count)
{
    return -ENOTSUP;
}

ssize_t fs_enotsup_write(file_t * file, const void * buf, size_t count)
{
    return -ENOTSUP;
}

int fs_enotsup_ioctl(file_t * file, unsigned request, void * arg,
                     size_t arg_len)
{
    return -ENOTTY;
}

int fs_enotsup_file_opened(proc_info_t * p, vnode_t * vnode)
{
    return 0;
}

void fs_enotsup_file_closed(proc_info_t * p, file_t * file)
{
}

int fs_enotsup_create(vnode_t * dir, const char * name, mode_t mode,
                      vnode_t ** result)
{
    return -ENOTSUP;
}

int fs_enotsup_mknod(vnode_t * dir, const char * name, int mode,
                     void * specinfo, vnode_t ** result)
{
    return -ENOTSUP;
}

int fs_enotsup_lookup(vnode_t * dir, const char * name, vnode_t ** result)
{
    return -ENOTSUP;
}

int fs_enotsup_link(vnode_t * dir, vnode_t * vnode, const char * name)
{
    return -EACCES;
}

int fs_enotsup_unlink(vnode_t * dir, const char * name)
{
    return -EACCES;
}

int fs_enotsup_mkdir(vnode_t * dir,  const char * name, mode_t mode)
{
    return -ENOTSUP;
}

int fs_enotsup_rmdir(vnode_t * dir,  const char * name)
{
    return -ENOTSUP;
}

int fs_enotsup_readdir(vnode_t * dir, struct dirent * d, off_t * off)
{
    return -ENOTSUP;
}

int fs_enotsup_stat(vnode_t * vnode, struct stat * buf)
{
    return -ENOTSUP;
}

int fs_enotsup_utimes(vnode_t * vnode, const struct timespec times[2])
{
    return -EPERM;
}

int fs_enotsup_chmod(vnode_t * vnode, mode_t mode)
{
    return -ENOTSUP;
}

int fs_enotsup_chflags(vnode_t * vnode, fflags_t flags)
{
    return -ENOTSUP;
}

int fs_enotsup_chown(vnode_t * vnode, uid_t owner, gid_t group)
{
    return -ENOTSUP;
}