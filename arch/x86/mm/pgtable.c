#include <linux/mm.h>
#include <asm/pgalloc.h>
#include <asm/tlb.h>

pte_t *pte_alloc_one_kernel(struct mm_struct *mm, unsigned long address)
{
	return (pte_t *)__get_free_page(GFP_KERNEL|__GFP_REPEAT|__GFP_ZERO);
}

pgtable_t pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	struct page *pte;

#ifdef CONFIG_HIGHPTE
	pte = alloc_pages(GFP_KERNEL|__GFP_HIGHMEM|__GFP_REPEAT|__GFP_ZERO, 0);
#else
	pte = alloc_pages(GFP_KERNEL|__GFP_REPEAT|__GFP_ZERO, 0);
#endif
	if (pte)
		pgtable_page_ctor(pte);
	return pte;
}

void __pte_free_tlb(struct mmu_gather *tlb, struct page *pte)
{
	pgtable_page_dtor(pte);
	paravirt_release_pte(page_to_pfn(pte));
	tlb_remove_page(tlb, pte);
}

#if PAGETABLE_LEVELS > 2
void __pmd_free_tlb(struct mmu_gather *tlb, pmd_t *pmd)
{
	paravirt_release_pmd(__pa(pmd) >> PAGE_SHIFT);
	tlb_remove_page(tlb, virt_to_page(pmd));
}

#if PAGETABLE_LEVELS > 3
void __pud_free_tlb(struct mmu_gather *tlb, pud_t *pud)
{
	paravirt_release_pud(__pa(pud) >> PAGE_SHIFT);
	tlb_remove_page(tlb, virt_to_page(pud));
}
#endif	/* PAGETABLE_LEVELS > 3 */
#endif	/* PAGETABLE_LEVELS > 2 */

static inline void pgd_list_add(pgd_t *pgd)
{
	struct page *page = virt_to_page(pgd);

	list_add(&page->lru, &pgd_list);
}

static inline void pgd_list_del(pgd_t *pgd)
{
	struct page *page = virt_to_page(pgd);

	list_del(&page->lru);
}

#ifdef CONFIG_X86_64
pgd_t *pgd_alloc(struct mm_struct *mm)
{
	unsigned boundary;
	pgd_t *pgd = (pgd_t *)__get_free_page(GFP_KERNEL|__GFP_REPEAT);
	unsigned long flags;
	if (!pgd)
		return NULL;
	spin_lock_irqsave(&pgd_lock, flags);
	pgd_list_add(pgd);
	spin_unlock_irqrestore(&pgd_lock, flags);
	/*
	 * Copy kernel pointers in from init.
	 * Could keep a freelist or slab cache of those because the kernel
	 * part never changes.
	 */
	boundary = pgd_index(__PAGE_OFFSET);
	memset(pgd, 0, boundary * sizeof(pgd_t));
	memcpy(pgd + boundary,
	       init_level4_pgt + boundary,
	       (PTRS_PER_PGD - boundary) * sizeof(pgd_t));
	return pgd;
}

void pgd_free(struct mm_struct *mm, pgd_t *pgd)
{
	unsigned long flags;
	BUG_ON((unsigned long)pgd & (PAGE_SIZE-1));
	spin_lock_irqsave(&pgd_lock, flags);
	pgd_list_del(pgd);
	spin_unlock_irqrestore(&pgd_lock, flags);
	free_page((unsigned long)pgd);
}
#else
/*
 * List of all pgd's needed for non-PAE so it can invalidate entries
 * in both cached and uncached pgd's; not needed for PAE since the
 * kernel pmd is shared. If PAE were not to share the pmd a similar
 * tactic would be needed. This is essentially codepath-based locking
 * against pageattr.c; it is the unique case in which a valid change
 * of kernel pagetables can't be lazily synchronized by vmalloc faults.
 * vmalloc faults work because attached pagetables are never freed.
 * -- wli
 */
#define UNSHARED_PTRS_PER_PGD				\
	(SHARED_KERNEL_PMD ? USER_PTRS_PER_PGD : PTRS_PER_PGD)

static void pgd_ctor(void *p)
{
	pgd_t *pgd = p;
	unsigned long flags;

	/* Clear usermode parts of PGD */
	memset(pgd, 0, USER_PTRS_PER_PGD*sizeof(pgd_t));

	spin_lock_irqsave(&pgd_lock, flags);

	/* If the pgd points to a shared pagetable level (either the
	   ptes in non-PAE, or shared PMD in PAE), then just copy the
	   references from swapper_pg_dir. */
	if (PAGETABLE_LEVELS == 2 ||
	    (PAGETABLE_LEVELS == 3 && SHARED_KERNEL_PMD)) {
		clone_pgd_range(pgd + USER_PTRS_PER_PGD,
				swapper_pg_dir + USER_PTRS_PER_PGD,
				KERNEL_PGD_PTRS);
		paravirt_alloc_pmd_clone(__pa(pgd) >> PAGE_SHIFT,
					 __pa(swapper_pg_dir) >> PAGE_SHIFT,
					 USER_PTRS_PER_PGD,
					 KERNEL_PGD_PTRS);
	}

	/* list required to sync kernel mapping updates */
	if (!SHARED_KERNEL_PMD)
		pgd_list_add(pgd);

	spin_unlock_irqrestore(&pgd_lock, flags);
}

static void pgd_dtor(void *pgd)
{
	unsigned long flags; /* can be called from interrupt context */

	if (SHARED_KERNEL_PMD)
		return;

	spin_lock_irqsave(&pgd_lock, flags);
	pgd_list_del(pgd);
	spin_unlock_irqrestore(&pgd_lock, flags);
}

#ifdef CONFIG_X86_PAE
/*
 * Mop up any pmd pages which may still be attached to the pgd.
 * Normally they will be freed by munmap/exit_mmap, but any pmd we
 * preallocate which never got a corresponding vma will need to be
 * freed manually.
 */
static void pgd_mop_up_pmds(struct mm_struct *mm, pgd_t *pgdp)
{
	int i;

	for(i = 0; i < UNSHARED_PTRS_PER_PGD; i++) {
		pgd_t pgd = pgdp[i];

		if (pgd_val(pgd) != 0) {
			pmd_t *pmd = (pmd_t *)pgd_page_vaddr(pgd);

			pgdp[i] = native_make_pgd(0);

			paravirt_release_pmd(pgd_val(pgd) >> PAGE_SHIFT);
			pmd_free(mm, pmd);
		}
	}
}

/*
 * In PAE mode, we need to do a cr3 reload (=tlb flush) when
 * updating the top-level pagetable entries to guarantee the
 * processor notices the update.  Since this is expensive, and
 * all 4 top-level entries are used almost immediately in a
 * new process's life, we just pre-populate them here.
 *
 * Also, if we're in a paravirt environment where the kernel pmd is
 * not shared between pagetables (!SHARED_KERNEL_PMDS), we allocate
 * and initialize the kernel pmds here.
 */
static int pgd_prepopulate_pmd(struct mm_struct *mm, pgd_t *pgd)
{
	pud_t *pud;
	unsigned long addr;
	int i;

	pud = pud_offset(pgd, 0);
 	for (addr = i = 0; i < UNSHARED_PTRS_PER_PGD;
	     i++, pud++, addr += PUD_SIZE) {
		pmd_t *pmd = pmd_alloc_one(mm, addr);

		if (!pmd) {
			pgd_mop_up_pmds(mm, pgd);
			return 0;
		}

		if (i >= USER_PTRS_PER_PGD)
			memcpy(pmd, (pmd_t *)pgd_page_vaddr(swapper_pg_dir[i]),
			       sizeof(pmd_t) * PTRS_PER_PMD);

		pud_populate(mm, pud, pmd);
	}

	return 1;
}

void pud_populate(struct mm_struct *mm, pud_t *pudp, pmd_t *pmd)
{
	paravirt_alloc_pmd(mm, __pa(pmd) >> PAGE_SHIFT);

	/* Note: almost everything apart from _PAGE_PRESENT is
	   reserved at the pmd (PDPT) level. */
	set_pud(pudp, __pud(__pa(pmd) | _PAGE_PRESENT));

	/*
	 * According to Intel App note "TLBs, Paging-Structure Caches,
	 * and Their Invalidation", April 2007, document 317080-001,
	 * section 8.1: in PAE mode we explicitly have to flush the
	 * TLB via cr3 if the top-level pgd is changed...
	 */
	if (mm == current->active_mm)
		write_cr3(read_cr3());
}
#else  /* !CONFIG_X86_PAE */
/* No need to prepopulate any pagetable entries in non-PAE modes. */
static int pgd_prepopulate_pmd(struct mm_struct *mm, pgd_t *pgd)
{
	return 1;
}

static void pgd_mop_up_pmds(struct mm_struct *mm, pgd_t *pgd)
{
}
#endif	/* CONFIG_X86_PAE */

pgd_t *pgd_alloc(struct mm_struct *mm)
{
	pgd_t *pgd = (pgd_t *)__get_free_page(GFP_KERNEL | __GFP_ZERO);

	/* so that alloc_pmd can use it */
	mm->pgd = pgd;
	if (pgd)
		pgd_ctor(pgd);

	if (pgd && !pgd_prepopulate_pmd(mm, pgd)) {
		pgd_dtor(pgd);
		free_page((unsigned long)pgd);
		pgd = NULL;
	}

	return pgd;
}

void pgd_free(struct mm_struct *mm, pgd_t *pgd)
{
	pgd_mop_up_pmds(mm, pgd);
	pgd_dtor(pgd);
	free_page((unsigned long)pgd);
}
#endif
