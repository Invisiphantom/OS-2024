#pragma once

#include <aarch64/mmu.h>

struct pgdir {
    PTEntriesPtr pt; // (内核地址)
    int level;
};

void init_pgdir(struct pgdir* pgdir);
void free_pgdir(struct pgdir* pgdir);
void attach_pgdir(struct pgdir* pgdir);
