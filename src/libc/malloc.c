/*
pcboot - bootable PC demo/game kernel
Copyright (C) 2018  John Tsiombikas <nuclear@member.fsf.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY, without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "mem.h"
#include "panic.h"

#define SINGLE_POOL
#define MALLOC_DEBUG

struct mem_desc {
	size_t size;
	uint32_t magic;
#ifdef MALLOC_DEBUG
	uint32_t dbg;
#endif
	struct mem_desc *next;
};

#define MAGIC_USED	0xdeadf00d
#define MAGIC_FREE	0x1ee7d00d

#define DESC_PTR(b)	((void*)((struct mem_desc*)(b) + 1))
#define PTR_DESC(p)	((struct mem_desc*)(p) - 1)

#ifdef SINGLE_POOL
static struct mem_desc *pool;
#else

#define NUM_POOLS	16
#define FIRST_POOL_POW2	5
/* 2**(x+5) size pools: 0->32, 1->64, 2->128 .. 15->1048576 */
#define POOL_SIZE(x)	(1 << ((x) + FIRST_POOL_POW2))
#define MAX_POOL_SIZE	(1 << ((NUM_POOLS - 1) + FIRST_POOL_POW2))
#define MAX_POOL_PAGES	BYTES_TO_PAGES(MAX_POOL_SIZE)
static struct mem_desc *pools[NUM_POOLS];

static int add_to_pool(struct mem_desc *b);

static int pool_index(int sz)
{
	int x = 0;
	--sz;
	while(sz) {
		sz >>= 1;
		++x;
	}
	return x - FIRST_POOL_POW2;
}
#endif	/* !SINGLE_POOL */


#ifdef SINGLE_POOL
void *malloc(size_t sz)
{
	int pg0, npages;
	size_t total_sz;
	struct mem_desc *mem, *prev, dummy;
	int found = 0;

	total_sz = sz + sizeof(struct mem_desc);

	dummy.next = pool;
	prev = &dummy;
	while(prev->next) {
		mem = prev->next;
		if(mem->size == total_sz) {
			prev->next = mem->next;
			found = 1;
			break;
		}
		if(mem->size > total_sz) {
			void *ptr = (char*)mem + mem->size - total_sz;
			mem->size -= total_sz;
			mem = ptr;
			found = 1;
			break;
		}
		prev = prev->next;
	}
	pool = dummy.next;

	if(found) {
		mem->size = total_sz;
		mem->magic = MAGIC_USED;
		mem->next = 0;
		return DESC_PTR(mem);
	}

	/* did not find a free block, grab a new one */
	npages = BYTES_TO_PAGES(total_sz);
	if((pg0 = alloc_ppages(npages)) == -1) {
		errno = ENOMEM;
		return 0;
	}
	mem = PAGE_TO_PTR(pg0);
	mem->size = npages * 4096;
	mem->next = 0;
	mem->magic = MAGIC_USED;
	return DESC_PTR(mem);
}

void free(void *p)
{
	struct mem_desc *mem, *prev, dummy;

	if(!p) return;
	mem = PTR_DESC(p);

	if(mem->magic != MAGIC_USED) {
		if(mem->magic == MAGIC_FREE) {
			panic("free(%p): double-free\n", p);
		} else {
			panic("free(%p): corrupted magic (%x)!\n", p, mem->magic);
		}
	}
	mem->magic = MAGIC_FREE;

	dummy.next = pool;
	prev = &dummy;

	while(prev->next) {
		char *end = (char*)mem + mem->size;
		if(end == (char*)prev->next) {
			mem->size += prev->next->size;
			mem->next = prev->next->next;
			prev->next->magic = 0;
			prev->next = mem;
			return;
		}
		if(end < (char*)prev->next) {
			mem->next = prev->next;
			prev->next = mem;
			return;
		}

		prev = prev->next;
	}
	pool = dummy.next;

	prev->next = mem;
	mem->next = 0;
}

#else	/* !SINGLE_POOL */

void *malloc(size_t sz)
{
	int pidx, pg0;
	size_t total_sz, halfsz;
	struct mem_desc *mem, *other;

	total_sz = sz + sizeof(struct mem_desc);

	if(total_sz > MAX_POOL_SIZE) {
		/*printf("  allocation too big, hitting sys_alloc directly\n");*/
		if((pg0 = alloc_ppages(BYTES_TO_PAGES(total_sz))) == -1) {
			errno = ENOMEM;
			return 0;
		}
		mem = PAGE_TO_PTR(pg0);
		mem->magic = MAGIC_USED;
		mem->size = total_sz;
		mem->next = 0;
		return DESC_PTR(mem);
	}

	pidx = pool_index(total_sz);
	while(pidx < NUM_POOLS) {
		if(pools[pidx]) {
			/* found a pool with a free block that fits */
			mem = pools[pidx];
			pools[pidx] = mem->next;
			mem->next = 0;

			/*printf("  using pool %d (size %ld)\n", pidx, (unsigned long)mem->size);*/

			/* while the mem size is larger than twice the allocation, split it */
			while(pidx-- > 0 && total_sz <= (halfsz = mem->size / 2)) {
				/*printf("  splitting %ld block in half and ", (unsigned long)mem->size);*/
				other = (struct mem_desc*)((char*)mem + halfsz);
				mem->size = other->size = halfsz;

				add_to_pool(other);
			}

			if(mem->magic != MAGIC_FREE) {
				panic("Trying to allocate range surrounded by an aura of wrong MAGIC\n");
			}
			mem->magic = MAGIC_USED;
			return DESC_PTR(mem);
		}

		++pidx;
	}

	/* did not find a free block, add a new one */
	pidx = NUM_POOLS - 1;
	if((pg0 = alloc_ppages(MAX_POOL_PAGES)) == -1) {
		errno = ENOMEM;
		return 0;
	}
	mem = PAGE_TO_PTR(pg0);
	mem->size = MAX_POOL_SIZE;
	mem->next = pools[pidx];
	mem->magic = MAGIC_FREE;
	pools[pidx] = mem;

	/* try again now that there is a free block */
	return malloc(sz);
}


void free(void *p)
{
	int pg0;
	struct mem_desc *mem;

	if(!p) return;

	mem = PTR_DESC(p);
	if(mem->magic != MAGIC_USED) {
		if(mem->magic == MAGIC_FREE) {
			panic("free(%p): double-free\n", p);
		} else {
			panic("free(%p): corrupted magic (%x)!\n", p, mem->magic);
		}
	}

	if(mem->size > MAX_POOL_SIZE) {
		/*printf("foo_free(%p): %ld bytes. block too large for pools, returning to system\n",
				(void*)p, (unsigned long)mem->size);*/
		pg0 = ADDR_TO_PAGE(mem);
		free_ppages(pg0, BYTES_TO_PAGES(mem->size));
		return;
	}

	/*printf("foo_free(%p): block of %ld bytes and ", (void*)p, (unsigned long)mem->size);*/
	add_to_pool(mem);
}

#endif	/* !def SINGLE_POOL */


void *calloc(size_t num, size_t size)
{
	void *ptr = malloc(num * size);
	if(ptr) {
		memset(ptr, 0, num * size);
	}
	return ptr;
}

void *realloc(void *ptr, size_t size)
{
	struct mem_desc *mem;
	void *newp;

	if(!ptr) {
		return malloc(size);
	}

	mem = PTR_DESC(ptr);
	if(mem->size >= size) {
		return ptr;	/* TODO: shrink */
	}

	if(!(newp = malloc(size))) {
		return 0;
	}
	free(ptr);
	return newp;
}

#ifdef MALLOC_DEBUG
static void check_cycles(struct mem_desc *mem)
{
	static uint32_t dbg = 42;
	uint32_t cur_dbg = dbg++;

	while(mem) {
		if(mem->magic != MAGIC_FREE) {
			panic("check_cycles: NON-FREE MAGIC!\n");
		}
		if(mem->dbg == cur_dbg) {
			panic("CYCLE DETECTED\n");
		}
		mem->dbg = cur_dbg;
		mem = mem->next;
	}
}
#endif	/* MALLOC_DEBUG */

#ifndef SINGLE_POOL
static int add_to_pool(struct mem_desc *mem)
{
	int pidx;
	struct mem_desc head;
	struct mem_desc *iter, *pnode;

	pidx = pool_index(mem->size);

	/*printf("adding %ld block to pool %d\n", (unsigned long)mem->size, pidx);*/

	iter = &head;
	head.next = pools[pidx];

	while(iter->next) {
		pnode = iter->next;
		if(mem->size == pnode->size) {	/* only coalesce same-sized blocks */
			size_t size = mem->size;

			if((char*)mem == (char*)pnode - size) {
				iter->next = pnode->next;	/* unlink pnode */
				pools[pidx] = head.next;
				mem->next = 0;
				mem->size += size;

				/*printf("  coalescing %p with %p and ", (void*)mem, (void*)pnode);*/
				return add_to_pool(mem);
			}
			if((char*)mem == (char*)pnode + size) {
				iter->next = pnode->next;	/* unlink pnode */
				pools[pidx] = head.next;
				pnode->next = 0;
				pnode->size += size;

				/*printf("  coalescing %p with %p and ", (void*)mem, (void*)pnode);*/
				return add_to_pool(pnode);
			}
		}
		iter = iter->next;
	}

	/* otherwise just add it to the pool */
	mem->magic = MAGIC_FREE;
	mem->next = pools[pidx];
	pools[pidx] = mem;

	check_cycles(pools[pidx]);
	return 0;
}
#endif	/* !def SINGLE_POOL */
