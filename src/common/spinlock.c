#include <aarch64/intrinsic.h>
#include <common/spinlock.h>

// 初始化自旋锁
void init_spinlock(SpinLock* lock) {
    lock->locked = 0;
}

// 尝试获取自旋锁
bool try_acquire_spinlock(SpinLock* lock) {
    // 如果锁未被占用, 则尝试获取锁
    if (!lock->locked && !__atomic_test_and_set(&lock->locked, __ATOMIC_ACQUIRE)) {
        return true;
    } 
    // 否则, 返回获取失败
    else {
        return false;
    }
}

// 循环获取自旋锁
void acquire_spinlock(SpinLock* lock) {
    while (!try_acquire_spinlock(lock))
        arch_yield();
}

// 释放自旋锁
void release_spinlock(SpinLock* lock) {
    __atomic_clear(&lock->locked, __ATOMIC_RELEASE);
}
