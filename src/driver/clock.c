#include <aarch64/intrinsic.h>
#include <kernel/sched.h>
#include <driver/base.h>
#include <driver/clock.h>
#include <driver/interrupt.h>
#include <kernel/printk.h>
#include <driver/timer.h>

// 定时器中断处理函数
static struct {
    ClockHandler handler;
} clock;

// 初始化定时器
void init_clock()
{
    enable_timer();    // 启用定时器
    reset_clock(10); // 设置等待0.01秒
}

// 重置定时器 等待时间 (毫秒)
void reset_clock(u64 interval_ms)
{
    // 将时间ms转换为CPU时钟周期数clk
    u64 interval_clk = interval_ms * get_clock_frequency() / 1000;

    // 确保周期数clk不超过最大限度
    ASSERT(interval_clk <= 0x7fffffff);

    // 设置定时器 等待的时钟周期数
    set_cntv_tval_el0(interval_clk);
}

// 配置定时器中断处理函数 (TIMER_IRQ)
// cpu.c->init_clock_handler 跳转到这里
void set_clock_handler(ClockHandler handler)
{
    clock.handler = handler;
    set_interrupt_handler(TIMER_IRQ, invoke_clock_handler);
}

// 调用定时器中断处理函数
// interrupt.c->interrupt_global_handler 跳转到这里
void invoke_clock_handler()
{
    if (!clock.handler)
        PANIC();
    clock.handler();
}

// 获取当前时间戳 (毫秒)
u64 get_timestamp_ms() { return get_timestamp() * 1000 / get_clock_frequency(); }
