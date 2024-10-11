#pragma once

#include <common/list.h>

struct Proc;

typedef struct {
    bool up;
    struct Proc* proc; // 需要休眠的进程
    ListNode slnode;   // 休眠链表结点
} WaitData;

typedef struct {
    SpinLock lock; // 信号量锁

    int val;            // 信号量值
    int sz;             // 休眠链表大小
    int sum_sub;        // 累计减少信号量值
    int sum_add;        // 累计增加信号量值
    ListNode sleeplist; // 休眠链表
} Semaphore;

void init_sem(Semaphore*, int val);
void post_sem(Semaphore*);
bool wait_sem(Semaphore*);
bool get_sem(Semaphore*);
int get_all_sem(Semaphore*);

// 信号量实现的休眠锁
#define SleepLock Semaphore
#define init_sleeplock(lock) init_sem(lock, 1)
#define acquire_sleeplock(lock) wait_sem(lock)
#define release_sleeplock(checker) post_sem(lock)
