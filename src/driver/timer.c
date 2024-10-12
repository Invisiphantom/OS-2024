#include <common/defines.h>
#include <driver/interrupt.h>
#include <aarch64/intrinsic.h>
#include <kernel/printk.h>

#define CNTV_CTL_ENABLE (1 << 0)
#define CNTV_CTL_IMASK (1 << 1)
#define CNTV_CTL_ISTATUS (1 << 2)

// 启用定时器功能
void enable_timer()
{
    u64 c = get_cntv_ctl_el0();
    c |= CNTV_CTL_ENABLE; // 启用定时器
    c &= ~CNTV_CTL_IMASK; // 清除定时器中断屏蔽位
    set_cntv_ctl_el0(c);
}

// 禁用定时器功能
void disable_timer()
{
    u64 c = get_cntv_ctl_el0();
    c &= ~CNTV_CTL_ENABLE; // 禁用定时器
    c |= CNTV_CTL_IMASK;   // 设置定时器中断屏蔽位
    set_cntv_ctl_el0(c);
}

// 判断定时器是否启用
bool timer_enabled()
{
    u64 c = get_cntv_ctl_el0();
    return c & 1;
}