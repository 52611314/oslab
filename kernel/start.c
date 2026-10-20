/*
 * lab1 初始骨架代码(自动生成): 系统启动与串口控制台输出。
 * 启动至此的前期初始化流程，需要由你在本实验中设计并实现。
 * 你需要实现: entry.S(start 前的 M 态准备可另置 start.c)、串口轮询输出、
 * 最小 printf。链接脚本 kernel.ld 带注释保留; 底层宏 riscv.h 完整保留。
 * 代码导读路线与设计引导问题详见《实验说明书(lab1)》。
 *
 * 两个环境注意事项(说明书 §2"环境前置条件"与附录 C, 动手前必读):
 *  1. start() 的 M→S 切换清单必须包含 PMP 配置(最简两行):
 *       w_pmpaddr0(0x3fffffffffffffull); w_pmpcfg0(0xf);
 *     否则在新版 QEMU 上 mret 进 S 态的第一条取指即触发 fault(全程无输出)。
 *  2. entry.S 里的陷阱向量标号前加 .balign 4(mtvec 要求 4 字节对齐,
 *     不满足时写入会被硬件静默丢弃)。
 */
#include "types.h"
#include "riscv.h"
#include "course_sid.h"

// 启动栈按 16 字节对齐，容量沿用个人 Lab1 参数。
char stack0[LAB1_STACK_KB * 1024] __attribute__((aligned(16)));

// 陷阱入口占位
__attribute__((aligned(4)))
void trap_entry(void) {
    while (1) { }
}

extern void main(void);

void start(void) {
    // 1. 配置 mstatus.MPP = S，
    uint64 x = r_mstatus();
    x &= ~MSTATUS_MPP_MASK;
    x |= MSTATUS_MPP_S;
    w_mstatus(x);

    // 2. 设置 mepc = main
    w_mepc((uint64)main);

    // 3. 配置 PMP：允许 S/U 态访问全部物理地址
    w_pmpaddr0(0x3fffffffffffffull);
    w_pmpcfg0(0xf);

    // 4. 委托所有中断和异常给 S 态
    w_medeleg(0xffff);
    w_mideleg(0xffff);

    // 5. 关闭 S 态中断
    w_sie(0);

    // Lab2 使用 Bare 模式（satp=0），内核和用户程序直接使用物理地址。
    w_satp(0);
    // menvcfg.STCE 允许 S 态通过 stimecmp 设置时钟中断期限。
    w_menvcfg(r_menvcfg() | MENVCFG_STCE);
    // mcounteren 第 1 位 TM：允许 S 态读取 time 计数器。
    w_mcounteren(r_mcounteren() | 2);
    // 开放 S 态时钟中断来源；S 态还要在 trapinit 中设置 sie.STIE。
    w_mie(r_mie() | MIE_STIE);
    // tp 保存 hart 编号，供内核使用；本阶段只运行 hart 0。
    w_tp(r_mhartid());

    // 6. 执行 mret，降权到 S 态，PC = main
    asm volatile("mret");
}