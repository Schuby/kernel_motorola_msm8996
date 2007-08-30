/*
 * Copyright (C) 2007 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/smp_lock.h>
#include <linux/backing-dev.h>
#include <linux/mpage.h>
#include <linux/swap.h>
#include <linux/writeback.h>
#include <linux/statfs.h>
#include <linux/compat.h>
#include <linux/bit_spinlock.h>
#include <linux/version.h>
#include "ctree.h"
#include "disk-io.h"
#include "transaction.h"
#include "btrfs_inode.h"
#include "ioctl.h"
#include "print-tree.h"

struct btrfs_iget_args {
	u64 ino;
	struct btrfs_root *root;
};

static struct inode_operations btrfs_dir_inode_operations;
static struct inode_operations btrfs_symlink_inode_operations;
static struct inode_operations btrfs_dir_ro_inode_operations;
static struct inode_operations btrfs_special_inode_operations;
static struct inode_operations btrfs_file_inode_operations;
static struct address_space_operations btrfs_aops;
static struct address_space_operations btrfs_symlink_aops;
static struct file_operations btrfs_dir_file_operations;
static struct extent_map_ops btrfs_extent_map_ops;

static struct kmem_cache *btrfs_inode_cachep;
struct kmem_cache *btrfs_trans_handle_cachep;
struct kmem_cache *btrfs_transaction_cachep;
struct kmem_cache *btrfs_bit_radix_cachep;
struct kmem_cache *btrfs_path_cachep;

#define S_SHIFT 12
static unsigned char btrfs_type_by_mode[S_IFMT >> S_SHIFT] = {
	[S_IFREG >> S_SHIFT]	= BTRFS_FT_REG_FILE,
	[S_IFDIR >> S_SHIFT]	= BTRFS_FT_DIR,
	[S_IFCHR >> S_SHIFT]	= BTRFS_FT_CHRDEV,
	[S_IFBLK >> S_SHIFT]	= BTRFS_FT_BLKDEV,
	[S_IFIFO >> S_SHIFT]	= BTRFS_FT_FIFO,
	[S_IFSOCK >> S_SHIFT]	= BTRFS_FT_SOCK,
	[S_IFLNK >> S_SHIFT]	= BTRFS_FT_SYMLINK,
};

static int run_delalloc_range(struct inode *inode, u64 start, u64 end)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;
	struct btrfs_key ins;
	u64 alloc_hint = 0;
	u64 num_blocks;
	int ret;
	u64 blocksize = 1 << inode->i_blkbits;

	mutex_lock(&root->fs_info->fs_mutex);
	trans = btrfs_start_transaction(root, 1);
	btrfs_set_trans_block_group(trans, inode);
	BUG_ON(!trans);
	num_blocks = (end - start + blocksize) & ~(blocksize - 1);
	ret = btrfs_drop_extents(trans, root, inode,
				 start, start + num_blocks, &alloc_hint);
	num_blocks = num_blocks >> inode->i_blkbits;
	ret = btrfs_alloc_extent(trans, root, inode->i_ino, num_blocks, 0,
				 alloc_hint, (u64)-1, &ins, 1);
	if (ret) {
		WARN_ON(1);
		goto out;
	}
	ret = btrfs_insert_file_extent(trans, root, inode->i_ino,
				       start, ins.objectid, ins.offset,
				       ins.offset);
out:
	btrfs_end_transaction(trans, root);
	mutex_unlock(&root->fs_info->fs_mutex);
	return ret;
}

int btrfs_writepage_io_hook(struct page *page, u64 start, u64 end)
{
	struct inode *inode = page->mapping->host;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;
	char *kaddr;
	int ret;
	u64 page_start = page->index << PAGE_CACHE_SHIFT;
	size_t offset = start - page_start;

	mutex_lock(&root->fs_info->fs_mutex);
	trans = btrfs_start_transaction(root, 1);
	btrfs_set_trans_block_group(trans, inode);
	kaddr = kmap(page);
	btrfs_csum_file_block(trans, root, inode->i_ino,
			      start, kaddr + offset, end - start + 1);
	kunmap(page);
	ret = btrfs_end_transaction(trans, root);
	BUG_ON(ret);
	mutex_unlock(&root->fs_info->fs_mutex);
	return ret;
}

int btrfs_readpage_io_hook(struct page *page, u64 start, u64 end)
{
	int ret = 0;
	struct inode *inode = page->mapping->host;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct extent_map_tree *em_tree = &BTRFS_I(inode)->extent_tree;
	struct btrfs_csum_item *item;
	struct btrfs_path *path = NULL;
	u64 private;

	mutex_lock(&root->fs_info->fs_mutex);
	path = btrfs_alloc_path();
	item = btrfs_lookup_csum(NULL, root, path, inode->i_ino, start, 0);
	if (IS_ERR(item)) {
		ret = PTR_ERR(item);
		/* a csum that isn't present is a preallocated region. */
		if (ret == -ENOENT || ret == -EFBIG)
			ret = 0;
		private = 0;
		goto out;
	}
	memcpy((char *)&private, &item->csum, BTRFS_CRC32_SIZE);
	set_state_private(em_tree, start, private);
out:
	if (path)
		btrfs_free_path(path);
	mutex_unlock(&root->fs_info->fs_mutex);
	return ret;
}

int btrfs_readpage_end_io_hook(struct page *page, u64 start, u64 end)
{
	char csum[BTRFS_CRC32_SIZE];
	size_t offset = start - (page->index << PAGE_CACHE_SHIFT);
	struct inode *inode = page->mapping->host;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct extent_map_tree *em_tree = &BTRFS_I(inode)->extent_tree;
	char *kaddr;
	u64 private;
	int ret;

	ret = get_state_private(em_tree, start, &private);
	kaddr = kmap_atomic(page, KM_IRQ0);
	if (ret) {
		goto zeroit;
	}
	ret = btrfs_csum_data(root, kaddr + offset, end - start + 1, csum);
	BUG_ON(ret);
	if (memcmp(csum, &private, BTRFS_CRC32_SIZE)) {
		goto zeroit;
	}
	kunmap_atomic(kaddr, KM_IRQ0);
	return 0;

zeroit:
	printk("btrfs csum failed ino %lu off %llu\n",
	       page->mapping->host->i_ino, (unsigned long long)start);
	memset(kaddr + offset, 1, end - start + 1); flush_dcache_page(page);
	kunmap_atomic(kaddr, KM_IRQ0);
	return 0;
}

void btrfs_read_locked_inode(struct inode *inode)
{
	struct btrfs_path *path;
	struct btrfs_inode_item *inode_item;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_key location;
	u64 alloc_group_block;
	u32 rdev;
	int ret;

	path = btrfs_alloc_path();
	BUG_ON(!path);
	mutex_lock(&root->fs_info->fs_mutex);

	memcpy(&location, &BTRFS_I(inode)->location, sizeof(location));
	ret = btrfs_lookup_inode(NULL, root, path, &location, 0);
	if (ret) {
		btrfs_free_path(path);
		goto make_bad;
	}
	inode_item = btrfs_item_ptr(btrfs_buffer_leaf(path->nodes[0]),
				  path->slots[0],
				  struct btrfs_inode_item);

	inode->i_mode = btrfs_inode_mode(inode_item);
	inode->i_nlink = btrfs_inode_nlink(inode_item);
	inode->i_uid = btrfs_inode_uid(inode_item);
	inode->i_gid = btrfs_inode_gid(inode_item);
	inode->i_size = btrfs_inode_size(inode_item);
	inode->i_atime.tv_sec = btrfs_timespec_sec(&inode_item->atime);
	inode->i_atime.tv_nsec = btrfs_timespec_nsec(&inode_item->atime);
	inode->i_mtime.tv_sec = btrfs_timespec_sec(&inode_item->mtime);
	inode->i_mtime.tv_nsec = btrfs_timespec_nsec(&inode_item->mtime);
	inode->i_ctime.tv_sec = btrfs_timespec_sec(&inode_item->ctime);
	inode->i_ctime.tv_nsec = btrfs_timespec_nsec(&inode_item->ctime);
	inode->i_blocks = btrfs_inode_nblocks(inode_item);
	inode->i_generation = btrfs_inode_generation(inode_item);
	inode->i_rdev = 0;
	rdev = btrfs_inode_rdev(inode_item);
	alloc_group_block = btrfs_inode_block_group(inode_item);
	BTRFS_I(inode)->block_group = btrfs_lookup_block_group(root->fs_info,
						       alloc_group_block);

	btrfs_free_path(path);
	inode_item = NULL;

	mutex_unlock(&root->fs_info->fs_mutex);

	switch (inode->i_mode & S_IFMT) {
	case S_IFREG:
		inode->i_mapping->a_ops = &btrfs_aops;
		BTRFS_I(inode)->extent_tree.ops = &btrfs_extent_map_ops;
		inode->i_fop = &btrfs_file_operations;
		inode->i_op = &btrfs_file_inode_operations;
		break;
	case S_IFDIR:
		inode->i_fop = &btrfs_dir_file_operations;
		if (root == root->fs_info->tree_root)
			inode->i_op = &btrfs_dir_ro_inode_operations;
		else
			inode->i_op = &btrfs_dir_inode_operations;
		break;
	case S_IFLNK:
		inode->i_op = &btrfs_symlink_inode_operations;
		inode->i_mapping->a_ops = &btrfs_symlink_aops;
		break;
	default:
		init_special_inode(inode, inode->i_mode, rdev);
		break;
	}
	return;

make_bad:
	btrfs_release_path(root, path);
	btrfs_free_path(path);
	mutex_unlock(&root->fs_info->fs_mutex);
	make_bad_inode(inode);
}

static void fill_inode_item(struct btrfs_inode_item *item,
			    struct inode *inode)
{
	btrfs_set_inode_uid(item, inode->i_uid);
	btrfs_set_inode_gid(item, inode->i_gid);
	btrfs_set_inode_size(item, inode->i_size);
	btrfs_set_inode_mode(item, inode->i_mode);
	btrfs_set_inode_nlink(item, inode->i_nlink);
	btrfs_set_timespec_sec(&item->atime, inode->i_atime.tv_sec);
	btrfs_set_timespec_nsec(&item->atime, inode->i_atime.tv_nsec);
	btrfs_set_timespec_sec(&item->mtime, inode->i_mtime.tv_sec);
	btrfs_set_timespec_nsec(&item->mtime, inode->i_mtime.tv_nsec);
	btrfs_set_timespec_sec(&item->ctime, inode->i_ctime.tv_sec);
	btrfs_set_timespec_nsec(&item->ctime, inode->i_ctime.tv_nsec);
	btrfs_set_inode_nblocks(item, inode->i_blocks);
	btrfs_set_inode_generation(item, inode->i_generation);
	btrfs_set_inode_rdev(item, inode->i_rdev);
	btrfs_set_inode_block_group(item,
				    BTRFS_I(inode)->block_group->key.objectid);
}

int btrfs_update_inode(struct btrfs_trans_handle *trans,
			      struct btrfs_root *root,
			      struct inode *inode)
{
	struct btrfs_inode_item *inode_item;
	struct btrfs_path *path;
	int ret;

	path = btrfs_alloc_path();
	BUG_ON(!path);
	ret = btrfs_lookup_inode(trans, root, path,
				 &BTRFS_I(inode)->location, 1);
	if (ret) {
		if (ret > 0)
			ret = -ENOENT;
		goto failed;
	}

	inode_item = btrfs_item_ptr(btrfs_buffer_leaf(path->nodes[0]),
				  path->slots[0],
				  struct btrfs_inode_item);

	fill_inode_item(inode_item, inode);
	btrfs_mark_buffer_dirty(path->nodes[0]);
	btrfs_set_inode_last_trans(trans, inode);
	ret = 0;
failed:
	btrfs_release_path(root, path);
	btrfs_free_path(path);
	return ret;
}


static int btrfs_unlink_trans(struct btrfs_trans_handle *trans,
			      struct btrfs_root *root,
			      struct inode *dir,
			      struct dentry *dentry)
{
	struct btrfs_path *path;
	const char *name = dentry->d_name.name;
	int name_len = dentry->d_name.len;
	int ret = 0;
	u64 objectid;
	struct btrfs_dir_item *di;

	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto err;
	}

	di = btrfs_lookup_dir_item(trans, root, path, dir->i_ino,
				    name, name_len, -1);
	if (IS_ERR(di)) {
		ret = PTR_ERR(di);
		goto err;
	}
	if (!di) {
		ret = -ENOENT;
		goto err;
	}
	objectid = btrfs_disk_key_objectid(&di->location);
	ret = btrfs_delete_one_dir_name(trans, root, path, di);
	if (ret)
		goto err;
	btrfs_release_path(root, path);

	di = btrfs_lookup_dir_index_item(trans, root, path, dir->i_ino,
					 objectid, name, name_len, -1);
	if (IS_ERR(di)) {
		ret = PTR_ERR(di);
		goto err;
	}
	if (!di) {
		ret = -ENOENT;
		goto err;
	}
	ret = btrfs_delete_one_dir_name(trans, root, path, di);

	dentry->d_inode->i_ctime = dir->i_ctime;
err:
	btrfs_free_path(path);
	if (!ret) {
		dir->i_size -= name_len * 2;
		dir->i_mtime = dir->i_ctime = CURRENT_TIME;
		btrfs_update_inode(trans, root, dir);
		drop_nlink(dentry->d_inode);
		ret = btrfs_update_inode(trans, root, dentry->d_inode);
		dir->i_sb->s_dirt = 1;
	}
	return ret;
}

static int btrfs_unlink(struct inode *dir, struct dentry *dentry)
{
	struct btrfs_root *root;
	struct btrfs_trans_handle *trans;
	int ret;

	root = BTRFS_I(dir)->root;
	mutex_lock(&root->fs_info->fs_mutex);
	trans = btrfs_start_transaction(root, 1);
	btrfs_set_trans_block_group(trans, dir);
	ret = btrfs_unlink_trans(trans, root, dir, dentry);
	btrfs_end_transaction(trans, root);
	mutex_unlock(&root->fs_info->fs_mutex);
	btrfs_btree_balance_dirty(root);
	return ret;
}

static int btrfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	struct inode *inode = dentry->d_inode;
	int err;
	int ret;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct btrfs_path *path;
	struct btrfs_key key;
	struct btrfs_trans_handle *trans;
	struct btrfs_key found_key;
	int found_type;
	struct btrfs_leaf *leaf;
	char *goodnames = "..";

	path = btrfs_alloc_path();
	BUG_ON(!path);
	mutex_lock(&root->fs_info->fs_mutex);
	trans = btrfs_start_transaction(root, 1);
	btrfs_set_trans_block_group(trans, dir);
	key.objectid = inode->i_ino;
	key.offset = (u64)-1;
	key.flags = (u32)-1;
	while(1) {
		ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
		if (ret < 0) {
			err = ret;
			goto out;
		}
		BUG_ON(ret == 0);
		if (path->slots[0] == 0) {
			err = -ENOENT;
			goto out;
		}
		path->slots[0]--;
		leaf = btrfs_buffer_leaf(path->nodes[0]);
		btrfs_disk_key_to_cpu(&found_key,
				      &leaf->items[path->slots[0]].key);
		found_type = btrfs_key_type(&found_key);
		if (found_key.objectid != inode->i_ino) {
			err = -ENOENT;
			goto out;
		}
		if ((found_type != BTRFS_DIR_ITEM_KEY &&
		     found_type != BTRFS_DIR_INDEX_KEY) ||
	            (!btrfs_match_dir_item_name(root, path, goodnames, 2) &&
	            !btrfs_match_dir_item_name(root, path, goodnames, 1))) {
			err = -ENOTEMPTY;
			goto out;
		}
		ret = btrfs_del_item(trans, root, path);
		BUG_ON(ret);

		if (found_type == BTRFS_DIR_ITEM_KEY && found_key.offset == 1)
			break;
		btrfs_release_path(root, path);
	}
	ret = 0;
	btrfs_release_path(root, path);

	/* now the directory is empty */
	err = btrfs_unlink_trans(trans, root, dir, dentry);
	if (!err) {
		inode->i_size = 0;
	}
out:
	btrfs_release_path(root, path);
	btrfs_free_path(path);
	mutex_unlock(&root->fs_info->fs_mutex);
	ret = btrfs_end_transaction(trans, root);
	btrfs_btree_balance_dirty(root);
	if (ret && !err)
		err = ret;
	return err;
}

static int btrfs_free_inode(struct btrfs_trans_handle *trans,
			    struct btrfs_root *root,
			    struct inode *inode)
{
	struct btrfs_path *path;
	int ret;

	clear_inode(inode);

	path = btrfs_alloc_path();
	BUG_ON(!path);
	ret = btrfs_lookup_inode(trans, root, path,
				 &BTRFS_I(inode)->location, -1);
	if (ret > 0)
		ret = -ENOENT;
	if (!ret)
		ret = btrfs_del_item(trans, root, path);
	btrfs_free_path(path);
	return ret;
}

/*
 * this can truncate away extent items, csum items and directory items.
 * It starts at a high offset and removes keys until it can't find
 * any higher than i_size.
 *
 * csum items that cross the new i_size are truncated to the new size
 * as well.
 */
static int btrfs_truncate_in_trans(struct btrfs_trans_handle *trans,
				   struct btrfs_root *root,
				   struct inode *inode)
{
	int ret;
	struct btrfs_path *path;
	struct btrfs_key key;
	struct btrfs_disk_key *found_key;
	u32 found_type;
	struct btrfs_leaf *leaf;
	struct btrfs_file_extent_item *fi;
	u64 extent_start = 0;
	u64 extent_num_blocks = 0;
	u64 item_end = 0;
	int found_extent;
	int del_item;

	btrfs_drop_extent_cache(inode, inode->i_size, (u64)-1);
	path = btrfs_alloc_path();
	path->reada = -1;
	BUG_ON(!path);
	/* FIXME, add redo link to tree so we don't leak on crash */
	key.objectid = inode->i_ino;
	key.offset = (u64)-1;
	key.flags = (u32)-1;
	while(1) {
		btrfs_init_path(path);
		fi = NULL;
		ret = btrfs_search_slot(trans, root, &key, path, -1, 1);
		if (ret < 0) {
			goto error;
		}
		if (ret > 0) {
			BUG_ON(path->slots[0] == 0);
			path->slots[0]--;
		}
		leaf = btrfs_buffer_leaf(path->nodes[0]);
		found_key = &leaf->items[path->slots[0]].key;
		found_type = btrfs_disk_key_type(found_key);

		if (btrfs_disk_key_objectid(found_key) != inode->i_ino)
			break;
		if (found_type != BTRFS_CSUM_ITEM_KEY &&
		    found_type != BTRFS_DIR_ITEM_KEY &&
		    found_type != BTRFS_DIR_INDEX_KEY &&
		    found_type != BTRFS_EXTENT_DATA_KEY)
			break;

		item_end = btrfs_disk_key_offset(found_key);
		if (found_type == BTRFS_EXTENT_DATA_KEY) {
			fi = btrfs_item_ptr(btrfs_buffer_leaf(path->nodes[0]),
					    path->slots[0],
					    struct btrfs_file_extent_item);
			if (btrfs_file_extent_type(fi) !=
			    BTRFS_FILE_EXTENT_INLINE) {
				item_end += btrfs_file_extent_num_blocks(fi) <<
						inode->i_blkbits;
			}
		}
		if (found_type == BTRFS_CSUM_ITEM_KEY) {
			ret = btrfs_csum_truncate(trans, root, path,
						  inode->i_size);
			BUG_ON(ret);
		}
		if (item_end < inode->i_size) {
			if (found_type == BTRFS_DIR_ITEM_KEY) {
				found_type = BTRFS_INODE_ITEM_KEY;
			} else if (found_type == BTRFS_EXTENT_ITEM_KEY) {
				found_type = BTRFS_CSUM_ITEM_KEY;
			} else if (found_type) {
				found_type--;
			} else {
				break;
			}
			btrfs_set_key_type(&key, found_type - 1);
			continue;
		}
		if (btrfs_disk_key_offset(found_key) >= inode->i_size)
			del_item = 1;
		else
			del_item = 0;
		found_extent = 0;

		/* FIXME, shrink the extent if the ref count is only 1 */
		if (found_type == BTRFS_EXTENT_DATA_KEY &&
			   btrfs_file_extent_type(fi) !=
			   BTRFS_FILE_EXTENT_INLINE) {
			u64 num_dec;
			if (!del_item) {
				u64 orig_num_blocks =
					btrfs_file_extent_num_blocks(fi);
				extent_num_blocks = inode->i_size -
					btrfs_disk_key_offset(found_key) +
					root->blocksize - 1;
				extent_num_blocks >>= inode->i_blkbits;
				btrfs_set_file_extent_num_blocks(fi,
							 extent_num_blocks);
				inode->i_blocks -= (orig_num_blocks -
					extent_num_blocks) << 3;
				btrfs_mark_buffer_dirty(path->nodes[0]);
			} else {
				extent_start =
					btrfs_file_extent_disk_blocknr(fi);
				extent_num_blocks =
					btrfs_file_extent_disk_num_blocks(fi);
				/* FIXME blocksize != 4096 */
				num_dec = btrfs_file_extent_num_blocks(fi) << 3;
				if (extent_start != 0) {
					found_extent = 1;
					inode->i_blocks -= num_dec;
				}
			}
		}
		if (del_item) {
			ret = btrfs_del_item(trans, root, path);
			if (ret)
				goto error;
		} else {
			break;
		}
		btrfs_release_path(root, path);
		if (found_extent) {
			ret = btrfs_free_extent(trans, root, extent_start,
						extent_num_blocks, 0);
			BUG_ON(ret);
		}
	}
	ret = 0;
error:
	btrfs_release_path(root, path);
	btrfs_free_path(path);
	inode->i_sb->s_dirt = 1;
	return ret;
}

static int btrfs_cow_one_page(struct inode *inode, struct page *page,
			      size_t zero_start)
{
	char *kaddr;
	int ret = 0;
	struct extent_map_tree *em_tree = &BTRFS_I(inode)->extent_tree;
	u64 page_start = page->index << PAGE_CACHE_SHIFT;
	u64 page_end = page_start + PAGE_CACHE_SIZE - 1;

	if (!PagePrivate(page)) {
		SetPagePrivate(page);
		set_page_private(page, 1);
		WARN_ON(!page->mapping->a_ops->invalidatepage);
		page_cache_get(page);
	}

	lock_extent(em_tree, page_start, page_end, GFP_NOFS);
	set_extent_delalloc(&BTRFS_I(inode)->extent_tree, page_start,
			    page_end, GFP_NOFS);
	if (zero_start != PAGE_CACHE_SIZE) {
		kaddr = kmap(page);
		memset(kaddr + zero_start, 0, PAGE_CACHE_SIZE - zero_start);
		flush_dcache_page(page);
		kunmap(page);
	}
	set_page_dirty(page);
	unlock_extent(em_tree, page_start, page_end, GFP_NOFS);

	return ret;
}

/*
 * taken from block_truncate_page, but does cow as it zeros out
 * any bytes left in the last page in the file.
 */
static int btrfs_truncate_page(struct address_space *mapping, loff_t from)
{
	struct inode *inode = mapping->host;
	unsigned blocksize = 1 << inode->i_blkbits;
	pgoff_t index = from >> PAGE_CACHE_SHIFT;
	unsigned offset = from & (PAGE_CACHE_SIZE-1);
	struct page *page;
	int ret = 0;
	u64 page_start;

	if ((offset & (blocksize - 1)) == 0)
		goto out;

	ret = -ENOMEM;
	page = grab_cache_page(mapping, index);
	if (!page)
		goto out;
	if (!PageUptodate(page)) {
		ret = btrfs_readpage(NULL, page);
		lock_page(page);
		if (!PageUptodate(page)) {
			ret = -EIO;
			goto out;
		}
	}
	page_start = page->index << PAGE_CACHE_SHIFT;

	ret = btrfs_cow_one_page(inode, page, offset);

	unlock_page(page);
	page_cache_release(page);
out:
	return ret;
}

static int btrfs_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = dentry->d_inode;
	int err;

	err = inode_change_ok(inode, attr);
	if (err)
		return err;

	if (S_ISREG(inode->i_mode) &&
	    attr->ia_valid & ATTR_SIZE && attr->ia_size > inode->i_size) {
		struct btrfs_trans_handle *trans;
		struct btrfs_root *root = BTRFS_I(inode)->root;
		u64 mask = root->blocksize - 1;
		u64 pos = (inode->i_size + mask) & ~mask;
		u64 hole_size;

		if (attr->ia_size <= pos)
			goto out;

		btrfs_truncate_page(inode->i_mapping, inode->i_size);

		hole_size = (attr->ia_size - pos + mask) & ~mask;
		hole_size >>= inode->i_blkbits;

		mutex_lock(&root->fs_info->fs_mutex);
		trans = btrfs_start_transaction(root, 1);
		btrfs_set_trans_block_group(trans, inode);
		err = btrfs_insert_file_extent(trans, root, inode->i_ino,
					       pos, 0, 0, hole_size);
		btrfs_end_transaction(trans, root);
		mutex_unlock(&root->fs_info->fs_mutex);
		if (err)
			return err;
	}
out:
	err = inode_setattr(inode, attr);

	return err;
}
void btrfs_delete_inode(struct inode *inode)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	int ret;

	truncate_inode_pages(&inode->i_data, 0);
	if (is_bad_inode(inode)) {
		goto no_delete;
	}
	inode->i_size = 0;
	mutex_lock(&root->fs_info->fs_mutex);
	trans = btrfs_start_transaction(root, 1);
	btrfs_set_trans_block_group(trans, inode);
	ret = btrfs_truncate_in_trans(trans, root, inode);
	if (ret)
		goto no_delete_lock;
	ret = btrfs_free_inode(trans, root, inode);
	if (ret)
		goto no_delete_lock;
	btrfs_end_transaction(trans, root);
	mutex_unlock(&root->fs_info->fs_mutex);
	btrfs_btree_balance_dirty(root);
	return;

no_delete_lock:
	btrfs_end_transaction(trans, root);
	mutex_unlock(&root->fs_info->fs_mutex);
	btrfs_btree_balance_dirty(root);
no_delete:
	clear_inode(inode);
}

/*
 * this returns the key found in the dir entry in the location pointer.
 * If no dir entries were found, location->objectid is 0.
 */
static int btrfs_inode_by_name(struct inode *dir, struct dentry *dentry,
			       struct btrfs_key *location)
{
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;
	struct btrfs_dir_item *di;
	struct btrfs_path *path;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	int ret;

	path = btrfs_alloc_path();
	BUG_ON(!path);
	di = btrfs_lookup_dir_item(NULL, root, path, dir->i_ino, name,
				    namelen, 0);
	if (!di || IS_ERR(di)) {
		location->objectid = 0;
		ret = 0;
		goto out;
	}
	btrfs_disk_key_to_cpu(location, &di->location);
out:
	btrfs_release_path(root, path);
	btrfs_free_path(path);
	return ret;
}

/*
 * when we hit a tree root in a directory, the btrfs part of the inode
 * needs to be changed to reflect the root directory of the tree root.  This
 * is kind of like crossing a mount point.
 */
static int fixup_tree_root_location(struct btrfs_root *root,
			     struct btrfs_key *location,
			     struct btrfs_root **sub_root,
			     struct dentry *dentry)
{
	struct btrfs_path *path;
	struct btrfs_root_item *ri;

	if (btrfs_key_type(location) != BTRFS_ROOT_ITEM_KEY)
		return 0;
	if (location->objectid == BTRFS_ROOT_TREE_OBJECTID)
		return 0;

	path = btrfs_alloc_path();
	BUG_ON(!path);
	mutex_lock(&root->fs_info->fs_mutex);

	*sub_root = btrfs_read_fs_root(root->fs_info, location,
					dentry->d_name.name,
					dentry->d_name.len);
	if (IS_ERR(*sub_root))
		return PTR_ERR(*sub_root);

	ri = &(*sub_root)->root_item;
	location->objectid = btrfs_root_dirid(ri);
	location->flags = 0;
	btrfs_set_key_type(location, BTRFS_INODE_ITEM_KEY);
	location->offset = 0;

	btrfs_free_path(path);
	mutex_unlock(&root->fs_info->fs_mutex);
	return 0;
}

static int btrfs_init_locked_inode(struct inode *inode, void *p)
{
	struct btrfs_iget_args *args = p;
	inode->i_ino = args->ino;
	BTRFS_I(inode)->root = args->root;
	extent_map_tree_init(&BTRFS_I(inode)->extent_tree,
			     inode->i_mapping, GFP_NOFS);
	return 0;
}

static int btrfs_find_actor(struct inode *inode, void *opaque)
{
	struct btrfs_iget_args *args = opaque;
	return (args->ino == inode->i_ino &&
		args->root == BTRFS_I(inode)->root);
}

struct inode *btrfs_iget_locked(struct super_block *s, u64 objectid,
				struct btrfs_root *root)
{
	struct inode *inode;
	struct btrfs_iget_args args;
	args.ino = objectid;
	args.root = root;

	inode = iget5_locked(s, objectid, btrfs_find_actor,
			     btrfs_init_locked_inode,
			     (void *)&args);
	return inode;
}

static struct dentry *btrfs_lookup(struct inode *dir, struct dentry *dentry,
				   struct nameidata *nd)
{
	struct inode * inode;
	struct btrfs_inode *bi = BTRFS_I(dir);
	struct btrfs_root *root = bi->root;
	struct btrfs_root *sub_root = root;
	struct btrfs_key location;
	int ret;

	if (dentry->d_name.len > BTRFS_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);
	mutex_lock(&root->fs_info->fs_mutex);
	ret = btrfs_inode_by_name(dir, dentry, &location);
	mutex_unlock(&root->fs_info->fs_mutex);
	if (ret < 0)
		return ERR_PTR(ret);
	inode = NULL;
	if (location.objectid) {
		ret = fixup_tree_root_location(root, &location, &sub_root,
						dentry);
		if (ret < 0)
			return ERR_PTR(ret);
		if (ret > 0)
			return ERR_PTR(-ENOENT);
		inode = btrfs_iget_locked(dir->i_sb, location.objectid,
					  sub_root);
		if (!inode)
			return ERR_PTR(-EACCES);
		if (inode->i_state & I_NEW) {
			/* the inode and parent dir are two different roots */
			if (sub_root != root) {
				igrab(inode);
				sub_root->inode = inode;
			}
			BTRFS_I(inode)->root = sub_root;
			memcpy(&BTRFS_I(inode)->location, &location,
			       sizeof(location));
			btrfs_read_locked_inode(inode);
			unlock_new_inode(inode);
		}
	}
	return d_splice_alias(inode, dentry);
}

static unsigned char btrfs_filetype_table[] = {
	DT_UNKNOWN, DT_REG, DT_DIR, DT_CHR, DT_BLK, DT_FIFO, DT_SOCK, DT_LNK
};

static int btrfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_path.dentry->d_inode;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_item *item;
	struct btrfs_dir_item *di;
	struct btrfs_key key;
	struct btrfs_path *path;
	int ret;
	u32 nritems;
	struct btrfs_leaf *leaf;
	int slot;
	int advance;
	unsigned char d_type;
	int over = 0;
	u32 di_cur;
	u32 di_total;
	u32 di_len;
	int key_type = BTRFS_DIR_INDEX_KEY;

	/* FIXME, use a real flag for deciding about the key type */
	if (root->fs_info->tree_root == root)
		key_type = BTRFS_DIR_ITEM_KEY;
	mutex_lock(&root->fs_info->fs_mutex);
	key.objectid = inode->i_ino;
	key.flags = 0;
	btrfs_set_key_type(&key, key_type);
	key.offset = filp->f_pos;
	path = btrfs_alloc_path();
	path->reada = 2;
	ret = btrfs_search_slot(NULL, root, &key, path, 0, 0);
	if (ret < 0)
		goto err;
	advance = 0;
	while(1) {
		leaf = btrfs_buffer_leaf(path->nodes[0]);
		nritems = btrfs_header_nritems(&leaf->header);
		slot = path->slots[0];
		if (advance || slot >= nritems) {
			if (slot >= nritems -1) {
				ret = btrfs_next_leaf(root, path);
				if (ret)
					break;
				leaf = btrfs_buffer_leaf(path->nodes[0]);
				nritems = btrfs_header_nritems(&leaf->header);
				slot = path->slots[0];
			} else {
				slot++;
				path->slots[0]++;
			}
		}
		advance = 1;
		item = leaf->items + slot;
		if (btrfs_disk_key_objectid(&item->key) != key.objectid)
			break;
		if (btrfs_disk_key_type(&item->key) != key_type)
			break;
		if (btrfs_disk_key_offset(&item->key) < filp->f_pos)
			continue;
		filp->f_pos = btrfs_disk_key_offset(&item->key);
		advance = 1;
		di = btrfs_item_ptr(leaf, slot, struct btrfs_dir_item);
		di_cur = 0;
		di_total = btrfs_item_size(leaf->items + slot);
		while(di_cur < di_total) {
			d_type = btrfs_filetype_table[btrfs_dir_type(di)];
			over = filldir(dirent, (const char *)(di + 1),
				       btrfs_dir_name_len(di),
				       btrfs_disk_key_offset(&item->key),
				       btrfs_disk_key_objectid(&di->location),
				       d_type);
			if (over)
				goto nopos;
			di_len = btrfs_dir_name_len(di) + sizeof(*di);
			di_cur += di_len;
			di = (struct btrfs_dir_item *)((char *)di + di_len);
		}
	}
	filp->f_pos++;
nopos:
	ret = 0;
err:
	btrfs_release_path(root, path);
	btrfs_free_path(path);
	mutex_unlock(&root->fs_info->fs_mutex);
	return ret;
}

int btrfs_write_inode(struct inode *inode, int wait)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;
	int ret = 0;

	if (wait) {
		mutex_lock(&root->fs_info->fs_mutex);
		trans = btrfs_start_transaction(root, 1);
		btrfs_set_trans_block_group(trans, inode);
		ret = btrfs_commit_transaction(trans, root);
		mutex_unlock(&root->fs_info->fs_mutex);
	}
	return ret;
}

/*
 * This is somewhat expensive, updating the tree every time the
 * inode changes.  But, it is most likely to find the inode in cache.
 * FIXME, needs more benchmarking...there are no reasons other than performance
 * to keep or drop this code.
 */
void btrfs_dirty_inode(struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_trans_handle *trans;

	mutex_lock(&root->fs_info->fs_mutex);
	trans = btrfs_start_transaction(root, 1);
	btrfs_set_trans_block_group(trans, inode);
	btrfs_update_inode(trans, root, inode);
	btrfs_end_transaction(trans, root);
	mutex_unlock(&root->fs_info->fs_mutex);
}

static struct inode *btrfs_new_inode(struct btrfs_trans_handle *trans,
				     struct btrfs_root *root,
				     u64 objectid,
				     struct btrfs_block_group_cache *group,
				     int mode)
{
	struct inode *inode;
	struct btrfs_inode_item inode_item;
	struct btrfs_key *location;
	int ret;
	int owner;

	inode = new_inode(root->fs_info->sb);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	extent_map_tree_init(&BTRFS_I(inode)->extent_tree,
			     inode->i_mapping, GFP_NOFS);
	BTRFS_I(inode)->root = root;

	if (mode & S_IFDIR)
		owner = 0;
	else
		owner = 1;
	group = btrfs_find_block_group(root, group, 0, 0, owner);
	BTRFS_I(inode)->block_group = group;

	inode->i_uid = current->fsuid;
	inode->i_gid = current->fsgid;
	inode->i_mode = mode;
	inode->i_ino = objectid;
	inode->i_blocks = 0;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	fill_inode_item(&inode_item, inode);
	location = &BTRFS_I(inode)->location;
	location->objectid = objectid;
	location->flags = 0;
	location->offset = 0;
	btrfs_set_key_type(location, BTRFS_INODE_ITEM_KEY);

	ret = btrfs_insert_inode(trans, root, objectid, &inode_item);
	if (ret)
		return ERR_PTR(ret);
	insert_inode_hash(inode);
	return inode;
}

static inline u8 btrfs_inode_type(struct inode *inode)
{
	return btrfs_type_by_mode[(inode->i_mode & S_IFMT) >> S_SHIFT];
}

static int btrfs_add_link(struct btrfs_trans_handle *trans,
			    struct dentry *dentry, struct inode *inode)
{
	int ret;
	struct btrfs_key key;
	struct btrfs_root *root = BTRFS_I(dentry->d_parent->d_inode)->root;
	struct inode *parent_inode;
	key.objectid = inode->i_ino;
	key.flags = 0;
	btrfs_set_key_type(&key, BTRFS_INODE_ITEM_KEY);
	key.offset = 0;

	ret = btrfs_insert_dir_item(trans, root,
				    dentry->d_name.name, dentry->d_name.len,
				    dentry->d_parent->d_inode->i_ino,
				    &key, btrfs_inode_type(inode));
	if (ret == 0) {
		parent_inode = dentry->d_parent->d_inode;
		parent_inode->i_size += dentry->d_name.len * 2;
		parent_inode->i_mtime = parent_inode->i_ctime = CURRENT_TIME;
		ret = btrfs_update_inode(trans, root,
					 dentry->d_parent->d_inode);
	}
	return ret;
}

static int btrfs_add_nondir(struct btrfs_trans_handle *trans,
			    struct dentry *dentry, struct inode *inode)
{
	int err = btrfs_add_link(trans, dentry, inode);
	if (!err) {
		d_instantiate(dentry, inode);
		return 0;
	}
	if (err > 0)
		err = -EEXIST;
	return err;
}

static int btrfs_mknod(struct inode *dir, struct dentry *dentry,
			int mode, dev_t rdev)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct inode *inode;
	int err;
	int drop_inode = 0;
	u64 objectid;

	if (!new_valid_dev(rdev))
		return -EINVAL;

	mutex_lock(&root->fs_info->fs_mutex);
	trans = btrfs_start_transaction(root, 1);
	btrfs_set_trans_block_group(trans, dir);

	err = btrfs_find_free_objectid(trans, root, dir->i_ino, &objectid);
	if (err) {
		err = -ENOSPC;
		goto out_unlock;
	}

	inode = btrfs_new_inode(trans, root, objectid,
				BTRFS_I(dir)->block_group, mode);
	err = PTR_ERR(inode);
	if (IS_ERR(inode))
		goto out_unlock;

	btrfs_set_trans_block_group(trans, inode);
	err = btrfs_add_nondir(trans, dentry, inode);
	if (err)
		drop_inode = 1;
	else {
		inode->i_op = &btrfs_special_inode_operations;
		init_special_inode(inode, inode->i_mode, rdev);
		btrfs_update_inode(trans, root, inode);
	}
	dir->i_sb->s_dirt = 1;
	btrfs_update_inode_block_group(trans, inode);
	btrfs_update_inode_block_group(trans, dir);
out_unlock:
	btrfs_end_transaction(trans, root);
	mutex_unlock(&root->fs_info->fs_mutex);

	if (drop_inode) {
		inode_dec_link_count(inode);
		iput(inode);
	}
	btrfs_btree_balance_dirty(root);
	return err;
}

static int btrfs_create(struct inode *dir, struct dentry *dentry,
			int mode, struct nameidata *nd)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct inode *inode;
	int err;
	int drop_inode = 0;
	u64 objectid;

	mutex_lock(&root->fs_info->fs_mutex);
	trans = btrfs_start_transaction(root, 1);
	btrfs_set_trans_block_group(trans, dir);

	err = btrfs_find_free_objectid(trans, root, dir->i_ino, &objectid);
	if (err) {
		err = -ENOSPC;
		goto out_unlock;
	}

	inode = btrfs_new_inode(trans, root, objectid,
				BTRFS_I(dir)->block_group, mode);
	err = PTR_ERR(inode);
	if (IS_ERR(inode))
		goto out_unlock;

	btrfs_set_trans_block_group(trans, inode);
	err = btrfs_add_nondir(trans, dentry, inode);
	if (err)
		drop_inode = 1;
	else {
		inode->i_mapping->a_ops = &btrfs_aops;
		inode->i_fop = &btrfs_file_operations;
		inode->i_op = &btrfs_file_inode_operations;
		extent_map_tree_init(&BTRFS_I(inode)->extent_tree,
				     inode->i_mapping, GFP_NOFS);
		BTRFS_I(inode)->extent_tree.ops = &btrfs_extent_map_ops;
	}
	dir->i_sb->s_dirt = 1;
	btrfs_update_inode_block_group(trans, inode);
	btrfs_update_inode_block_group(trans, dir);
out_unlock:
	btrfs_end_transaction(trans, root);
	mutex_unlock(&root->fs_info->fs_mutex);

	if (drop_inode) {
		inode_dec_link_count(inode);
		iput(inode);
	}
	btrfs_btree_balance_dirty(root);
	return err;
}

static int btrfs_link(struct dentry *old_dentry, struct inode *dir,
		      struct dentry *dentry)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct inode *inode = old_dentry->d_inode;
	int err;
	int drop_inode = 0;

	if (inode->i_nlink == 0)
		return -ENOENT;

	inc_nlink(inode);
	mutex_lock(&root->fs_info->fs_mutex);
	trans = btrfs_start_transaction(root, 1);
	btrfs_set_trans_block_group(trans, dir);
	atomic_inc(&inode->i_count);
	err = btrfs_add_nondir(trans, dentry, inode);
	if (err)
		drop_inode = 1;
	dir->i_sb->s_dirt = 1;
	btrfs_update_inode_block_group(trans, dir);
	err = btrfs_update_inode(trans, root, inode);
	if (err)
		drop_inode = 1;

	btrfs_end_transaction(trans, root);
	mutex_unlock(&root->fs_info->fs_mutex);

	if (drop_inode) {
		inode_dec_link_count(inode);
		iput(inode);
	}
	btrfs_btree_balance_dirty(root);
	return err;
}

static int btrfs_make_empty_dir(struct btrfs_trans_handle *trans,
				struct btrfs_root *root,
				u64 objectid, u64 dirid)
{
	int ret;
	char buf[2];
	struct btrfs_key key;

	buf[0] = '.';
	buf[1] = '.';

	key.objectid = objectid;
	key.offset = 0;
	key.flags = 0;
	btrfs_set_key_type(&key, BTRFS_INODE_ITEM_KEY);

	ret = btrfs_insert_dir_item(trans, root, buf, 1, objectid,
				    &key, BTRFS_FT_DIR);
	if (ret)
		goto error;
	key.objectid = dirid;
	ret = btrfs_insert_dir_item(trans, root, buf, 2, objectid,
				    &key, BTRFS_FT_DIR);
	if (ret)
		goto error;
error:
	return ret;
}

static int btrfs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	struct inode *inode;
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	int err = 0;
	int drop_on_err = 0;
	u64 objectid;

	mutex_lock(&root->fs_info->fs_mutex);
	trans = btrfs_start_transaction(root, 1);
	btrfs_set_trans_block_group(trans, dir);
	if (IS_ERR(trans)) {
		err = PTR_ERR(trans);
		goto out_unlock;
	}

	err = btrfs_find_free_objectid(trans, root, dir->i_ino, &objectid);
	if (err) {
		err = -ENOSPC;
		goto out_unlock;
	}

	inode = btrfs_new_inode(trans, root, objectid,
				BTRFS_I(dir)->block_group, S_IFDIR | mode);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto out_fail;
	}
	drop_on_err = 1;
	inode->i_op = &btrfs_dir_inode_operations;
	inode->i_fop = &btrfs_dir_file_operations;
	btrfs_set_trans_block_group(trans, inode);

	err = btrfs_make_empty_dir(trans, root, inode->i_ino, dir->i_ino);
	if (err)
		goto out_fail;

	inode->i_size = 6;
	err = btrfs_update_inode(trans, root, inode);
	if (err)
		goto out_fail;
	err = btrfs_add_link(trans, dentry, inode);
	if (err)
		goto out_fail;
	d_instantiate(dentry, inode);
	drop_on_err = 0;
	dir->i_sb->s_dirt = 1;
	btrfs_update_inode_block_group(trans, inode);
	btrfs_update_inode_block_group(trans, dir);

out_fail:
	btrfs_end_transaction(trans, root);
out_unlock:
	mutex_unlock(&root->fs_info->fs_mutex);
	if (drop_on_err)
		iput(inode);
	btrfs_btree_balance_dirty(root);
	return err;
}

struct extent_map *btrfs_get_extent(struct inode *inode, struct page *page,
				    size_t page_offset, u64 start, u64 end,
				    int create)
{
	int ret;
	int err = 0;
	u64 blocknr;
	u64 extent_start = 0;
	u64 extent_end = 0;
	u64 objectid = inode->i_ino;
	u32 found_type;
	int failed_insert = 0;
	struct btrfs_path *path;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_file_extent_item *item;
	struct btrfs_leaf *leaf;
	struct btrfs_disk_key *found_key;
	struct extent_map *em = NULL;
	struct extent_map_tree *em_tree = &BTRFS_I(inode)->extent_tree;
	struct btrfs_trans_handle *trans = NULL;

	path = btrfs_alloc_path();
	BUG_ON(!path);
	mutex_lock(&root->fs_info->fs_mutex);

again:
	em = lookup_extent_mapping(em_tree, start, end);
	if (em) {
		goto out;
	}
	if (!em) {
		em = alloc_extent_map(GFP_NOFS);
		if (!em) {
			err = -ENOMEM;
			goto out;
		}
		em->start = 0;
		em->end = 0;
	}
	em->bdev = inode->i_sb->s_bdev;
	ret = btrfs_lookup_file_extent(NULL, root, path,
				       objectid, start, 0);
	if (ret < 0) {
		err = ret;
		goto out;
	}

	if (ret != 0) {
		if (path->slots[0] == 0)
			goto not_found;
		path->slots[0]--;
	}

	item = btrfs_item_ptr(btrfs_buffer_leaf(path->nodes[0]), path->slots[0],
			      struct btrfs_file_extent_item);
	leaf = btrfs_buffer_leaf(path->nodes[0]);
	blocknr = btrfs_file_extent_disk_blocknr(item);
	blocknr += btrfs_file_extent_offset(item);

	/* are we inside the extent that was found? */
	found_key = &leaf->items[path->slots[0]].key;
	found_type = btrfs_disk_key_type(found_key);
	if (btrfs_disk_key_objectid(found_key) != objectid ||
	    found_type != BTRFS_EXTENT_DATA_KEY) {
		goto not_found;
	}

	found_type = btrfs_file_extent_type(item);
	extent_start = btrfs_disk_key_offset(&leaf->items[path->slots[0]].key);
	if (found_type == BTRFS_FILE_EXTENT_REG) {
		extent_end = extent_start +
		       (btrfs_file_extent_num_blocks(item) << inode->i_blkbits);
		err = 0;
		if (start < extent_start || start >= extent_end) {
			em->start = start;
			if (start < extent_start) {
				if (end < extent_start)
					goto not_found;
				em->end = extent_end - 1;
			} else {
				em->end = end;
			}
			goto not_found_em;
		}
		if (btrfs_file_extent_disk_blocknr(item) == 0) {
			em->start = extent_start;
			em->end = extent_end - 1;
			em->block_start = 0;
			em->block_end = 0;
			goto insert;
		}
		em->block_start = blocknr << inode->i_blkbits;
		em->block_end = em->block_start +
			(btrfs_file_extent_num_blocks(item) <<
			 inode->i_blkbits) - 1;
		em->start = extent_start;
		em->end = extent_end - 1;
		goto insert;
	} else if (found_type == BTRFS_FILE_EXTENT_INLINE) {
		char *ptr;
		char *map;
		u32 size;

		size = btrfs_file_extent_inline_len(leaf->items +
						    path->slots[0]);
		extent_end = extent_start + size;
		if (start < extent_start || start >= extent_end) {
			em->start = start;
			if (start < extent_start) {
				if (end < extent_start)
					goto not_found;
				em->end = extent_end - 1;
			} else {
				em->end = end;
			}
			goto not_found_em;
		}
		em->block_start = EXTENT_MAP_INLINE;
		em->block_end = EXTENT_MAP_INLINE;
		em->start = extent_start;
		em->end = extent_end - 1;
		if (!page) {
			goto insert;
		}
		ptr = btrfs_file_extent_inline_start(item);
		map = kmap(page);
		memcpy(map + page_offset, ptr, size);
		flush_dcache_page(result->b_page);
		kunmap(page);
		set_extent_uptodate(em_tree, extent_start,
				    extent_end, GFP_NOFS);
		goto insert;
	} else {
		printk("unkknown found_type %d\n", found_type);
		WARN_ON(1);
	}
not_found:
	em->start = start;
	em->end = end;
not_found_em:
	em->block_start = 0;
	em->block_end = 0;
insert:
	btrfs_release_path(root, path);
	if (em->start > start || em->end < start) {
		printk("bad extent! em: [%Lu %Lu] passed [%Lu %Lu]\n", em->start, em->end, start, end);
		err = -EIO;
		goto out;
	}
	ret = add_extent_mapping(em_tree, em);
	if (ret == -EEXIST) {
		free_extent_map(em);
		failed_insert++;
		if (failed_insert > 5) {
			printk("failing to insert %Lu %Lu\n", start, end);
			err = -EIO;
			goto out;
		}
		em = NULL;
		goto again;
	}
	err = 0;
out:
	btrfs_free_path(path);
	if (trans) {
		ret = btrfs_end_transaction(trans, root);
		if (!err)
			err = ret;
	}
	mutex_unlock(&root->fs_info->fs_mutex);
	if (err) {
		free_extent_map(em);
		WARN_ON(1);
		return ERR_PTR(err);
	}
	return em;
}


/*
 * FIBMAP and others want to pass in a fake buffer head.  They need to
 * use BTRFS_GET_BLOCK_NO_DIRECT to make sure we don't try to memcpy
 * any packed file data into the fake bh
 */
#define BTRFS_GET_BLOCK_NO_CREATE 0
#define BTRFS_GET_BLOCK_CREATE 1
#define BTRFS_GET_BLOCK_NO_DIRECT 2

/*
 * FIXME create==1 doe not work.
 */
static int btrfs_get_block_lock(struct inode *inode, sector_t iblock,
				struct buffer_head *result, int create)
{
	int ret;
	int err = 0;
	u64 blocknr;
	u64 extent_start = 0;
	u64 extent_end = 0;
	u64 objectid = inode->i_ino;
	u32 found_type;
	u64 alloc_hint = 0;
	struct btrfs_path *path;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_file_extent_item *item;
	struct btrfs_leaf *leaf;
	struct btrfs_disk_key *found_key;
	struct btrfs_trans_handle *trans = NULL;

	path = btrfs_alloc_path();
	BUG_ON(!path);
	if (create & BTRFS_GET_BLOCK_CREATE) {
		/*
		 * danger!, this only works if the page is properly up
		 * to date somehow
		 */
		trans = btrfs_start_transaction(root, 1);
		if (!trans) {
			err = -ENOMEM;
			goto out;
		}
		ret = btrfs_drop_extents(trans, root, inode,
					 iblock << inode->i_blkbits,
					 (iblock + 1) << inode->i_blkbits,
					 &alloc_hint);
		BUG_ON(ret);
	}

	ret = btrfs_lookup_file_extent(NULL, root, path,
				       objectid,
				       iblock << inode->i_blkbits, 0);
	if (ret < 0) {
		err = ret;
		goto out;
	}

	if (ret != 0) {
		if (path->slots[0] == 0) {
			btrfs_release_path(root, path);
			goto not_found;
		}
		path->slots[0]--;
	}

	item = btrfs_item_ptr(btrfs_buffer_leaf(path->nodes[0]), path->slots[0],
			      struct btrfs_file_extent_item);
	leaf = btrfs_buffer_leaf(path->nodes[0]);
	blocknr = btrfs_file_extent_disk_blocknr(item);
	blocknr += btrfs_file_extent_offset(item);

	/* are we inside the extent that was found? */
	found_key = &leaf->items[path->slots[0]].key;
	found_type = btrfs_disk_key_type(found_key);
	if (btrfs_disk_key_objectid(found_key) != objectid ||
	    found_type != BTRFS_EXTENT_DATA_KEY) {
		extent_end = 0;
		extent_start = 0;
		goto not_found;
	}
	found_type = btrfs_file_extent_type(item);
	extent_start = btrfs_disk_key_offset(&leaf->items[path->slots[0]].key);
	if (found_type == BTRFS_FILE_EXTENT_REG) {
		extent_start = extent_start >> inode->i_blkbits;
		extent_end = extent_start + btrfs_file_extent_num_blocks(item);
		err = 0;
		if (btrfs_file_extent_disk_blocknr(item) == 0)
			goto out;
		if (iblock >= extent_start && iblock < extent_end) {
			btrfs_map_bh_to_logical(root, result, blocknr +
						iblock - extent_start);
			goto out;
		}
	} else if (found_type == BTRFS_FILE_EXTENT_INLINE) {
		char *ptr;
		char *map;
		u32 size;

		if (create & BTRFS_GET_BLOCK_NO_DIRECT) {
			err = -EINVAL;
			goto out;
		}
		size = btrfs_file_extent_inline_len(leaf->items +
						    path->slots[0]);
		extent_end = (extent_start + size) >> inode->i_blkbits;
		extent_start >>= inode->i_blkbits;
		if (iblock < extent_start || iblock > extent_end) {
			goto not_found;
		}
		ptr = btrfs_file_extent_inline_start(item);
		map = kmap(result->b_page);
		memcpy(map, ptr, size);
		memset(map + size, 0, PAGE_CACHE_SIZE - size);
		flush_dcache_page(result->b_page);
		kunmap(result->b_page);
		set_buffer_uptodate(result);
		SetPageChecked(result->b_page);
		btrfs_map_bh_to_logical(root, result, 0);
	}
not_found:
	if (create & BTRFS_GET_BLOCK_CREATE) {
		struct btrfs_key ins;
		ret = btrfs_alloc_extent(trans, root, inode->i_ino,
					 1, 0, alloc_hint, (u64)-1,
					 &ins, 1);
		if (ret) {
			err = ret;
			goto out;
		}
		ret = btrfs_insert_file_extent(trans, root, inode->i_ino,
					       iblock << inode->i_blkbits,
					       ins.objectid, ins.offset,
					       ins.offset);
		if (ret) {
			err = ret;
			goto out;
		}
		btrfs_map_bh_to_logical(root, result, ins.objectid);
	}
out:
	if (trans) {
		ret = btrfs_end_transaction(trans, root);
		if (!err)
			err = ret;
	}
	btrfs_free_path(path);
	return err;
}

int btrfs_get_block(struct inode *inode, sector_t iblock,
		    struct buffer_head *result, int create)
{
	int err;
	struct btrfs_root *root = BTRFS_I(inode)->root;
	mutex_lock(&root->fs_info->fs_mutex);
	err = btrfs_get_block_lock(inode, iblock, result, create);
	mutex_unlock(&root->fs_info->fs_mutex);
	return err;
}

static int btrfs_get_block_bmap(struct inode *inode, sector_t iblock,
			   struct buffer_head *result, int create)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	u64 start = iblock << inode->i_blkbits;
	u64 end = start + root->blocksize -1;
	struct extent_map *em;

	em = btrfs_get_extent(inode, NULL, 0, start, end, 0);
	if (em && !IS_ERR(em) && em->block_start != EXTENT_MAP_INLINE &&
	    em->block_start != 0) {
		u64 offset;
		offset = start - em->start;
		start = (em->block_start + offset) >> inode->i_blkbits;
		btrfs_map_bh_to_logical(root, result, start);
	}
	return 0;
}

static sector_t btrfs_bmap(struct address_space *as, sector_t block)
{
	return generic_block_bmap(as, block, btrfs_get_block_bmap);
}

static int btrfs_prepare_write(struct file *file, struct page *page,
			       unsigned from, unsigned to)
{
	return extent_prepare_write(&BTRFS_I(page->mapping->host)->extent_tree,
				    page->mapping->host, page, from, to,
				    btrfs_get_extent);
}

int btrfs_readpage(struct file *file, struct page *page)
{
	struct extent_map_tree *tree;
	tree = &BTRFS_I(page->mapping->host)->extent_tree;
	return extent_read_full_page(tree, page, btrfs_get_extent);
}
static int btrfs_writepage(struct page *page, struct writeback_control *wbc)
{
	struct extent_map_tree *tree;


	if (current->flags & PF_MEMALLOC) {
		redirty_page_for_writepage(wbc, page);
		unlock_page(page);
		return 0;
	}
	tree = &BTRFS_I(page->mapping->host)->extent_tree;
	return extent_write_full_page(tree, page, btrfs_get_extent, wbc);
}

static int btrfs_releasepage(struct page *page, gfp_t unused_gfp_flags)
{
	struct extent_map_tree *tree;
	int ret;

	if (page->private != 1) {
		WARN_ON(1);
		return try_to_free_buffers(page);
	}
	tree = &BTRFS_I(page->mapping->host)->extent_tree;
	ret = try_release_extent_mapping(tree, page);
	if (ret == 1) {
		ClearPagePrivate(page);
		set_page_private(page, 0);
		page_cache_release(page);
	}
	return ret;
}

static void btrfs_invalidatepage(struct page *page, unsigned long offset)
{
	struct extent_map_tree *tree;

	tree = &BTRFS_I(page->mapping->host)->extent_tree;
	extent_invalidatepage(tree, page, offset);
	btrfs_releasepage(page, GFP_NOFS);
}

/*
 * btrfs_page_mkwrite() is not allowed to change the file size as it gets
 * called from a page fault handler when a page is first dirtied. Hence we must
 * be careful to check for EOF conditions here. We set the page up correctly
 * for a written page which means we get ENOSPC checking when writing into
 * holes and correct delalloc and unwritten extent mapping on filesystems that
 * support these features.
 *
 * We are not allowed to take the i_mutex here so we have to play games to
 * protect against truncate races as the page could now be beyond EOF.  Because
 * vmtruncate() writes the inode size before removing pages, once we have the
 * page lock we can determine safely if the page is beyond EOF. If it is not
 * beyond EOF, then the page is guaranteed safe against truncation until we
 * unlock the page.
 */
int btrfs_page_mkwrite(struct vm_area_struct *vma, struct page *page)
{
	struct inode *inode = vma->vm_file->f_path.dentry->d_inode;
	unsigned long end;
	loff_t size;
	int ret = -EINVAL;
	u64 page_start;

	lock_page(page);
	wait_on_page_writeback(page);
	size = i_size_read(inode);
	page_start = page->index << PAGE_CACHE_SHIFT;

	if ((page->mapping != inode->i_mapping) ||
	    (page_start > size)) {
		/* page got truncated out from underneath us */
		goto out_unlock;
	}

	/* page is wholly or partially inside EOF */
	if (page_start + PAGE_CACHE_SIZE > size)
		end = size & ~PAGE_CACHE_MASK;
	else
		end = PAGE_CACHE_SIZE;

	ret = btrfs_cow_one_page(inode, page, end);

out_unlock:
	unlock_page(page);
	return ret;
}

static void btrfs_truncate(struct inode *inode)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	int ret;
	struct btrfs_trans_handle *trans;

	if (!S_ISREG(inode->i_mode))
		return;
	if (IS_APPEND(inode) || IS_IMMUTABLE(inode))
		return;

	btrfs_truncate_page(inode->i_mapping, inode->i_size);

	mutex_lock(&root->fs_info->fs_mutex);
	trans = btrfs_start_transaction(root, 1);
	btrfs_set_trans_block_group(trans, inode);

	/* FIXME, add redo link to tree so we don't leak on crash */
	ret = btrfs_truncate_in_trans(trans, root, inode);
	btrfs_update_inode(trans, root, inode);
	ret = btrfs_end_transaction(trans, root);
	BUG_ON(ret);
	mutex_unlock(&root->fs_info->fs_mutex);
	btrfs_btree_balance_dirty(root);
}

int btrfs_commit_write(struct file *file, struct page *page,
		       unsigned from, unsigned to)
{
	return extent_commit_write(&BTRFS_I(page->mapping->host)->extent_tree,
				   page->mapping->host, page, from, to);
}

static int create_subvol(struct btrfs_root *root, char *name, int namelen)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_key key;
	struct btrfs_root_item root_item;
	struct btrfs_inode_item *inode_item;
	struct buffer_head *subvol;
	struct btrfs_leaf *leaf;
	struct btrfs_root *new_root;
	struct inode *inode;
	struct inode *dir;
	int ret;
	int err;
	u64 objectid;
	u64 new_dirid = BTRFS_FIRST_FREE_OBJECTID;

	mutex_lock(&root->fs_info->fs_mutex);
	trans = btrfs_start_transaction(root, 1);
	BUG_ON(!trans);

	subvol = btrfs_alloc_free_block(trans, root, 0, 0);
	if (IS_ERR(subvol))
		return PTR_ERR(subvol);
	leaf = btrfs_buffer_leaf(subvol);
	btrfs_set_header_nritems(&leaf->header, 0);
	btrfs_set_header_level(&leaf->header, 0);
	btrfs_set_header_blocknr(&leaf->header, bh_blocknr(subvol));
	btrfs_set_header_generation(&leaf->header, trans->transid);
	btrfs_set_header_owner(&leaf->header, root->root_key.objectid);
	memcpy(leaf->header.fsid, root->fs_info->disk_super->fsid,
	       sizeof(leaf->header.fsid));
	btrfs_mark_buffer_dirty(subvol);

	inode_item = &root_item.inode;
	memset(inode_item, 0, sizeof(*inode_item));
	btrfs_set_inode_generation(inode_item, 1);
	btrfs_set_inode_size(inode_item, 3);
	btrfs_set_inode_nlink(inode_item, 1);
	btrfs_set_inode_nblocks(inode_item, 1);
	btrfs_set_inode_mode(inode_item, S_IFDIR | 0755);

	btrfs_set_root_blocknr(&root_item, bh_blocknr(subvol));
	btrfs_set_root_refs(&root_item, 1);
	btrfs_set_root_blocks_used(&root_item, 0);
	memset(&root_item.drop_progress, 0, sizeof(root_item.drop_progress));
	root_item.drop_level = 0;
	brelse(subvol);
	subvol = NULL;

	ret = btrfs_find_free_objectid(trans, root->fs_info->tree_root,
				       0, &objectid);
	if (ret)
		goto fail;

	btrfs_set_root_dirid(&root_item, new_dirid);

	key.objectid = objectid;
	key.offset = 1;
	key.flags = 0;
	btrfs_set_key_type(&key, BTRFS_ROOT_ITEM_KEY);
	ret = btrfs_insert_root(trans, root->fs_info->tree_root, &key,
				&root_item);
	if (ret)
		goto fail;

	/*
	 * insert the directory item
	 */
	key.offset = (u64)-1;
	dir = root->fs_info->sb->s_root->d_inode;
	ret = btrfs_insert_dir_item(trans, root->fs_info->tree_root,
				    name, namelen, dir->i_ino, &key,
				    BTRFS_FT_DIR);
	if (ret)
		goto fail;

	ret = btrfs_commit_transaction(trans, root);
	if (ret)
		goto fail_commit;

	new_root = btrfs_read_fs_root(root->fs_info, &key, name, namelen);
	BUG_ON(!new_root);

	trans = btrfs_start_transaction(new_root, 1);
	BUG_ON(!trans);

	inode = btrfs_new_inode(trans, new_root, new_dirid,
				BTRFS_I(dir)->block_group, S_IFDIR | 0700);
	if (IS_ERR(inode))
		goto fail;
	inode->i_op = &btrfs_dir_inode_operations;
	inode->i_fop = &btrfs_dir_file_operations;
	new_root->inode = inode;

	ret = btrfs_make_empty_dir(trans, new_root, new_dirid, new_dirid);
	if (ret)
		goto fail;

	inode->i_nlink = 1;
	inode->i_size = 6;
	ret = btrfs_update_inode(trans, new_root, inode);
	if (ret)
		goto fail;
fail:
	err = btrfs_commit_transaction(trans, root);
	if (err && !ret)
		ret = err;
fail_commit:
	mutex_unlock(&root->fs_info->fs_mutex);
	btrfs_btree_balance_dirty(root);
	return ret;
}

static int create_snapshot(struct btrfs_root *root, char *name, int namelen)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_key key;
	struct btrfs_root_item new_root_item;
	struct buffer_head *tmp;
	int ret;
	int err;
	u64 objectid;

	if (!root->ref_cows)
		return -EINVAL;

	mutex_lock(&root->fs_info->fs_mutex);
	trans = btrfs_start_transaction(root, 1);
	BUG_ON(!trans);

	ret = btrfs_update_inode(trans, root, root->inode);
	if (ret)
		goto fail;

	ret = btrfs_find_free_objectid(trans, root->fs_info->tree_root,
				       0, &objectid);
	if (ret)
		goto fail;

	memcpy(&new_root_item, &root->root_item,
	       sizeof(new_root_item));

	key.objectid = objectid;
	key.offset = 1;
	key.flags = 0;
	btrfs_set_key_type(&key, BTRFS_ROOT_ITEM_KEY);
	btrfs_cow_block(trans, root, root->node, NULL, 0, &tmp);
	btrfs_set_root_blocknr(&new_root_item, bh_blocknr(root->node));

	ret = btrfs_insert_root(trans, root->fs_info->tree_root, &key,
				&new_root_item);
	if (ret)
		goto fail;

	/*
	 * insert the directory item
	 */
	key.offset = (u64)-1;
	ret = btrfs_insert_dir_item(trans, root->fs_info->tree_root,
				    name, namelen,
				    root->fs_info->sb->s_root->d_inode->i_ino,
				    &key, BTRFS_FT_DIR);

	if (ret)
		goto fail;

	ret = btrfs_inc_root_ref(trans, root);
	if (ret)
		goto fail;

fail:
	err = btrfs_commit_transaction(trans, root);
	if (err && !ret)
		ret = err;
	mutex_unlock(&root->fs_info->fs_mutex);
	btrfs_btree_balance_dirty(root);
	return ret;
}

int btrfs_ioctl(struct inode *inode, struct file *filp, unsigned int
		cmd, unsigned long arg)
{
	struct btrfs_root *root = BTRFS_I(inode)->root;
	struct btrfs_ioctl_vol_args vol_args;
	int ret = 0;
	struct btrfs_dir_item *di;
	int namelen;
	struct btrfs_path *path;
	u64 root_dirid;

	switch (cmd) {
	case BTRFS_IOC_SNAP_CREATE:
		if (copy_from_user(&vol_args,
				   (struct btrfs_ioctl_vol_args __user *)arg,
				   sizeof(vol_args)))
			return -EFAULT;
		namelen = strlen(vol_args.name);
		if (namelen > BTRFS_VOL_NAME_MAX)
			return -EINVAL;
		if (strchr(vol_args.name, '/'))
			return -EINVAL;
		path = btrfs_alloc_path();
		if (!path)
			return -ENOMEM;
		root_dirid = root->fs_info->sb->s_root->d_inode->i_ino,
		mutex_lock(&root->fs_info->fs_mutex);
		di = btrfs_lookup_dir_item(NULL, root->fs_info->tree_root,
				    path, root_dirid,
				    vol_args.name, namelen, 0);
		mutex_unlock(&root->fs_info->fs_mutex);
		btrfs_free_path(path);
		if (di && !IS_ERR(di))
			return -EEXIST;
		if (IS_ERR(di))
			return PTR_ERR(di);

		if (root == root->fs_info->tree_root)
			ret = create_subvol(root, vol_args.name, namelen);
		else
			ret = create_snapshot(root, vol_args.name, namelen);
		break;

	case BTRFS_IOC_DEFRAG:
		mutex_lock(&root->fs_info->fs_mutex);
		btrfs_defrag_root(root, 0);
		btrfs_defrag_root(root->fs_info->extent_root, 0);
		mutex_unlock(&root->fs_info->fs_mutex);
		ret = 0;
		break;
	default:
		return -ENOTTY;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
long btrfs_compat_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg)
{
	struct inode *inode = file->f_path.dentry->d_inode;
	int ret;
	lock_kernel();
	ret = btrfs_ioctl(inode, file, cmd, (unsigned long) compat_ptr(arg));
	unlock_kernel();
	return ret;

}
#endif

/*
 * Called inside transaction, so use GFP_NOFS
 */
struct inode *btrfs_alloc_inode(struct super_block *sb)
{
	struct btrfs_inode *ei;

	ei = kmem_cache_alloc(btrfs_inode_cachep, GFP_NOFS);
	if (!ei)
		return NULL;
	ei->last_trans = 0;
	return &ei->vfs_inode;
}

void btrfs_destroy_inode(struct inode *inode)
{
	WARN_ON(!list_empty(&inode->i_dentry));
	WARN_ON(inode->i_data.nrpages);

	kmem_cache_free(btrfs_inode_cachep, BTRFS_I(inode));
}

static void init_once(void * foo, struct kmem_cache * cachep,
		      unsigned long flags)
{
	struct btrfs_inode *ei = (struct btrfs_inode *) foo;

	inode_init_once(&ei->vfs_inode);
}

void btrfs_destroy_cachep(void)
{
	if (btrfs_inode_cachep)
		kmem_cache_destroy(btrfs_inode_cachep);
	if (btrfs_trans_handle_cachep)
		kmem_cache_destroy(btrfs_trans_handle_cachep);
	if (btrfs_transaction_cachep)
		kmem_cache_destroy(btrfs_transaction_cachep);
	if (btrfs_bit_radix_cachep)
		kmem_cache_destroy(btrfs_bit_radix_cachep);
	if (btrfs_path_cachep)
		kmem_cache_destroy(btrfs_path_cachep);
}

static struct kmem_cache *cache_create(const char *name, size_t size,
				       unsigned long extra_flags,
				       void (*ctor)(void *, struct kmem_cache *,
						    unsigned long))
{
	return kmem_cache_create(name, size, 0, (SLAB_RECLAIM_ACCOUNT |
				 SLAB_MEM_SPREAD | extra_flags), ctor
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
				 ,NULL
#endif
				);
}

int btrfs_init_cachep(void)
{
	btrfs_inode_cachep = cache_create("btrfs_inode_cache",
					  sizeof(struct btrfs_inode),
					  0, init_once);
	if (!btrfs_inode_cachep)
		goto fail;
	btrfs_trans_handle_cachep = cache_create("btrfs_trans_handle_cache",
					     sizeof(struct btrfs_trans_handle),
					     0, NULL);
	if (!btrfs_trans_handle_cachep)
		goto fail;
	btrfs_transaction_cachep = cache_create("btrfs_transaction_cache",
					     sizeof(struct btrfs_transaction),
					     0, NULL);
	if (!btrfs_transaction_cachep)
		goto fail;
	btrfs_path_cachep = cache_create("btrfs_path_cache",
					 sizeof(struct btrfs_transaction),
					 0, NULL);
	if (!btrfs_path_cachep)
		goto fail;
	btrfs_bit_radix_cachep = cache_create("btrfs_radix", 256,
					      SLAB_DESTROY_BY_RCU, NULL);
	if (!btrfs_bit_radix_cachep)
		goto fail;
	return 0;
fail:
	btrfs_destroy_cachep();
	return -ENOMEM;
}

static int btrfs_getattr(struct vfsmount *mnt,
			 struct dentry *dentry, struct kstat *stat)
{
	struct inode *inode = dentry->d_inode;
	generic_fillattr(inode, stat);
	stat->blksize = 256 * 1024;
	return 0;
}

static int btrfs_rename(struct inode * old_dir, struct dentry *old_dentry,
			   struct inode * new_dir,struct dentry *new_dentry)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(old_dir)->root;
	struct inode *new_inode = new_dentry->d_inode;
	struct inode *old_inode = old_dentry->d_inode;
	struct timespec ctime = CURRENT_TIME;
	struct btrfs_path *path;
	struct btrfs_dir_item *di;
	int ret;

	if (S_ISDIR(old_inode->i_mode) && new_inode &&
	    new_inode->i_size > BTRFS_EMPTY_DIR_SIZE) {
		return -ENOTEMPTY;
	}
	mutex_lock(&root->fs_info->fs_mutex);
	trans = btrfs_start_transaction(root, 1);
	btrfs_set_trans_block_group(trans, new_dir);
	path = btrfs_alloc_path();
	if (!path) {
		ret = -ENOMEM;
		goto out_fail;
	}

	old_dentry->d_inode->i_nlink++;
	old_dir->i_ctime = old_dir->i_mtime = ctime;
	new_dir->i_ctime = new_dir->i_mtime = ctime;
	old_inode->i_ctime = ctime;
	if (S_ISDIR(old_inode->i_mode) && old_dir != new_dir) {
		struct btrfs_key *location = &BTRFS_I(new_dir)->location;
		u64 old_parent_oid;
		di = btrfs_lookup_dir_item(trans, root, path, old_inode->i_ino,
					   "..", 2, -1);
		if (IS_ERR(di)) {
			ret = PTR_ERR(di);
			goto out_fail;
		}
		if (!di) {
			ret = -ENOENT;
			goto out_fail;
		}
		old_parent_oid = btrfs_disk_key_objectid(&di->location);
		ret = btrfs_del_item(trans, root, path);
		if (ret) {
			goto out_fail;
		}
		btrfs_release_path(root, path);

		di = btrfs_lookup_dir_index_item(trans, root, path,
						 old_inode->i_ino,
						 old_parent_oid,
						 "..", 2, -1);
		if (IS_ERR(di)) {
			ret = PTR_ERR(di);
			goto out_fail;
		}
		if (!di) {
			ret = -ENOENT;
			goto out_fail;
		}
		ret = btrfs_del_item(trans, root, path);
		if (ret) {
			goto out_fail;
		}
		btrfs_release_path(root, path);

		ret = btrfs_insert_dir_item(trans, root, "..", 2,
					    old_inode->i_ino, location,
					    BTRFS_FT_DIR);
		if (ret)
			goto out_fail;
	}


	ret = btrfs_unlink_trans(trans, root, old_dir, old_dentry);
	if (ret)
		goto out_fail;

	if (new_inode) {
		new_inode->i_ctime = CURRENT_TIME;
		ret = btrfs_unlink_trans(trans, root, new_dir, new_dentry);
		if (ret)
			goto out_fail;
		if (S_ISDIR(new_inode->i_mode))
			clear_nlink(new_inode);
		else
			drop_nlink(new_inode);
		ret = btrfs_update_inode(trans, root, new_inode);
		if (ret)
			goto out_fail;
	}
	ret = btrfs_add_link(trans, new_dentry, old_inode);
	if (ret)
		goto out_fail;

out_fail:
	btrfs_free_path(path);
	btrfs_end_transaction(trans, root);
	mutex_unlock(&root->fs_info->fs_mutex);
	return ret;
}

static int btrfs_symlink(struct inode *dir, struct dentry *dentry,
			 const char *symname)
{
	struct btrfs_trans_handle *trans;
	struct btrfs_root *root = BTRFS_I(dir)->root;
	struct btrfs_path *path;
	struct btrfs_key key;
	struct inode *inode;
	int err;
	int drop_inode = 0;
	u64 objectid;
	int name_len;
	int datasize;
	char *ptr;
	struct btrfs_file_extent_item *ei;

	name_len = strlen(symname) + 1;
	if (name_len > BTRFS_MAX_INLINE_DATA_SIZE(root))
		return -ENAMETOOLONG;
	mutex_lock(&root->fs_info->fs_mutex);
	trans = btrfs_start_transaction(root, 1);
	btrfs_set_trans_block_group(trans, dir);

	err = btrfs_find_free_objectid(trans, root, dir->i_ino, &objectid);
	if (err) {
		err = -ENOSPC;
		goto out_unlock;
	}

	inode = btrfs_new_inode(trans, root, objectid,
				BTRFS_I(dir)->block_group, S_IFLNK|S_IRWXUGO);
	err = PTR_ERR(inode);
	if (IS_ERR(inode))
		goto out_unlock;

	btrfs_set_trans_block_group(trans, inode);
	err = btrfs_add_nondir(trans, dentry, inode);
	if (err)
		drop_inode = 1;
	else {
		inode->i_mapping->a_ops = &btrfs_aops;
		inode->i_fop = &btrfs_file_operations;
		inode->i_op = &btrfs_file_inode_operations;
		extent_map_tree_init(&BTRFS_I(inode)->extent_tree,
				     inode->i_mapping, GFP_NOFS);
		BTRFS_I(inode)->extent_tree.ops = &btrfs_extent_map_ops;
	}
	dir->i_sb->s_dirt = 1;
	btrfs_update_inode_block_group(trans, inode);
	btrfs_update_inode_block_group(trans, dir);
	if (drop_inode)
		goto out_unlock;

	path = btrfs_alloc_path();
	BUG_ON(!path);
	key.objectid = inode->i_ino;
	key.offset = 0;
	key.flags = 0;
	btrfs_set_key_type(&key, BTRFS_EXTENT_DATA_KEY);
	datasize = btrfs_file_extent_calc_inline_size(name_len);
	err = btrfs_insert_empty_item(trans, root, path, &key,
				      datasize);
	if (err) {
		drop_inode = 1;
		goto out_unlock;
	}
	ei = btrfs_item_ptr(btrfs_buffer_leaf(path->nodes[0]),
	       path->slots[0], struct btrfs_file_extent_item);
	btrfs_set_file_extent_generation(ei, trans->transid);
	btrfs_set_file_extent_type(ei,
				   BTRFS_FILE_EXTENT_INLINE);
	ptr = btrfs_file_extent_inline_start(ei);
	btrfs_memcpy(root, path->nodes[0]->b_data,
		     ptr, symname, name_len);
	btrfs_mark_buffer_dirty(path->nodes[0]);
	btrfs_free_path(path);
	inode->i_op = &btrfs_symlink_inode_operations;
	inode->i_mapping->a_ops = &btrfs_symlink_aops;
	inode->i_size = name_len - 1;
	err = btrfs_update_inode(trans, root, inode);
	if (err)
		drop_inode = 1;

out_unlock:
	btrfs_end_transaction(trans, root);
	mutex_unlock(&root->fs_info->fs_mutex);
	if (drop_inode) {
		inode_dec_link_count(inode);
		iput(inode);
	}
	btrfs_btree_balance_dirty(root);
	return err;
}

static struct inode_operations btrfs_dir_inode_operations = {
	.lookup		= btrfs_lookup,
	.create		= btrfs_create,
	.unlink		= btrfs_unlink,
	.link		= btrfs_link,
	.mkdir		= btrfs_mkdir,
	.rmdir		= btrfs_rmdir,
	.rename		= btrfs_rename,
	.symlink	= btrfs_symlink,
	.setattr	= btrfs_setattr,
	.mknod		= btrfs_mknod,
};

static struct inode_operations btrfs_dir_ro_inode_operations = {
	.lookup		= btrfs_lookup,
};

static struct file_operations btrfs_dir_file_operations = {
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir,
	.readdir	= btrfs_readdir,
	.ioctl		= btrfs_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= btrfs_compat_ioctl,
#endif
};

static struct extent_map_ops btrfs_extent_map_ops = {
	.fill_delalloc = run_delalloc_range,
	.writepage_io_hook = btrfs_writepage_io_hook,
	.readpage_io_hook = btrfs_readpage_io_hook,
	.readpage_end_io_hook = btrfs_readpage_end_io_hook,
};

static struct address_space_operations btrfs_aops = {
	.readpage	= btrfs_readpage,
	.writepage	= btrfs_writepage,
	.sync_page	= block_sync_page,
	.prepare_write	= btrfs_prepare_write,
	.commit_write	= btrfs_commit_write,
	.bmap		= btrfs_bmap,
	.invalidatepage = btrfs_invalidatepage,
	.releasepage	= btrfs_releasepage,
	.set_page_dirty	= __set_page_dirty_nobuffers,
};

static struct address_space_operations btrfs_symlink_aops = {
	.readpage	= btrfs_readpage,
	.writepage	= btrfs_writepage,
};

static struct inode_operations btrfs_file_inode_operations = {
	.truncate	= btrfs_truncate,
	.getattr	= btrfs_getattr,
	.setattr	= btrfs_setattr,
};

static struct inode_operations btrfs_special_inode_operations = {
	.getattr	= btrfs_getattr,
	.setattr	= btrfs_setattr,
};

static struct inode_operations btrfs_symlink_inode_operations = {
	.readlink	= generic_readlink,
	.follow_link	= page_follow_link_light,
	.put_link	= page_put_link,
};
