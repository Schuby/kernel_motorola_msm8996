/*
 * Generic VM initialization for x86-64 NUMA setups.
 * Copyright 2002,2003 Andi Kleen, SuSE Labs.
 */
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/memblock.h>
#include <linux/mmzone.h>
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/nodemask.h>
#include <linux/sched.h>
#include <linux/acpi.h>

#include <asm/e820.h>
#include <asm/proto.h>
#include <asm/dma.h>
#include <asm/numa.h>
#include <asm/acpi.h>
#include <asm/amd_nb.h>

struct pglist_data *node_data[MAX_NUMNODES] __read_mostly;
EXPORT_SYMBOL(node_data);

nodemask_t cpu_nodes_parsed __initdata;
nodemask_t mem_nodes_parsed __initdata;

struct memnode memnode;

static unsigned long __initdata nodemap_addr;
static unsigned long __initdata nodemap_size;

static int num_node_memblks __initdata;
static struct bootnode node_memblk_range[NR_NODE_MEMBLKS] __initdata;
static int memblk_nodeid[NR_NODE_MEMBLKS] __initdata;

struct bootnode numa_nodes[MAX_NUMNODES] __initdata;

/*
 * Given a shift value, try to populate memnodemap[]
 * Returns :
 * 1 if OK
 * 0 if memnodmap[] too small (of shift too small)
 * -1 if node overlap or lost ram (shift too big)
 */
static int __init populate_memnodemap(const struct bootnode *nodes,
				      int numnodes, int shift, int *nodeids)
{
	unsigned long addr, end;
	int i, res = -1;

	memset(memnodemap, 0xff, sizeof(s16)*memnodemapsize);
	for (i = 0; i < numnodes; i++) {
		addr = nodes[i].start;
		end = nodes[i].end;
		if (addr >= end)
			continue;
		if ((end >> shift) >= memnodemapsize)
			return 0;
		do {
			if (memnodemap[addr >> shift] != NUMA_NO_NODE)
				return -1;

			if (!nodeids)
				memnodemap[addr >> shift] = i;
			else
				memnodemap[addr >> shift] = nodeids[i];

			addr += (1UL << shift);
		} while (addr < end);
		res = 1;
	}
	return res;
}

static int __init allocate_cachealigned_memnodemap(void)
{
	unsigned long addr;

	memnodemap = memnode.embedded_map;
	if (memnodemapsize <= ARRAY_SIZE(memnode.embedded_map))
		return 0;

	addr = 0x8000;
	nodemap_size = roundup(sizeof(s16) * memnodemapsize, L1_CACHE_BYTES);
	nodemap_addr = memblock_find_in_range(addr, get_max_mapped(),
				      nodemap_size, L1_CACHE_BYTES);
	if (nodemap_addr == MEMBLOCK_ERROR) {
		printk(KERN_ERR
		       "NUMA: Unable to allocate Memory to Node hash map\n");
		nodemap_addr = nodemap_size = 0;
		return -1;
	}
	memnodemap = phys_to_virt(nodemap_addr);
	memblock_x86_reserve_range(nodemap_addr, nodemap_addr + nodemap_size, "MEMNODEMAP");

	printk(KERN_DEBUG "NUMA: Allocated memnodemap from %lx - %lx\n",
	       nodemap_addr, nodemap_addr + nodemap_size);
	return 0;
}

/*
 * The LSB of all start and end addresses in the node map is the value of the
 * maximum possible shift.
 */
static int __init extract_lsb_from_nodes(const struct bootnode *nodes,
					 int numnodes)
{
	int i, nodes_used = 0;
	unsigned long start, end;
	unsigned long bitfield = 0, memtop = 0;

	for (i = 0; i < numnodes; i++) {
		start = nodes[i].start;
		end = nodes[i].end;
		if (start >= end)
			continue;
		bitfield |= start;
		nodes_used++;
		if (end > memtop)
			memtop = end;
	}
	if (nodes_used <= 1)
		i = 63;
	else
		i = find_first_bit(&bitfield, sizeof(unsigned long)*8);
	memnodemapsize = (memtop >> i)+1;
	return i;
}

static int __init compute_hash_shift(struct bootnode *nodes, int numnodes,
				     int *nodeids)
{
	int shift;

	shift = extract_lsb_from_nodes(nodes, numnodes);
	if (allocate_cachealigned_memnodemap())
		return -1;
	printk(KERN_DEBUG "NUMA: Using %d for the hash shift.\n",
		shift);

	if (populate_memnodemap(nodes, numnodes, shift, nodeids) != 1) {
		printk(KERN_INFO "Your memory is not aligned you need to "
		       "rebuild your kernel with a bigger NODEMAPSIZE "
		       "shift=%d\n", shift);
		return -1;
	}
	return shift;
}

int __meminit  __early_pfn_to_nid(unsigned long pfn)
{
	return phys_to_nid(pfn << PAGE_SHIFT);
}

static void * __init early_node_mem(int nodeid, unsigned long start,
				    unsigned long end, unsigned long size,
				    unsigned long align)
{
	unsigned long mem;

	/*
	 * put it on high as possible
	 * something will go with NODE_DATA
	 */
	if (start < (MAX_DMA_PFN<<PAGE_SHIFT))
		start = MAX_DMA_PFN<<PAGE_SHIFT;
	if (start < (MAX_DMA32_PFN<<PAGE_SHIFT) &&
	    end > (MAX_DMA32_PFN<<PAGE_SHIFT))
		start = MAX_DMA32_PFN<<PAGE_SHIFT;
	mem = memblock_x86_find_in_range_node(nodeid, start, end, size, align);
	if (mem != MEMBLOCK_ERROR)
		return __va(mem);

	/* extend the search scope */
	end = max_pfn_mapped << PAGE_SHIFT;
	start = MAX_DMA_PFN << PAGE_SHIFT;
	mem = memblock_find_in_range(start, end, size, align);
	if (mem != MEMBLOCK_ERROR)
		return __va(mem);

	printk(KERN_ERR "Cannot find %lu bytes in node %d\n",
		       size, nodeid);

	return NULL;
}

static __init int conflicting_memblks(unsigned long start, unsigned long end)
{
	int i;
	for (i = 0; i < num_node_memblks; i++) {
		struct bootnode *nd = &node_memblk_range[i];
		if (nd->start == nd->end)
			continue;
		if (nd->end > start && nd->start < end)
			return memblk_nodeid[i];
		if (nd->end == end && nd->start == start)
			return memblk_nodeid[i];
	}
	return -1;
}

int __init numa_add_memblk(int nid, u64 start, u64 end)
{
	int i;

	i = conflicting_memblks(start, end);
	if (i == nid) {
		printk(KERN_WARNING "NUMA: Warning: node %d (%Lx-%Lx) overlaps with itself (%Lx-%Lx)\n",
		       nid, start, end, numa_nodes[i].start, numa_nodes[i].end);
	} else if (i >= 0) {
		printk(KERN_ERR "NUMA: node %d (%Lx-%Lx) overlaps with node %d (%Lx-%Lx)\n",
		       nid, start, end, i,
		       numa_nodes[i].start, numa_nodes[i].end);
		return -EINVAL;
	}

	node_memblk_range[num_node_memblks].start = start;
	node_memblk_range[num_node_memblks].end = end;
	memblk_nodeid[num_node_memblks] = nid;
	num_node_memblks++;
	return 0;
}

static __init void cutoff_node(int i, unsigned long start, unsigned long end)
{
	struct bootnode *nd = &numa_nodes[i];

	if (nd->start < start) {
		nd->start = start;
		if (nd->end < nd->start)
			nd->start = nd->end;
	}
	if (nd->end > end) {
		nd->end = end;
		if (nd->start > nd->end)
			nd->start = nd->end;
	}
}

/* Initialize bootmem allocator for a node */
void __init
setup_node_bootmem(int nodeid, unsigned long start, unsigned long end)
{
	unsigned long start_pfn, last_pfn, nodedata_phys;
	const int pgdat_size = roundup(sizeof(pg_data_t), PAGE_SIZE);
	int nid;

	if (!end)
		return;

	/*
	 * Don't confuse VM with a node that doesn't have the
	 * minimum amount of memory:
	 */
	if (end && (end - start) < NODE_MIN_SIZE)
		return;

	start = roundup(start, ZONE_ALIGN);

	printk(KERN_INFO "Initmem setup node %d %016lx-%016lx\n", nodeid,
	       start, end);

	start_pfn = start >> PAGE_SHIFT;
	last_pfn = end >> PAGE_SHIFT;

	node_data[nodeid] = early_node_mem(nodeid, start, end, pgdat_size,
					   SMP_CACHE_BYTES);
	if (node_data[nodeid] == NULL)
		return;
	nodedata_phys = __pa(node_data[nodeid]);
	memblock_x86_reserve_range(nodedata_phys, nodedata_phys + pgdat_size, "NODE_DATA");
	printk(KERN_INFO "  NODE_DATA [%016lx - %016lx]\n", nodedata_phys,
		nodedata_phys + pgdat_size - 1);
	nid = phys_to_nid(nodedata_phys);
	if (nid != nodeid)
		printk(KERN_INFO "    NODE_DATA(%d) on node %d\n", nodeid, nid);

	memset(NODE_DATA(nodeid), 0, sizeof(pg_data_t));
	NODE_DATA(nodeid)->node_id = nodeid;
	NODE_DATA(nodeid)->node_start_pfn = start_pfn;
	NODE_DATA(nodeid)->node_spanned_pages = last_pfn - start_pfn;

	node_set_online(nodeid);
}

/*
 * Sanity check to catch more bad NUMA configurations (they are amazingly
 * common).  Make sure the nodes cover all memory.
 */
static int __init nodes_cover_memory(const struct bootnode *nodes)
{
	unsigned long numaram, e820ram;
	int i;

	numaram = 0;
	for_each_node_mask(i, mem_nodes_parsed) {
		unsigned long s = nodes[i].start >> PAGE_SHIFT;
		unsigned long e = nodes[i].end >> PAGE_SHIFT;
		numaram += e - s;
		numaram -= __absent_pages_in_range(i, s, e);
		if ((long)numaram < 0)
			numaram = 0;
	}

	e820ram = max_pfn -
		(memblock_x86_hole_size(0, max_pfn<<PAGE_SHIFT) >> PAGE_SHIFT);
	/* We seem to lose 3 pages somewhere. Allow 1M of slack. */
	if ((long)(e820ram - numaram) >= (1<<(20 - PAGE_SHIFT))) {
		printk(KERN_ERR "NUMA: nodes only cover %luMB of your %luMB e820 RAM. Not used.\n",
			(numaram << PAGE_SHIFT) >> 20,
			(e820ram << PAGE_SHIFT) >> 20);
		return 0;
	}
	return 1;
}

static int __init numa_register_memblks(void)
{
	int i;

	/*
	 * Join together blocks on the same node, holes between
	 * which don't overlap with memory on other nodes.
	 */
	for (i = 0; i < num_node_memblks; ++i) {
		int j, k;

		for (j = i + 1; j < num_node_memblks; ++j) {
			unsigned long start, end;

			if (memblk_nodeid[i] != memblk_nodeid[j])
				continue;
			start = min(node_memblk_range[i].end,
			            node_memblk_range[j].end);
			end = max(node_memblk_range[i].start,
			          node_memblk_range[j].start);
			for (k = 0; k < num_node_memblks; ++k) {
				if (memblk_nodeid[i] == memblk_nodeid[k])
					continue;
				if (start < node_memblk_range[k].end &&
				    end > node_memblk_range[k].start)
					break;
			}
			if (k < num_node_memblks)
				continue;
			start = min(node_memblk_range[i].start,
			            node_memblk_range[j].start);
			end = max(node_memblk_range[i].end,
			          node_memblk_range[j].end);
			printk(KERN_INFO "NUMA: Node %d [%Lx,%Lx) + [%Lx,%Lx) -> [%lx,%lx)\n",
			       memblk_nodeid[i],
			       node_memblk_range[i].start,
			       node_memblk_range[i].end,
			       node_memblk_range[j].start,
			       node_memblk_range[j].end,
			       start, end);
			node_memblk_range[i].start = start;
			node_memblk_range[i].end = end;
			k = --num_node_memblks - j;
			memmove(memblk_nodeid + j, memblk_nodeid + j+1,
				k * sizeof(*memblk_nodeid));
			memmove(node_memblk_range + j, node_memblk_range + j+1,
				k * sizeof(*node_memblk_range));
			--j;
		}
	}

	memnode_shift = compute_hash_shift(node_memblk_range, num_node_memblks,
					   memblk_nodeid);
	if (memnode_shift < 0) {
		printk(KERN_ERR "NUMA: No NUMA node hash function found. Contact maintainer\n");
		return -EINVAL;
	}

	for (i = 0; i < num_node_memblks; i++)
		memblock_x86_register_active_regions(memblk_nodeid[i],
				node_memblk_range[i].start >> PAGE_SHIFT,
				node_memblk_range[i].end >> PAGE_SHIFT);

	/* for out of order entries */
	sort_node_map();
	if (!nodes_cover_memory(numa_nodes))
		return -EINVAL;

	init_memory_mapping_high();

	/* Finally register nodes. */
	for_each_node_mask(i, node_possible_map)
		setup_node_bootmem(i, numa_nodes[i].start, numa_nodes[i].end);

	/*
	 * Try again in case setup_node_bootmem missed one due to missing
	 * bootmem.
	 */
	for_each_node_mask(i, node_possible_map)
		if (!node_online(i))
			setup_node_bootmem(i, numa_nodes[i].start,
					   numa_nodes[i].end);

	return 0;
}

#ifdef CONFIG_NUMA_EMU
/* Numa emulation */
static struct bootnode nodes[MAX_NUMNODES] __initdata;
static struct bootnode physnodes[MAX_NUMNODES] __cpuinitdata;
static char *cmdline __initdata;

void __init numa_emu_cmdline(char *str)
{
	cmdline = str;
}

static int __init setup_physnodes(unsigned long start, unsigned long end)
{
	int ret = 0;
	int i;

	memset(physnodes, 0, sizeof(physnodes));

	for_each_node_mask(i, mem_nodes_parsed) {
		physnodes[i].start = numa_nodes[i].start;
		physnodes[i].end = numa_nodes[i].end;
	}

	/*
	 * Basic sanity checking on the physical node map: there may be errors
	 * if the SRAT or AMD code incorrectly reported the topology or the mem=
	 * kernel parameter is used.
	 */
	for (i = 0; i < MAX_NUMNODES; i++) {
		if (physnodes[i].start == physnodes[i].end)
			continue;
		if (physnodes[i].start > end) {
			physnodes[i].end = physnodes[i].start;
			continue;
		}
		if (physnodes[i].end < start) {
			physnodes[i].start = physnodes[i].end;
			continue;
		}
		if (physnodes[i].start < start)
			physnodes[i].start = start;
		if (physnodes[i].end > end)
			physnodes[i].end = end;
		ret++;
	}

	/*
	 * If no physical topology was detected, a single node is faked to cover
	 * the entire address space.
	 */
	if (!ret) {
		physnodes[ret].start = start;
		physnodes[ret].end = end;
		ret = 1;
	}
	return ret;
}

static void __init fake_physnodes(int acpi, int amd, int nr_nodes)
{
	int i;

	BUG_ON(acpi && amd);
#ifdef CONFIG_ACPI_NUMA
	if (acpi)
		acpi_fake_nodes(nodes, nr_nodes);
#endif
#ifdef CONFIG_AMD_NUMA
	if (amd)
		amd_fake_nodes(nodes, nr_nodes);
#endif
	if (!acpi && !amd)
		for (i = 0; i < nr_cpu_ids; i++)
			numa_set_node(i, 0);
}

/*
 * Setups up nid to range from addr to addr + size.  If the end
 * boundary is greater than max_addr, then max_addr is used instead.
 * The return value is 0 if there is additional memory left for
 * allocation past addr and -1 otherwise.  addr is adjusted to be at
 * the end of the node.
 */
static int __init setup_node_range(int nid, u64 *addr, u64 size, u64 max_addr)
{
	int ret = 0;
	nodes[nid].start = *addr;
	*addr += size;
	if (*addr >= max_addr) {
		*addr = max_addr;
		ret = -1;
	}
	nodes[nid].end = *addr;
	node_set(nid, node_possible_map);
	printk(KERN_INFO "Faking node %d at %016Lx-%016Lx (%LuMB)\n", nid,
	       nodes[nid].start, nodes[nid].end,
	       (nodes[nid].end - nodes[nid].start) >> 20);
	return ret;
}

/*
 * Sets up nr_nodes fake nodes interleaved over physical nodes ranging from addr
 * to max_addr.  The return value is the number of nodes allocated.
 */
static int __init split_nodes_interleave(u64 addr, u64 max_addr, int nr_nodes)
{
	nodemask_t physnode_mask = NODE_MASK_NONE;
	u64 size;
	int big;
	int ret = 0;
	int i;

	if (nr_nodes <= 0)
		return -1;
	if (nr_nodes > MAX_NUMNODES) {
		pr_info("numa=fake=%d too large, reducing to %d\n",
			nr_nodes, MAX_NUMNODES);
		nr_nodes = MAX_NUMNODES;
	}

	size = (max_addr - addr - memblock_x86_hole_size(addr, max_addr)) / nr_nodes;
	/*
	 * Calculate the number of big nodes that can be allocated as a result
	 * of consolidating the remainder.
	 */
	big = ((size & ~FAKE_NODE_MIN_HASH_MASK) * nr_nodes) /
		FAKE_NODE_MIN_SIZE;

	size &= FAKE_NODE_MIN_HASH_MASK;
	if (!size) {
		pr_err("Not enough memory for each node.  "
			"NUMA emulation disabled.\n");
		return -1;
	}

	for (i = 0; i < MAX_NUMNODES; i++)
		if (physnodes[i].start != physnodes[i].end)
			node_set(i, physnode_mask);

	/*
	 * Continue to fill physical nodes with fake nodes until there is no
	 * memory left on any of them.
	 */
	while (nodes_weight(physnode_mask)) {
		for_each_node_mask(i, physnode_mask) {
			u64 end = physnodes[i].start + size;
			u64 dma32_end = PFN_PHYS(MAX_DMA32_PFN);

			if (ret < big)
				end += FAKE_NODE_MIN_SIZE;

			/*
			 * Continue to add memory to this fake node if its
			 * non-reserved memory is less than the per-node size.
			 */
			while (end - physnodes[i].start -
				memblock_x86_hole_size(physnodes[i].start, end) < size) {
				end += FAKE_NODE_MIN_SIZE;
				if (end > physnodes[i].end) {
					end = physnodes[i].end;
					break;
				}
			}

			/*
			 * If there won't be at least FAKE_NODE_MIN_SIZE of
			 * non-reserved memory in ZONE_DMA32 for the next node,
			 * this one must extend to the boundary.
			 */
			if (end < dma32_end && dma32_end - end -
			    memblock_x86_hole_size(end, dma32_end) < FAKE_NODE_MIN_SIZE)
				end = dma32_end;

			/*
			 * If there won't be enough non-reserved memory for the
			 * next node, this one must extend to the end of the
			 * physical node.
			 */
			if (physnodes[i].end - end -
			    memblock_x86_hole_size(end, physnodes[i].end) < size)
				end = physnodes[i].end;

			/*
			 * Avoid allocating more nodes than requested, which can
			 * happen as a result of rounding down each node's size
			 * to FAKE_NODE_MIN_SIZE.
			 */
			if (nodes_weight(physnode_mask) + ret >= nr_nodes)
				end = physnodes[i].end;

			if (setup_node_range(ret++, &physnodes[i].start,
						end - physnodes[i].start,
						physnodes[i].end) < 0)
				node_clear(i, physnode_mask);
		}
	}
	return ret;
}

/*
 * Returns the end address of a node so that there is at least `size' amount of
 * non-reserved memory or `max_addr' is reached.
 */
static u64 __init find_end_of_node(u64 start, u64 max_addr, u64 size)
{
	u64 end = start + size;

	while (end - start - memblock_x86_hole_size(start, end) < size) {
		end += FAKE_NODE_MIN_SIZE;
		if (end > max_addr) {
			end = max_addr;
			break;
		}
	}
	return end;
}

/*
 * Sets up fake nodes of `size' interleaved over physical nodes ranging from
 * `addr' to `max_addr'.  The return value is the number of nodes allocated.
 */
static int __init split_nodes_size_interleave(u64 addr, u64 max_addr, u64 size)
{
	nodemask_t physnode_mask = NODE_MASK_NONE;
	u64 min_size;
	int ret = 0;
	int i;

	if (!size)
		return -1;
	/*
	 * The limit on emulated nodes is MAX_NUMNODES, so the size per node is
	 * increased accordingly if the requested size is too small.  This
	 * creates a uniform distribution of node sizes across the entire
	 * machine (but not necessarily over physical nodes).
	 */
	min_size = (max_addr - addr - memblock_x86_hole_size(addr, max_addr)) /
						MAX_NUMNODES;
	min_size = max(min_size, FAKE_NODE_MIN_SIZE);
	if ((min_size & FAKE_NODE_MIN_HASH_MASK) < min_size)
		min_size = (min_size + FAKE_NODE_MIN_SIZE) &
						FAKE_NODE_MIN_HASH_MASK;
	if (size < min_size) {
		pr_err("Fake node size %LuMB too small, increasing to %LuMB\n",
			size >> 20, min_size >> 20);
		size = min_size;
	}
	size &= FAKE_NODE_MIN_HASH_MASK;

	for (i = 0; i < MAX_NUMNODES; i++)
		if (physnodes[i].start != physnodes[i].end)
			node_set(i, physnode_mask);
	/*
	 * Fill physical nodes with fake nodes of size until there is no memory
	 * left on any of them.
	 */
	while (nodes_weight(physnode_mask)) {
		for_each_node_mask(i, physnode_mask) {
			u64 dma32_end = MAX_DMA32_PFN << PAGE_SHIFT;
			u64 end;

			end = find_end_of_node(physnodes[i].start,
						physnodes[i].end, size);
			/*
			 * If there won't be at least FAKE_NODE_MIN_SIZE of
			 * non-reserved memory in ZONE_DMA32 for the next node,
			 * this one must extend to the boundary.
			 */
			if (end < dma32_end && dma32_end - end -
			    memblock_x86_hole_size(end, dma32_end) < FAKE_NODE_MIN_SIZE)
				end = dma32_end;

			/*
			 * If there won't be enough non-reserved memory for the
			 * next node, this one must extend to the end of the
			 * physical node.
			 */
			if (physnodes[i].end - end -
			    memblock_x86_hole_size(end, physnodes[i].end) < size)
				end = physnodes[i].end;

			/*
			 * Setup the fake node that will be allocated as bootmem
			 * later.  If setup_node_range() returns non-zero, there
			 * is no more memory available on this physical node.
			 */
			if (setup_node_range(ret++, &physnodes[i].start,
						end - physnodes[i].start,
						physnodes[i].end) < 0)
				node_clear(i, physnode_mask);
		}
	}
	return ret;
}

/*
 * Sets up the system RAM area from start_pfn to last_pfn according to the
 * numa=fake command-line option.
 */
static int __init numa_emulation(unsigned long start_pfn,
			unsigned long last_pfn, int acpi, int amd)
{
	u64 addr = start_pfn << PAGE_SHIFT;
	u64 max_addr = last_pfn << PAGE_SHIFT;
	int num_nodes;
	int i;

	/*
	 * If the numa=fake command-line contains a 'M' or 'G', it represents
	 * the fixed node size.  Otherwise, if it is just a single number N,
	 * split the system RAM into N fake nodes.
	 */
	if (strchr(cmdline, 'M') || strchr(cmdline, 'G')) {
		u64 size;

		size = memparse(cmdline, &cmdline);
		num_nodes = split_nodes_size_interleave(addr, max_addr, size);
	} else {
		unsigned long n;

		n = simple_strtoul(cmdline, NULL, 0);
		num_nodes = split_nodes_interleave(addr, max_addr, n);
	}

	if (num_nodes < 0)
		return num_nodes;
	memnode_shift = compute_hash_shift(nodes, num_nodes, NULL);
	if (memnode_shift < 0) {
		memnode_shift = 0;
		printk(KERN_ERR "No NUMA hash function found.  NUMA emulation "
		       "disabled.\n");
		return -1;
	}

	/*
	 * We need to vacate all active ranges that may have been registered for
	 * the e820 memory map.
	 */
	remove_all_active_ranges();
	for_each_node_mask(i, node_possible_map)
		memblock_x86_register_active_regions(i, nodes[i].start >> PAGE_SHIFT,
						nodes[i].end >> PAGE_SHIFT);
	init_memory_mapping_high();
	for_each_node_mask(i, node_possible_map)
		setup_node_bootmem(i, nodes[i].start, nodes[i].end);
	setup_physnodes(addr, max_addr);
	fake_physnodes(acpi, amd, num_nodes);
	numa_init_array();
	return 0;
}
#endif /* CONFIG_NUMA_EMU */

static int dummy_numa_init(void)
{
	printk(KERN_INFO "%s\n",
	       numa_off ? "NUMA turned off" : "No NUMA configuration found");
	printk(KERN_INFO "Faking a node at %016lx-%016lx\n",
	       0LU, max_pfn << PAGE_SHIFT);

	node_set(0, cpu_nodes_parsed);
	node_set(0, mem_nodes_parsed);
	numa_add_memblk(0, 0, (u64)max_pfn << PAGE_SHIFT);
	numa_nodes[0].start = 0;
	numa_nodes[0].end = (u64)max_pfn << PAGE_SHIFT;

	return 0;
}

void __init initmem_init(void)
{
	int (*numa_init[])(void) = { [2] = dummy_numa_init };
	int i, j;

	if (!numa_off) {
#ifdef CONFIG_ACPI_NUMA
		numa_init[0] = x86_acpi_numa_init;
#endif
#ifdef CONFIG_AMD_NUMA
		numa_init[1] = amd_numa_init;
#endif
	}

	for (i = 0; i < ARRAY_SIZE(numa_init); i++) {
		if (!numa_init[i])
			continue;

		for (j = 0; j < MAX_LOCAL_APIC; j++)
			set_apicid_to_node(j, NUMA_NO_NODE);

		nodes_clear(cpu_nodes_parsed);
		nodes_clear(mem_nodes_parsed);
		nodes_clear(node_possible_map);
		nodes_clear(node_online_map);
		num_node_memblks = 0;
		memset(node_memblk_range, 0, sizeof(node_memblk_range));
		memset(memblk_nodeid, 0, sizeof(memblk_nodeid));
		memset(numa_nodes, 0, sizeof(numa_nodes));
		remove_all_active_ranges();

		if (numa_init[i]() < 0)
			continue;

		/* clean up the node list */
		for (j = 0; j < MAX_NUMNODES; j++)
			cutoff_node(j, 0, max_pfn << PAGE_SHIFT);

#ifdef CONFIG_NUMA_EMU
		setup_physnodes(0, max_pfn << PAGE_SHIFT);
		if (cmdline && !numa_emulation(0, max_pfn, i == 0, i == 1))
			return;
		setup_physnodes(0, max_pfn << PAGE_SHIFT);
		nodes_clear(node_possible_map);
		nodes_clear(node_online_map);
#endif
		/* Account for nodes with cpus and no memory */
		nodes_or(node_possible_map, mem_nodes_parsed, cpu_nodes_parsed);
		if (WARN_ON(nodes_empty(node_possible_map)))
			continue;

		if (numa_register_memblks() < 0)
			continue;

		for (j = 0; j < nr_cpu_ids; j++) {
			int nid = early_cpu_to_node(j);

			if (nid == NUMA_NO_NODE)
				continue;
			if (!node_online(nid))
				numa_clear_node(j);
		}
		numa_init_array();
		return;
	}
	BUG();
}

unsigned long __init numa_free_all_bootmem(void)
{
	unsigned long pages = 0;
	int i;

	for_each_online_node(i)
		pages += free_all_bootmem_node(NODE_DATA(i));

	pages += free_all_memory_core_early(MAX_NUMNODES);

	return pages;
}

int __cpuinit numa_cpu_node(int cpu)
{
	int apicid = early_per_cpu(x86_cpu_to_apicid, cpu);

	if (apicid != BAD_APICID)
		return __apicid_to_node[apicid];
	return NUMA_NO_NODE;
}

/*
 * UGLINESS AHEAD: Currently, CONFIG_NUMA_EMU is 64bit only and makes use
 * of 64bit specific data structures.  The distinction is artificial and
 * should be removed.  numa_{add|remove}_cpu() are implemented in numa.c
 * for both 32 and 64bit when CONFIG_NUMA_EMU is disabled but here when
 * enabled.
 *
 * NUMA emulation is planned to be made generic and the following and other
 * related code should be moved to numa.c.
 */
#ifdef CONFIG_NUMA_EMU
# ifndef CONFIG_DEBUG_PER_CPU_MAPS
void __cpuinit numa_add_cpu(int cpu)
{
	unsigned long addr;
	int physnid, nid;

	nid = numa_cpu_node(cpu);
	if (nid == NUMA_NO_NODE)
		nid = early_cpu_to_node(cpu);
	BUG_ON(nid == NUMA_NO_NODE || !node_online(nid));

	/*
	 * Use the starting address of the emulated node to find which physical
	 * node it is allocated on.
	 */
	addr = node_start_pfn(nid) << PAGE_SHIFT;
	for (physnid = 0; physnid < MAX_NUMNODES; physnid++)
		if (addr >= physnodes[physnid].start &&
		    addr < physnodes[physnid].end)
			break;

	/*
	 * Map the cpu to each emulated node that is allocated on the physical
	 * node of the cpu's apic id.
	 */
	for_each_online_node(nid) {
		addr = node_start_pfn(nid) << PAGE_SHIFT;
		if (addr >= physnodes[physnid].start &&
		    addr < physnodes[physnid].end)
			cpumask_set_cpu(cpu, node_to_cpumask_map[nid]);
	}
}

void __cpuinit numa_remove_cpu(int cpu)
{
	int i;

	for_each_online_node(i)
		cpumask_clear_cpu(cpu, node_to_cpumask_map[i]);
}
# else	/* !CONFIG_DEBUG_PER_CPU_MAPS */
static void __cpuinit numa_set_cpumask(int cpu, int enable)
{
	int node = early_cpu_to_node(cpu);
	struct cpumask *mask;
	int i;

	if (node == NUMA_NO_NODE) {
		/* early_cpu_to_node() already emits a warning and trace */
		return;
	}
	for_each_online_node(i) {
		unsigned long addr;

		addr = node_start_pfn(i) << PAGE_SHIFT;
		if (addr < physnodes[node].start ||
					addr >= physnodes[node].end)
			continue;
		mask = debug_cpumask_set_cpu(cpu, enable);
		if (!mask)
			return;

		if (enable)
			cpumask_set_cpu(cpu, mask);
		else
			cpumask_clear_cpu(cpu, mask);
	}
}

void __cpuinit numa_add_cpu(int cpu)
{
	numa_set_cpumask(cpu, 1);
}

void __cpuinit numa_remove_cpu(int cpu)
{
	numa_set_cpumask(cpu, 0);
}
# endif	/* !CONFIG_DEBUG_PER_CPU_MAPS */
#endif	/* CONFIG_NUMA_EMU */
