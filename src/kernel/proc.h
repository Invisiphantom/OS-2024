#pragma once

#include <common/defines.h>
#include <common/list.h>
#include <common/sem.h>
#include <common/rbtree.h>

// 进程状态
enum procstate { UNUSED, RUNNABLE, RUNNING, SLEEPING, ZOMBIE };

// 用户进程上下文
typedef struct UserContext {
    // TODO: customize your trap frame
} UserContext;

// 内核进程上下文
typedef struct KernelContext {
    // TODO: customize your context
} KernelContext;

// 调度信息
struct schinfo {
    // TODO: customize your sched info
};

// 进程结构体
typedef struct Proc {
    bool killed;  // 是否被终止
    bool idle;    // 是否为idle进程

    int pid;               // Process ID
    int exitcode;          // 退出码
    enum procstate state;  // 进程状态

    Semaphore childexit;     // 子进程退出信号量
    ListNode children;       // 子进程链表
    ListNode ptnode;         // 所属父进程的链表节点
    struct Proc* parent;     // 父进程
    struct schinfo schinfo;  // 调度信息

    void* kstack;             // 进程内核栈
    UserContext* ucontext;    // 用户上下文 (trapframe)
    KernelContext* kcontext;  // 内核上下文
} Proc;

void init_kproc();
void init_proc(Proc*);

Proc* create_proc();                                 // 创建子进程
int start_proc(Proc*, void (*entry)(u64), u64 arg);  // 启动子进程

NO_RETURN void exit(int code);  // 子进程退出 (弃子的父进程改为root_proc)
int wait(int* exitcode);        // 父进程等待子进程退出
