#include <aarch64/mmu.h>
#include <common/rc.h>
#include <common/spinlock.h>
#include <driver/memlayout.h>
#include <kernel/mem.h>
#include <kernel/printk.h>
#include <common/defines.h>
#include <common/string.h>
#include <common/list.h>

RefCount kalloc_page_cnt;
static SpinLock kalloc_page_lock;
extern char end[];

// 空闲页链表 (单向链表)
typedef struct FreePage {
    struct FreePage* next;
} FreePage;

static FreePage* free_page_head;

// Slab分配器 (静态数组)
typedef struct SlabAlloc {
    SpinLock sa_lock; // 分配器锁
    ListNode partial;   // 部分页链表
    ListNode full;      // 完全页链表
    u16 obj_size;       // 对象长度
} SlabAlloc;

#define SA_TYPES 32 // Slab分配器种类数
static SlabAlloc SA[SA_TYPES];
static const u16 SA_SIZES[SA_TYPES]
    = { 2, 4, 8, 16, 32, 64, 128, 256, 512, 1016, 2032, 4072 };

// Slab页 (双向链表)
typedef struct SlabPage {
    u8 sa_type;               // 所属的分配器类型
    u16 obj_cnt;              // 已分配对象数量
    u16 free_obj_offset; // 首对象偏移位置
    ListNode node;            // 串在页链表中的结点
} SlabPage;

// Slab对象 (单向链表)
typedef struct SlabObj {
    u16 next_offset; // (2字节)
} SlabObj;

void kinit()
{
    init_rc(&kalloc_page_cnt);
    init_spinlock(&kalloc_page_lock);

    // 空闲页链表的初始地址: end关于PAGE_SIZE的对齐
    free_page_head = (struct FreePage*)round_up((u64)end, PAGE_SIZE);

    // 初始化页空闲链表
    auto p = (u64)free_page_head;
    auto page = (struct FreePage*)p;
    for (; p < P2K(PHYSTOP); p += PAGE_SIZE) {
        page = (struct FreePage*)p;
        page->next = (struct FreePage*)(p + PAGE_SIZE);
    }
    page->next = NULL; // 末尾页的next指针为NULL

    // 初始化所有Slab分配器
    for (int i = 0; i < SA_TYPES; i++) {
        init_spinlock(&SA[i].sa_lock);
        SA[i].obj_size = SA_SIZES[i];
        init_list_node(&SA[i].partial);
        init_list_node(&SA[i].full);
    }
}

// 直接分配一页
void* kalloc_page()
{
    increment_rc(&kalloc_page_cnt);
    acquire_spinlock(&kalloc_page_lock);

    auto page = free_page_head;
    free_page_head = free_page_head->next;

    release_spinlock(&kalloc_page_lock);

    ASSERT(page != NULL);
    return page;
}

// 直接释放一页 (需要页对齐)
void kfree_page(void* p)
{
    // 确保地址页对齐
    ASSERT(((u64)p & (PAGE_SIZE - 1)) == 0);

    decrement_rc(&kalloc_page_cnt);
    acquire_spinlock(&kalloc_page_lock);

    auto page = (struct FreePage*)p;
    page->next = free_page_head;
    free_page_head = page;

    release_spinlock(&kalloc_page_lock);
    return;
}

void* kalloc(unsigned long long size)
{
    for (int i = 0; i < SA_TYPES; i++) {
        if (size > SA[i].obj_size)
            continue; // 如果分配器过短，则跳过

        acquire_spinlock(&SA[i].sa_lock);

        // 从部分页链表中取出一页
        SlabPage* page = container_of(SA[i].partial.next, SlabPage, node);

        // 如果没有可用页，则分配新页
        if (_empty_list(&SA[i].partial)) {
            page = (SlabPage*)kalloc_page();
            memset(page, 0, PAGE_SIZE);
            page->sa_type = i;           // 所属分配器类型
            page->obj_cnt = 0;           // 无已分配对象
            init_list_node(&page->node); // 初始化链表节点

            // 首对象 偏移位置 (地址对齐)
            page->free_obj_offset = sizeof(SlabPage);

            // 初始化 对象链表
            u16 obj_offset = page->free_obj_offset;
            auto obj = (SlabObj*)((u64)page + obj_offset);
            for (; obj_offset <= PAGE_SIZE - SA[i].obj_size;
                 obj_offset += SA[i].obj_size) {
                obj = (SlabObj*)((u64)page + obj_offset);
                obj->next_offset = obj_offset + SA[i].obj_size;
            }
            obj->next_offset = NULL; // 末尾对象的next指针为NULL

            // 更新 部分链表首页
            _insert_into_list(&SA[i].partial, &page->node);
        }

        auto obj = (SlabObj*)((u64)page + page->free_obj_offset);
        page->free_obj_offset = obj->next_offset;
        page->obj_cnt++;

        // 如果页已满，则从部分链表移至完全链表
        if (page->free_obj_offset == NULL) {
            _detach_from_list(&page->node);
            _insert_into_list(&SA[i].full, &page->node);
        }

        release_spinlock(&SA[i].sa_lock);
        return obj;
    }

    printk("kalloc: out of memory\n");
    PANIC();
    return NULL;
}

void kfree(void* ptr)
{
    auto obj = (SlabObj*)ptr;
    auto page = (SlabPage*)PAGE_BASE((u64)obj);

    acquire_spinlock(&SA[page->sa_type].sa_lock);

    obj->next_offset = page->free_obj_offset;
    page->free_obj_offset = (u16)((u64)obj - (u64)page);
    page->obj_cnt--;

    // 如果此时页在完全链表, 则将其移至部分链表
    if (obj->next_offset == NULL) {
        _detach_from_list(&page->node);
        _insert_into_list(&SA[page->sa_type].partial, &page->node);
    }

    release_spinlock(&SA[page->sa_type].sa_lock);
    return;
}
