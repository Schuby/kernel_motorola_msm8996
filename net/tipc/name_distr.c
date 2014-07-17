/*
 * net/tipc/name_distr.c: TIPC name distribution code
 *
 * Copyright (c) 2000-2006, Ericsson AB
 * Copyright (c) 2005, 2010-2011, Wind River Systems
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "core.h"
#include "link.h"
#include "name_distr.h"

/**
 * struct publ_list - list of publications made by this node
 * @list: circular list of publications
 * @list_size: number of entries in list
 */
struct publ_list {
	struct list_head list;
	u32 size;
};

static struct publ_list publ_zone = {
	.list = LIST_HEAD_INIT(publ_zone.list),
	.size = 0,
};

static struct publ_list publ_cluster = {
	.list = LIST_HEAD_INIT(publ_cluster.list),
	.size = 0,
};

static struct publ_list publ_node = {
	.list = LIST_HEAD_INIT(publ_node.list),
	.size = 0,
};

static struct publ_list *publ_lists[] = {
	NULL,
	&publ_zone,	/* publ_lists[TIPC_ZONE_SCOPE]		*/
	&publ_cluster,	/* publ_lists[TIPC_CLUSTER_SCOPE]	*/
	&publ_node	/* publ_lists[TIPC_NODE_SCOPE]		*/
};


/**
 * publ_to_item - add publication info to a publication message
 */
static void publ_to_item(struct distr_item *i, struct publication *p)
{
	i->type = htonl(p->type);
	i->lower = htonl(p->lower);
	i->upper = htonl(p->upper);
	i->ref = htonl(p->ref);
	i->key = htonl(p->key);
}

/**
 * named_prepare_buf - allocate & initialize a publication message
 */
static struct sk_buff *named_prepare_buf(u32 type, u32 size, u32 dest)
{
	struct sk_buff *buf = tipc_buf_acquire(INT_H_SIZE + size);
	struct tipc_msg *msg;

	if (buf != NULL) {
		msg = buf_msg(buf);
		tipc_msg_init(msg, NAME_DISTRIBUTOR, type, INT_H_SIZE, dest);
		msg_set_size(msg, INT_H_SIZE + size);
	}
	return buf;
}

void named_cluster_distribute(struct sk_buff *buf)
{
	struct sk_buff *obuf;
	struct tipc_node *node;
	u32 dnode;

	rcu_read_lock();
	list_for_each_entry_rcu(node, &tipc_node_list, list) {
		dnode = node->addr;
		if (in_own_node(dnode))
			continue;
		if (!tipc_node_active_links(node))
			continue;
		obuf = skb_copy(buf, GFP_ATOMIC);
		if (!obuf)
			break;
		msg_set_destnode(buf_msg(obuf), dnode);
		tipc_link_xmit2(obuf, dnode, dnode);
	}
	rcu_read_unlock();

	kfree_skb(buf);
}

/**
 * tipc_named_publish - tell other nodes about a new publication by this node
 */
struct sk_buff *tipc_named_publish(struct publication *publ)
{
	struct sk_buff *buf;
	struct distr_item *item;

	list_add_tail(&publ->local_list, &publ_lists[publ->scope]->list);
	publ_lists[publ->scope]->size++;

	if (publ->scope == TIPC_NODE_SCOPE)
		return NULL;

	buf = named_prepare_buf(PUBLICATION, ITEM_SIZE, 0);
	if (!buf) {
		pr_warn("Publication distribution failure\n");
		return NULL;
	}

	item = (struct distr_item *)msg_data(buf_msg(buf));
	publ_to_item(item, publ);
	return buf;
}

/**
 * tipc_named_withdraw - tell other nodes about a withdrawn publication by this node
 */
struct sk_buff *tipc_named_withdraw(struct publication *publ)
{
	struct sk_buff *buf;
	struct distr_item *item;

	list_del(&publ->local_list);
	publ_lists[publ->scope]->size--;

	if (publ->scope == TIPC_NODE_SCOPE)
		return NULL;

	buf = named_prepare_buf(WITHDRAWAL, ITEM_SIZE, 0);
	if (!buf) {
		pr_warn("Withdrawal distribution failure\n");
		return NULL;
	}

	item = (struct distr_item *)msg_data(buf_msg(buf));
	publ_to_item(item, publ);
	return buf;
}

/**
 * named_distribute - prepare name info for bulk distribution to another node
 * @msg_list: list of messages (buffers) to be returned from this function
 * @dnode: node to be updated
 * @pls: linked list of publication items to be packed into buffer chain
 */
static void named_distribute(struct list_head *msg_list, u32 dnode,
			     struct publ_list *pls)
{
	struct publication *publ;
	struct sk_buff *buf = NULL;
	struct distr_item *item = NULL;
	uint dsz = pls->size * ITEM_SIZE;
	uint msg_dsz = (tipc_node_get_mtu(dnode, 0) / ITEM_SIZE) * ITEM_SIZE;
	uint rem = dsz;
	uint msg_rem = 0;

	list_for_each_entry(publ, &pls->list, local_list) {
		/* Prepare next buffer: */
		if (!buf) {
			msg_rem = min_t(uint, rem, msg_dsz);
			rem -= msg_rem;
			buf = named_prepare_buf(PUBLICATION, msg_rem, dnode);
			if (!buf) {
				pr_warn("Bulk publication failure\n");
				return;
			}
			item = (struct distr_item *)msg_data(buf_msg(buf));
		}

		/* Pack publication into message: */
		publ_to_item(item, publ);
		item++;
		msg_rem -= ITEM_SIZE;

		/* Append full buffer to list: */
		if (!msg_rem) {
			list_add_tail((struct list_head *)buf, msg_list);
			buf = NULL;
		}
	}
}

/**
 * tipc_named_node_up - tell specified node about all publications by this node
 */
void tipc_named_node_up(u32 dnode)
{
	LIST_HEAD(msg_list);
	struct sk_buff *buf_chain;

	read_lock_bh(&tipc_nametbl_lock);
	named_distribute(&msg_list, dnode, &publ_cluster);
	named_distribute(&msg_list, dnode, &publ_zone);
	read_unlock_bh(&tipc_nametbl_lock);

	/* Convert circular list to linear list and send: */
	buf_chain = (struct sk_buff *)msg_list.next;
	((struct sk_buff *)msg_list.prev)->next = NULL;
	tipc_link_xmit2(buf_chain, dnode, dnode);
}

/**
 * named_purge_publ - remove publication associated with a failed node
 *
 * Invoked for each publication issued by a newly failed node.
 * Removes publication structure from name table & deletes it.
 */
static void named_purge_publ(struct publication *publ)
{
	struct publication *p;

	write_lock_bh(&tipc_nametbl_lock);
	p = tipc_nametbl_remove_publ(publ->type, publ->lower,
				     publ->node, publ->ref, publ->key);
	if (p)
		tipc_nodesub_unsubscribe(&p->subscr);
	write_unlock_bh(&tipc_nametbl_lock);

	if (p != publ) {
		pr_err("Unable to remove publication from failed node\n"
		       " (type=%u, lower=%u, node=0x%x, ref=%u, key=%u)\n",
		       publ->type, publ->lower, publ->node, publ->ref,
		       publ->key);
	}

	kfree(p);
}

/**
 * tipc_named_rcv - process name table update message sent by another node
 */
void tipc_named_rcv(struct sk_buff *buf)
{
	struct publication *publ;
	struct tipc_msg *msg = buf_msg(buf);
	struct distr_item *item = (struct distr_item *)msg_data(msg);
	u32 count = msg_data_sz(msg) / ITEM_SIZE;

	write_lock_bh(&tipc_nametbl_lock);
	while (count--) {
		if (msg_type(msg) == PUBLICATION) {
			publ = tipc_nametbl_insert_publ(ntohl(item->type),
							ntohl(item->lower),
							ntohl(item->upper),
							TIPC_CLUSTER_SCOPE,
							msg_orignode(msg),
							ntohl(item->ref),
							ntohl(item->key));
			if (publ) {
				tipc_nodesub_subscribe(&publ->subscr,
						       msg_orignode(msg),
						       publ,
						       (net_ev_handler)
						       named_purge_publ);
			}
		} else if (msg_type(msg) == WITHDRAWAL) {
			publ = tipc_nametbl_remove_publ(ntohl(item->type),
							ntohl(item->lower),
							msg_orignode(msg),
							ntohl(item->ref),
							ntohl(item->key));

			if (publ) {
				tipc_nodesub_unsubscribe(&publ->subscr);
				kfree(publ);
			} else {
				pr_err("Unable to remove publication by node 0x%x\n"
				       " (type=%u, lower=%u, ref=%u, key=%u)\n",
				       msg_orignode(msg), ntohl(item->type),
				       ntohl(item->lower), ntohl(item->ref),
				       ntohl(item->key));
			}
		} else {
			pr_warn("Unrecognized name table message received\n");
		}
		item++;
	}
	write_unlock_bh(&tipc_nametbl_lock);
	kfree_skb(buf);
}

/**
 * tipc_named_reinit - re-initialize local publications
 *
 * This routine is called whenever TIPC networking is enabled.
 * All name table entries published by this node are updated to reflect
 * the node's new network address.
 */
void tipc_named_reinit(void)
{
	struct publication *publ;
	int scope;

	write_lock_bh(&tipc_nametbl_lock);

	for (scope = TIPC_ZONE_SCOPE; scope <= TIPC_NODE_SCOPE; scope++)
		list_for_each_entry(publ, &publ_lists[scope]->list, local_list)
			publ->node = tipc_own_addr;

	write_unlock_bh(&tipc_nametbl_lock);
}
