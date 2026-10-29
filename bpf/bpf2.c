// SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

/*
 * Shirudo authorizes tasks marked after exec from the authenticated runtime.
 * These hooks track verity devices, restrict DM controls, and protect the
 * runtime/config mounts alongside the shared bpffs and BPF admission checks.
 */
#include "verity_policy.h"
#include "admission.h"

char _license[] SEC("license") = "Dual BSD/GPL";

#define SHIRUDO_MOUNT_WALK_MAX		1024
#define SHIRUDO_MOUNT_DEPTH_MAX		32
#define SHIRUDO_MOUNT_MASTERS_MAX	4

static __always_inline struct mount *shirudo_mount_of(struct vfsmount *mnt)
{
	return (struct mount *)((__u64)mnt -
				bpf_core_field_offset(struct mount, mnt));
}

static __always_inline struct mount *shirudo_mount_child(__u64 entry)
{
	return (struct mount *)(entry -
				bpf_core_field_offset(struct mount, mnt_child));
}

static __always_inline __u64 shirudo_mount_children(struct mount *m)
{
	return (__u64)m + bpf_core_field_offset(struct mount, mnt_mounts);
}

/* The peer group a mount propagates to, 0 for one that propagates to none. */
static __always_inline int shirudo_mount_group(struct mount *m)
{
	if (!(BPF_CORE_READ(m, mnt_t_flags) & T_SHARED))
		return 0;
	return BPF_CORE_READ(m, mnt_group_id);
}

/*
 * Whether unmounting m unmounts a: a is m, a peer of m, or a slave of
 * m's peer group, directly or through masters of its own.
 */
static __always_inline bool shirudo_umount_reaches(struct mount *m, int group,
						   struct mount *a)
{
	int i;

	if (a == m)
		return true;
	if (!group)
		return false;
	bpf_for(i, 0, SHIRUDO_MOUNT_MASTERS_MAX) {
		if (shirudo_mount_group(a) == group)
			return true;
		a = BPF_CORE_READ(a, mnt_master);
		if (!a)
			return false;
	}
	return true;
}

/*
 * Whether unmounting m unmounts the bpffs mount init made or one above
 * it. The root of the namespace has no parent to be unmounted from.
 */
static __always_inline bool shirudo_umount_takes_bpffs(struct mount *m,
						       struct mount *bpffs)
{
	struct mount *a = bpffs, *parent;
	int group = shirudo_mount_group(m);
	int i;

	bpf_for(i, 0, SHIRUDO_MOUNT_DEPTH_MAX) {
		parent = BPF_CORE_READ(a, mnt_parent);
		if (!parent || parent == a)
			return false;
		if (shirudo_umount_reaches(m, group, a))
			return true;
		a = parent;
	}
	return true;
}

/*
 * The pins hold the enforcement links through the bpffs superblock,
 * which goes with the last mount of it: a lazy unmount of /run took the
 * private bpffs with it, and so did one of a copy of /run in another
 * namespace whose mounts still propagated to init's. Walk what the
 * unmount of t takes down, its subtree in first-child, next-sibling
 * order without a stack and through each mount what propagation takes
 * along, and refuse one that reaches the recorded mount or one above
 * it. The namespace lock is not held here, so a tree that cannot be
 * walked within the bounds is refused rather than taken for safe.
 */
static __always_inline int shirudo_umount_above(struct vfsmount *mnt)
{
	struct shirudo_mount_record *rec;
	struct mount *t, *m, *bpffs, *parent;
	__u32 zero = 0;
	bool sibling;
	__u64 next;
	int i, j;

	rec = bpf_map_lookup_elem(&shirudo_mnt_rec, &zero);
	if (!rec || !rec->mount)
		return 0;
	bpffs = (struct mount *)rec->mount;
	t = shirudo_mount_of(mnt);
	m = t;
	bpf_for(i, 0, SHIRUDO_MOUNT_WALK_MAX) {
		if (shirudo_umount_takes_bpffs(m, bpffs))
			goto deny;
		next = (__u64)BPF_CORE_READ(m, mnt_mounts.next);
		if (!next)
			goto deny;
		if (next != shirudo_mount_children(m)) {
			m = shirudo_mount_child(next);
			continue;
		}
		sibling = false;
		bpf_for(j, 0, SHIRUDO_MOUNT_DEPTH_MAX) {
			if (m == t)
				return 0;
			parent = BPF_CORE_READ(m, mnt_parent);
			next = (__u64)BPF_CORE_READ(m, mnt_child.next);
			if (!parent || !next)
				goto deny;
			if (next != shirudo_mount_children(parent)) {
				m = shirudo_mount_child(next);
				sibling = true;
				break;
			}
			m = parent;
		}
		if (!sibling)
			goto deny;
	}
deny:
	return shirudo_tamper_deny(SHIRUDO_TAMPER_BPFFS_UMOUNT_ABOVE, -EPERM);
}

/*
 * A pin opened as a file reads a map's contents through bpffs, so pins
 * open for shirudo alone; directories stay listable. The bpffs mount of
 * init's namespace is recorded once when the bootstrap task opens its
 * root with the mount guards live: the one the unmount guard keeps,
 * where a copy in another namespace is any process's to drop.
 */
SEC("lsm/file_open")
int BPF_PROG(shirudo_lsm_file_open, struct file *file, int ret)
{
	struct shirudo_sb_id *id = shirudo_policy();
	struct shirudo_mount_record *rec;
	struct vfsmount *mnt;
	__u32 zero = 0;

	if (ret || !id)
		return ret;
	mnt = BPF_CORE_READ(file, f_path.mnt);
	if (!mnt || BPF_CORE_READ(mnt, mnt_sb, s_dev) != id->sb_dev)
		return 0;
	if ((BPF_CORE_READ(file, f_inode, i_mode) & S_IFMT) != S_IFDIR)
		return shirudo_self_or_deny(SHIRUDO_TAMPER_OBJ_GET);
	if (!shirudo_bootstrap_task())
		return 0;
	rec = bpf_map_lookup_elem(&shirudo_mnt_rec, &zero);
	if (rec && !rec->mount)
		rec->mount = (__u64)shirudo_mount_of(mnt);
	return 0;
}

static __always_inline int shirudo_parent_mount(struct super_block *sb,
						bool trusted_target)
{
	struct shirudo_sb_id *id = shirudo_policy();

	if (!sb || !id || !id->run_dev ||
	    BPF_CORE_READ(sb, s_dev) != id->run_dev)
		return 0;
	return trusted_target && shirudo_task_is_self(id) ? 0 : -EPERM;
}

static __always_inline int shirudo_image_mount(struct super_block *sb,
					       bool replace)
{
	struct shirudo_sb_id *id = shirudo_policy();
	struct shirudo_device *device;
	struct block_device *bdev;

	if (!sb || !id)
		return 0;
	bdev = BPF_CORE_READ(sb, s_bdev);
	if (!bdev)
		return 0;
	device = shirudo_bdev_device(bdev);
	if (!device)
		return 0;
	if (replace && device->kind == SHIRUDO_DEVICE_CONFIG &&
	    shirudo_task_is_self(id))
		return 0;
	return -EPERM;
}

SEC("lsm/sb_umount")
int BPF_PROG(shirudo_lsm_sb_umount, struct vfsmount *mnt, int flags,
	     int ret)
{
	struct super_block *sb = BPF_CORE_READ(mnt, mnt_sb);
	__u32 dev;

	if (ret)
		return ret;
	ret = shirudo_parent_mount(sb, false);
	if (ret)
		return ret;
	if (BPF_CORE_READ_INTO(&dev, sb, s_dev))
		return shirudo_tamper_deny(SHIRUDO_TAMPER_BPFFS_UMOUNT,
					   -EPERM);
	ret = shirudo_deny_bpffs_mount(dev, SHIRUDO_TAMPER_BPFFS_UMOUNT);
	if (!ret)
		ret = shirudo_umount_above(mnt);
	return ret ? ret : shirudo_image_mount(sb, true);
}

SEC("lsm/move_mount")
int BPF_PROG(shirudo_lsm_move_mount, const struct path *from_path,
	     const struct path *to_path, int ret)
{
	struct super_block *from = BPF_CORE_READ(from_path, mnt, mnt_sb);
	struct super_block *to = BPF_CORE_READ(to_path, mnt, mnt_sb);

	if (ret)
		return ret;
	if (shirudo_parent_mount(from, false) ||
	    shirudo_parent_mount(to, true))
		return -EPERM;
	if (shirudo_deny_bpffs_mount(BPF_CORE_READ(from, s_dev),
				     SHIRUDO_TAMPER_BPFFS_MOUNT) ||
	    shirudo_deny_bpffs_mount(BPF_CORE_READ(to, s_dev),
				     SHIRUDO_TAMPER_BPFFS_MOUNT))
		return -EPERM;
	ret = shirudo_image_mount(from, true);
	return ret ? ret : shirudo_image_mount(to, true);
}

SEC("lsm/sb_mount")
int BPF_PROG(shirudo_lsm_sb_mount, const char *dev_name,
	     const struct path *path, const char *type,
	     unsigned long flags, void *data, int ret)
{
	struct super_block *sb = BPF_CORE_READ(path, mnt, mnt_sb);

	if (ret)
		return ret;
	if ((flags & ((1UL << 17) | (1UL << 18) | (1UL << 19) |
		      (1UL << 20))) && shirudo_parent_mount(sb, false))
		return -EPERM;
	ret = shirudo_parent_mount(sb, true);
	if (ret)
		return ret;
	ret = shirudo_deny_bpffs_mount(BPF_CORE_READ(sb, s_dev),
				       SHIRUDO_TAMPER_BPFFS_MOUNT);
	return ret ? ret : shirudo_image_mount(sb, true);
}

SEC("lsm/sb_remount")
int BPF_PROG(shirudo_lsm_sb_remount, struct super_block *sb,
	     void *mnt_opts, int ret)
{
	if (ret)
		return ret;
	ret = shirudo_parent_mount(sb, false);
	if (ret)
		return ret;
	ret = shirudo_deny_bpffs_mount(BPF_CORE_READ(sb, s_dev),
				       SHIRUDO_TAMPER_BPFFS_MOUNT);
	return ret ? ret : shirudo_image_mount(sb, false);
}

struct dm_verity_digest___shirudo {
	const char *alg;
	const __u8 *digest;
	size_t digest_len;
} __attribute__((preserve_access_index));

/*
 * Runtime roots match the frozen UKI value. Config roots must match the current
 * trusted task's one-use expectation; unrelated mappings gain no status.
 */
SEC("lsm.s/bdev_setintegrity")
int BPF_PROG(shirudo_lsm_bdev_setintegrity, struct block_device *bdev,
	     enum lsm_integrity_type type, const void *value,
	     size_t size, int ret)
{
	const struct dm_verity_digest___shirudo *digest = value;
	struct shirudo_sb_id *id = shirudo_policy();
	struct shirudo_expected_root *expected;
	struct shirudo_device device = {};
	__u64 cookie = (__u64)bdev;
	const unsigned char *root;
	const char *algorithm;
	unsigned int i;
	char name[8];
	int diff;

	if (ret)
		return ret;
	if (!id || type != LSM_INT_DMVERITY_ROOTHASH)
		return 0;
	if (!value) {
		bpf_map_delete_elem(&shirudo_devices, &cookie);
		return 0;
	}
	if (shirudo_bdev_device(bdev))
		return -EPERM;
	if (BPF_CORE_READ(digest, digest_len) != sizeof(device.root))
		return 0;
	algorithm = BPF_CORE_READ(digest, alg);
	if (bpf_probe_read_kernel_str(name, sizeof(name), algorithm) != 7 ||
	    name[0] != 's' || name[1] != 'h' || name[2] != 'a' ||
	    name[3] != '2' || name[4] != '5' || name[5] != '6')
		return 0;
	root = BPF_CORE_READ(digest, digest);
	if (bpf_probe_read_kernel(device.root, sizeof(device.root), root))
		return -EACCES;
	diff = 0;
	for (i = 0; i < sizeof(device.root); i++)
		diff |= device.root[i] ^ id->runtime_root[i];
	if (!diff) {
		device.kind = SHIRUDO_DEVICE_RUNTIME;
	} else {
		if (!shirudo_task_is_self(id))
			return 0;
		expected = bpf_task_storage_get(&shirudo_roots,
						bpf_get_current_task_btf(),
						NULL, 0);
		if (!expected || !expected->valid)
			return 0;
		diff = 0;
		for (i = 0; i < sizeof(device.root); i++)
			diff |= device.root[i] ^ expected->root[i];
		if (diff)
			return 0;
		expected->valid = 0;
		device.kind = SHIRUDO_DEVICE_CONFIG;
	}
	device.dev = BPF_CORE_READ(bdev, bd_dev);
	device.diskseq = BPF_CORE_READ(bdev, bd_disk, diskseq);
	if (!device.diskseq)
		return -EACCES;
	if (bpf_map_update_elem(&shirudo_devices, &cookie, &device,
				BPF_ANY))
		return -ENOMEM;
	return 0;
}

SEC("lsm/bdev_free_security")
void BPF_PROG(shirudo_lsm_bdev_free_security, struct block_device *bdev)
{
	__u64 cookie = (__u64)bdev;

	bpf_map_delete_elem(&shirudo_devices, &cookie);
}

#define SHIRUDO_DM_CONTROL_DEV	((10U << 20) | 236U)
#define SHIRUDO_DM_IOCTL_TYPE	0xfdU

/* Match the kernel's type/number dispatch. */
static __always_inline int shirudo_dm_ioctl(struct file *file,
					    unsigned int cmd, int ret)
{
	struct shirudo_sb_id *id = shirudo_policy();
	struct inode *inode;

	if (ret || !id || shirudo_task_is_self(id))
		return ret;
	inode = BPF_CORE_READ(file, f_inode);
	if ((BPF_CORE_READ(inode, i_mode) & 0170000) != 0020000 ||
	    BPF_CORE_READ(inode, i_rdev) != SHIRUDO_DM_CONTROL_DEV)
		return 0;
	if (((cmd >> 8) & 0xff) != SHIRUDO_DM_IOCTL_TYPE)
		return -EPERM;
	switch (cmd & 0xff) {
	case 0:  /* DM_VERSION */
	case 2:  /* DM_LIST_DEVICES */
	case 7:  /* DM_DEV_STATUS */
	case 8:  /* DM_DEV_WAIT */
	case 11: /* DM_TABLE_DEPS */
	case 12: /* DM_TABLE_STATUS */
	case 13: /* DM_LIST_VERSIONS */
	case 17: /* DM_GET_TARGET_VERSION */
		return 0;
	default:
		return -EPERM;
	}
}

/*
 * PERF_EVENT_IOC_SET_BPF binds a program to a perf event without a bpf()
 * command, and a program's fd is root's for the asking by id: a kprobe
 * program a signed loader created would run at a probe of root's own.
 * Keep it to the caller BPF_LINK_CREATE is kept to.
 */
static __always_inline int shirudo_ioctl(struct file *file, unsigned int cmd,
					 int ret)
{
	if (ret)
		return ret;
	if (cmd == SHIRUDO_PERF_EVENT_IOC_SET_BPF)
		return shirudo_self_or_deny(SHIRUDO_TAMPER_PERF_ATTACH);
	return shirudo_dm_ioctl(file, cmd, ret);
}

SEC("lsm/file_ioctl")
int BPF_PROG(shirudo_lsm_file_ioctl, struct file *file, unsigned int cmd,
	     unsigned long arg, int ret)
{
	return shirudo_ioctl(file, cmd, ret);
}

SEC("lsm/file_ioctl_compat")
int BPF_PROG(shirudo_lsm_file_ioctl_compat, struct file *file,
	     unsigned int cmd, unsigned long arg, int ret)
{
	return shirudo_ioctl(file, cmd, ret);
}
