/**************************************************************************
 *
 * Copyright (c) 2006-2009 VMware, Inc., Palo Alto, CA., USA
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Authors: Thomas Hellstrom <thellstrom-at-vmware-dot-com>
 */

#include <linux/sched.h>
#include <linux/highmem.h>
#include <linux/pagemap.h>
#include <linux/shmem_fs.h>
#include <linux/file.h>
#include <linux/swap.h>
#include <linux/slab.h>
#include <linux/export.h>
#include "drm_cache.h"
#include "drm_mem_util.h"
#include "ttm/ttm_module.h"
#include "ttm/ttm_bo_driver.h"
#include "ttm/ttm_placement.h"
#include "ttm/ttm_page_alloc.h"

static int ttm_tt_swapin(struct ttm_tt *ttm);

/**
 * Allocates storage for pointers to the pages that back the ttm.
 */
static void ttm_tt_alloc_page_directory(struct ttm_tt *ttm)
{
	ttm->pages = drm_calloc_large(ttm->num_pages, sizeof(*ttm->pages));
	ttm->dma_address = drm_calloc_large(ttm->num_pages,
					    sizeof(*ttm->dma_address));
}

static void ttm_tt_free_page_directory(struct ttm_tt *ttm)
{
	drm_free_large(ttm->pages);
	ttm->pages = NULL;
	drm_free_large(ttm->dma_address);
	ttm->dma_address = NULL;
}

static struct page *__ttm_tt_get_page(struct ttm_tt *ttm, int index)
{
	struct page *p;
	struct ttm_mem_global *mem_glob = ttm->glob->mem_glob;
	int ret;

	if (NULL == (p = ttm->pages[index])) {

		ret = ttm_get_pages(&p, ttm->page_flags, ttm->caching_state, 1,
				    &ttm->dma_address[index]);
		if (ret != 0)
			return NULL;

		ret = ttm_mem_global_alloc_page(mem_glob, p, false, false);
		if (unlikely(ret != 0))
			goto out_err;

		ttm->pages[index] = p;
	}
	return p;
out_err:
	ttm_put_pages(&p, 1, ttm->page_flags,
		      ttm->caching_state, &ttm->dma_address[index]);
	return NULL;
}

struct page *ttm_tt_get_page(struct ttm_tt *ttm, int index)
{
	int ret;

	if (unlikely(ttm->page_flags & TTM_PAGE_FLAG_SWAPPED)) {
		ret = ttm_tt_swapin(ttm);
		if (unlikely(ret != 0))
			return NULL;
	}
	return __ttm_tt_get_page(ttm, index);
}

int ttm_tt_populate(struct ttm_tt *ttm)
{
	struct page *page;
	unsigned long i;
	int ret;

	if (ttm->state != tt_unpopulated)
		return 0;

	if (unlikely(ttm->page_flags & TTM_PAGE_FLAG_SWAPPED)) {
		ret = ttm_tt_swapin(ttm);
		if (unlikely(ret != 0))
			return ret;
	}

	for (i = 0; i < ttm->num_pages; ++i) {
		page = __ttm_tt_get_page(ttm, i);
		if (!page)
			return -ENOMEM;
	}
	ttm->state = tt_unbound;
	return 0;
}
EXPORT_SYMBOL(ttm_tt_populate);

#ifdef CONFIG_X86
static inline int ttm_tt_set_page_caching(struct page *p,
					  enum ttm_caching_state c_old,
					  enum ttm_caching_state c_new)
{
	int ret = 0;

	if (PageHighMem(p))
		return 0;

	if (c_old != tt_cached) {
		/* p isn't in the default caching state, set it to
		 * writeback first to free its current memtype. */

		ret = set_pages_wb(p, 1);
		if (ret)
			return ret;
	}

	if (c_new == tt_wc)
		ret = set_memory_wc((unsigned long) page_address(p), 1);
	else if (c_new == tt_uncached)
		ret = set_pages_uc(p, 1);

	return ret;
}
#else /* CONFIG_X86 */
static inline int ttm_tt_set_page_caching(struct page *p,
					  enum ttm_caching_state c_old,
					  enum ttm_caching_state c_new)
{
	return 0;
}
#endif /* CONFIG_X86 */

/*
 * Change caching policy for the linear kernel map
 * for range of pages in a ttm.
 */

static int ttm_tt_set_caching(struct ttm_tt *ttm,
			      enum ttm_caching_state c_state)
{
	int i, j;
	struct page *cur_page;
	int ret;

	if (ttm->caching_state == c_state)
		return 0;

	if (ttm->state == tt_unpopulated) {
		/* Change caching but don't populate */
		ttm->caching_state = c_state;
		return 0;
	}

	if (ttm->caching_state == tt_cached)
		drm_clflush_pages(ttm->pages, ttm->num_pages);

	for (i = 0; i < ttm->num_pages; ++i) {
		cur_page = ttm->pages[i];
		if (likely(cur_page != NULL)) {
			ret = ttm_tt_set_page_caching(cur_page,
						      ttm->caching_state,
						      c_state);
			if (unlikely(ret != 0))
				goto out_err;
		}
	}

	ttm->caching_state = c_state;

	return 0;

out_err:
	for (j = 0; j < i; ++j) {
		cur_page = ttm->pages[j];
		if (likely(cur_page != NULL)) {
			(void)ttm_tt_set_page_caching(cur_page, c_state,
						      ttm->caching_state);
		}
	}

	return ret;
}

int ttm_tt_set_placement_caching(struct ttm_tt *ttm, uint32_t placement)
{
	enum ttm_caching_state state;

	if (placement & TTM_PL_FLAG_WC)
		state = tt_wc;
	else if (placement & TTM_PL_FLAG_UNCACHED)
		state = tt_uncached;
	else
		state = tt_cached;

	return ttm_tt_set_caching(ttm, state);
}
EXPORT_SYMBOL(ttm_tt_set_placement_caching);

static void ttm_tt_free_alloced_pages(struct ttm_tt *ttm)
{
	unsigned i;

	for (i = 0; i < ttm->num_pages; ++i) {
		if (ttm->pages[i]) {
			ttm_mem_global_free_page(ttm->glob->mem_glob,
						 ttm->pages[i]);
			ttm_put_pages(&ttm->pages[i], 1, ttm->page_flags,
				      ttm->caching_state, &ttm->dma_address[i]);
		}
	}
	ttm->state = tt_unpopulated;
}

void ttm_tt_destroy(struct ttm_tt *ttm)
{
	if (unlikely(ttm == NULL))
		return;

	if (ttm->state == tt_bound) {
		ttm_tt_unbind(ttm);
	}

	if (likely(ttm->pages != NULL)) {
		ttm_tt_free_alloced_pages(ttm);
		ttm_tt_free_page_directory(ttm);
	}

	if (!(ttm->page_flags & TTM_PAGE_FLAG_PERSISTENT_SWAP) &&
	    ttm->swap_storage)
		fput(ttm->swap_storage);

	ttm->swap_storage = NULL;
	ttm->func->destroy(ttm);
}

int ttm_tt_init(struct ttm_tt *ttm, struct ttm_bo_device *bdev,
		unsigned long size, uint32_t page_flags,
		struct page *dummy_read_page)
{
	ttm->bdev = bdev;
	ttm->glob = bdev->glob;
	ttm->num_pages = (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	ttm->caching_state = tt_cached;
	ttm->page_flags = page_flags;
	ttm->dummy_read_page = dummy_read_page;
	ttm->state = tt_unpopulated;

	ttm_tt_alloc_page_directory(ttm);
	if (!ttm->pages || !ttm->dma_address) {
		ttm_tt_destroy(ttm);
		printk(KERN_ERR TTM_PFX "Failed allocating page table\n");
		return -ENOMEM;
	}
	return 0;
}
EXPORT_SYMBOL(ttm_tt_init);

void ttm_tt_unbind(struct ttm_tt *ttm)
{
	int ret;

	if (ttm->state == tt_bound) {
		ret = ttm->func->unbind(ttm);
		BUG_ON(ret);
		ttm->state = tt_unbound;
	}
}

int ttm_tt_bind(struct ttm_tt *ttm, struct ttm_mem_reg *bo_mem)
{
	int ret = 0;

	if (!ttm)
		return -EINVAL;

	if (ttm->state == tt_bound)
		return 0;

	ret = ttm_tt_populate(ttm);
	if (ret)
		return ret;

	ret = ttm->func->bind(ttm, bo_mem);
	if (unlikely(ret != 0))
		return ret;

	ttm->state = tt_bound;

	return 0;
}
EXPORT_SYMBOL(ttm_tt_bind);

static int ttm_tt_swapin(struct ttm_tt *ttm)
{
	struct address_space *swap_space;
	struct file *swap_storage;
	struct page *from_page;
	struct page *to_page;
	void *from_virtual;
	void *to_virtual;
	int i;
	int ret = -ENOMEM;

	swap_storage = ttm->swap_storage;
	BUG_ON(swap_storage == NULL);

	swap_space = swap_storage->f_path.dentry->d_inode->i_mapping;

	for (i = 0; i < ttm->num_pages; ++i) {
		from_page = shmem_read_mapping_page(swap_space, i);
		if (IS_ERR(from_page)) {
			ret = PTR_ERR(from_page);
			goto out_err;
		}
		to_page = __ttm_tt_get_page(ttm, i);
		if (unlikely(to_page == NULL))
			goto out_err;

		preempt_disable();
		from_virtual = kmap_atomic(from_page, KM_USER0);
		to_virtual = kmap_atomic(to_page, KM_USER1);
		memcpy(to_virtual, from_virtual, PAGE_SIZE);
		kunmap_atomic(to_virtual, KM_USER1);
		kunmap_atomic(from_virtual, KM_USER0);
		preempt_enable();
		page_cache_release(from_page);
	}

	if (!(ttm->page_flags & TTM_PAGE_FLAG_PERSISTENT_SWAP))
		fput(swap_storage);
	ttm->swap_storage = NULL;
	ttm->page_flags &= ~TTM_PAGE_FLAG_SWAPPED;

	return 0;
out_err:
	ttm_tt_free_alloced_pages(ttm);
	return ret;
}

int ttm_tt_swapout(struct ttm_tt *ttm, struct file *persistent_swap_storage)
{
	struct address_space *swap_space;
	struct file *swap_storage;
	struct page *from_page;
	struct page *to_page;
	void *from_virtual;
	void *to_virtual;
	int i;
	int ret = -ENOMEM;

	BUG_ON(ttm->state != tt_unbound && ttm->state != tt_unpopulated);
	BUG_ON(ttm->caching_state != tt_cached);

	if (!persistent_swap_storage) {
		swap_storage = shmem_file_setup("ttm swap",
						ttm->num_pages << PAGE_SHIFT,
						0);
		if (unlikely(IS_ERR(swap_storage))) {
			printk(KERN_ERR "Failed allocating swap storage.\n");
			return PTR_ERR(swap_storage);
		}
	} else
		swap_storage = persistent_swap_storage;

	swap_space = swap_storage->f_path.dentry->d_inode->i_mapping;

	for (i = 0; i < ttm->num_pages; ++i) {
		from_page = ttm->pages[i];
		if (unlikely(from_page == NULL))
			continue;
		to_page = shmem_read_mapping_page(swap_space, i);
		if (unlikely(IS_ERR(to_page))) {
			ret = PTR_ERR(to_page);
			goto out_err;
		}
		preempt_disable();
		from_virtual = kmap_atomic(from_page, KM_USER0);
		to_virtual = kmap_atomic(to_page, KM_USER1);
		memcpy(to_virtual, from_virtual, PAGE_SIZE);
		kunmap_atomic(to_virtual, KM_USER1);
		kunmap_atomic(from_virtual, KM_USER0);
		preempt_enable();
		set_page_dirty(to_page);
		mark_page_accessed(to_page);
		page_cache_release(to_page);
	}

	ttm_tt_free_alloced_pages(ttm);
	ttm->swap_storage = swap_storage;
	ttm->page_flags |= TTM_PAGE_FLAG_SWAPPED;
	if (persistent_swap_storage)
		ttm->page_flags |= TTM_PAGE_FLAG_PERSISTENT_SWAP;

	return 0;
out_err:
	if (!persistent_swap_storage)
		fput(swap_storage);

	return ret;
}
