#pragma once

#include <common/defines.h>
#include <common/list.h>
#include <common/sem.h>
#include <common/rbtree.h>

// 进程状态
enum procstate { UNUSED, RUNNABLE, RUNNING, SLEEPING, ZOMBIE };

// TODO 用户进程上下文
typedef struct UserContext {

    // Special Regs
    u64 sp_el0;    // Stack Pointer
    u64 spsr_el1;  // Program Status Register
    u64 elr_el1;   // Exception Link Register

    // General Regs
    u64 x0;
    u64 x1;
    u64 x2;
    u64 x3;
    u64 x4;
    u64 x5;
    u64 x6;
    u64 x7;
    u64 x8;
    u64 x9;
    u64 x10;
    u64 x11;
    u64 x12;
    u64 x13;
    u64 x14;
    u64 x15;
    u64 x16;
    u64 x17;
    u64 x18;
    u64 x19;
    u64 x20;
    u64 x21;
    u64 x22;
    u64 x23;
    u64 x24;
    u64 x25;
    u64 x26;
    u64 x27;
    u64 x28;
    u64 x29;  // Frame Pointer
    u64 x30;  // Procedure Link Register
} UserContext;

// TODO 内核进程上下文
typedef struct KernelContext {
    u64 x0; // 第一个参数 (start_proc)
    u64 x1; // 第二个参数 (start_proc)
    u64 x19;
    u64 x20;
    u64 x21;
    u64 x22;
    u64 x23;
    u64 x24;
    u64 x25;
    u64 x26;
    u64 x27;
    u64 x28;
    u64 x29; // Frame Pointer
    u64 x30; // Procedure Link Register
} KernelContext;

// TODO 进程的调度信息
struct schinfo {
    ListNode sched_node; // 串在调度队列中的结点
 };

// 进程结构体
typedef struct Proc {
    bool killed; // 是否被终止
    bool idle;   // 是否为idle进程

    int pid;              // Process ID
    int exitcode;         // 退出码
    enum procstate state; // 进程状态

    Semaphore childexit;    // 子进程退出信号量
    ListNode children;      // 子进程链表
    ListNode ptnode;        // 作为子进程时, 自己串在链表上的节点
    struct Proc* parent;    // 父进程

    struct schinfo schinfo; // 调度信息

    void* kstack;            // 进程内核栈
    UserContext* ucontext;   // 用户上下文 (进程栈sp)
    KernelContext* kcontext; // 内核上下文 (进程栈sp)
} Proc;



void init_kproc();
void init_proc(Proc*);

Proc* create_proc();                                // 创建子进程
int start_proc(Proc*, void (*entry)(u64), u64 arg); // 启动子进程

NO_RETURN void exit(int code); // 子进程退出 (弃子的父进程改为root_proc)
int wait(int* exitcode);       // 父进程等待子进程退出
