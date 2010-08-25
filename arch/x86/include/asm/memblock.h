#ifndef _X86_MEMBLOCK_H
#define _X86_MEMBLOCK_H

#define ARCH_DISCARD_MEMBLOCK

u64 memblock_x86_find_in_range_size(u64 start, u64 *sizep, u64 align);
void memblock_x86_to_bootmem(u64 start, u64 end);

void memblock_x86_reserve_range(u64 start, u64 end, char *name);
void memblock_x86_free_range(u64 start, u64 end);
struct range;
int get_free_all_memory_range(struct range **rangep, int nodeid);

#endif
