/**
 *******************************************************************************
 * @file    fs.c
 * @author  Olli Vanhoja
 * @brief   Virtual file system.
 * @section LICENSE
 * Copyright (c) 2013 - 2015 Olli Vanhoja <olli.vanhoja@cs.helsinki.fi>
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

#include <kinit.h>
#include <libkern.h>
#include <kerror.h>
#include <kstring.h>
#include <kmalloc.h>
#include <vm/vm.h>
#include <thread.h>
#include <proc.h>
#include <ksignal.h>
#include <buf.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/sysctl.h>
#include <fs/mbr.h>
#include <fs/fs.h>
#include <fs/fs_util.h>

/*
 * File system global locking.
 */
mtx_t fslock;
#define FS_LOCK()       mtx_lock(&fslock)
#define FS_UNLOCK()     mtx_unlock(&fslock)
#define FS_TESTLOCK()   mtx_test(&fslock)
#define FS_LOCK_INIT()  mtx_init(&fslock, MTX_TYPE_SPIN, 0)

SYSCTL_NODE(, CTL_VFS, vfs, CTLFLAG_RW, 0,
        "File system");

SYSCTL_DECL(_vfs_limits);
SYSCTL_NODE(_vfs, OID_AUTO, limits, CTLFLAG_RD, 0,
        "File system limits and information");

SYSCTL_INT(_vfs_limits, OID_AUTO, name_max, CTLFLAG_RD, 0, NAME_MAX,
        "Limit for the length of a file name component.");
SYSCTL_INT(_vfs_limits, OID_AUTO, path_max, CTLFLAG_RD, 0, PATH_MAX,
        "Limit for for length of an entire file name.");

/**
 * Linked list of registered file systems.
 */
static SLIST_HEAD(fs_list, fs) fs_list_head =
    SLIST_HEAD_INITIALIZER(fs_list_head);


int fs_init(void) __attribute__((constructor));
int fs_init(void)
{
    SUBSYS_INIT("fs");

    FS_LOCK_INIT();

    return 0;
}

/**
 * Register a new file system driver.
 * @param fs file system control struct.
 */
int fs_register(fs_t * fs)
{
#ifdef configFS_DEBUG
    KERROR(KERROR_DEBUG, "fs_register(fs:\"%s\")\n", fs->fsname);
#endif

    FS_LOCK();
    mtx_lock(&fs->fs_giant);
    SLIST_INSERT_HEAD(&fs_list_head, fs, _fs_list);
    mtx_unlock(&fs->fs_giant);
    FS_UNLOCK();

    return 0;
}

fs_t * fs_by_name(const char * fsname)
{
    struct fs * fs;

    KASSERT(fsname != NULL, "fsname should be set\n");

    FS_LOCK();
    SLIST_FOREACH(fs, &fs_list_head, _fs_list) {
        if (strcmp(fs->fsname, fsname) == 0) {
            FS_UNLOCK();
            return fs;
        }
    }
    FS_UNLOCK();

    return NULL;
}

fs_t * fs_iterate(fs_t * fsp)
{
    FS_LOCK();
    if (SLIST_EMPTY(&fs_list_head)) {
        FS_UNLOCK();
        return NULL;
    }

    if (!fsp)
        fsp = SLIST_FIRST(&fs_list_head);
    else
        fsp = SLIST_NEXT(fsp, _fs_list);
    FS_UNLOCK();

    return fsp;
}

/**
 * Get base vnode of a mountpoint.
 * If vn is not a mountpoint nothing is done and vn is returned as is,
 * otherwise the first vnode in a mount stack is returned. The first vnode
 * is the base mountpoint vnode.
 */
static vnode_t * get_base_vnode(vnode_t * vn)
{
    while (vn->vn_prev_mountpoint != vn) {
        /*
         * We loop here to get to the first file system mounted on this
         * mountpoint.
         */
        vn = vn->vn_prev_mountpoint;
        KASSERT(vn != NULL, "prev_mountpoint should be always valid");
    }
    return vn;
}

/**
 * Get the top root vnode on a mountpoint.
 * If there is no mounts on this vnode vn is returned as is,
 * otherwise the top most root vnode on a mount stack is returned.
 */
static vnode_t * get_top_vnode(vnode_t * vn)
{
    while (vn->vn_next_mountpoint != vn) {
        vn = vn->vn_next_mountpoint;
        KASSERT(vn != NULL, "next_mountpoint should be always valid");
    }
    return vn;
}

int fs_mount(vnode_t * target, const char * source, const char * fsname,
             uint32_t flags, const char * parm, int parm_len)
{
    fs_t * fs = NULL;
    struct fs_superblock * sb;
    vnode_t * root;
    istate_t s;
    int err;

#ifdef configFS_DEBUG
     KERROR(KERROR_DEBUG,
            "fs_mount(target \"%p\", source \"%s\", fsname \"%s\", "
            "flags %x, parm \"%s\", parm_len %d)\n",
            target, source, fsname, flags, parm, parm_len);
#endif
    KASSERT(target, "target must be set");

    if (fsname) {
        fs = fs_by_name(fsname);
    } else {
        /* TODO Try to determine the type of the fs */
    }
    if (!fs)
        return -ENOTSUP; /* fs doesn't exist. */

#ifdef configFS_DEBUG
    KERROR(KERROR_DEBUG, "Found fs: %s\n", fsname);
#endif

    if (!fs->mount) {
#ifdef configFS_DEBUG
        KERROR(KERROR_DEBUG, "fs %s isn't mountable\n", fsname);
#endif
        return -ENOTSUP; /* Not a mountable file system. */
    }

    err = fs->mount(source, flags, parm, parm_len, &sb);
    if (err)
        return err;
    KASSERT((uintptr_t)sb > configKERNEL_START, "sb is not a stack address");
    KASSERT(sb->root, "sb->root must be set");

    target = get_top_vnode(target);
    root = sb->root;

    /* We test for int mask to avoid blocking in kinit. */
    s = get_interrupt_state();
    if (!(s & PSR_INT_MASK)) {
        VN_LOCK(root);
        VN_LOCK(target);
    }

    sb->mountpoint = target;
    target->vn_next_mountpoint = root;
    root->vn_prev_mountpoint = target;
    root->vn_next_mountpoint = root;

    if (!(s & PSR_INT_MASK)) {
        VN_UNLOCK(target);
        VN_UNLOCK(root);
    }

    /* TODO inherit permissions */

#ifdef configFS_DEBUG
    KERROR(KERROR_DEBUG, "Mount OK\n");
#endif

    return 0;
}

int fs_umount(struct fs_superblock * sb)
{
    vnode_t * root;
    vnode_t * prev;
    vnode_t * next;

#ifdef configFS_DEBUG
    KERROR(KERROR_DEBUG, "fs_umount(sb:%p)\n", sb);
#endif
    KASSERT(sb, "sb is set");
    KASSERT(sb->fs && sb->root && sb->root->vn_prev_mountpoint &&
            sb->root->vn_prev_mountpoint->vn_next_mountpoint,
            "Sanity check");

    if (!sb->umount)
        return -ENOTSUP;

    root = sb->root;
    if (root->vn_prev_mountpoint == root)
        return -EINVAL; /* Can't unmount rootfs */

    /*
     * Reverse the mount process to unmount.
     */
    VN_LOCK(root);
    prev = root->vn_prev_mountpoint;
    next = root->vn_next_mountpoint;
    KASSERT(root != prev, "FS can't handle umount if root == prev");

    VN_LOCK(prev);
    if (next && next != root) {
        VN_LOCK(next);
        prev->vn_next_mountpoint = root->vn_next_mountpoint;
        next->vn_prev_mountpoint = prev;
        VN_UNLOCK(next);
    } else {
        prev->vn_next_mountpoint = prev;
    }
    VN_UNLOCK(prev);
    root->vn_next_mountpoint = root;
    root->vn_prev_mountpoint = root;
    VN_UNLOCK(root);

    return sb->umount(sb);
}

int lookup_vnode(vnode_t ** result, vnode_t * root, const char * str, int oflags)
{
    char * path;
    char * nodename;
    char * lasts;
    struct vnode * orig_vn;
    int retval = 0;

    if (!(result && root && root->vnode_ops && str))
        return -EINVAL;

    path = kstrdup(str, PATH_MAX);
    if (!path)
        return -ENOMEM;

    if (!(nodename = kstrtok(path, PATH_DELIMS, &lasts))) {
        retval = -EINVAL;
        goto out;
    }

    /*
     * Start looking up for a vnode.
     * We don't care if root is not a directory because lookup will spot it
     * anyway.
     */
    vref(root);
    *result = root;
    do {
        vnode_t * vnode = NULL;

        if (!strcmp(nodename, "."))
            continue;

again:  /* Get vnode by name in this dir. */
        orig_vn = *result;
        retval = orig_vn->vnode_ops->lookup(*result, nodename, &vnode);
        if (!retval) {
            KASSERT(vnode != NULL, "vnode should be valid if !retval");
            vrele(orig_vn);
        }
        if (retval != 0 && retval != -EDOM) {
            goto out;
        } else if (!vnode) {
            retval = -ENOENT;
            goto out;
        }

        /*
         * If retval == -EDOM the result and vnode are equal so we are at
         * the root of the physical file system and trying to exit its
         * mountpoint, this requires some additional processing as follows.
         */
        if (retval == -EDOM && !strcmp(nodename, "..") &&
            vnode->vn_prev_mountpoint != vnode) {
            /* Get prev dir of prev fs sb from mount point. */
            vnode = get_base_vnode(vnode);
            *result = vnode;
            vref(vnode);

            /* Restart from the begining to get the actual prev dir. */
            goto again;
        } else {
            orig_vn = vnode;

            /*
             * TODO
             * - soft links support
             * - if O_NOFOLLOW we should fail on soft link and return
             *   (-ELOOP)
             */

            /* Go to the last mountpoint. */
            vnode = get_top_vnode(vnode);
            *result = vnode;
            vrele(orig_vn);
            vref(vnode);
        }
        retval = 0;

#if configFS_DEBUG
        KASSERT(*result != NULL, "vfs is in inconsistent state");
#endif
    } while ((nodename = kstrtok(0, PATH_DELIMS, &lasts)));

    if ((oflags & O_DIRECTORY) && !S_ISDIR((*result)->vn_mode)) {
        vrele(*result);
        retval = -ENOTDIR;
        goto out;
    }

out:

    kfree(path);
    return retval;
}

int fs_namei_proc(vnode_t ** result, int fd, const char * path, int atflags)
{
    vnode_t * start;
    int oflags = atflags & AT_SYMLINK_NOFOLLOW;
    int retval;

#ifdef configFS_DEBUG
    KERROR(KERROR_DEBUG,
           "fs_namei_proc(result %p, fd %d, path \"%s\", atflags %d)\n",
           result, fd, path, atflags);
#endif

    if (path[0] == '\0')
        return -EINVAL;

    if (path[0] == '/') { /* Absolute path */
        path++;
        start = curproc->croot;
        if (path[0] == '\0') {
            *result = start;
            return 0;
        }
    } else if (atflags & AT_FDARG) { /* AT_FDARG */
        file_t * file;

        file = fs_fildes_ref(curproc->files, fd, 1);
        if (!file)
            return -EBADF;
        start = file->vnode;
    } else { /* Implicit AT_FDCWD */
        start = curproc->cwd;
    }

    if (path[strlenn(path, PATH_MAX)] == '/') {
        oflags |= O_DIRECTORY;
    }

    retval = lookup_vnode(result, start, path, oflags);

    if (atflags & AT_FDARG)
        fs_fildes_ref(curproc->files, fd, -1);

    return retval;
}

int chkperm(struct stat * stat, uid_t euid, gid_t egid, int oflags)
{
    oflags &= O_ACCMODE;

    if (oflags & R_OK) {
        mode_t req_perm = 0;

        if (stat->st_uid == euid)
            req_perm |= S_IRUSR;
        if (stat->st_gid == egid)
            req_perm |= S_IRGRP;
        req_perm |= S_IROTH;

        if (!(req_perm & stat->st_mode))
            return -EPERM;
    }

    if (oflags & W_OK) {
        mode_t req_perm = 0;

        if (stat->st_uid == euid)
            req_perm |= S_IWUSR;
        if (stat->st_gid == egid)
            req_perm |= S_IWGRP;
        req_perm |= S_IWOTH;

        if (!(req_perm & stat->st_mode))
            return -EPERM;
    }

    if ((oflags & X_OK) || (S_ISDIR(stat->st_mode))) {
        mode_t req_perm = 0;

        if (stat->st_uid == euid)
            req_perm |= S_IXUSR;
        if (stat->st_gid == egid)
            req_perm |= S_IXGRP;
        req_perm |= S_IXOTH;

        if (!(req_perm & stat->st_mode))
            return -EPERM;
    }

    return 0;
}

int chkperm_curproc(struct stat * stat, int oflags)
{
    uid_t euid = curproc->cred.euid;
    gid_t egid = curproc->cred.egid;

    return chkperm(stat, euid, egid, oflags);
}

int chkperm_vnode_curproc(vnode_t * vnode, int oflags)
{
    uid_t euid = curproc->cred.euid;
    gid_t egid = curproc->cred.egid;

    return chkperm_vnode(vnode, euid, egid, oflags);
}

int chkperm_vnode(vnode_t * vnode, uid_t euid, gid_t egid, int oflags)
{
    struct stat stat;
    int err;

    KASSERT(vnode != NULL, "vnode should be set\n");

    err = vnode->vnode_ops->stat(vnode, &stat);
    if (err)
        return err;

    err = chkperm(&stat, euid, egid, oflags);

    return err;
}

int fs_fildes_set(file_t * fildes, vnode_t * vnode, int oflags)
{
    if (!(fildes && vnode))
        return -1;

    fildes->vnode = vnode;
    fildes->oflags = oflags;
    fildes->refcount = 1;

    return 0;
}

int fs_fildes_create_curproc(vnode_t * vnode, int oflags)
{
    file_t * new_fildes;
    int err, retval;

    if (!vnode)
        return -EINVAL;
    vref(vnode);

    if (curproc->cred.euid == 0)
        goto perms_ok;

    /* Check if user perms gives access */
    err = chkperm_vnode_curproc(vnode, oflags);
    if (err) {
        retval = err;
        goto out;
    }

perms_ok:
    /* Check other oflags */
    if ((oflags & O_DIRECTORY) && (!S_ISDIR(vnode->vn_mode))) {
        retval = -ENOTDIR;
        goto out;
    }

    retval = vnode->vnode_ops->file_opened(curproc, vnode);
    if (retval < 0)
        goto out;

    new_fildes = kcalloc(1, sizeof(file_t));
    if (!new_fildes) {
        retval = -ENOMEM;
        goto out;
    }

    if (S_ISDIR(vnode->vn_mode))
        new_fildes->seek_pos = DIRENT_SEEK_START;

    int fd = fs_fildes_curproc_next(new_fildes, 0);
    if (fd < 0) {
        kfree(new_fildes);
        retval = fd;
        goto out;
    }
    fs_fildes_set(curproc->files->fd[fd], vnode, oflags);
    new_fildes->fdflags |= FD_KFREEABLE;

    retval = fd;
out:
    if (retval < 0)
        vrele(vnode);
    return retval;
}

int fs_fildes_curproc_next(file_t * new_file, int start)
{
    files_t * files = curproc->files;

    if (!new_file)
        return -EBADF;

    if (start > files->count - 1)
        return -EMFILE;

    for (int i = start; i < files->count; i++) {
        if (!(files->fd[i])) {
            curproc->files->fd[i] = new_file;
            return i;
        }
    }

    return -ENFILE;
}

file_t * fs_fildes_ref(files_t * files, int fd, int count)
{
    file_t * file;
    int old_refcount;

    KASSERT(files != NULL, "files should be set");

    if (fd >= files->count)
        return NULL;

    file = files->fd[fd];
    if (!file)
        return NULL;

    old_refcount = atomic_add(&file->refcount, count);
    if ((old_refcount + count) <= 0) {
        vnode_t * vn = file->vnode;

        if (file->fdflags & FD_KFREEABLE)
            kfree(file);
        vrele(vn);
        files->fd[fd] = NULL;

        return NULL;
    }

    return file;
}

int fs_fildes_close(struct proc_info * p, int fildes)
{
    file_t * fd = fs_fildes_ref(p->files, fildes, 1);
    if (!fd)
        return -EBADF;

    fd->vnode->vnode_ops->file_closed(p, fd);

    fs_fildes_ref(p->files, fildes, -2);
    p->files->fd[fildes] = NULL;

    return 0;
}

ssize_t fs_readwrite_curproc(int fildes, void * buf, size_t nbyte, int oper)
{
    vnode_t * vnode;
    file_t * file;
    ssize_t retval = -1;

    KASSERT(buf != NULL, "buf should be set\n");

    file = fs_fildes_ref(curproc->files, fildes, 1);
    if (!file)
        return -EBADF;
    vnode = file->vnode;

    /*
     * Check that file is opened with a correct mode and the vnode exist.
     */
    if (!(file->oflags & oper) || !vnode) {
        retval = -EBADF;
        goto out;
    }

    KASSERT((oper & O_ACCMODE) != (O_RDONLY | O_WRONLY),
            "Only read or write selected");
#if 0
    if ((oper & O_ACCMODE) == (O_RDONLY | O_WRONLY)) {
        /* Invalid operation code */
        retval = -ENOTSUP;
        goto out;
    }
#endif

    if (oper & O_RDONLY) {
        retval = vnode->vnode_ops->read(file, buf, nbyte);
    } else {
        retval = vnode->vnode_ops->write(file, buf, nbyte);
        if (retval == 0)
            retval = -EIO;
    }

out:
    fs_fildes_ref(curproc->files, fildes, -1);
    return retval;
}

/**
 * Get directory vnode of a target file and the actual directory entry name.
 * @param[in]   pathname    is a path to the target.
 * @param[out]  dir         is the directory containing the entry.
 * @param[out]  filename    is the actual file name / directory entry name.
 * @param[in]   flag        0 = file should exist; O_CREAT = file should not exist.
 */
static int getvndir(const char * pathname, vnode_t ** dir, char ** filename, int flag)
{
    vnode_t * vn_file;
    char * path = NULL;
    char * name = NULL;
    int err;

    if (pathname[0] == '\0') {
        err = -EINVAL;
        goto out;
    }

    err = fs_namei_proc(&vn_file, -1, pathname, AT_FDCWD);
    if (err == 0)
        vrele(vn_file);
    if (flag & O_CREAT) { /* File should not exist */
        if (err == 0) {
            err = -EEXIST;
            goto out;
        } else if (err != -ENOENT) {
            goto out;
        }
    } else if (err) { /* File should exist */
        goto out;
    }

    err = parsenames(pathname, &path, &name);
    if (err)
        goto out;

    kpalloc(name); /* Incr ref count */
    *filename = name;

    err = fs_namei_proc(dir, -1, path, AT_FDCWD);
    if (err)
        goto out;

out:
    kfree(path);
    kfree(name);

    return err;
}

int fs_creat_curproc(const char * pathname, mode_t mode, vnode_t ** result)
{
    char * name = NULL;
    vnode_t * dir = NULL;
    int retval = 0;

#if configFS_DEBUG
    KERROR(KERROR_DEBUG, "fs_creat_curproc(pathname \"%s\", mode %u)\n",
           pathname, (unsigned)mode);
#endif

    retval = getvndir(pathname, &dir, &name, O_CREAT);
    if (retval)
        goto out;

    /* We know that the returned vnode is a dir so we can just call mknod() */
    *result = NULL;
    mode &= ~S_IFMT; /* Filter out file type bits */
    mode &= ~curproc->files->umask;
    retval = dir->vnode_ops->create(dir, name, mode, result);

#if configFS_DEBUG
    KERROR(KERROR_DEBUG, "\tresult: %p\n", *result);
#endif

out:
    if (dir)
        vrele(dir);
    kfree(name);

    return retval;
}

int fs_link_curproc(const char * path1, size_t path1_len,
        const char * path2, size_t path2_len)
{
    char * targetname = NULL;
    vnode_t * vn_src = NULL;
    vnode_t * vndir_dst = NULL;
    int err;

    /* Get the source vnode */
    err = fs_namei_proc(&vn_src, -1, path1, AT_FDCWD);
    if (err)
        return err;

    /* Check write access to the source vnode */
    err = chkperm_vnode_curproc(vn_src, O_WRONLY);
    if (err)
        goto out;

    /* Get vnode of the target directory */
    err = getvndir(path2, &vndir_dst, &targetname, O_CREAT);
    if (err)
        goto out;

    if (vn_src->sb->vdev_id != vndir_dst->sb->vdev_id) {
        /*
         * The link named by path2 and the file named by path1 are
         * on different file systems.
         */
        err = -EXDEV;
        goto out;
    }

    err = chkperm_vnode_curproc(vndir_dst, O_WRONLY);
    if (err)
        goto out;

    err = vndir_dst->vnode_ops->link(vndir_dst, vn_src, targetname);

out:
    if (vn_src)
        vrele(vn_src);
    if (vndir_dst)
        vrele(vndir_dst);
    kfree(targetname);

    return err;
}

int fs_unlink_curproc(int fd, const char * path, size_t path_len, int atflags)
{
    char * dirpath = NULL;
    char * filename = NULL;
    struct stat stat;
    vnode_t * dir = NULL;
    int err;

    {
        vnode_t * fnode;

        err = fs_namei_proc(&fnode, fd, path, atflags);
        if (err)
            return err;

        /* unlink() is prohibited on directories for non-root users. */
        err = fnode->vnode_ops->stat(fnode, &stat);
        vrele(fnode);
        if (err)
            goto out;
        if (S_ISDIR(stat.st_mode) && curproc->cred.euid != 0) {
            err = -EPERM;
            goto out;
        }
    }

    err = parsenames(path, &dirpath, &filename);
    if (err)
        goto out;

    /* Get the vnode of the containing directory */
    if (fs_namei_proc(&dir, fd, dirpath, atflags)) {
        err = -ENOENT;
        goto out;
    }

    /*
     * We need a write access to the containing directory to allow removal of
     * a directory entry.
     */
    err = chkperm_vnode_curproc(dir, O_WRONLY);
    if (err) {
        if (err == -EPERM)
            err = -EACCES;
        goto out;
    }

    err = dir->vnode_ops->unlink(dir, filename);

out:
    if (dir)
        vrele(dir);
    kfree(dirpath);
    kfree(filename);

    return err;
}

int fs_mkdir_curproc(const char * pathname, mode_t mode)
{
    char * name = 0;
    vnode_t * dir = NULL;
    int retval = 0;

    retval = getvndir(pathname, &dir, &name, O_CREAT);
    if (retval)
        goto out;

    /* Check that we have a permission to write this dir. */
    retval = chkperm_vnode_curproc(dir, O_WRONLY);
    if (retval)
        goto out;

    mode &= ~S_IFMT; /* Filter out file type bits */
    mode &= ~curproc->files->umask;
    retval = dir->vnode_ops->mkdir(dir, name, mode);

out:
    if (dir)
        vrele(dir);
    kfree(name);

    return retval;
}

int fs_rmdir_curproc(const char * pathname)
{
    char * name = 0;
    vnode_t * dir = NULL;
    int retval;

    retval = getvndir(pathname, &dir, &name, 0);
    if (retval)
        goto out;

    /* Check that we have a permission to write this dir. */
    retval = chkperm_vnode_curproc(dir, O_WRONLY);
    if (retval)
        goto out;

    retval = dir->vnode_ops->rmdir(dir, name);

out:
    if (dir)
        vrele(dir);
    kfree(name);

    return retval;
}

int fs_utimes_curproc(int fildes, const struct timespec times[2])
{
    vnode_t * vnode;
    file_t * file;
    int retval = 0;

    file = fs_fildes_ref(curproc->files, fildes, 1);
    if (!file)
        return -EBADF;
    vnode = file->vnode;

    if (!((file->oflags & O_WRONLY) || !chkperm_vnode_curproc(vnode, W_OK))) {
        retval = -EPERM;
        goto out;
    }

    retval = vnode->vnode_ops->utimes(vnode, times);

out:
    fs_fildes_ref(curproc->files, fildes, -1);

    return retval;
}

int fs_chmod_curproc(int fildes, mode_t mode)
{
    vnode_t * vnode;
    file_t * file;
    int retval = 0;

    file = fs_fildes_ref(curproc->files, fildes, 1);
    if (!file)
        return -EBADF;
    vnode = file->vnode;

    if (!((file->oflags & O_WRONLY) || !chkperm_vnode_curproc(vnode, W_OK))) {
        retval = -EPERM;
        goto out;
    }

    retval = vnode->vnode_ops->chmod(vnode, mode);

out:
    fs_fildes_ref(curproc->files, fildes, -1);

    return retval;
}

int fs_chflags_curproc(int fildes, fflags_t flags)
{
    vnode_t * vnode;
    file_t * file;
    int retval = 0;

    file = fs_fildes_ref(curproc->files, fildes, 1);
    if (!file)
        return -EBADF;
    vnode = file->vnode;

    if (!((file->oflags & O_WRONLY) || !chkperm_vnode_curproc(vnode, W_OK))) {
        retval = -EPERM;
        goto out;
    }
    retval = priv_check(&curproc->cred, PRIV_VFS_SYSFLAGS);
    if (retval)
        goto out;

    retval = vnode->vnode_ops->chflags(vnode, flags);

out:
    fs_fildes_ref(curproc->files, fildes, -1);

    return retval;
}

int fs_chown_curproc(int fildes, uid_t owner, gid_t group)
{
    vnode_t * vnode;
    file_t * file;
    int retval = 0;

    file = fs_fildes_ref(curproc->files, fildes, 1);
    if (!file)
        return -EBADF;
    vnode = file->vnode;

    if (!((file->oflags & O_WRONLY) || !chkperm_vnode_curproc(vnode, W_OK))) {
        retval = -EPERM;
        goto out;
    }

    retval = vnode->vnode_ops->chown(vnode, owner, group);

out:
    fs_fildes_ref(curproc->files, fildes, -1);

    return retval;
}

#define KERROR_VREF_FMT(_STR_) "%s(%s:%u): " _STR_
#define KERROR_VREF(_LVL_, _FMT_, ...) \
    KERROR(_LVL_, KERROR_VREF_FMT(_FMT_), __func__, \
           vnode->sb->fs->fsname, (uint32_t)(vnode->vn_num), ##__VA_ARGS__)

int vrefcnt(struct vnode * vnode)
{
    int retval;

    retval = atomic_read(&vnode->vn_refcount);

    return retval;
}

void vrefset(vnode_t * vnode, int refcnt)
{
    atomic_set(&vnode->vn_refcount, refcnt);
}

int vref(vnode_t * vnode)
{
    int prev;

    prev = atomic_read(&vnode->vn_refcount);
    if (prev < 0) {
#if configFS_VREF_DEBUG
        KERROR_VREF(KERROR_ERR, "Failed, vnode will be freed soon or it's "
                                "orphan (%d)\n",
                    prev);
#endif
        return -ENOLINK;
    }

    prev = atomic_inc(&vnode->vn_refcount);

#if configFS_VREF_DEBUG
    KERROR_VREF(KERROR_DEBUG, "%d\n", prev);
#endif

    return 0;
}

void vrele(vnode_t * vnode)
{
    int prev;

    prev = atomic_dec(&vnode->vn_refcount);

#if configFS_VREF_DEBUG
    KERROR_VREF(KERROR_DEBUG, "%d\n", prev);
#endif

    if (prev <= 1)
        vnode->sb->delete_vnode(vnode);
}

void vrele_nunlink(vnode_t * vnode)
{
    atomic_dec(&vnode->vn_refcount);
}

void vput(vnode_t * vnode)
{
    int prev;

    KASSERT(mtx_test(&vnode->vn_lock), "vnode should be locked");

    prev = atomic_dec(&vnode->vn_refcount);
    VN_UNLOCK(vnode);
    if (prev <= 1)
        vnode->sb->delete_vnode(vnode);
}

void vunref(vnode_t * vnode)
{
    int prev;

    KASSERT(mtx_test(&vnode->vn_lock), "vnode should be locked");

    prev = atomic_dec(&vnode->vn_refcount);
    if (prev <= 1)
        vnode->sb->delete_vnode(vnode);
}
