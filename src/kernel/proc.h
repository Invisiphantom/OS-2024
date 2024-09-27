#pragma once

#include <common/defines.h>
#include <common/list.h>
#include <common/sem.h>
#include <common/rbtree.h>

enum procstate { UNUSED, RUNNABLE, RUNNING, SLEEPING, ZOMBIE };

typedef struct UserContext {
    // TODO: customize your trap frame
} UserContext;

typedef struct KernelContext {
    // TODO: customize your context
} KernelContext;

// embeded data for procs
struct schinfo {
    // TODO: customize your sched info
};


// 进程退出时, 将其子进程的父改为 root_proc, 以避免孤儿进程
typedef struct Proc {
    bool killed;  // 是否被终止
    bool idle;    // 是否为idle进程
    int pid;
    int exitcode;             // 退出码
    enum procstate state;     // 进程状态
    Semaphore childexit;      // 子进程退出信号量
    ListNode children;        // 子进程链表
    ListNode ptnode;          // 所属父进程的链表节点
    struct Proc* parent;      // 父进程
    struct schinfo schinfo;   // 调度信息
    void* kstack;             // 进程内核栈
    UserContext* ucontext;    // 用户态上下文
    KernelContext* kcontext;  // 内核态上下文
} Proc;

void init_kproc();
void init_proc(Proc*);
Proc* create_proc();
int start_proc(Proc*, void (*entry)(u64), u64 arg);
NO_RETURN void exit(int code);
int wait(int* exitcode);
