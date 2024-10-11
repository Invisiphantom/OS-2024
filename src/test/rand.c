#include <aarch64/intrinsic.h>
#include <common/defines.h>

static u64 next[4] = { 1111, 2222, 3333, 4444 };

// 生成32位随机数 (线性同余法)
unsigned rand(void)
{
    // RAND_MAX assumed to be 32767
    next[cpuid()] = next[cpuid()] * 1103515245 + 12345;
    return (unsigned int)(next[cpuid()] / 65536) % 32768;
}

void srand(unsigned seed)
{
    next[cpuid()] = seed;
}