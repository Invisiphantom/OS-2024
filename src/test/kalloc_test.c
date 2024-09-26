#include <aarch64/intrinsic.h>
#include <aarch64/mmu.h>
#include <common/rc.h>
#include <common/string.h>
#include <kernel/mem.h>
#include <kernel/printk.h>
#include <test/test.h>

extern RefCount kalloc_page_cnt;

static void* p[4][10000];
static short sz[4][10000];

#define FAIL(...)            \
    {                        \
        printk(__VA_ARGS__); \
        while (1)            \
            ;                \
    }
#define SYNC(i)             \
    arch_dsb_sy();          \
    increment_rc(&x);       \
    while (x.count < 4 * i) \
        ;                   \
    arch_dsb_sy();

// 启用单CPU调试gdb
// #define SINGLE_CORE  // TODO

#ifndef SINGLE_CORE
static RefCount x;
#endif

void kalloc_test() {
    int i = cpuid();  // CPU编号
    int r = kalloc_page_cnt.count;
    int y = 10000 - i * 500;

    if (i == 0)
        printk("SYNC(1)\n");
#ifndef SINGLE_CORE
    SYNC(1)  // 确保4个CPU都到达
#endif

    for (int j = 0; j < y; j++) {
        p[i][j] = kalloc_page();
        if (!p[i][j] || ((u64)p[i][j] & 4095))  // 如果p[i][j]为NULL 或者 p[i][j]不是页对齐
            FAIL("FAIL: kalloc_page() = %p\n", p[i][j]);  // 打印错误信息
        memset(p[i][j], i ^ j, PAGE_SIZE);  // 将p[i][j]页的每个字节都设置为 i^j
    }

    for (int j = 0; j < y; j++) {
        u8 m = (i ^ j) & 255;  // 取i^j的低8位
        for (int k = 0; k < PAGE_SIZE; k++)
            if (((u8*)p[i][j])[k] != m)  // 验证p[i][j]页的每个字节都是i^j
                FAIL("FAIL: page[%d][%d] wrong\n", i, j);
        kfree_page(p[i][j]);
    }

    if (i == 0)
        printk("SYNC(2)\n");
#ifndef SINGLE_CORE
    SYNC(2)
#endif

    if (kalloc_page_cnt.count != r)  // 确保kalloc_page_cnt.count没有变化
        FAIL("FAIL: kalloc_page_cnt %d -> %lld\n", r, kalloc_page_cnt.count);

    if (i == 0)
        printk("SYNC(3)\n");
#ifndef SINGLE_CORE
    SYNC(3)
#endif

    for (int j = 0; j < 10000;) {
        // 前1000次, 或者概率为 9/16
        if (j < 1000 || rand() > RAND_MAX / 16 * 7) {
            int z = 0;
            int r = rand() & 255;  // [0,255]

            // r=[0,126]  49.6%
            if (r < 127) {
                z = rand() % 48 + 17;       // z=[17,64]
                z = round_up((u64)z, 4ll);  // 向上对4的倍数取整
            }

            // r=[127,180]  20.7%
            else if (r < 181) {
                z = rand() % 16 + 1;  // z=[1,16]
            }

            // r=[181,234]  20.7%
            else if (r < 235) {
                z = rand() % 192 + 65;      // z=[65,256]
                z = round_up((u64)z, 8ll);  // 向上对8的倍数取整
            }

            // r=[235,254]  7.42%
            else if (r < 255) {
                z = rand() % 256 + 257;     // z=[257,512]
                z = round_up((u64)z, 8ll);  // 向上对8的倍数取整
            }

            // r=[255]  0.04%
            else {
                z = rand() % 1528 + 513;    // z=[513,2040]
                z = round_up((u64)z, 8ll);  // 向上对8的倍数取整
            }

            sz[i][j] = z;         // 记录大小
            p[i][j] = kalloc(z);  // 分配大小为z的内存
            u64 q = (u64)p[i][j];

            // 检查内存地址是否对齐
            //        z ->        q
            // _______0 -> _______0 (2 byte)
            // ______00 -> ______00 (4 byte)
            // _0000000 -> _0000000 (8 byte)
            
            
            // if (p[i][j] == NULL || ((z & 1) == 0 && (q & 1) != 0) ||
            //     ((z & 3) == 0 && (q & 3) != 0) || ((z & 7) == 0 && (q & 7) != 0))
            //     FAIL("FAIL: kalloc(%d) = %p\n", z, p[i][j]);

            if (p[i][j] == NULL)
                FAIL("NULL: kalloc(%d) = %p\n", z, p[i][j]);
            
            if ((z & 1) == 0 && (q & 1) != 0)
                FAIL("2 byte: kalloc(%d) = %p\n", z, p[i][j]);

            if ((z & 3) == 0 && (q & 3) != 0)
                FAIL("4 byte: kalloc(%d) = %p\n", z, p[i][j]);

            if ((z & 7) == 0 && (q & 7) != 0)
                FAIL("8 byte: kalloc(%d) = %p\n", z, p[i][j]);

            memset(p[i][j], i ^ z, z);  // 将p[i][j]的每个字节都设置为i^z
            j++;
        }

        // 其他情况
        else {
            int k = rand() % j;  // 随机选择一个内存块
            if (p[i][k] == NULL)
                FAIL("FAIL: block[%d][%d] null\n", i, k);
            int m = (i ^ sz[i][k]) & 255;

            // 检查内存块的每个字节是否都是i^z
            for (int t = 0; t < sz[i][k]; t++)
                if (((u8*)p[i][k])[t] != m)
                    FAIL("FAIL: block[%d][%d] wrong\n", i, k);

            kfree(p[i][k]);

            j--;  // 将末尾的内存块填补到k位置
            p[i][k] = p[i][j];
            sz[i][k] = sz[i][j];
        }
    }

    if (i == 0)
        printk("SYNC(4)\n");
#ifndef SINGLE_CORE
    SYNC(4)
#endif

    if (cpuid() == 0) {
        i64 z = 0;
        // 计算目前分配的内存总大小
        for (int j = 0; j < 4; j++)
            for (int k = 0; k < 10000; k++) {
                z += sz[j][k];
            }
        // 打印总大小和当前页使用大小
        printk("Total: %lld\nUsage: %lld\n", z, kalloc_page_cnt.count - r);
    }

    if (i == 0)
        printk("SYNC(5)\n");
#ifndef SINGLE_CORE
    SYNC(5)
#endif

    for (int j = 0; j < 10000; j++)
        kfree(p[i][j]);

    if (i == 0)
        printk("SYNC(6)\n");
#ifndef SINGLE_CORE
    SYNC(6)
#endif

    if (cpuid() == 0)
        printk("kalloc_test PASS\n");
}
