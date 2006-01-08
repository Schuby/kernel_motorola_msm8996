/*
 *  linux/mm/vmscan.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  Swap reorganised 29.12.95, Stephen Tweedie.
 *  kswapd added: 7.1.96  sct
 *  Removed kswapd_ctl limits, and swap out as many pages as needed
 *  to bring the system back to freepages.high: 2.4.97, Rik van Riel.
 *  Zone aware kswapd started 02/00, Kanoj Sarcar (kanoj@sgi.com).
 *  Multiqueue VM started 5.8.00, Rik van Riel.
 */

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/pagemap.h>
#include <linux/init.h>
#include <linux/highmem.h>
#include <linux/file.h>
#include <linux/writeback.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>	/* for try_to_release_page(),
					buffer_heads_over_limit */
#include <linux/mm_inline.h>
#include <linux/pagevec.h>
#include <linux/backing-dev.h>
#include <linux/rmap.h>
#include <linux/topology.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/notifier.h>
#include <linux/rwsem.h>

#include <asm/tlbflush.h>
#include <asm/div64.h>

#include <linux/swapops.h>

/* possible outcome of pageout() */
typedef enum {
	/* failed to write page out, page is locked */
	PAGE_KEEP,
	/* move page to the active list, page is locked */
	PAGE_ACTIVATE,
	/* page has been sent to the disk successfully, page is unlocked */
	PAGE_SUCCESS,
	/* page is clean and locked */
	PAGE_CLEAN,
} pageout_t;

struct scan_control {
	/* Ask refill_inactive_zone, or shrink_cache to scan this many pages */
	unsigned long nr_to_scan;

	/* Incremented by the number of inactive pages that were scanned */
	unsigned long nr_scanned;

	/* Incremented by the number of pages reclaimed */
	unsigned long nr_reclaimed;

	unsigned long nr_mapped;	/* From page_state */

	/* Ask shrink_caches, or shrink_zone to scan at this priority */
	unsigned int priority;

	/* This context's GFP mask */
	gfp_t gfp_mask;

	int may_writepage;

	/* This context's SWAP_CLUSTER_MAX. If freeing memory for
	 * suspend, we effectively ignore SWAP_CLUSTER_MAX.
	 * In this context, it doesn't matter that we scan the
	 * whole list at once. */
	int swap_cluster_max;
};

/*
 * The list of shrinker callbacks used by to apply pressure to
 * ageable caches.
 */
struct shrinker {
	shrinker_t		shrinker;
	struct list_head	list;
	int			seeks;	/* seeks to recreate an obj */
	long			nr;	/* objs pending delete */
};

#define lru_to_page(_head) (list_entry((_head)->prev, struct page, lru))

#ifdef ARCH_HAS_PREFETCH
#define prefetch_prev_lru_page(_page, _base, _field)			\
	do {								\
		if ((_page)->lru.prev != _base) {			\
			struct page *prev;				\
									\
			prev = lru_to_page(&(_page->lru));		\
			prefetch(&prev->_field);			\
		}							\
	} while (0)
#else
#define prefetch_prev_lru_page(_page, _base, _field) do { } while (0)
#endif

#ifdef ARCH_HAS_PREFETCHW
#define prefetchw_prev_lru_page(_page, _base, _field)			\
	do {								\
		if ((_page)->lru.prev != _base) {			\
			struct page *prev;				\
									\
			prev = lru_to_page(&(_page->lru));		\
			prefetchw(&prev->_field);			\
		}							\
	} while (0)
#else
#define prefetchw_prev_lru_page(_page, _base, _field) do { } while (0)
#endif

/*
 * From 0 .. 100.  Higher means more swappy.
 */
int vm_swappiness = 60;
static long total_memory;

static LIST_HEAD(shrinker_list);
static DECLARE_RWSEM(shrinker_rwsem);

/*
 * Add a shrinker callback to be called from the vm
 */
struct shrinker *set_shrinker(int seeks, shrinker_t theshrinker)
{
        struct shrinker *shrinker;

        shrinker = kmalloc(sizeof(*shrinker), GFP_KERNEL);
        if (shrinker) {
	        shrinker->shrinker = theshrinker;
	        shrinker->seeks = seeks;
	        shrinker->nr = 0;
	        down_write(&shrinker_rwsem);
	        list_add_tail(&shrinker->list, &shrinker_list);
	        up_write(&shrinker_rwsem);
	}
	return shrinker;
}
EXPORT_SYMBOL(set_shrinker);

/*
 * Remove one
 */
void remove_shrinker(struct shrinker *shrinker)
{
	down_write(&shrinker_rwsem);
	list_del(&shrinker->list);
	up_write(&shrinker_rwsem);
	kfree(shrinker);
}
EXPORT_SYMBOL(remove_shrinker);

#define SHRINK_BATCH 128
/*
 * Call the shrink functions to age shrinkable caches
 *
 * Here we assume it costs one seek to replace a lru page and that it also
 * takes a seek to recreate a cache object.  With this in mind we age equal
 * percentages of the lru and ageable caches.  This should balance the seeks
 * generated by these structures.
 *
 * If the vm encounted mapped pages on the LRU it increase the pressure on
 * slab to avoid swapping.
 *
 * We do weird things to avoid (scanned*seeks*entries) overflowing 32 bits.
 *
 * `lru_pages' represents the number of on-LRU pages in all the zones which
 * are eligible for the caller's allocation attempt.  It is used for balancing
 * slab reclaim versus page reclaim.
 *
 * Returns the number of slab objects which we shrunk.
 */
int shrink_slab(unsigned long scanned, gfp_t gfp_mask, unsigned long lru_pages)
{
	struct shrinker *shrinker;
	int ret = 0;

	if (scanned == 0)
		scanned = SWAP_CLUSTER_MAX;

	if (!down_read_trylock(&shrinker_rwsem))
		return 1;	/* Assume we'll be able to shrink next time */

	list_for_each_entry(shrinker, &shrinker_list, list) {
		unsigned long long delta;
		unsigned long total_scan;
		unsigned long max_pass = (*shrinker->shrinker)(0, gfp_mask);

		delta = (4 * scanned) / shrinker->seeks;
		delta *= max_pass;
		do_div(delta, lru_pages + 1);
		shrinker->nr += delta;
		if (shrinker->nr < 0) {
			printk(KERN_ERR "%s: nr=%ld\n",
					__FUNCTION__, shrinker->nr);
			shrinker->nr = max_pass;
		}

		/*
		 * Avoid risking looping forever due to too large nr value:
		 * never try to free more than twice the estimate number of
		 * freeable entries.
		 */
		if (shrinker->nr > max_pass * 2)
			shrinker->nr = max_pass * 2;

		total_scan = shrinker->nr;
		shrinker->nr = 0;

		while (total_scan >= SHRINK_BATCH) {
			long this_scan = SHRINK_BATCH;
			int shrink_ret;
			int nr_before;

			nr_before = (*shrinker->shrinker)(0, gfp_mask);
			shrink_ret = (*shrinker->shrinker)(this_scan, gfp_mask);
			if (shrink_ret == -1)
				break;
			if (shrink_ret < nr_before)
				ret += nr_before - shrink_ret;
			mod_page_state(slabs_scanned, this_scan);
			total_scan -= this_scan;

			cond_resched();
		}

		shrinker->nr += total_scan;
	}
	up_read(&shrinker_rwsem);
	return ret;
}

/* Called without lock on whether page is mapped, so answer is unstable */
static inline int page_mapping_inuse(struct page *page)
{
	struct address_space *mapping;

	/* Page is in somebody's page tables. */
	if (page_mapped(page))
		return 1;

	/* Be more reluctant to reclaim swapcache than pagecache */
	if (PageSwapCache(page))
		return 1;

	mapping = page_mapping(page);
	if (!mapping)
		return 0;

	/* File is mmap'd by somebody? */
	return mapping_mapped(mapping);
}

static inline int is_page_cache_freeable(struct page *page)
{
	return page_count(page) - !!PagePrivate(page) == 2;
}

static int may_write_to_queue(struct backing_dev_info *bdi)
{
	if (current_is_kswapd())
		return 1;
	if (current_is_pdflush())	/* This is unlikely, but why not... */
		return 1;
	if (!bdi_write_congested(bdi))
		return 1;
	if (bdi == current->backing_dev_info)
		return 1;
	return 0;
}

/*
 * We detected a synchronous write error writing a page out.  Probably
 * -ENOSPC.  We need to propagate that into the address_space for a subsequent
 * fsync(), msync() or close().
 *
 * The tricky part is that after writepage we cannot touch the mapping: nothing
 * prevents it from being freed up.  But we have a ref on the page and once
 * that page is locked, the mapping is pinned.
 *
 * We're allowed to run sleeping lock_page() here because we know the caller has
 * __GFP_FS.
 */
static void handle_write_error(struct address_space *mapping,
				struct page *page, int error)
{
	lock_page(page);
	if (page_mapping(page) == mapping) {
		if (error == -ENOSPC)
			set_bit(AS_ENOSPC, &mapping->flags);
		else
			set_bit(AS_EIO, &mapping->flags);
	}
	unlock_page(page);
}

/*
 * pageout is called by shrink_list() for each dirty page. Calls ->writepage().
 */
static pageout_t pageout(struct page *page, struct address_space *mapping)
{
	/*
	 * If the page is dirty, only perform writeback if that write
	 * will be non-blocking.  To prevent this allocation from being
	 * stalled by pagecache activity.  But note that there may be
	 * stalls if we need to run get_block().  We could test
	 * PagePrivate for that.
	 *
	 * If this process is currently in generic_file_write() against
	 * this page's queue, we can perform writeback even if that
	 * will block.
	 *
	 * If the page is swapcache, write it back even if that would
	 * block, for some throttling. This happens by accident, because
	 * swap_backing_dev_info is bust: it doesn't reflect the
	 * congestion state of the swapdevs.  Easy to fix, if needed.
	 * See swapfile.c:page_queue_congested().
	 */
	if (!is_page_cache_freeable(page))
		return PAGE_KEEP;
	if (!mapping) {
		/*
		 * Some data journaling orphaned pages can have
		 * page->mapping == NULL while being dirty with clean buffers.
		 */
		if (PagePrivate(page)) {
			if (try_to_free_buffers(page)) {
				ClearPageDirty(page);
				printk("%s: orphaned page\n", __FUNCTION__);
				return PAGE_CLEAN;
			}
		}
		return PAGE_KEEP;
	}
	if (mapping->a_ops->writepage == NULL)
		return PAGE_ACTIVATE;
	if (!may_write_to_queue(mapping->backing_dev_info))
		return PAGE_KEEP;

	if (clear_page_dirty_for_io(page)) {
		int res;
		struct writeback_control wbc = {
			.sync_mode = WB_SYNC_NONE,
			.nr_to_write = SWAP_CLUSTER_MAX,
			.nonblocking = 1,
			.for_reclaim = 1,
		};

		SetPageReclaim(page);
		res = mapping->a_ops->writepage(page, &wbc);
		if (res < 0)
			handle_write_error(mapping, page, res);
		if (res == AOP_WRITEPAGE_ACTIVATE) {
			ClearPageReclaim(page);
			return PAGE_ACTIVATE;
		}
		if (!PageWriteback(page)) {
			/* synchronous write or broken a_ops? */
			ClearPageReclaim(page);
		}

		return PAGE_SUCCESS;
	}

	return PAGE_CLEAN;
}

/*
 * shrink_list adds the number of reclaimed pages to sc->nr_reclaimed
 */
static int shrink_list(struct list_head *page_list, struct scan_control *sc)
{
	LIST_HEAD(ret_pages);
	struct pagevec freed_pvec;
	int pgactivate = 0;
	int reclaimed = 0;

	cond_resched();

	pagevec_init(&freed_pvec, 1);
	while (!list_empty(page_list)) {
		struct address_space *mapping;
		struct page *page;
		int may_enter_fs;
		int referenced;

		cond_resched();

		page = lru_to_page(page_list);
		list_del(&page->lru);

		if (TestSetPageLocked(page))
			goto keep;

		BUG_ON(PageActive(page));

		sc->nr_scanned++;
		/* Double the slab pressure for mapped and swapcache pages */
		if (page_mapped(page) || PageSwapCache(page))
			sc->nr_scanned++;

		if (PageWriteback(page))
			goto keep_locked;

		referenced = page_referenced(page, 1);
		/* In active use or really unfreeable?  Activate it. */
		if (referenced && page_mapping_inuse(page))
			goto activate_locked;

#ifdef CONFIG_SWAP
		/*
		 * Anonymous process memory has backing store?
		 * Try to allocate it some swap space here.
		 */
		if (PageAnon(page) && !PageSwapCache(page)) {
			if (!add_to_swap(page))
				goto activate_locked;
		}
#endif /* CONFIG_SWAP */

		mapping = page_mapping(page);
		may_enter_fs = (sc->gfp_mask & __GFP_FS) ||
			(PageSwapCache(page) && (sc->gfp_mask & __GFP_IO));

		/*
		 * The page is mapped into the page tables of one or more
		 * processes. Try to unmap it here.
		 */
		if (page_mapped(page) && mapping) {
			switch (try_to_unmap(page)) {
			case SWAP_FAIL:
				goto activate_locked;
			case SWAP_AGAIN:
				goto keep_locked;
			case SWAP_SUCCESS:
				; /* try to free the page below */
			}
		}

		if (PageDirty(page)) {
			if (referenced)
				goto keep_locked;
			if (!may_enter_fs)
				goto keep_locked;
			if (laptop_mode && !sc->may_writepage)
				goto keep_locked;

			/* Page is dirty, try to write it out here */
			switch(pageout(page, mapping)) {
			case PAGE_KEEP:
				goto keep_locked;
			case PAGE_ACTIVATE:
				goto activate_locked;
			case PAGE_SUCCESS:
				if (PageWriteback(page) || PageDirty(page))
					goto keep;
				/*
				 * A synchronous write - probably a ramdisk.  Go
				 * ahead and try to reclaim the page.
				 */
				if (TestSetPageLocked(page))
					goto keep;
				if (PageDirty(page) || PageWriteback(page))
					goto keep_locked;
				mapping = page_mapping(page);
			case PAGE_CLEAN:
				; /* try to free the page below */
			}
		}

		/*
		 * If the page has buffers, try to free the buffer mappings
		 * associated with this page. If we succeed we try to free
		 * the page as well.
		 *
		 * We do this even if the page is PageDirty().
		 * try_to_release_page() does not perform I/O, but it is
		 * possible for a page to have PageDirty set, but it is actually
		 * clean (all its buffers are clean).  This happens if the
		 * buffers were written out directly, with submit_bh(). ext3
		 * will do this, as well as the blockdev mapping. 
		 * try_to_release_page() will discover that cleanness and will
		 * drop the buffers and mark the page clean - it can be freed.
		 *
		 * Rarely, pages can have buffers and no ->mapping.  These are
		 * the pages which were not successfully invalidated in
		 * truncate_complete_page().  We try to drop those buffers here
		 * and if that worked, and the page is no longer mapped into
		 * process address space (page_count == 1) it can be freed.
		 * Otherwise, leave the page on the LRU so it is swappable.
		 */
		if (PagePrivate(page)) {
			if (!try_to_release_page(page, sc->gfp_mask))
				goto activate_locked;
			if (!mapping && page_count(page) == 1)
				goto free_it;
		}

		if (!mapping)
			goto keep_locked;	/* truncate got there first */

		write_lock_irq(&mapping->tree_lock);

		/*
		 * The non-racy check for busy page.  It is critical to check
		 * PageDirty _after_ making sure that the page is freeable and
		 * not in use by anybody. 	(pagecache + us == 2)
		 */
		if (unlikely(page_count(page) != 2))
			goto cannot_free;
		smp_rmb();
		if (unlikely(PageDirty(page)))
			goto cannot_free;

#ifdef CONFIG_SWAP
		if (PageSwapCache(page)) {
			swp_entry_t swap = { .val = page_private(page) };
			__delete_from_swap_cache(page);
			write_unlock_irq(&mapping->tree_lock);
			swap_free(swap);
			__put_page(page);	/* The pagecache ref */
			goto free_it;
		}
#endif /* CONFIG_SWAP */

		__remove_from_page_cache(page);
		write_unlock_irq(&mapping->tree_lock);
		__put_page(page);

free_it:
		unlock_page(page);
		reclaimed++;
		if (!pagevec_add(&freed_pvec, page))
			__pagevec_release_nonlru(&freed_pvec);
		continue;

cannot_free:
		write_unlock_irq(&mapping->tree_lock);
		goto keep_locked;

activate_locked:
		SetPageActive(page);
		pgactivate++;
keep_locked:
		unlock_page(page);
keep:
		list_add(&page->lru, &ret_pages);
		BUG_ON(PageLRU(page));
	}
	list_splice(&ret_pages, page_list);
	if (pagevec_count(&freed_pvec))
		__pagevec_release_nonlru(&freed_pvec);
	mod_page_state(pgactivate, pgactivate);
	sc->nr_reclaimed += reclaimed;
	return reclaimed;
}

/*
 * zone->lru_lock is heavily contended.  Some of the functions that
 * shrink the lists perform better by taking out a batch of pages
 * and working on them outside the LRU lock.
 *
 * For pagecache intensive workloads, this function is the hottest
 * spot in the kernel (apart from copy_*_user functions).
 *
 * Appropriate locks must be held before calling this function.
 *
 * @nr_to_scan:	The number of pages to look through on the list.
 * @src:	The LRU list to pull pages off.
 * @dst:	The temp list to put pages on to.
 * @scanned:	The number of pages that were scanned.
 *
 * returns how many pages were moved onto *@dst.
 */
static int isolate_lru_pages(int nr_to_scan, struct list_head *src,
			     struct list_head *dst, int *scanned)
{
	int nr_taken = 0;
	struct page *page;
	int scan = 0;

	while (scan++ < nr_to_scan && !list_empty(src)) {
		page = lru_to_page(src);
		prefetchw_prev_lru_page(page, src, flags);

		switch (__isolate_lru_page(page)) {
		case 1:
			/* Succeeded to isolate page */
			list_move(&page->lru, dst);
			nr_taken++;
			break;
		case -ENOENT:
			/* Not possible to isolate */
			list_move(&page->lru, src);
			break;
		default:
			BUG();
		}
	}

	*scanned = scan;
	return nr_taken;
}

static void lru_add_drain_per_cpu(void *dummy)
{
	lru_add_drain();
}

/*
 * Isolate one page from the LRU lists and put it on the
 * indicated list. Do necessary cache draining if the
 * page is not on the LRU lists yet.
 *
 * Result:
 *  0 = page not on LRU list
 *  1 = page removed from LRU list and added to the specified list.
 * -ENOENT = page is being freed elsewhere.
 */
int isolate_lru_page(struct page *page)
{
	int rc = 0;
	struct zone *zone = page_zone(page);

redo:
	spin_lock_irq(&zone->lru_lock);
	rc = __isolate_lru_page(page);
	if (rc == 1) {
		if (PageActive(page))
			del_page_from_active_list(zone, page);
		else
			del_page_from_inactive_list(zone, page);
	}
	spin_unlock_irq(&zone->lru_lock);
	if (rc == 0) {
		/*
		 * Maybe this page is still waiting for a cpu to drain it
		 * from one of the lru lists?
		 */
		rc = schedule_on_each_cpu(lru_add_drain_per_cpu, NULL);
		if (rc == 0 && PageLRU(page))
			goto redo;
	}
	return rc;
}

/*
 * shrink_cache() adds the number of pages reclaimed to sc->nr_reclaimed
 */
static void shrink_cache(struct zone *zone, struct scan_control *sc)
{
	LIST_HEAD(page_list);
	struct pagevec pvec;
	int max_scan = sc->nr_to_scan;

	pagevec_init(&pvec, 1);

	lru_add_drain();
	spin_lock_irq(&zone->lru_lock);
	while (max_scan > 0) {
		struct page *page;
		int nr_taken;
		int nr_scan;
		int nr_freed;

		nr_taken = isolate_lru_pages(sc->swap_cluster_max,
					     &zone->inactive_list,
					     &page_list, &nr_scan);
		zone->nr_inactive -= nr_taken;
		zone->pages_scanned += nr_scan;
		spin_unlock_irq(&zone->lru_lock);

		if (nr_taken == 0)
			goto done;

		max_scan -= nr_scan;
		nr_freed = shrink_list(&page_list, sc);

		local_irq_disable();
		if (current_is_kswapd()) {
			__mod_page_state_zone(zone, pgscan_kswapd, nr_scan);
			__mod_page_state(kswapd_steal, nr_freed);
		} else
			__mod_page_state_zone(zone, pgscan_direct, nr_scan);
		__mod_page_state_zone(zone, pgsteal, nr_freed);

		spin_lock(&zone->lru_lock);
		/*
		 * Put back any unfreeable pages.
		 */
		while (!list_empty(&page_list)) {
			page = lru_to_page(&page_list);
			if (TestSetPageLRU(page))
				BUG();
			list_del(&page->lru);
			if (PageActive(page))
				add_page_to_active_list(zone, page);
			else
				add_page_to_inactive_list(zone, page);
			if (!pagevec_add(&pvec, page)) {
				spin_unlock_irq(&zone->lru_lock);
				__pagevec_release(&pvec);
				spin_lock_irq(&zone->lru_lock);
			}
		}
  	}
	spin_unlock_irq(&zone->lru_lock);
done:
	pagevec_release(&pvec);
}

static inline void move_to_lru(struct page *page)
{
	list_del(&page->lru);
	if (PageActive(page)) {
		/*
		 * lru_cache_add_active checks that
		 * the PG_active bit is off.
		 */
		ClearPageActive(page);
		lru_cache_add_active(page);
	} else {
		lru_cache_add(page);
	}
	put_page(page);
}

/*
 * Add isolated pages on the list back to the LRU
 *
 * returns the number of pages put back.
 */
int putback_lru_pages(struct list_head *l)
{
	struct page *page;
	struct page *page2;
	int count = 0;

	list_for_each_entry_safe(page, page2, l, lru) {
		move_to_lru(page);
		count++;
	}
	return count;
}

/*
 * This moves pages from the active list to the inactive list.
 *
 * We move them the other way if the page is referenced by one or more
 * processes, from rmap.
 *
 * If the pages are mostly unmapped, the processing is fast and it is
 * appropriate to hold zone->lru_lock across the whole operation.  But if
 * the pages are mapped, the processing is slow (page_referenced()) so we
 * should drop zone->lru_lock around each page.  It's impossible to balance
 * this, so instead we remove the pages from the LRU while processing them.
 * It is safe to rely on PG_active against the non-LRU pages in here because
 * nobody will play with that bit on a non-LRU page.
 *
 * The downside is that we have to touch page->_count against each page.
 * But we had to alter page->flags anyway.
 */
static void
refill_inactive_zone(struct zone *zone, struct scan_control *sc)
{
	int pgmoved;
	int pgdeactivate = 0;
	int pgscanned;
	int nr_pages = sc->nr_to_scan;
	LIST_HEAD(l_hold);	/* The pages which were snipped off */
	LIST_HEAD(l_inactive);	/* Pages to go onto the inactive_list */
	LIST_HEAD(l_active);	/* Pages to go onto the active_list */
	struct page *page;
	struct pagevec pvec;
	int reclaim_mapped = 0;
	long mapped_ratio;
	long distress;
	long swap_tendency;

	lru_add_drain();
	spin_lock_irq(&zone->lru_lock);
	pgmoved = isolate_lru_pages(nr_pages, &zone->active_list,
				    &l_hold, &pgscanned);
	zone->pages_scanned += pgscanned;
	zone->nr_active -= pgmoved;
	spin_unlock_irq(&zone->lru_lock);

	/*
	 * `distress' is a measure of how much trouble we're having reclaiming
	 * pages.  0 -> no problems.  100 -> great trouble.
	 */
	distress = 100 >> zone->prev_priority;

	/*
	 * The point of this algorithm is to decide when to start reclaiming
	 * mapped memory instead of just pagecache.  Work out how much memory
	 * is mapped.
	 */
	mapped_ratio = (sc->nr_mapped * 100) / total_memory;

	/*
	 * Now decide how much we really want to unmap some pages.  The mapped
	 * ratio is downgraded - just because there's a lot of mapped memory
	 * doesn't necessarily mean that page reclaim isn't succeeding.
	 *
	 * The distress ratio is important - we don't want to start going oom.
	 *
	 * A 100% value of vm_swappiness overrides this algorithm altogether.
	 */
	swap_tendency = mapped_ratio / 2 + distress + vm_swappiness;

	/*
	 * Now use this metric to decide whether to start moving mapped memory
	 * onto the inactive list.
	 */
	if (swap_tendency >= 100)
		reclaim_mapped = 1;

	while (!list_empty(&l_hold)) {
		cond_resched();
		page = lru_to_page(&l_hold);
		list_del(&page->lru);
		if (page_mapped(page)) {
			if (!reclaim_mapped ||
			    (total_swap_pages == 0 && PageAnon(page)) ||
			    page_referenced(page, 0)) {
				list_add(&page->lru, &l_active);
				continue;
			}
		}
		list_add(&page->lru, &l_inactive);
	}

	pagevec_init(&pvec, 1);
	pgmoved = 0;
	spin_lock_irq(&zone->lru_lock);
	while (!list_empty(&l_inactive)) {
		page = lru_to_page(&l_inactive);
		prefetchw_prev_lru_page(page, &l_inactive, flags);
		if (TestSetPageLRU(page))
			BUG();
		if (!TestClearPageActive(page))
			BUG();
		list_move(&page->lru, &zone->inactive_list);
		pgmoved++;
		if (!pagevec_add(&pvec, page)) {
			zone->nr_inactive += pgmoved;
			spin_unlock_irq(&zone->lru_lock);
			pgdeactivate += pgmoved;
			pgmoved = 0;
			if (buffer_heads_over_limit)
				pagevec_strip(&pvec);
			__pagevec_release(&pvec);
			spin_lock_irq(&zone->lru_lock);
		}
	}
	zone->nr_inactive += pgmoved;
	pgdeactivate += pgmoved;
	if (buffer_heads_over_limit) {
		spin_unlock_irq(&zone->lru_lock);
		pagevec_strip(&pvec);
		spin_lock_irq(&zone->lru_lock);
	}

	pgmoved = 0;
	while (!list_empty(&l_active)) {
		page = lru_to_page(&l_active);
		prefetchw_prev_lru_page(page, &l_active, flags);
		if (TestSetPageLRU(page))
			BUG();
		BUG_ON(!PageActive(page));
		list_move(&page->lru, &zone->active_list);
		pgmoved++;
		if (!pagevec_add(&pvec, page)) {
			zone->nr_active += pgmoved;
			pgmoved = 0;
			spin_unlock_irq(&zone->lru_lock);
			__pagevec_release(&pvec);
			spin_lock_irq(&zone->lru_lock);
		}
	}
	zone->nr_active += pgmoved;
	spin_unlock(&zone->lru_lock);

	__mod_page_state_zone(zone, pgrefill, pgscanned);
	__mod_page_state(pgdeactivate, pgdeactivate);
	local_irq_enable();

	pagevec_release(&pvec);
}

/*
 * This is a basic per-zone page freer.  Used by both kswapd and direct reclaim.
 */
static void
shrink_zone(struct zone *zone, struct scan_control *sc)
{
	unsigned long nr_active;
	unsigned long nr_inactive;

	atomic_inc(&zone->reclaim_in_progress);

	/*
	 * Add one to `nr_to_scan' just to make sure that the kernel will
	 * slowly sift through the active list.
	 */
	zone->nr_scan_active += (zone->nr_active >> sc->priority) + 1;
	nr_active = zone->nr_scan_active;
	if (nr_active >= sc->swap_cluster_max)
		zone->nr_scan_active = 0;
	else
		nr_active = 0;

	zone->nr_scan_inactive += (zone->nr_inactive >> sc->priority) + 1;
	nr_inactive = zone->nr_scan_inactive;
	if (nr_inactive >= sc->swap_cluster_max)
		zone->nr_scan_inactive = 0;
	else
		nr_inactive = 0;

	while (nr_active || nr_inactive) {
		if (nr_active) {
			sc->nr_to_scan = min(nr_active,
					(unsigned long)sc->swap_cluster_max);
			nr_active -= sc->nr_to_scan;
			refill_inactive_zone(zone, sc);
		}

		if (nr_inactive) {
			sc->nr_to_scan = min(nr_inactive,
					(unsigned long)sc->swap_cluster_max);
			nr_inactive -= sc->nr_to_scan;
			shrink_cache(zone, sc);
		}
	}

	throttle_vm_writeout();

	atomic_dec(&zone->reclaim_in_progress);
}

/*
 * This is the direct reclaim path, for page-allocating processes.  We only
 * try to reclaim pages from zones which will satisfy the caller's allocation
 * request.
 *
 * We reclaim from a zone even if that zone is over pages_high.  Because:
 * a) The caller may be trying to free *extra* pages to satisfy a higher-order
 *    allocation or
 * b) The zones may be over pages_high but they must go *over* pages_high to
 *    satisfy the `incremental min' zone defense algorithm.
 *
 * Returns the number of reclaimed pages.
 *
 * If a zone is deemed to be full of pinned pages then just give it a light
 * scan then give up on it.
 */
static void
shrink_caches(struct zone **zones, struct scan_control *sc)
{
	int i;

	for (i = 0; zones[i] != NULL; i++) {
		struct zone *zone = zones[i];

		if (!populated_zone(zone))
			continue;

		if (!cpuset_zone_allowed(zone, __GFP_HARDWALL))
			continue;

		zone->temp_priority = sc->priority;
		if (zone->prev_priority > sc->priority)
			zone->prev_priority = sc->priority;

		if (zone->all_unreclaimable && sc->priority != DEF_PRIORITY)
			continue;	/* Let kswapd poll it */

		shrink_zone(zone, sc);
	}
}
 
/*
 * This is the main entry point to direct page reclaim.
 *
 * If a full scan of the inactive list fails to free enough memory then we
 * are "out of memory" and something needs to be killed.
 *
 * If the caller is !__GFP_FS then the probability of a failure is reasonably
 * high - the zone may be full of dirty or under-writeback pages, which this
 * caller can't do much about.  We kick pdflush and take explicit naps in the
 * hope that some of these pages can be written.  But if the allocating task
 * holds filesystem locks which prevent writeout this might not work, and the
 * allocation attempt will fail.
 */
int try_to_free_pages(struct zone **zones, gfp_t gfp_mask)
{
	int priority;
	int ret = 0;
	int total_scanned = 0, total_reclaimed = 0;
	struct reclaim_state *reclaim_state = current->reclaim_state;
	struct scan_control sc;
	unsigned long lru_pages = 0;
	int i;

	sc.gfp_mask = gfp_mask;
	sc.may_writepage = 0;

	inc_page_state(allocstall);

	for (i = 0; zones[i] != NULL; i++) {
		struct zone *zone = zones[i];

		if (!cpuset_zone_allowed(zone, __GFP_HARDWALL))
			continue;

		zone->temp_priority = DEF_PRIORITY;
		lru_pages += zone->nr_active + zone->nr_inactive;
	}

	for (priority = DEF_PRIORITY; priority >= 0; priority--) {
		sc.nr_mapped = read_page_state(nr_mapped);
		sc.nr_scanned = 0;
		sc.nr_reclaimed = 0;
		sc.priority = priority;
		sc.swap_cluster_max = SWAP_CLUSTER_MAX;
		if (!priority)
			disable_swap_token();
		shrink_caches(zones, &sc);
		shrink_slab(sc.nr_scanned, gfp_mask, lru_pages);
		if (reclaim_state) {
			sc.nr_reclaimed += reclaim_state->reclaimed_slab;
			reclaim_state->reclaimed_slab = 0;
		}
		total_scanned += sc.nr_scanned;
		total_reclaimed += sc.nr_reclaimed;
		if (total_reclaimed >= sc.swap_cluster_max) {
			ret = 1;
			goto out;
		}

		/*
		 * Try to write back as many pages as we just scanned.  This
		 * tends to cause slow streaming writers to write data to the
		 * disk smoothly, at the dirtying rate, which is nice.   But
		 * that's undesirable in laptop mode, where we *want* lumpy
		 * writeout.  So in laptop mode, write out the whole world.
		 */
		if (total_scanned > sc.swap_cluster_max + sc.swap_cluster_max/2) {
			wakeup_pdflush(laptop_mode ? 0 : total_scanned);
			sc.may_writepage = 1;
		}

		/* Take a nap, wait for some writeback to complete */
		if (sc.nr_scanned && priority < DEF_PRIORITY - 2)
			blk_congestion_wait(WRITE, HZ/10);
	}
out:
	for (i = 0; zones[i] != 0; i++) {
		struct zone *zone = zones[i];

		if (!cpuset_zone_allowed(zone, __GFP_HARDWALL))
			continue;

		zone->prev_priority = zone->temp_priority;
	}
	return ret;
}

/*
 * For kswapd, balance_pgdat() will work across all this node's zones until
 * they are all at pages_high.
 *
 * If `nr_pages' is non-zero then it is the number of pages which are to be
 * reclaimed, regardless of the zone occupancies.  This is a software suspend
 * special.
 *
 * Returns the number of pages which were actually freed.
 *
 * There is special handling here for zones which are full of pinned pages.
 * This can happen if the pages are all mlocked, or if they are all used by
 * device drivers (say, ZONE_DMA).  Or if they are all in use by hugetlb.
 * What we do is to detect the case where all pages in the zone have been
 * scanned twice and there has been zero successful reclaim.  Mark the zone as
 * dead and from now on, only perform a short scan.  Basically we're polling
 * the zone for when the problem goes away.
 *
 * kswapd scans the zones in the highmem->normal->dma direction.  It skips
 * zones which have free_pages > pages_high, but once a zone is found to have
 * free_pages <= pages_high, we scan that zone and the lower zones regardless
 * of the number of free pages in the lower zones.  This interoperates with
 * the page allocator fallback scheme to ensure that aging of pages is balanced
 * across the zones.
 */
static int balance_pgdat(pg_data_t *pgdat, int nr_pages, int order)
{
	int to_free = nr_pages;
	int all_zones_ok;
	int priority;
	int i;
	int total_scanned, total_reclaimed;
	struct reclaim_state *reclaim_state = current->reclaim_state;
	struct scan_control sc;

loop_again:
	total_scanned = 0;
	total_reclaimed = 0;
	sc.gfp_mask = GFP_KERNEL;
	sc.may_writepage = 0;
	sc.nr_mapped = read_page_state(nr_mapped);

	inc_page_state(pageoutrun);

	for (i = 0; i < pgdat->nr_zones; i++) {
		struct zone *zone = pgdat->node_zones + i;

		zone->temp_priority = DEF_PRIORITY;
	}

	for (priority = DEF_PRIORITY; priority >= 0; priority--) {
		int end_zone = 0;	/* Inclusive.  0 = ZONE_DMA */
		unsigned long lru_pages = 0;

		/* The swap token gets in the way of swapout... */
		if (!priority)
			disable_swap_token();

		all_zones_ok = 1;

		if (nr_pages == 0) {
			/*
			 * Scan in the highmem->dma direction for the highest
			 * zone which needs scanning
			 */
			for (i = pgdat->nr_zones - 1; i >= 0; i--) {
				struct zone *zone = pgdat->node_zones + i;

				if (!populated_zone(zone))
					continue;

				if (zone->all_unreclaimable &&
						priority != DEF_PRIORITY)
					continue;

				if (!zone_watermark_ok(zone, order,
						zone->pages_high, 0, 0)) {
					end_zone = i;
					goto scan;
				}
			}
			goto out;
		} else {
			end_zone = pgdat->nr_zones - 1;
		}
scan:
		for (i = 0; i <= end_zone; i++) {
			struct zone *zone = pgdat->node_zones + i;

			lru_pages += zone->nr_active + zone->nr_inactive;
		}

		/*
		 * Now scan the zone in the dma->highmem direction, stopping
		 * at the last zone which needs scanning.
		 *
		 * We do this because the page allocator works in the opposite
		 * direction.  This prevents the page allocator from allocating
		 * pages behind kswapd's direction of progress, which would
		 * cause too much scanning of the lower zones.
		 */
		for (i = 0; i <= end_zone; i++) {
			struct zone *zone = pgdat->node_zones + i;
			int nr_slab;

			if (!populated_zone(zone))
				continue;

			if (zone->all_unreclaimable && priority != DEF_PRIORITY)
				continue;

			if (nr_pages == 0) {	/* Not software suspend */
				if (!zone_watermark_ok(zone, order,
						zone->pages_high, end_zone, 0))
					all_zones_ok = 0;
			}
			zone->temp_priority = priority;
			if (zone->prev_priority > priority)
				zone->prev_priority = priority;
			sc.nr_scanned = 0;
			sc.nr_reclaimed = 0;
			sc.priority = priority;
			sc.swap_cluster_max = nr_pages? nr_pages : SWAP_CLUSTER_MAX;
			atomic_inc(&zone->reclaim_in_progress);
			shrink_zone(zone, &sc);
			atomic_dec(&zone->reclaim_in_progress);
			reclaim_state->reclaimed_slab = 0;
			nr_slab = shrink_slab(sc.nr_scanned, GFP_KERNEL,
						lru_pages);
			sc.nr_reclaimed += reclaim_state->reclaimed_slab;
			total_reclaimed += sc.nr_reclaimed;
			total_scanned += sc.nr_scanned;
			if (zone->all_unreclaimable)
				continue;
			if (nr_slab == 0 && zone->pages_scanned >=
				    (zone->nr_active + zone->nr_inactive) * 4)
				zone->all_unreclaimable = 1;
			/*
			 * If we've done a decent amount of scanning and
			 * the reclaim ratio is low, start doing writepage
			 * even in laptop mode
			 */
			if (total_scanned > SWAP_CLUSTER_MAX * 2 &&
			    total_scanned > total_reclaimed+total_reclaimed/2)
				sc.may_writepage = 1;
		}
		if (nr_pages && to_free > total_reclaimed)
			continue;	/* swsusp: need to do more work */
		if (all_zones_ok)
			break;		/* kswapd: all done */
		/*
		 * OK, kswapd is getting into trouble.  Take a nap, then take
		 * another pass across the zones.
		 */
		if (total_scanned && priority < DEF_PRIORITY - 2)
			blk_congestion_wait(WRITE, HZ/10);

		/*
		 * We do this so kswapd doesn't build up large priorities for
		 * example when it is freeing in parallel with allocators. It
		 * matches the direct reclaim path behaviour in terms of impact
		 * on zone->*_priority.
		 */
		if ((total_reclaimed >= SWAP_CLUSTER_MAX) && (!nr_pages))
			break;
	}
out:
	for (i = 0; i < pgdat->nr_zones; i++) {
		struct zone *zone = pgdat->node_zones + i;

		zone->prev_priority = zone->temp_priority;
	}
	if (!all_zones_ok) {
		cond_resched();
		goto loop_again;
	}

	return total_reclaimed;
}

/*
 * The background pageout daemon, started as a kernel thread
 * from the init process. 
 *
 * This basically trickles out pages so that we have _some_
 * free memory available even if there is no other activity
 * that frees anything up. This is needed for things like routing
 * etc, where we otherwise might have all activity going on in
 * asynchronous contexts that cannot page things out.
 *
 * If there are applications that are active memory-allocators
 * (most normal use), this basically shouldn't matter.
 */
static int kswapd(void *p)
{
	unsigned long order;
	pg_data_t *pgdat = (pg_data_t*)p;
	struct task_struct *tsk = current;
	DEFINE_WAIT(wait);
	struct reclaim_state reclaim_state = {
		.reclaimed_slab = 0,
	};
	cpumask_t cpumask;

	daemonize("kswapd%d", pgdat->node_id);
	cpumask = node_to_cpumask(pgdat->node_id);
	if (!cpus_empty(cpumask))
		set_cpus_allowed(tsk, cpumask);
	current->reclaim_state = &reclaim_state;

	/*
	 * Tell the memory management that we're a "memory allocator",
	 * and that if we need more memory we should get access to it
	 * regardless (see "__alloc_pages()"). "kswapd" should
	 * never get caught in the normal page freeing logic.
	 *
	 * (Kswapd normally doesn't need memory anyway, but sometimes
	 * you need a small amount of memory in order to be able to
	 * page out something else, and this flag essentially protects
	 * us from recursively trying to free more memory as we're
	 * trying to free the first piece of memory in the first place).
	 */
	tsk->flags |= PF_MEMALLOC|PF_KSWAPD;

	order = 0;
	for ( ; ; ) {
		unsigned long new_order;

		try_to_freeze();

		prepare_to_wait(&pgdat->kswapd_wait, &wait, TASK_INTERRUPTIBLE);
		new_order = pgdat->kswapd_max_order;
		pgdat->kswapd_max_order = 0;
		if (order < new_order) {
			/*
			 * Don't sleep if someone wants a larger 'order'
			 * allocation
			 */
			order = new_order;
		} else {
			schedule();
			order = pgdat->kswapd_max_order;
		}
		finish_wait(&pgdat->kswapd_wait, &wait);

		balance_pgdat(pgdat, 0, order);
	}
	return 0;
}

/*
 * A zone is low on free memory, so wake its kswapd task to service it.
 */
void wakeup_kswapd(struct zone *zone, int order)
{
	pg_data_t *pgdat;

	if (!populated_zone(zone))
		return;

	pgdat = zone->zone_pgdat;
	if (zone_watermark_ok(zone, order, zone->pages_low, 0, 0))
		return;
	if (pgdat->kswapd_max_order < order)
		pgdat->kswapd_max_order = order;
	if (!cpuset_zone_allowed(zone, __GFP_HARDWALL))
		return;
	if (!waitqueue_active(&pgdat->kswapd_wait))
		return;
	wake_up_interruptible(&pgdat->kswapd_wait);
}

#ifdef CONFIG_PM
/*
 * Try to free `nr_pages' of memory, system-wide.  Returns the number of freed
 * pages.
 */
int shrink_all_memory(int nr_pages)
{
	pg_data_t *pgdat;
	int nr_to_free = nr_pages;
	int ret = 0;
	struct reclaim_state reclaim_state = {
		.reclaimed_slab = 0,
	};

	current->reclaim_state = &reclaim_state;
	for_each_pgdat(pgdat) {
		int freed;
		freed = balance_pgdat(pgdat, nr_to_free, 0);
		ret += freed;
		nr_to_free -= freed;
		if (nr_to_free <= 0)
			break;
	}
	current->reclaim_state = NULL;
	return ret;
}
#endif

#ifdef CONFIG_HOTPLUG_CPU
/* It's optimal to keep kswapds on the same CPUs as their memory, but
   not required for correctness.  So if the last cpu in a node goes
   away, we get changed to run anywhere: as the first one comes back,
   restore their cpu bindings. */
static int __devinit cpu_callback(struct notifier_block *nfb,
				  unsigned long action,
				  void *hcpu)
{
	pg_data_t *pgdat;
	cpumask_t mask;

	if (action == CPU_ONLINE) {
		for_each_pgdat(pgdat) {
			mask = node_to_cpumask(pgdat->node_id);
			if (any_online_cpu(mask) != NR_CPUS)
				/* One of our CPUs online: restore mask */
				set_cpus_allowed(pgdat->kswapd, mask);
		}
	}
	return NOTIFY_OK;
}
#endif /* CONFIG_HOTPLUG_CPU */

static int __init kswapd_init(void)
{
	pg_data_t *pgdat;
	swap_setup();
	for_each_pgdat(pgdat)
		pgdat->kswapd
		= find_task_by_pid(kernel_thread(kswapd, pgdat, CLONE_KERNEL));
	total_memory = nr_free_pagecache_pages();
	hotcpu_notifier(cpu_callback, 0);
	return 0;
}

module_init(kswapd_init)
