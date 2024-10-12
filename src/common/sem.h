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
    ListNode sleeplist; // 休眠链表
} Semaphore;

void init_sem(Semaphore*, int val);
void _post_sem(Semaphore*);
bool _wait_sem(Semaphore*);
bool _get_sem(Semaphore*);
int _query_sem(Semaphore*);
void _lock_sem(Semaphore*);
void _unlock_sem(Semaphore*);
int get_all_sem(Semaphore*);
int post_all_sem(Semaphore*);

#define post_sem(sem)                                                                    \
    ({                                                                                   \
        _lock_sem(sem);                                                                  \
        _post_sem(sem);                                                                  \
        _unlock_sem(sem);                                                                \
    })
#define wait_sem(sem)                                                                    \
    ({                                                                                   \
        _lock_sem(sem);                                                                  \
        bool __ret = _wait_sem(sem);                                                     \
        _unlock_sem(sem);                                                                \
        __ret;                                                                           \
    })
#define get_sem(sem)                                                                     \
    ({                                                                                   \
        _lock_sem(sem);                                                                  \
        bool __ret = _get_sem(sem);                                                      \
        _unlock_sem(sem);                                                                \
        __ret;                                                                           \
    })

// 信号量实现的休眠锁
#define SleepLock Semaphore
#define init_sleeplock(lock) init_sem(lock, 1)
#define acquire_sleeplock(lock) wait_sem(lock)
#define release_sleeplock(checker) post_sem(lock)
