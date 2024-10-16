#pragma once

#include <common/defines.h>
#include <aarch64/intrinsic.h>

typedef struct {
    volatile bool locked;
} SpinLock;

void init_spinlock(SpinLock*);         // 初始化自旋锁
bool try_acquire_spinlock(SpinLock*);  // 尝试获取自旋锁
void acquire_spinlock(SpinLock*);      // 获取自旋锁
void release_spinlock(SpinLock*);      // 释放自旋锁
