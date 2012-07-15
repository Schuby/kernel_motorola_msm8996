/*
 * Remote processor framework
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Copyright (C) 2011 Google, Inc.
 *
 * Ohad Ben-Cohen <ohad@wizery.com>
 * Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef REMOTEPROC_INTERNAL_H
#define REMOTEPROC_INTERNAL_H

#include <linux/irqreturn.h>
#include <linux/firmware.h>

struct rproc;

/* from remoteproc_core.c */
void rproc_release(struct kref *kref);
irqreturn_t rproc_vq_interrupt(struct rproc *rproc, int vq_id);

/* from remoteproc_virtio.c */
int rproc_add_virtio_dev(struct rproc_vdev *rvdev, int id);
void rproc_remove_virtio_dev(struct rproc_vdev *rvdev);

/* from remoteproc_debugfs.c */
void rproc_remove_trace_file(struct dentry *tfile);
struct dentry *rproc_create_trace_file(const char *name, struct rproc *rproc,
					struct rproc_mem_entry *trace);
void rproc_delete_debug_dir(struct rproc *rproc);
void rproc_create_debug_dir(struct rproc *rproc);
void rproc_init_debugfs(void);
void rproc_exit_debugfs(void);

void rproc_free_vring(struct rproc_vring *rvring);
int rproc_alloc_vring(struct rproc_vdev *rvdev, int i);

void *rproc_da_to_va(struct rproc *rproc, u64 da, int len);

struct resource_table *rproc_find_rsc_table(struct rproc *rproc,
						const struct firmware *fw,
						int *tablesz);
int rproc_load_segments(struct rproc *rproc, const struct firmware *fw);
int rproc_fw_sanity_check(struct rproc *rproc, const struct firmware *fw);
u32 rproc_get_boot_addr(struct rproc *rproc, const struct firmware *fw);

#endif /* REMOTEPROC_INTERNAL_H */
