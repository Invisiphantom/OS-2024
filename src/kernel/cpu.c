#include <kernel/cpu.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <kernel/proc.h>
#include <aarch64/mmu.h>
#include <driver/timer.h>
#include <driver/clock.h>

struct cpu cpus[NCPU];

// 定时器比较函数
// true:  lnode < rnode
// false: lnode >= rnode
static bool __timer_cmp(rb_node lnode, rb_node rnode)
{
    i64 d = container_of(lnode, struct timer, _node)->_key
        - container_of(rnode, struct timer, _node)->_key;

    // (lkey < rkey)
    if (d < 0)
        return true;

    // (lkey == rkey)
    // 键值相等时, 比较地址
    if (d == 0)
        return lnode < rnode;

    // (lkey > rkey)
    return false;
}

// 通过定时红黑树 更新定时器值
static void __timer_set_clock()
{
    // 获取红黑树中的最左侧结点 (最小时间戳)
    auto node = _rb_first(&cpus[cpuid()].timer);

    // 如果树为空, 则设置定时器为0.01秒
    if (!node) {
        reset_clock(10);
        return;
    }

    // 获取最接近的 定时中断时间戳
    auto t1 = container_of(node, struct timer, _node)->_key;

    // 获取当前时间戳
    auto t0 = get_timestamp_ms();

    // 更新定时器
    if (t1 <= t0)
        reset_clock(0);
    else
        reset_clock(t1 - t0);
}

// clock.c->invoke_clock_handler 跳转到这里
static void timer_clock_handler()
{
    // 重置定时器为0.01秒
    reset_clock(10);

    for (;;) {
        // 获取红黑树中的最左侧结点
        auto node = _rb_first(&cpus[cpuid()].timer);

        // 如果红黑树为空, 则退出
        if (node == NULL)
            break;

        // 获取当前最小的定时器时间戳
        auto timer = container_of(node, struct timer, _node);

        // 如果还没到触发时间, 则退出
        if (get_timestamp_ms() < timer->_key)
            break;

        // 从CPU定时红黑树卸载timer
        cancel_cpu_timer(timer);

        // 设置触发标志
        timer->triggered = true;

        // 调用定时器处理函数
        timer->handler(timer);
    }
}

// 初始化定时器中断处理函数 (main.c调用)
void init_clock_handler() { set_clock_handler(&timer_clock_handler); }

static struct timer hello_timer[4];

static void hello(struct timer* t)
{
    t->data++;                             // 增加触发计数
    printk("CPU %lld: living\n", cpuid()); // 打印CPU编号
    set_cpu_timer(&hello_timer[cpuid()]);  // 重新挂载hello_timer
}

// 把timer挂载到CPU的定时红黑树上
void set_cpu_timer(struct timer* timer)
{
    _arch_disable_trap(); // * 关闭中断

    // 如果已存在, 则移除原来的定时器
    if (_rb_lookup(&timer->_node, &cpus[cpuid()].timer, __timer_cmp))
        _rb_erase(&timer->_node, &cpus[cpuid()].timer);

    // 清除触发标志
    timer->triggered = false;

    // 设置键值为 当前时间+间隔时间
    timer->_key = get_timestamp_ms() + timer->elapse;

    // 将结点插入 定时红黑树
    ASSERT(0 == _rb_insert(&timer->_node, &cpus[cpuid()].timer, __timer_cmp));

    // 通过定时红黑树 更新定时器值
    __timer_set_clock();

    _arch_enable_trap(); // * 开启中断
}

// 从定时红黑树中删除结点timer
void cancel_cpu_timer(struct timer* timer)
{
    _arch_disable_trap(); // * 关闭中断

    // 确保此时触发标志为false
    ASSERT(timer->triggered == false);

    // 如果存在, 则从定时红黑树中删除结点timer
    if (_rb_lookup(&timer->_node, &cpus[cpuid()].timer, __timer_cmp))
        _rb_erase(&timer->_node, &cpus[cpuid()].timer);

    // 通过定时红黑树 更新定时器值
    __timer_set_clock();

    _arch_enable_trap(); // * 开启中断
}

void set_cpu_on()
{
    // 确保此时中断是关闭的
    ASSERT(_arch_disable_trap() == false);

    // 关闭低地址页表映射
    extern PTEntries invalid_pt;
    arch_set_ttbr0(K2P(&invalid_pt));

    // 设置vbar异常处理函数地址
    extern char exception_vector[];
    arch_set_vbar(exception_vector);

    // 重置esr异常状态信息
    arch_reset_esr();

    // 初始化定时器 (1秒)
    init_clock();

    // 标记当前CPU已上线
    cpus[cpuid()].online = true;

    // 打印欢迎信息
    printk("CPU %lld: hello\n", cpuid());

    // 配置hello_timer 并挂载到CPU定时红黑树上
    hello_timer[cpuid()].elapse = 5000;   // 间隔为5秒
    hello_timer[cpuid()].handler = hello; // 定时器处理函数
    set_cpu_timer(&hello_timer[cpuid()]);
}

void set_cpu_off()
{
    // 关闭当前CPU中断
    _arch_disable_trap();

    // 停用当前CPU
    cpus[cpuid()].online = false;

    // 打印结束信息
    printk("CPU %lld: stopped\n", cpuid());
}
