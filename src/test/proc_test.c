#include <kernel/sched.h>
#include <common/sem.h>
#include <test/test.h>
#include <kernel/mem.h>
#include <kernel/printk.h>

void set_parent_to_this(Proc* proc);

static Semaphore s1, s2, s3, s4, s5, s6;

#define DEBUG

static void proc_test_1b(u64 a)
{
#ifdef DEBUG
    printk("- pid=%d: proc_test_1b\ta=%lld\n\n", thisproc()->pid, a);
#endif

    switch (a / 10 - 1) {

    case 0: // pass
        break;

    // 进行三次调度
    // 然后退出唤醒root_proc
    case 1:
        yield();
        yield();
        yield();
        break;

    case 2: // pass
        post_sem(&s1);
        break;

    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        if (a & 1)
            post_sem(&s2);
        else
            wait_sem(&s2);
        break;

    case 8:
        wait_sem(&s3);
        post_sem(&s4);
        break;

    case 9:
        post_sem(&s5);
        wait_sem(&s6);
        break;
    }

    exit(a); // 退出码为a
}

static void proc_test_1a(u64 a)
{
#ifdef DEBUG
    printk("\n- pid=%d: proc_test_1a\ta=%lld\n\n", thisproc()->pid, a);
#endif

    // 再接着创建10个子进程
    for (int i = 0; i < 10; i++) {
        auto p = create_proc();
        set_parent_to_this(p);
        start_proc(p, proc_test_1b, a * 10 + i + 10);
    }

    switch (a) {

    case 0: { // pass
        int t = 0, x;
        // 等待10个子进程全部结束
        for (int i = 0; i < 10; i++) {
            wait(&x);
            t |= 1 << (x - 10);
        }
        ASSERT(t == 1023);
        ASSERT(wait(&x) == -1);
    } break;

    case 1: // pass
        break;

    case 2: { // pass
        for (int i = 0; i < 10; i++)
            ASSERT(wait_sem(&s1));
        // 确保此时信号量为0
        ASSERT(get_sem(&s1) == false);
    } break;

    case 3:
    case 4:
    case 5:
    case 6:
    case 7: {
        int x;
        // 等待10个子进程全部结束
        for (int i = 0; i < 10; i++)
            wait(&x);
        ASSERT(wait(&x) == -1);
    } break;

    case 8: {
        int x;
        for (int i = 0; i < 10; i++)
            post_sem(&s3);
        for (int i = 0; i < 10; i++)
            wait(&x);
        ASSERT(wait(&x) == -1);
        ASSERT(s3.val == 0);
        ASSERT(get_all_sem(&s4) == 10);
    } break;

    case 9: {
        int x;
        for (int i = 0; i < 10; i++)
            wait_sem(&s5);
        for (int i = 0; i < 10; i++)
            post_sem(&s6);
        for (int i = 0; i < 10; i++)
            wait(&x);
        ASSERT(wait(&x) == -1);
        ASSERT(s5.val == 0);
        ASSERT(s6.val == 0);
    } break;
    }

    exit(a); // 退出码为a
}

static void proc_test_1()
{
#ifdef DEBUG
    printk("\n- pid=%d: proc_test_1\n\n", thisproc()->pid);
#endif

    init_sem(&s1, 0);
    init_sem(&s2, 0);
    init_sem(&s3, 0);
    init_sem(&s4, 0);
    init_sem(&s5, 0);
    init_sem(&s6, 0);

    int pid[10];

    // 初始化10个进程 指向proc_test_1a
    for (int i = 0; i < 10; i++) {
        auto p = create_proc();
        set_parent_to_this(p);
        pid[i] = start_proc(p, proc_test_1a, i);
    }

    // 依次等待10个进程全部退出
    for (int i = 0; i < 10; i++) {
        int code, id;
        id = wait(&code);
        ASSERT(pid[code] == id);
#ifdef DEBUG
        printk(">>>>>>>> proc_test_1a-case %d exit <<<<<<<<\n\n", code);
#endif
    }

    exit(0); // 退出码为0
}

void proc_test()
{
    printk("proc_test\n");
    auto p = create_proc();
    int pid = start_proc(p, proc_test_1, 0); // 6

    int t = 0;
    while (1) {
        int code;
        int id = wait(&code);

        if (id == -1)
            break;

        if (id == pid)
            ASSERT(code == 0);
        else // 其他孤儿进程
            t |= 1 << (code - 20);
    }
    ASSERT(t == 1048575);
    printk("proc_test PASS\n");
}
