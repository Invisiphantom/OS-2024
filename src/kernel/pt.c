#include <kernel/pt.h>
#include <kernel/mem.h>
#include <common/string.h>
#include <aarch64/intrinsic.h>

// TODO: 查找四级页表, 返回虚拟地址va 对应的页表项
// alloc  1:分配新页表  0:不进行分配
PTEntriesPtr get_pte(struct pgdir* pgdir, u64 va, bool alloc)
{
    auto sign = va & KSPACE_MASK;
    ASSERT(sign == 0 || sign == KSPACE_MASK);

    // 如果PGD为空, 则分配新页
    if (pgdir->pt == NULL) {
        if (alloc == false)
            return NULL;
        pgdir->pt = (PTEntriesPtr)kalloc_page();
        memset(pgdir->pt, 0, PAGE_SIZE);
    }

    auto pt = pgdir->pt;
    for (int level = 0; level <= 2; level++) {
        PTEntriesPtr pte = &pt[VA_PART(va, level)];

        // 如果是有效项, 则获取下一级页表
        if (*pte & PTE_VALID)
            pt = (PTEntriesPtr)P2K(PTE_ADDRESS(*pte));

        // 如果不是有效项
        else {
            if (alloc == false)
                return NULL;
            // 给下一级页表分配一页内存
            pt = (PTEntriesPtr)kalloc_page();

            // 清空页表
            memset(pt, 0, PAGE_SIZE);

            // 填充页表项, 指向新分配的下一级页表
            *pte = K2P(pt) | PTE_VALID | PTE_TABLE | PTE_USER | PTE_RW;
        }
    }

    // 返回最后一级页表项的地址
    auto ret = &pt[VA_PART(va, 3)];
    return ret;
}

void init_pgdir(struct pgdir* pgdir)
{
    pgdir->pt = NULL;
    pgdir->level = 0;
}

// TODO: 递归地释放页表页, 但不释放其引用的物理内存
void free_pgdir(struct pgdir* pgdir)
{
    // 遍历页表的所有页表项
    if (pgdir->level <= 2) {
        for (int i = 0; i < N_PTE_PER_TABLE; i++) {
            auto pte = pgdir->pt[i];
            if (pte & PTE_VALID) {
                struct pgdir pgdir_child;
                pgdir_child.pt = (PTEntriesPtr)P2K(PTE_ADDRESS(pte));
                pgdir_child.level = pgdir->level + 1;
                free_pgdir(&pgdir_child);
            }
        }
    }

    // 释放当前页表页
    kfree_page(pgdir->pt);
    pgdir->pt = NULL;
}
void free_sub_pgdir(struct pgdir* pgdir, int level) { }

// 配置低地址页表ttbr0_el1 映射为pgdir
void attach_pgdir(struct pgdir* pgdir)
{
    extern PTEntries invalid_pt;
    if (pgdir->pt)
        arch_set_ttbr0(K2P(pgdir->pt));
    else
        arch_set_ttbr0(K2P(&invalid_pt));
}
