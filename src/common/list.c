#include <common/list.h>

// 初始化单循环结点
void init_list_node(ListNode* node)
{
    node->prev = node;
    node->next = node;
}

// 合并重组两个链表结点 (node1->node2)
ListNode* _merge_list(ListNode* node1, ListNode* node2)
{
    if (!node1)
        return node2;
    if (!node2)
        return node1;

    // 合并前:
    //   ... --> node1 -------> node3 --> ...
    //   ... <-- node2 <------- node4 <-- ...
    // 合并后:
    //   ... --> node1 --+  +-> node3 --> ...
    //                   |  |
    //   ... <-- node2 <-+  +-- node4 <-- ...

    ListNode* node3 = node1->next;
    ListNode* node4 = node2->prev;

    node1->next = node2;
    node2->prev = node1;
    node4->next = node3;
    node3->prev = node4;

    return node1;
}

// 向链表list中插入结点node, 并返回插入后的链表头
ListNode* _insert_into_list(ListNode* list, ListNode* node)
{
    init_list_node(node);
    return _merge_list(list, node);
}

// 从链表中移除结点node, 并返回node->prev
// 如果node是链表中的第一个结点, 则返回NULL
ListNode* _detach_from_list(ListNode* node)
{
    ListNode* prev = node->prev;
    ListNode* next = node->next;

    prev->next = next;
    next->prev = prev;
    init_list_node(node);

    if (prev == node)
        return NULL;
    return prev;
}

// 判断链表是否为空
inline bool _empty_list(ListNode* list) { return (list->next == list); }

// -------------------------------- Queue -------------------------------- //

// 初始化循环队列 (需持有队列锁)
void queue_init(Queue* x)
{
    x->begin = x->end = 0;
    x->sz = 0;
    init_spinlock(&x->lk);
}

// 获取队列锁
void queue_lock(Queue* x) { acquire_spinlock(&x->lk); }

// 释放队列锁
void queue_unlock(Queue* x) { release_spinlock(&x->lk); }

// 将结点item添加到队列x的尾部 (需持有队列锁)
void _queue_push(Queue* x, ListNode* item)
{
    init_list_node(item);

    if (x->sz == 0)
        x->begin = x->end = item;

    else {
        _merge_list(x->end, item);
        x->end = item;
    }

    x->sz++;
}

// 从队列x的头部移除一个结点 (需持有队列锁)
void _queue_pop(Queue* x)
{
    // 确保队列不为空
    if (x->sz == 0)
        PANIC();

    // 如果只有单元素, 则清空队列
    if (x->sz == 1)
        x->begin = x->end = 0;

    // 否则将头部结点移除
    else {
        auto t = x->begin;
        x->begin = x->begin->next;
        _detach_from_list(t);
    }

    // 长度减1
    x->sz--;
}

// 从队列x中移除结点item (需持有队列锁)
void _queue_detach(Queue* x, ListNode* item)
{
    if (x->sz == 0)
        PANIC();

    if (x->sz == 1)
        x->begin = x->end = 0;

    else if (x->begin == item)
        x->begin = x->begin->next;

    else if (x->end == item)
        x->end = x->end->prev;

    _detach_from_list(item);
    x->sz--;
}

// 返回队列x的头部结点
ListNode* queue_front(Queue* x)
{
    if (!x || !x->begin)
        PANIC();
    return x->begin;
}

// 判断队列x是否为空
bool queue_empty(Queue* x) { return x->sz == 0; }

// -------------------------------- QueueNode --------------------------------
// //

// 添加结点node到队列head的前面, 并返回添加的结点 (并发安全)
QueueNode* add_to_queue(QueueNode** head, QueueNode* node)
{
    do
        node->next = *head;
    while (!__atomic_compare_exchange_n(
        head, &node->next, node, true, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED));
    // if (*head == node->next) *head = node; return true;

    return node;
}

// 将结点node从队列head中移除, 并返回移除的结点 (并发安全)
QueueNode* fetch_from_queue(QueueNode** head)
{
    QueueNode* node;

    do
        node = *head;
    while (node
        && !__atomic_compare_exchange_n(
            head, &node, node->next, true, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED));
    // if (*head == node) *head = node->next; return true;

    return node;
}

// 从队列中移除所有结点, 并返回移除的结点 (并发安全)
QueueNode* fetch_all_from_queue(QueueNode** head)
{
    return __atomic_exchange_n(head, NULL, __ATOMIC_ACQ_REL);
    // old = *head; *head = NULL; return old;
}