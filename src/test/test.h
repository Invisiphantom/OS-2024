#pragma once
#include <common/defines.h>

#define RAND_MAX 32768

void kalloc_test();
void rbtree_test();
void proc_test();
void vm_test();
void user_proc_test();
unsigned rand();
void srand(unsigned seed);

// syscall
u64 myreport(u64 id);