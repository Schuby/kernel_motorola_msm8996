#include <linux/suspend.h>
#include <linux/utsname.h>

/* With SUSPEND_CONSOLE defined suspend looks *really* cool, but
   we probably do not take enough locks for switching consoles, etc,
   so bad things might happen.
*/
#if defined(CONFIG_VT) && defined(CONFIG_VT_CONSOLE)
#define SUSPEND_CONSOLE	(MAX_NR_CONSOLES-1)
#endif

#define MAX_PBES	((PAGE_SIZE - sizeof(struct new_utsname) \
			- 4 - 3*sizeof(unsigned long) - sizeof(int) \
			- sizeof(void *)) / sizeof(swp_entry_t))

struct swsusp_info {
	struct new_utsname	uts;
	u32			version_code;
	unsigned long		num_physpages;
	int			cpus;
	unsigned long		image_pages;
	unsigned long		pagedir_pages;
	suspend_pagedir_t	* suspend_pagedir;
	swp_entry_t		pagedir[MAX_PBES];
} __attribute__((aligned(PAGE_SIZE)));



#ifdef CONFIG_SOFTWARE_SUSPEND
extern int pm_suspend_disk(void);

#else
static inline int pm_suspend_disk(void)
{
	return -EPERM;
}
#endif
extern struct semaphore pm_sem;
#define power_attr(_name) \
static struct subsys_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

extern struct subsystem power_subsys;

extern int freeze_processes(void);
extern void thaw_processes(void);

extern int pm_prepare_console(void);
extern void pm_restore_console(void);


/* References to section boundaries */
extern const void __nosave_begin, __nosave_end;

extern unsigned int nr_copy_pages;
extern suspend_pagedir_t *pagedir_nosave;
extern suspend_pagedir_t *pagedir_save;

extern asmlinkage int swsusp_arch_suspend(void);
extern asmlinkage int swsusp_arch_resume(void);

extern int restore_highmem(void);
extern void free_pagedir(struct pbe *pblist);
extern struct pbe * alloc_pagedir(unsigned nr_pages);
extern void create_pbe_list(struct pbe *pblist, unsigned nr_pages);
extern int enough_swap(void);
