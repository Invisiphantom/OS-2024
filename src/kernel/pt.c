#include <kernel/pt.h>
#include <kernel/mem.h>
#include <common/string.h>
#include <aarch64/intrinsic.h>

// TODO: 查找页表, 返回虚拟地址va 对应的页表项
// alloc  1:分配新页表  0:不进行分配
PTEntriesPtr get_pte(struct pgdir* pgdir, u64 va, bool alloc) {
    
    
    
     return NULL; }

void init_pgdir(struct pgdir* pgdir) { pgdir->pt = NULL; }

// TODO: 递归地释放页表页, 但不释放其引用的物理内存
void free_pgdir(struct pgdir* pgdir) {
    
    
     return; }

// 配置低地址页表ttbr0_el1 映射为pgdir
void attach_pgdir(struct pgdir* pgdir)
{
    extern PTEntries invalid_pt;
    if (pgdir->pt)
        arch_set_ttbr0(K2P(pgdir->pt));
    else
        arch_set_ttbr0(K2P(&invalid_pt));
}
