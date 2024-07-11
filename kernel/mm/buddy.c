/*
 * Copyright (c) 2022 Institute of Parallel And Distributed Systems (IPADS)
 * ChCore-Lab is licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v1. You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v1 for more details.
 */

#include <common/util.h>
#include <common/macro.h>
#include <common/kprint.h>
#include <mm/buddy.h>

/*
 * The layout of a phys_mem_pool:
 * | page_metadata are (an array of struct page) | alignment pad | usable memory
 * |
 *
 * The usable memory: [pool_start_addr, pool_start_addr + pool_mem_size).
 */
void init_buddy(struct phys_mem_pool *pool, struct page *start_page,
                vaddr_t start_addr, u64 page_num)
{
        int order;
        int page_idx;
        struct page *page;

        /* Init the physical memory pool. */
        pool->pool_start_addr = start_addr;
        pool->page_metadata = start_page;
        pool->pool_mem_size = page_num * BUDDY_PAGE_SIZE;

        /* This field is for unit test only. */
        pool->pool_phys_page_num = page_num;

        /* Init the free lists */
        // 初始化 free_lists，将每个链表清空，并将计数器设为 0
        for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
                pool->free_lists[order].nr_free = 0;
                init_list_head(&(pool->free_lists[order].free_list));
        }

        /* Clear the page_metadata area. */
        memset((char *)start_page, 0, page_num * sizeof(struct page));

        /* Init the page_metadata area. */
        for (page_idx = 0; page_idx < page_num; ++page_idx) {
                page = start_page + page_idx;
                page->allocated = 1;
                page->order = 0;
                page->pool = pool;
        }

        /* Put each physical memory page into the free lists. */
        for (page_idx = 0; page_idx < page_num; ++page_idx) {
                page = start_page + page_idx;
                buddy_free_pages(pool, page);
        }
}

static struct page *get_buddy_chunk(struct phys_mem_pool *pool,
                                    struct page *chunk)
{
        u64 chunk_addr;
        u64 buddy_chunk_addr;
        int order;

        /* Get the address of the chunk. */
        chunk_addr = (u64)page_to_virt(chunk);
        order = chunk->order;
/*
 * Calculate the address of the buddy chunk according to the address
 * relationship between buddies.
 */
#define BUDDY_PAGE_SIZE_ORDER (12)
        buddy_chunk_addr = chunk_addr
                           ^ (1UL << (order + BUDDY_PAGE_SIZE_ORDER));

        /* Check whether the buddy_chunk_addr belongs to pool. */
        if ((buddy_chunk_addr < pool->pool_start_addr)
            || (buddy_chunk_addr
                >= (pool->pool_start_addr + pool->pool_mem_size))) {
                return NULL;
        }

        return virt_to_page((void *)buddy_chunk_addr);
}

// 设置从start_page开始的nums个page的order和allocated
void setPagesOrderAndAlloc(struct page *start_page, int setOrder,
                           int setIfAlloc)
{
        int pageNum = 1 << setOrder;
        for (int i = 0; i < pageNum; ++i) {
                struct page *page = start_page + i;
                page->order = setOrder;
                page->allocated = setIfAlloc;
        }
}

// order 是目标的大小，2^order 数量的连续的 4k 的chunk
// page* 就是要被分割的物理页的数组，我通过指针来操控这个数组
// page->order
// 存储的是当前page对应的是2^order的chunk，据此可以推断出chunk的起点和终点
static struct page *split_page(struct phys_mem_pool *pool, u64 order,
                               struct page *page)
{
        /* LAB 2 TODO 2 BEGIN */
        /*
         * Hint: Recursively put the buddy of current chunk into
         * a suitable free list.
         */
        // 把 order 地方对应的chunk 分裂成两个 然后放在order - 1

        while (page->order != order) {
                // 分割前的order
                int beforeSplitOrder = page->order;

                struct page *left_part = page;
                struct page *right_part = page + (1 << (page->order - 1));

                // 配置分割后左page的order和allocated
                setPagesOrderAndAlloc(left_part, beforeSplitOrder - 1, 0);
                // 从当前链表中删除并加入到新的链表
                list_del(&left_part->node);
                list_add(&left_part->node,
                         &pool->free_lists[beforeSplitOrder - 1].free_list);

                // 配置分割后右page的order和allocated
                setPagesOrderAndAlloc(right_part, beforeSplitOrder - 1, 0);
                // 无需删除，直接加入到新的链表
                list_add(&right_part->node,
                         &pool->free_lists[beforeSplitOrder - 1].free_list);

                // 更新 nr_free 的数量
                // beforeSplitOrder对应的链表空闲的数量减少一个
                // beforeSplitOrder - 1 对应的链表空闲的数量增加两个
                pool->free_lists[beforeSplitOrder].nr_free -= 1;
                pool->free_lists[beforeSplitOrder - 1].nr_free += 2;

                // 现在page指向的是左边的page，然后继续循环看看是否还需要分裂
                page = left_part;
        }
        return page;
        /* LAB 2 TODO 2 END */
}

// 在空闲的链表中找到一个满足页数为2^order的chunk，根据需要分裂
// 这个看课本的129面
struct page *buddy_get_pages(struct phys_mem_pool *pool, u64 order)
{
        /* LAB 2 TODO 2 BEGIN */
        /*
         * Hint: Find a chunk that satisfies the order requirement
         * in the free lists, then split it if necessary.
         */

        // 注意边界情况检查呢~ 好久没写代码了真是脑子要坏掉了
        if (order > BUDDY_MAX_ORDER) {
                return NULL;
        }

        u64 cur_order;
        struct list_head *free_list;
        struct page *cur_page = NULL;

        for (cur_order = order; cur_order <= BUDDY_MAX_ORDER; cur_order++) {
                free_list = &pool->free_lists[cur_order].free_list;
                // 如果当前链表，目标位置里面的存在空闲
                if (pool->free_lists[cur_order].nr_free > 0) {
                        cur_page = pool->free_lists[cur_order].free_list.next;
                        break;
                }
                // 如果cur_order已经是最大的order了，那么就直接返回NULL
                if (cur_order == BUDDY_MAX_ORDER) {
                        return NULL;
                }
        }

        // 若取出的伙伴块的order大于目标order，则需要分裂
        // 当然这里我们直接喂给split函数，让它自己判断，如果不合适就分裂
        cur_page = split_page(pool, order, cur_page);

        // split_page函数分割出来的默认都是un
        // allocated，所以这里得改味allocated
        setPagesOrderAndAlloc(cur_page, cur_page->order, 1);

        // 既然allocateed了，那么链表要删除对应元素，并且数量要减一
        list_del(&cur_page->node);
        pool->free_lists[cur_page->order].nr_free -= 1;
        return cur_page;
        /* LAB 2 TODO 2 END */
}

static struct page *merge_page(struct phys_mem_pool *pool, struct page *page)
{
        /* LAB 2 TODO 2 BEGIN */
        /*
         * Hint: Recursively merge current chunk with its buddy
         * if possible.
         */

        struct page *cur_page = page;
        struct page *buddy_page;
        //
        while (cur_page->order < BUDDY_MAX_ORDER - 1) {
                buddy_page = get_buddy_chunk(pool, cur_page);

                if (buddy_page == NULL || buddy_page->allocated == 1
                    || buddy_page->order != cur_page->order) {
                        break;
                }
                
                // 删除伙伴对应链表里面的东西
                list_del(&buddy_page->node);
                pool->free_lists[buddy_page->order].nr_free--;

                // 调整位置，保证 cur_page 为左伙伴 buddy_page 为右伙伴
                if (cur_page > buddy_page) {
                        struct page *tmp = buddy_page;
                        buddy_page = cur_page;
                        cur_page = tmp;
                }
                setPagesOrderAndAlloc(cur_page, cur_page->order + 1, 0);
        }

        // 还是觉得放在merge函数里面更合适
        // 加入到新的链表里面 然后修改空闲的个数
        list_add(&cur_page->node, &pool->free_lists[page->order].free_list);
        pool->free_lists[page->order].nr_free++;

        return cur_page;

        /* LAB 2 TODO 2 END */
}

void buddy_free_pages(struct phys_mem_pool *pool, struct page *page)
{
        /* LAB 2 TODO 2 BEGIN */
        /*
         * Hint: Merge the chunk with its buddy and put it into
         * a suitable free list.
         */
        // 先设置为未分配
        setPagesOrderAndAlloc(page, page->order, 0);
        // 释放前尝试merge
        page = merge_page(pool, page);
        /* LAB 2 TODO 2 END */
}

void *page_to_virt(struct page *page)
{
        u64 addr;
        struct phys_mem_pool *pool = page->pool;

        BUG_ON(pool == NULL);
        /* page_idx * BUDDY_PAGE_SIZE + start_addr */
        addr = (page - pool->page_metadata) * BUDDY_PAGE_SIZE
               + pool->pool_start_addr;
        return (void *)addr;
}

struct page *virt_to_page(void *ptr)
{
        struct page *page;
        struct phys_mem_pool *pool = NULL;
        u64 addr = (u64)ptr;
        int i;

        /* Find the corresponding physical memory pool. */
        for (i = 0; i < physmem_map_num; ++i) {
                if (addr >= global_mem[i].pool_start_addr
                    && addr < global_mem[i].pool_start_addr
                                       + global_mem[i].pool_mem_size) {
                        pool = &global_mem[i];
                        break;
                }
        }

        BUG_ON(pool == NULL);
        page = pool->page_metadata
               + (((u64)addr - pool->pool_start_addr) / BUDDY_PAGE_SIZE);
        return page;
}

u64 get_free_mem_size_from_buddy(struct phys_mem_pool *pool)
{
        int order;
        struct free_list *list;
        u64 current_order_size;
        u64 total_size = 0;

        for (order = 0; order < BUDDY_MAX_ORDER; order++) {
                /* 2^order * 4K */
                current_order_size = BUDDY_PAGE_SIZE * (1 << order);
                list = pool->free_lists + order;
                total_size += list->nr_free * current_order_size;

                /* debug : print info about current order */
                kdebug("buddy memory chunk order: %d, size: 0x%lx, num: %d\n",
                       order,
                       current_order_size,
                       list->nr_free);
        }
        return total_size;
}

#ifdef CHCORE_KERNEL_TEST
#include <mm/mm.h>
#include <lab.h>
void lab2_test_buddy(void)
{
        struct phys_mem_pool *pool = &global_mem[0];
        {
                u64 free_mem_size = get_free_mem_size_from_buddy(pool);
                lab_check(pool->pool_phys_page_num == free_mem_size / PAGE_SIZE,
                          "Init buddy");
        }
        {
                lab_check(buddy_get_pages(pool, BUDDY_MAX_ORDER + 1) == NULL
                                  && buddy_get_pages(pool, (u64)-1) == NULL,
                          "Check invalid order");
        }
        {
                bool ok = true;
                u64 expect_free_mem = pool->pool_phys_page_num * PAGE_SIZE;
                struct page *page = buddy_get_pages(pool, 0);
                BUG_ON(page == NULL);
                lab_assert(page->order == 0 && page->allocated);
                expect_free_mem -= PAGE_SIZE;
                lab_assert(get_free_mem_size_from_buddy(pool)
                           == expect_free_mem);
                buddy_free_pages(pool, page);
                expect_free_mem += PAGE_SIZE;
                lab_assert(get_free_mem_size_from_buddy(pool)
                           == expect_free_mem);
                lab_check(ok, "Allocate & free order 0");
        }
        {
                bool ok = true;
                u64 expect_free_mem = pool->pool_phys_page_num * PAGE_SIZE;
                struct page *page;
                for (int i = 0; i < BUDDY_MAX_ORDER; i++) {
                        page = buddy_get_pages(pool, i);
                        BUG_ON(page == NULL);
                        lab_assert(page->order == i && page->allocated);
                        expect_free_mem -= (1 << i) * PAGE_SIZE;
                        lab_assert(get_free_mem_size_from_buddy(pool)
                                   == expect_free_mem);
                        buddy_free_pages(pool, page);
                        expect_free_mem += (1 << i) * PAGE_SIZE;
                        lab_assert(get_free_mem_size_from_buddy(pool)
                                   == expect_free_mem);
                }
                for (int i = BUDDY_MAX_ORDER - 1; i >= 0; i--) {
                        page = buddy_get_pages(pool, i);
                        BUG_ON(page == NULL);
                        lab_assert(page->order == i && page->allocated);
                        expect_free_mem -= (1 << i) * PAGE_SIZE;
                        lab_assert(get_free_mem_size_from_buddy(pool)
                                   == expect_free_mem);
                        buddy_free_pages(pool, page);
                        expect_free_mem += (1 << i) * PAGE_SIZE;
                        lab_assert(get_free_mem_size_from_buddy(pool)
                                   == expect_free_mem);
                }
                lab_check(ok, "Allocate & free each order");
        }
        {
                bool ok = true;
                u64 expect_free_mem = pool->pool_phys_page_num * PAGE_SIZE;
                struct page *pages[BUDDY_MAX_ORDER];
                for (int i = 0; i < BUDDY_MAX_ORDER; i++) {
                        pages[i] = buddy_get_pages(pool, i);
                        BUG_ON(pages[i] == NULL);
                        lab_assert(pages[i]->order == i);
                        expect_free_mem -= (1 << i) * PAGE_SIZE;
                        lab_assert(get_free_mem_size_from_buddy(pool)
                                   == expect_free_mem);
                }
                for (int i = 0; i < BUDDY_MAX_ORDER; i++) {
                        buddy_free_pages(pool, pages[i]);
                        expect_free_mem += (1 << i) * PAGE_SIZE;
                        lab_assert(get_free_mem_size_from_buddy(pool)
                                   == expect_free_mem);
                }
                lab_check(ok, "Allocate & free all orders");
        }
        {
                bool ok = true;
                u64 expect_free_mem = pool->pool_phys_page_num * PAGE_SIZE;
                struct page *page;
                for (int i = 0; i < pool->pool_phys_page_num; i++) {
                        page = buddy_get_pages(pool, 0);
                        BUG_ON(page == NULL);
                        lab_assert(page->order == 0);
                        expect_free_mem -= PAGE_SIZE;
                        lab_assert(get_free_mem_size_from_buddy(pool)
                                   == expect_free_mem);
                }
                lab_assert(get_free_mem_size_from_buddy(pool) == 0);
                lab_assert(buddy_get_pages(pool, 0) == NULL);
                for (int i = 0; i < pool->pool_phys_page_num; i++) {
                        page = pool->page_metadata + i;
                        lab_assert(page->allocated);
                        buddy_free_pages(pool, page);
                        expect_free_mem += PAGE_SIZE;
                        lab_assert(get_free_mem_size_from_buddy(pool)
                                   == expect_free_mem);
                }
                lab_assert(pool->pool_phys_page_num * PAGE_SIZE
                           == expect_free_mem);
                lab_check(ok, "Allocate & free all memory");
        }
        printk("[TEST] Buddy tests finished\n");
}
#endif /* CHCORE_KERNEL_TEST */
