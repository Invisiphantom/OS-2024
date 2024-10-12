#pragma once
#include "common/defines.h"

// 红黑树结点
struct rb_node_ {
    u64 __rb_parent_color;     // 父结点+颜色
    struct rb_node_* rb_right; // 右结点
    struct rb_node_* rb_left;  // 左结点
};
typedef struct rb_node_* rb_node;

// 红黑树根结点
struct rb_root_ {
    rb_node rb_node;
};
typedef struct rb_root_* rb_root;

int _rb_insert(rb_node node, rb_root root, bool (*cmp)(rb_node lnode, rb_node rnode));
void _rb_erase(rb_node node, rb_root root);
rb_node _rb_lookup(rb_node node, rb_root rt, bool (*cmp)(rb_node lnode, rb_node rnode));
rb_node _rb_first(rb_root root);
