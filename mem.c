#include <aarch64/mmu.h>
#include <common/rc.h>
#include <common/spinlock.h>
#include <driver/memlayout.h>
#include <kernel/mem.h>
#include <kernel/printk.h>
#include <common/defines.h>

RefCount kalloc_page_cnt;
static SpinLock kalloc_page_lock;
extern char end[];

// 空闲页链表
static struct FreePage* free_page_head;
struct FreePage {
    struct FreePage* next;
    char data[PAGE_SIZE - sizeof(struct FreePage*)];
};

// Slab分配器
struct SlabAllocator {
    SpinLock list_lock;
    struct SlabPage* partial;  // 部分页链表
    struct SlabPage* full;     // 完全页链表
    u16 obj_size;              // 对象长度
};

#define SA_TYPES 18  // Slab分配器种类
static struct SlabAllocator SA[SA_TYPES];

// Slab页 (双向链表)
struct SlabPage {
    u8 sa_type;                  // Slab分配器类型
    u16 free_block_head_offset;  // 空闲链表头 偏移位置
    struct SlabPage* prev;       // 上一个Slab页
    struct SlabPage* next;       // 下一个Slab页
};

// Slab对象
struct SlabObj {
    u16 next_offset;  // (2bytes)
};

void kinit() {
    init_rc(&kalloc_page_cnt);
    init_spinlock(&kalloc_page_lock);

    // 页空闲链表的初始地址: end关于PAGE_SIZE的整数倍对齐
    free_page_head = (struct FreePage*)round_up((u64)end, PAGE_SIZE);

    // 初始化页空闲链表
    auto p = (u64)free_page_head;
    for (; p < P2K(PHYSTOP - PAGE_SIZE); p += PAGE_SIZE) {
        auto page = (struct FreePage*)p;
        page->next = (struct FreePage*)(p + PAGE_SIZE);
    }
    auto final_page = (struct FreePage*)p;
    final_page->next = NULL;  // 末尾页的next指针为NULL

    // 初始化Slab分配器
    for (int i = 0; i < SA_TYPES; i++) {
        init_spinlock(&SA[i].list_lock);
        SA[i].partial = NULL;
        SA[i].full = NULL;
    }

    SA[0].obj_size = 2;
    SA[1].obj_size = 4;
    SA[2].obj_size = 8;
    SA[3].obj_size = 12;
    SA[4].obj_size = 16;

    SA[5].obj_size = 24;
    SA[6].obj_size = 32;
    SA[7].obj_size = 40;
    SA[8].obj_size = 48;
    SA[9].obj_size = 56;
    SA[10].obj_size = 64;

    SA[11].obj_size = 96;
    SA[12].obj_size = 128;
    SA[13].obj_size = 192;
    SA[14].obj_size = 256;

    SA[15].obj_size = 512;
    SA[16].obj_size = 1024;
    SA[17].obj_size = 2048;
}

void* kalloc_page() {
    increment_rc(&kalloc_page_cnt);
    acquire_spinlock(&kalloc_page_lock);

    auto page = free_page_head;
    free_page_head = free_page_head->next;

    release_spinlock(&kalloc_page_lock);
    return page;
}

void kfree_page(void* p) {
    decrement_rc(&kalloc_page_cnt);
    acquire_spinlock(&kalloc_page_lock);

    auto page = (struct FreePage*)p;
    page->next = free_page_head;
    free_page_head = page;

    release_spinlock(&kalloc_page_lock);
    return;
}

void* kalloc(unsigned long long size) {
    for (int i = 0; i < SA_TYPES; i++) {
        if (size > SA[i].obj_size)
            continue;  // 如果分配器过短，则跳过

        acquire_spinlock(&SA[i].list_lock);
        struct SlabPage* page = SA[i].partial;

        // 如果没有可用页，则分配新页
        if (page == NULL) {
            page = (struct SlabPage*)kalloc_page();
            page->sa_type = i;              // 所属分配器类型
            page->prev = NULL;              // 无前页
            page->next = NULL;              // 无后页
            page->free_block_head_offset =  // 首空闲对象偏移位置
                round_up(sizeof(struct SlabPage), SA[i].obj_size);  // 地址对齐

            // 初始化内部对象链表
            u16 obj_offset = page->free_block_head_offset;
            for (; obj_offset < PAGE_SIZE - SA[i].obj_size; obj_offset += SA[i].obj_size) {
                auto obj = (struct SlabObj*)((u64)page + obj_offset);
                obj->next_offset = obj_offset + SA[i].obj_size;
            }
            auto final_obj = (struct SlabObj*)((u64)page + obj_offset);
            final_obj->next_offset = NULL;  // 末尾对象的next指针为NULL

            SA[i].partial = page; // 更新部分链表首页
        }

        auto obj = (struct SlabObj*)((u64)page + page->free_block_head_offset);
        page->free_block_head_offset = obj->next_offset;

        // 如果页已满，则从部分链表移至完全链表
        if (page->free_block_head_offset == NULL) {
            SA[i].partial = page->next;
            if (SA[i].partial != NULL)
                SA[i].partial->prev = NULL;

            page->next = SA[i].full;
            if (SA[i].full != NULL)
                SA[i].full->prev = page;
            SA[i].full = page;
        }

        release_spinlock(&SA[i].list_lock);
        return obj;
    }

    printk("kalloc: out of memory\n");
    return NULL;
}

void kfree(void* ptr) {
    auto obj = (struct SlabObj*)ptr;
    auto page = (struct SlabPage*)((u64)obj & ~(PAGE_SIZE - 1));

    acquire_spinlock(&SA[page->sa_type].list_lock);

    obj->next_offset = page->free_block_head_offset;
    page->free_block_head_offset = (u16)((u64)obj - (u64)page);

    // 如果此时页在完全链表, 则将其移至部分链表
    if(obj->next_offset == NULL) {
        if(page->prev != NULL)
            page->prev->next = page->next;
        if(page->next != NULL)
            page->next->prev = page->prev;

        if(SA[page->sa_type].full == page)
            SA[page->sa_type].full = page->next;

        if(SA[page->sa_type].partial != NULL)
            SA[page->sa_type].partial->prev = page;

        page->prev = NULL;
        page->next = SA[page->sa_type].partial;
        SA[page->sa_type].partial = page;
    }

    release_spinlock(&SA[page->sa_type].list_lock);
    return;
}
