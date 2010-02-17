/*
 * arch/sh/mm/pmb.c
 *
 * Privileged Space Mapping Buffer (PMB) Support.
 *
 * Copyright (C) 2005 - 2010  Paul Mundt
 * Copyright (C) 2010  Matt Fleming
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sysdev.h>
#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/rwlock.h>
#include <asm/sizes.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/mmu.h>
#include <asm/mmu_context.h>

struct pmb_entry;

struct pmb_entry {
	unsigned long vpn;
	unsigned long ppn;
	unsigned long flags;
	unsigned long size;

	spinlock_t lock;

	/*
	 * 0 .. NR_PMB_ENTRIES for specific entry selection, or
	 * PMB_NO_ENTRY to search for a free one
	 */
	int entry;

	/* Adjacent entry link for contiguous multi-entry mappings */
	struct pmb_entry *link;
};

static void pmb_unmap_entry(struct pmb_entry *);

static DEFINE_RWLOCK(pmb_rwlock);
static struct pmb_entry pmb_entry_list[NR_PMB_ENTRIES];
static DECLARE_BITMAP(pmb_map, NR_PMB_ENTRIES);

static __always_inline unsigned long mk_pmb_entry(unsigned int entry)
{
	return (entry & PMB_E_MASK) << PMB_E_SHIFT;
}

static __always_inline unsigned long mk_pmb_addr(unsigned int entry)
{
	return mk_pmb_entry(entry) | PMB_ADDR;
}

static __always_inline unsigned long mk_pmb_data(unsigned int entry)
{
	return mk_pmb_entry(entry) | PMB_DATA;
}

static int pmb_alloc_entry(void)
{
	int pos;

	pos = find_first_zero_bit(pmb_map, NR_PMB_ENTRIES);
	if (pos >= 0 && pos < NR_PMB_ENTRIES)
		__set_bit(pos, pmb_map);
	else
		pos = -ENOSPC;

	return pos;
}

static struct pmb_entry *pmb_alloc(unsigned long vpn, unsigned long ppn,
				   unsigned long flags, int entry)
{
	struct pmb_entry *pmbe;
	unsigned long irqflags;
	void *ret = NULL;
	int pos;

	write_lock_irqsave(&pmb_rwlock, irqflags);

	if (entry == PMB_NO_ENTRY) {
		pos = pmb_alloc_entry();
		if (unlikely(pos < 0)) {
			ret = ERR_PTR(pos);
			goto out;
		}
	} else {
		if (__test_and_set_bit(entry, pmb_map)) {
			ret = ERR_PTR(-ENOSPC);
			goto out;
		}

		pos = entry;
	}

	write_unlock_irqrestore(&pmb_rwlock, irqflags);

	pmbe = &pmb_entry_list[pos];

	spin_lock_init(&pmbe->lock);

	pmbe->vpn	= vpn;
	pmbe->ppn	= ppn;
	pmbe->flags	= flags;
	pmbe->entry	= pos;
	pmbe->size	= 0;

	return pmbe;

out:
	write_unlock_irqrestore(&pmb_rwlock, irqflags);
	return ret;
}

static void pmb_free(struct pmb_entry *pmbe)
{
	__clear_bit(pmbe->entry, pmb_map);
	pmbe->entry = PMB_NO_ENTRY;
}

/*
 * Ensure that the PMB entries match our cache configuration.
 *
 * When we are in 32-bit address extended mode, CCR.CB becomes
 * invalid, so care must be taken to manually adjust cacheable
 * translations.
 */
static __always_inline unsigned long pmb_cache_flags(void)
{
	unsigned long flags = 0;

#if defined(CONFIG_CACHE_WRITETHROUGH)
	flags |= PMB_C | PMB_WT | PMB_UB;
#elif defined(CONFIG_CACHE_WRITEBACK)
	flags |= PMB_C;
#endif

	return flags;
}

/*
 * Must be run uncached.
 */
static void __set_pmb_entry(struct pmb_entry *pmbe)
{
	jump_to_uncached();

	pmbe->flags &= ~PMB_CACHE_MASK;
	pmbe->flags |= pmb_cache_flags();

	__raw_writel(pmbe->vpn | PMB_V, mk_pmb_addr(pmbe->entry));
	__raw_writel(pmbe->ppn | pmbe->flags | PMB_V, mk_pmb_data(pmbe->entry));

	back_to_cached();
}

static void __clear_pmb_entry(struct pmb_entry *pmbe)
{
	unsigned int entry = pmbe->entry;
	unsigned long addr;

	jump_to_uncached();

	/* Clear V-bit */
	addr = mk_pmb_addr(entry);
	__raw_writel(__raw_readl(addr) & ~PMB_V, addr);

	addr = mk_pmb_data(entry);
	__raw_writel(__raw_readl(addr) & ~PMB_V, addr);

	back_to_cached();
}

static void set_pmb_entry(struct pmb_entry *pmbe)
{
	unsigned long flags;

	spin_lock_irqsave(&pmbe->lock, flags);
	__set_pmb_entry(pmbe);
	spin_unlock_irqrestore(&pmbe->lock, flags);
}

static struct {
	unsigned long size;
	int flag;
} pmb_sizes[] = {
	{ .size	= SZ_512M, .flag = PMB_SZ_512M, },
	{ .size = SZ_128M, .flag = PMB_SZ_128M, },
	{ .size = SZ_64M,  .flag = PMB_SZ_64M,  },
	{ .size = SZ_16M,  .flag = PMB_SZ_16M,  },
};

long pmb_remap(unsigned long vaddr, unsigned long phys,
	       unsigned long size, pgprot_t prot)
{
	struct pmb_entry *pmbp, *pmbe;
	unsigned long wanted;
	int pmb_flags, i;
	long err;
	u64 flags;

	flags = pgprot_val(prot);

	pmb_flags = PMB_WT | PMB_UB;

	/* Convert typical pgprot value to the PMB equivalent */
	if (flags & _PAGE_CACHABLE) {
		pmb_flags |= PMB_C;

		if ((flags & _PAGE_WT) == 0)
			pmb_flags &= ~(PMB_WT | PMB_UB);
	}

	pmbp = NULL;
	wanted = size;

again:
	for (i = 0; i < ARRAY_SIZE(pmb_sizes); i++) {
		unsigned long flags;

		if (size < pmb_sizes[i].size)
			continue;

		pmbe = pmb_alloc(vaddr, phys, pmb_flags | pmb_sizes[i].flag,
				 PMB_NO_ENTRY);
		if (IS_ERR(pmbe)) {
			err = PTR_ERR(pmbe);
			goto out;
		}

		spin_lock_irqsave(&pmbe->lock, flags);

		__set_pmb_entry(pmbe);

		phys	+= pmb_sizes[i].size;
		vaddr	+= pmb_sizes[i].size;
		size	-= pmb_sizes[i].size;

		pmbe->size = pmb_sizes[i].size;

		/*
		 * Link adjacent entries that span multiple PMB entries
		 * for easier tear-down.
		 */
		if (likely(pmbp)) {
			spin_lock(&pmbp->lock);
			pmbp->link = pmbe;
			spin_unlock(&pmbp->lock);
		}

		pmbp = pmbe;

		/*
		 * Instead of trying smaller sizes on every iteration
		 * (even if we succeed in allocating space), try using
		 * pmb_sizes[i].size again.
		 */
		i--;

		spin_unlock_irqrestore(&pmbe->lock, flags);
	}

	if (size >= SZ_16M)
		goto again;

	return wanted - size;

out:
	pmb_unmap_entry(pmbp);

	return err;
}

void pmb_unmap(unsigned long addr)
{
	struct pmb_entry *pmbe = NULL;
	int i;

	read_lock(&pmb_rwlock);

	for (i = 0; i < ARRAY_SIZE(pmb_entry_list); i++) {
		if (test_bit(i, pmb_map)) {
			pmbe = &pmb_entry_list[i];
			if (pmbe->vpn == addr)
				break;
		}
	}

	read_unlock(&pmb_rwlock);

	pmb_unmap_entry(pmbe);
}

static void pmb_unmap_entry(struct pmb_entry *pmbe)
{
	unsigned long flags;

	if (unlikely(!pmbe))
		return;

	write_lock_irqsave(&pmb_rwlock, flags);

	do {
		struct pmb_entry *pmblink = pmbe;

		/*
		 * We may be called before this pmb_entry has been
		 * entered into the PMB table via set_pmb_entry(), but
		 * that's OK because we've allocated a unique slot for
		 * this entry in pmb_alloc() (even if we haven't filled
		 * it yet).
		 *
		 * Therefore, calling __clear_pmb_entry() is safe as no
		 * other mapping can be using that slot.
		 */
		__clear_pmb_entry(pmbe);

		pmbe = pmblink->link;

		pmb_free(pmblink);
	} while (pmbe);

	write_unlock_irqrestore(&pmb_rwlock, flags);
}

static __always_inline unsigned int pmb_ppn_in_range(unsigned long ppn)
{
	return ppn >= __pa(memory_start) && ppn < __pa(memory_end);
}

static int pmb_synchronize_mappings(void)
{
	unsigned int applied = 0;
	struct pmb_entry *pmbp = NULL;
	int i, j;

	pr_info("PMB: boot mappings:\n");

	/*
	 * Run through the initial boot mappings, log the established
	 * ones, and blow away anything that falls outside of the valid
	 * PPN range. Specifically, we only care about existing mappings
	 * that impact the cached/uncached sections.
	 *
	 * Note that touching these can be a bit of a minefield; the boot
	 * loader can establish multi-page mappings with the same caching
	 * attributes, so we need to ensure that we aren't modifying a
	 * mapping that we're presently executing from, or may execute
	 * from in the case of straddling page boundaries.
	 *
	 * In the future we will have to tidy up after the boot loader by
	 * jumping between the cached and uncached mappings and tearing
	 * down alternating mappings while executing from the other.
	 */
	for (i = 0; i < NR_PMB_ENTRIES; i++) {
		unsigned long addr, data;
		unsigned long addr_val, data_val;
		unsigned long ppn, vpn, flags;
		unsigned long irqflags;
		unsigned int size;
		struct pmb_entry *pmbe;

		addr = mk_pmb_addr(i);
		data = mk_pmb_data(i);

		addr_val = __raw_readl(addr);
		data_val = __raw_readl(data);

		/*
		 * Skip over any bogus entries
		 */
		if (!(data_val & PMB_V) || !(addr_val & PMB_V))
			continue;

		ppn = data_val & PMB_PFN_MASK;
		vpn = addr_val & PMB_PFN_MASK;

		/*
		 * Only preserve in-range mappings.
		 */
		if (!pmb_ppn_in_range(ppn)) {
			/*
			 * Invalidate anything out of bounds.
			 */
			__raw_writel(addr_val & ~PMB_V, addr);
			__raw_writel(data_val & ~PMB_V, data);
			continue;
		}

		/*
		 * Update the caching attributes if necessary
		 */
		if (data_val & PMB_C) {
			data_val &= ~PMB_CACHE_MASK;
			data_val |= pmb_cache_flags();
			__raw_writel(data_val, data);
		}

		size = data_val & PMB_SZ_MASK;
		flags = size | (data_val & PMB_CACHE_MASK);

		pmbe = pmb_alloc(vpn, ppn, flags, i);
		if (IS_ERR(pmbe)) {
			WARN_ON_ONCE(1);
			continue;
		}

		spin_lock_irqsave(&pmbe->lock, irqflags);

		for (j = 0; j < ARRAY_SIZE(pmb_sizes); j++)
			if (pmb_sizes[j].flag == size)
				pmbe->size = pmb_sizes[j].size;

		if (pmbp) {
			spin_lock(&pmbp->lock);

			/*
			 * Compare the previous entry against the current one to
			 * see if the entries span a contiguous mapping. If so,
			 * setup the entry links accordingly.
			 */
			if ((pmbe->vpn == (pmbp->vpn + pmbp->size)) &&
			    (pmbe->ppn == (pmbp->ppn + pmbp->size)))
				pmbp->link = pmbe;

			spin_unlock(&pmbp->lock);
		}

		pmbp = pmbe;

		spin_unlock_irqrestore(&pmbe->lock, irqflags);

		pr_info("\t0x%08lx -> 0x%08lx [ %ldMB %scached ]\n",
			vpn >> PAGE_SHIFT, ppn >> PAGE_SHIFT, pmbe->size >> 20,
			(data_val & PMB_C) ? "" : "un");

		applied++;
	}

	return (applied == 0);
}

int pmb_init(void)
{
	int ret;

	jump_to_uncached();

	/*
	 * Sync our software copy of the PMB mappings with those in
	 * hardware. The mappings in the hardware PMB were either set up
	 * by the bootloader or very early on by the kernel.
	 */
	ret = pmb_synchronize_mappings();
	if (unlikely(ret == 0)) {
		back_to_cached();
		return 0;
	}

	__raw_writel(0, PMB_IRMCR);

	/* Flush out the TLB */
	__raw_writel(__raw_readl(MMUCR) | MMUCR_TI, MMUCR);

	back_to_cached();

	return 0;
}

bool __in_29bit_mode(void)
{
        return (__raw_readl(PMB_PASCR) & PASCR_SE) == 0;
}

static int pmb_seq_show(struct seq_file *file, void *iter)
{
	int i;

	seq_printf(file, "V: Valid, C: Cacheable, WT: Write-Through\n"
			 "CB: Copy-Back, B: Buffered, UB: Unbuffered\n");
	seq_printf(file, "ety   vpn  ppn  size   flags\n");

	for (i = 0; i < NR_PMB_ENTRIES; i++) {
		unsigned long addr, data;
		unsigned int size;
		char *sz_str = NULL;

		addr = __raw_readl(mk_pmb_addr(i));
		data = __raw_readl(mk_pmb_data(i));

		size = data & PMB_SZ_MASK;
		sz_str = (size == PMB_SZ_16M)  ? " 16MB":
			 (size == PMB_SZ_64M)  ? " 64MB":
			 (size == PMB_SZ_128M) ? "128MB":
					         "512MB";

		/* 02: V 0x88 0x08 128MB C CB  B */
		seq_printf(file, "%02d: %c 0x%02lx 0x%02lx %s %c %s %s\n",
			   i, ((addr & PMB_V) && (data & PMB_V)) ? 'V' : ' ',
			   (addr >> 24) & 0xff, (data >> 24) & 0xff,
			   sz_str, (data & PMB_C) ? 'C' : ' ',
			   (data & PMB_WT) ? "WT" : "CB",
			   (data & PMB_UB) ? "UB" : " B");
	}

	return 0;
}

static int pmb_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmb_seq_show, NULL);
}

static const struct file_operations pmb_debugfs_fops = {
	.owner		= THIS_MODULE,
	.open		= pmb_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init pmb_debugfs_init(void)
{
	struct dentry *dentry;

	dentry = debugfs_create_file("pmb", S_IFREG | S_IRUGO,
				     sh_debugfs_root, NULL, &pmb_debugfs_fops);
	if (!dentry)
		return -ENOMEM;
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	return 0;
}
postcore_initcall(pmb_debugfs_init);

#ifdef CONFIG_PM
static int pmb_sysdev_suspend(struct sys_device *dev, pm_message_t state)
{
	static pm_message_t prev_state;
	int i;

	/* Restore the PMB after a resume from hibernation */
	if (state.event == PM_EVENT_ON &&
	    prev_state.event == PM_EVENT_FREEZE) {
		struct pmb_entry *pmbe;

		read_lock(&pmb_rwlock);

		for (i = 0; i < ARRAY_SIZE(pmb_entry_list); i++) {
			if (test_bit(i, pmb_map)) {
				pmbe = &pmb_entry_list[i];
				set_pmb_entry(pmbe);
			}
		}

		read_unlock(&pmb_rwlock);
	}

	prev_state = state;

	return 0;
}

static int pmb_sysdev_resume(struct sys_device *dev)
{
	return pmb_sysdev_suspend(dev, PMSG_ON);
}

static struct sysdev_driver pmb_sysdev_driver = {
	.suspend = pmb_sysdev_suspend,
	.resume = pmb_sysdev_resume,
};

static int __init pmb_sysdev_init(void)
{
	return sysdev_driver_register(&cpu_sysdev_class, &pmb_sysdev_driver);
}
subsys_initcall(pmb_sysdev_init);
#endif
