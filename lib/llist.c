/*
 * Lock-less NULL terminated single linked list
 *
 * The basic atomic operation of this list is cmpxchg on long.  On
 * architectures that don't have NMI-safe cmpxchg implementation, the
 * list can NOT be used in NMI handler.  So code uses the list in NMI
 * handler should depend on CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG.
 *
 * Copyright 2010,2011 Intel Corp.
 *   Author: Huang Ying <ying.huang@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/llist.h>

#include <asm/system.h>

/**
 * llist_add - add a new entry
 * @new:	new entry to be added
 * @head:	the head for your lock-less list
 */
void llist_add(struct llist_node *new, struct llist_head *head)
{
	struct llist_node *entry, *old_entry;

#ifndef CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG
	BUG_ON(in_nmi());
#endif

	entry = head->first;
	do {
		old_entry = entry;
		new->next = entry;
		cpu_relax();
	} while ((entry = cmpxchg(&head->first, old_entry, new)) != old_entry);
}
EXPORT_SYMBOL_GPL(llist_add);

/**
 * llist_add_batch - add several linked entries in batch
 * @new_first:	first entry in batch to be added
 * @new_last:	last entry in batch to be added
 * @head:	the head for your lock-less list
 */
void llist_add_batch(struct llist_node *new_first, struct llist_node *new_last,
		     struct llist_head *head)
{
	struct llist_node *entry, *old_entry;

#ifndef CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG
	BUG_ON(in_nmi());
#endif

	entry = head->first;
	do {
		old_entry = entry;
		new_last->next = entry;
		cpu_relax();
	} while ((entry = cmpxchg(&head->first, old_entry, new_first)) != old_entry);
}
EXPORT_SYMBOL_GPL(llist_add_batch);

/**
 * llist_del_first - delete the first entry of lock-less list
 * @head:	the head for your lock-less list
 *
 * If list is empty, return NULL, otherwise, return the first entry
 * deleted, this is the newest added one.
 *
 * Only one llist_del_first user can be used simultaneously with
 * multiple llist_add users without lock.  Because otherwise
 * llist_del_first, llist_add, llist_add (or llist_del_all, llist_add,
 * llist_add) sequence in another user may change @head->first->next,
 * but keep @head->first.  If multiple consumers are needed, please
 * use llist_del_all or use lock between consumers.
 */
struct llist_node *llist_del_first(struct llist_head *head)
{
	struct llist_node *entry, *old_entry, *next;

#ifndef CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG
	BUG_ON(in_nmi());
#endif

	entry = head->first;
	do {
		if (entry == NULL)
			return NULL;
		old_entry = entry;
		next = entry->next;
		cpu_relax();
	} while ((entry = cmpxchg(&head->first, old_entry, next)) != old_entry);

	return entry;
}
EXPORT_SYMBOL_GPL(llist_del_first);

/**
 * llist_del_all - delete all entries from lock-less list
 * @head:	the head of lock-less list to delete all entries
 *
 * If list is empty, return NULL, otherwise, delete all entries and
 * return the pointer to the first entry.  The order of entries
 * deleted is from the newest to the oldest added one.
 */
struct llist_node *llist_del_all(struct llist_head *head)
{
#ifndef CONFIG_ARCH_HAVE_NMI_SAFE_CMPXCHG
	BUG_ON(in_nmi());
#endif

	return xchg(&head->first, NULL);
}
EXPORT_SYMBOL_GPL(llist_del_all);
