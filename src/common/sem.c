#include <common/sem.h>
#include <kernel/mem.h>
#include <kernel/sched.h>
#include <kernel/printk.h>
#include <common/list.h>

// 初始化信号量sem, 初始值为val
void init_sem(Semaphore* sem, int val)
{
    sem->val = val;
    init_spinlock(&sem->lock);
    init_list_node(&sem->sleeplist);
}

// 尝试获取信号量sem
// 获取成功返回true, 失败返回false
bool get_sem(Semaphore* sem)
{
    bool ret = false;
    acquire_spinlock(&sem->lock);
    if (sem->val > 0) {
        sem->val--;
        ret = true;
    }
    release_spinlock(&sem->lock);
    return ret;
}

// 获取信号量sem的所有值
int get_all_sem(Semaphore* sem)
{
    int ret = 0;
    acquire_spinlock(&sem->lock);
    if (sem->val > 0) {
        ret = sem->val;
        sem->val = 0;
    }
    release_spinlock(&sem->lock);
    return ret;
}

// 等待信号量sem
// 如果是被唤醒的, 返回true
// 如果是自己醒来的, 返回false
bool wait_sem(Semaphore* sem)
{
    acquire_spinlock(&sem->lock); // 获取信号量锁

    sem->val--;        // 信号量值减1
    if (sem->val >= 0) // 成功获取信号量 返回
    {
        release_spinlock(&sem->lock);
        return true;
    }

    // 初始化等待体
    WaitData* wait = kalloc(sizeof(WaitData));
    wait->proc = thisproc(); // 获取当前进程
    wait->up = false;        // 未被唤醒

    // 将等待体 添加到 信号量的休眠链表 开始排队
    _insert_into_list(&sem->sleeplist, &wait->slnode);

    acquire_sched_lock();         // 获取调度锁
    release_spinlock(&sem->lock); // 释放信号量锁
    sched(SLEEPING);              // 设置当前进程为休眠 并启用调度

    acquire_spinlock(&sem->lock); // 重新获取信号量锁

    if (wait->up == false) // 如果未被唤醒 (自己醒来的?)
    {
        sem->val++;
        ASSERT(sem->val <= 0);
        _detach_from_list(&wait->slnode);
    }

    release_spinlock(&sem->lock); // 释放信号量锁

    // 返回唤醒状态
    bool ret = wait->up;
    kfree(wait);
    return ret;
}

// 释放信号量sem, 并唤醒一个最早等待的进程
void post_sem(Semaphore* sem)
{
    acquire_spinlock(&sem->lock);

    sem->val++;          // 增加信号量的值
    if (sem->val <= 0) { // 如果信号量的值小于等于0，表示有进程在等待

        ASSERT(!_empty_list(&sem->sleeplist)); // 确保休眠链表不为空

        // 获取休眠链表上最早的等待体
        auto wait = container_of(sem->sleeplist.prev, WaitData, slnode);

        wait->up = true;                  // 标记为已唤醒
        _detach_from_list(&wait->slnode); // 从休眠链表中移除
        activate_proc(wait->proc); // 设置为RUNNABLE 并加入调度队列
    }

    release_spinlock(&sem->lock);
}