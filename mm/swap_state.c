/*
 *  linux/mm/swap_state.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *  Swap reorganised 29.12.95, Stephen Tweedie
 *
 *  Rewritten to use page cache, (C) 1998 Stephen Tweedie
 */

#include <linux/mm.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/swapctl.h>
#include <linux/init.h>
#include <linux/pagemap.h>
#include <linux/smp_lock.h>

#include <asm/pgtable.h>

/*
 * We may have stale swap cache pages in memory: notice
 * them here and get rid of the unnecessary final write.
 */
static int swap_writepage(struct page *page)
{
	/* One for the page cache, one for this user, one for page->buffers */
	if (page_count(page) > 2 + !!page->buffers)
		goto in_use;
	if (swap_count(page) > 1)
		goto in_use;

	delete_from_swap_cache_nolock(page);
	UnlockPage(page);
	return 0;

in_use:
	rw_swap_page(WRITE, page);
	return 0;
}

static struct address_space_operations swap_aops = {
	writepage: swap_writepage,
	sync_page: block_sync_page,
};

struct address_space swapper_space = {
	LIST_HEAD_INIT(swapper_space.clean_pages),
	LIST_HEAD_INIT(swapper_space.dirty_pages),
	LIST_HEAD_INIT(swapper_space.locked_pages),
	0,				/* nrpages	*/
	&swap_aops,
};

#ifdef SWAP_CACHE_INFO
unsigned long swap_cache_add_total;
unsigned long swap_cache_del_total;
unsigned long swap_cache_find_total;
unsigned long swap_cache_find_success;

void show_swap_cache_info(void)
{
	printk("Swap cache: add %ld, delete %ld, find %ld/%ld\n",
		swap_cache_add_total, 
		swap_cache_del_total,
		swap_cache_find_success, swap_cache_find_total);
}
#endif

void add_to_swap_cache(struct page *page, swp_entry_t entry)
{
	unsigned long flags;

#ifdef SWAP_CACHE_INFO
	swap_cache_add_total++;
#endif
	if (!PageLocked(page))
		BUG();
	if (PageTestandSetSwapCache(page))
		BUG();
	if (page->mapping)
		BUG();

	/* clear PG_dirty so a subsequent set_page_dirty takes effect */
	flags = page->flags & ~(1 << PG_error | 1 << PG_dirty | 1 << PG_arch_1 | 1 << PG_referenced);
	page->flags = flags | (1 << PG_uptodate);
	add_to_page_cache_locked(page, &swapper_space, entry.val);
}

/*
 * This must be called only on pages that have
 * been verified to be in the swap cache.
 */
void __delete_from_swap_cache(struct page *page)
{
	struct address_space *mapping = page->mapping;
	swp_entry_t entry;

#ifdef SWAP_CACHE_INFO
	swap_cache_del_total++;
#endif
	if (mapping != &swapper_space)
		BUG();
	if (!PageSwapCache(page) || !PageLocked(page))
		BUG();

	entry.val = page->index;
	PageClearSwapCache(page);
	ClearPageDirty(page);
	__remove_inode_page(page);
	swap_free(entry);
}

/*
 * This will never put the page into the free list, the caller has
 * a reference on the page.
 */
void delete_from_swap_cache_nolock(struct page *page)
{
	if (!PageLocked(page))
		BUG();

	if (block_flushpage(page, 0))
		lru_cache_del(page);

	spin_lock(&pagecache_lock);
	__delete_from_swap_cache(page);
	spin_unlock(&pagecache_lock);
	page_cache_release(page);
}

/*
 * This must be called only on pages that have
 * been verified to be in the swap cache and locked.
 */
void delete_from_swap_cache(struct page *page)
{
	lock_page(page);
	delete_from_swap_cache_nolock(page);
	UnlockPage(page);
}

/* 
 * Perform a free_page(), also freeing any swap cache associated with
 * this page if it is the last user of the page. Can not do a lock_page,
 * as we are holding the page_table_lock spinlock.
 */
void free_page_and_swap_cache(struct page *page)
{
	/* 
	 * If we are the only user, then try to free up the swap cache. 
	 * 
	 * Its ok to check for PageSwapCache without the page lock
	 * here because we are going to recheck again inside 
	 * exclusive_swap_page() _with_ the lock. 
	 * 					- Marcelo
	 */
	if (PageSwapCache(page) && !TryLockPage(page)) {
		if (exclusive_swap_page(page))
			delete_from_swap_cache_nolock(page);
		UnlockPage(page);
	}
	page_cache_release(page);
}

/*
 * Lookup a swap entry in the swap cache. A found page will be returned
 * unlocked and with its refcount incremented - we rely on the kernel
 * lock getting page table operations atomic even if we drop the page
 * lock before returning.
 */
struct page * lookup_swap_cache(swp_entry_t entry)
{
	struct page *found;

#ifdef SWAP_CACHE_INFO
	swap_cache_find_total++;
#endif
	found = find_get_page(&swapper_space, entry.val);
	/*
	 * Unsafe to assert PageSwapCache and mapping on page found:
	 * if SMP nothing prevents swapoff from deleting this page from
	 * the swap cache at this moment.  find_lock_page would prevent
	 * that, but no need to change: we _have_ got the right page.
	 */
#ifdef SWAP_CACHE_INFO
	if (found)
		swap_cache_find_success++;
#endif
	return found;
}

/* 
 * Locate a page of swap in physical memory, reserving swap cache space
 * and reading the disk if it is not already cached.
 * A failure return means that either the page allocation failed or that
 * the swap entry is no longer in use.
 */
struct page * read_swap_cache_async(swp_entry_t entry)
{
	struct page *found_page, *new_page;
	struct page **hash;
	
	/*
	 * Look for the page in the swap cache.  Since we normally call
	 * this only after lookup_swap_cache() failed, re-calling that
	 * would confuse the statistics: use __find_get_page() directly.
	 */
	hash = page_hash(&swapper_space, entry.val);
	found_page = __find_get_page(&swapper_space, entry.val, hash);
	if (found_page)
		goto out;

	new_page = alloc_page(GFP_HIGHUSER);
	if (!new_page)
		goto out;		/* Out of memory */

	/*
	 * Check the swap cache again, in case we stalled above.
	 * The BKL is guarding against races between this check
	 * and where the new page is added to the swap cache below.
	 */
	found_page = __find_get_page(&swapper_space, entry.val, hash);
	if (found_page)
		goto out_free_page;

	/*
	 * Make sure the swap entry is still in use.  It could have gone
	 * while caller waited for BKL, or while allocating page above,
	 * or while allocating page in prior call via swapin_readahead.
	 */
	if (!swap_duplicate(entry))	/* Account for the swap cache */
		goto out_free_page;

	/* 
	 * Add it to the swap cache and read its contents.
	 */
	if (TryLockPage(new_page))
		BUG();
	add_to_swap_cache(new_page, entry);
	rw_swap_page(READ, new_page);
	return new_page;

out_free_page:
	page_cache_release(new_page);
out:
	return found_page;
}
