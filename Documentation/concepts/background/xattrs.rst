..
   SPDX-License-Identifier: BSD-2-Clause
   Copyright (c) 2026 Authors of shirudo

:page-type: Explanation
:audience: All readers

Extended attributes
===================

A security policy often has to mark files: this one belongs to the
policy, that one may be executed. Owner and mode bits cannot carry such
a mark, and a list of paths breaks as soon as a file is renamed or a
directory is bind-mounted elsewhere. Extended attributes attach the mark
to the inode itself. This page explains them, how security modules use
them, and how shirudo's seal does.

Names and namespaces
--------------------

An extended attribute (xattr) is a name and a value stored with an
inode. Userspace reads and writes them with ``getxattr(2)``,
``setxattr(2)``, ``listxattr(2)`` and ``removexattr(2)``. Every name
starts with a namespace prefix, and the namespace decides who may access
it:

``user.``
    Ordinary file permissions apply: reading needs read permission on
    the file, writing needs write permission. Only regular files,
    directories and sockets take them, and on a sticky directory only
    its owner or a privileged task may write them.

``trusted.``
    Reading and writing both need ``CAP_SYS_ADMIN``.

``security.``
    Reserved for security modules. The file system layer does not
    restrict reading. Writing needs ``CAP_SYS_ADMIN`` in the file system's
    user namespace, unless a security module takes over that check for
    its own names, and every write also passes the ``inode_setxattr``
    hooks of all active LSMs.

``system.``
    Used by the kernel itself, for POSIX ACLs for example. The file
    system decides access.

How security modules label inodes
---------------------------------

An LSM that keeps per-file state stores it as a ``security.`` xattr:
SELinux uses ``security.selinux``, IMA ``security.ima``, file
capabilities ``security.capability``. The label travels with the inode
through renames and bind mounts.

A label is most useful when an inode carries it from the moment it
exists. When a file system creates an inode, it calls the
``inode_init_security`` hook. Each LSM may fill reserved slots with the
xattrs the new inode should carry, and the file system writes them as
part of the same operation that creates the inode. A file system without
that support passes no slots, and the inode starts unlabelled.

Later changes go through ``setxattr(2)`` and ``removexattr(2)``. The
``inode_setxattr`` and ``inode_removexattr`` hooks run before the change
and can refuse it. The ``inode_post_setxattr`` and
``inode_post_removexattr`` hooks run after it.

security.bpf. namespace and its kfuncs
--------------------------------------

BPF LSM programs (see :doc:`bpf-lsm`) can work with xattrs through
kfuncs:

``bpf_get_file_xattr()`` and ``bpf_get_dentry_xattr()``
    Read an xattr whose name starts with ``user.`` or ``security.bpf.``.
    They first check that the current task may read the inode
    (``inode_permission()`` with ``MAY_READ``) and return its error if
    not. Both may sleep, so only sleepable programs call them.

``bpf_set_dentry_xattr()`` and ``bpf_remove_dentry_xattr()``
    Write or remove a ``security.bpf.`` xattr, after a check that the
    current task may write the inode. Only sleepable LSM programs may
    call them.

``bpf_inode_init_xattr()``
    Fills one of the slots of the ``inode_init_security`` hook with a
    ``security.bpf.`` xattr, so that a new inode is created with it. Only
    a program on that hook may call it, and the BPF LSM reserves two
    slots per inode.

Userspace can also write ``security.bpf.`` names with ``setxattr(2)``,
under the rules of the ``security.`` namespace.

File system support
-------------------

Whether a file system stores ``security.`` xattrs depends on the file
system and its configuration. ext4 stores them with
``CONFIG_EXT4_FS_SECURITY``. tmpfs stores ``security.``, ``trusted.``
and ``user.`` names with ``CONFIG_TMPFS_XATTR``. bpffs stores
``security.`` and ``trusted.`` names. On a file system without support,
the calls fail with ``EOPNOTSUPP``.

overlayfs needs care. Before the first write to a file from a lower
layer, overlayfs copies the file up into the upper layer, and the copy
takes the lower file's xattrs with it, labels included. The write then
changes the copy, and the original stays as it was.

How shirudo uses xattrs
-----------------------

shirudo's seal marks writable state with the xattr
``security.bpf.shirudo``. Its value is a fixed 64-character string, the
same on every target; the seal treats any inode that carries a label
under that name as protected. ``shirudo init`` labels the state tree
under ``/var/lib/shirudo`` on its first run, and ``/run/shirudo`` on
every boot. From then on, the seal's ``lsm/inode_init_security``
program labels every new inode created in a marked directory, with
``bpf_inode_init_xattr()``.

The seal's sleepable programs read the label with
``bpf_get_file_xattr()`` and ``bpf_get_dentry_xattr()``. A labelled inode
may be opened for writing, removed, renamed, changed in its attributes or
xattrs only by a task with runtime authority. Only such a task may set
the label itself; anyone else's attempt counts as ``SealLabel``, and
other xattr changes on a labelled inode as ``SealXattr``. The label marks
what is protected; it never grants authority to the task that carries
or copies it. The seal also refuses an overlay copy-up of a marked inode
(``SealCopyUp``). See :doc:`/internals/seal`.

The second xattr, ``security.bpf.shirudo.payload``, sits on each upatch
directory in the private bpffs, ``/run/shirudo/bpffs/upatches/<name>``.
``shirudo upatch load`` writes the 32-byte SHA-256 of the payload there.
``shirudo upatch unload`` reads it and refuses with ``payload_mismatch``
when it differs from the digest the signed unload directive names, or
cannot be read. shirudo's ``lsm/inode_setxattr`` and
``lsm/inode_removexattr`` programs refuse xattr changes in the private
bpffs to every task without runtime authority (``PinXattr``). See
:doc:`/reference/files` for the paths.

Limits
------

The read kfuncs check the calling task's read permission even for
``security.bpf.`` names. When another LSM, or file permissions, deny that
task read access, the seal sees no label and allows the operation; the
file system's own permission checks then decide. The threat model lists
this under threats to resolve: :doc:`/security/threat-model`.

The seal caches the label of directories in inode storage, which is lost
when a directory leaves the inode cache. A file system without
``security.`` xattrs cannot carry the label at all.
