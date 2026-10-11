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
# kernel/entry.S
# 启动入口：QEMU 上电后 PC = 0x1000，CPU 在 M 态
#include "course_sid.h"
.section .text
.globl _entry

_entry:
    # ---- 1. 关中断 ----
    li t0,8
    csrc mstatus,t0

    # ---- 2. 设置初始内核栈 ----
    la sp, stack0
    li t0, LAB1_STACK_KB * 1024
    add sp, sp, t0

    # ---- 3. 读 mhartid，如果非0跳转到 park ----
    csrr t0, mhartid
    bnez t0, park

    # ---- 4. 设置 mtvec ----
    la t0, trap_entry
    csrw mtvec, t0

    # ---- 5. 跳转到 start() ----
    call start

    # 如果 start 返回了，死循环（正常不会返回）
park:
    wfi
    j park