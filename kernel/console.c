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
#include "memlayout.h"
#include "console.h"
#include "course_sid.h"
#include "proc.h"

#define LSR 5
#define THR 0
#define THROTTLE_PERIOD (16 + COURSE_SID % 16)

static int throttle_count = 0;
// r 为累计读位置，w 为累计写位置，ready 为可读边界；数组下标对容量取模。
// w-r 为未消费字节数；个人 LAB2_BUF_SIZE 为 64。
static struct {
    char buf[LAB2_BUF_SIZE];
    uint64 r, w, ready;
} input;

// 定义 uartputc_sync
void uartputc_sync(char c) {
    while ((*(volatile uint8*)(UART0 + LSR) & (1 << 5)) == 0) { }
    *(volatile uint8*)(UART0 + THR) = c;

    if (++throttle_count >= THROTTLE_PERIOD) {
        throttle_count = 0;
        for (volatile int i = 0; i < 1000; i++) {
            __asm__ __volatile__("nop");
        }
    }
}

// 定义 consoleputc
void consoleputc(char c) {
    uartputc_sync(c);
}

// 配置 UART 接收与 PLIC 路由，串口输出仍使用 Lab1 的轮询方式。
void consoleinit(void) {
    // 先关闭 UART 中断，避免初始化途中触发接收处理。
    *(volatile uint8 *)(UART0 + 1) = 0;
    *(volatile uint8 *)(UART0 + 3) = 3; // eight-bit characters, DLAB clear
    *(volatile uint8 *)(UART0 + 2) = 7; // enable and clear RX/TX FIFOs
    // 清空输入缓冲状态。
    input.r = input.w = input.ready = 0;
    // UART 源优先级为 1，在 hart 0 的 S 态上下文使能，阈值为 0。
    *(volatile uint *)(PLIC_PRIORITY + UART0_IRQ * 4) = 1;
    *(volatile uint *)PLIC_SENABLE(0) |= 1U << UART0_IRQ;
    *(volatile uint *)PLIC_SPRIORITY(0) = 0;
    *(volatile uint8 *)(UART0 + 1) = 1; // receive interrupts only
}

// 一次中断尽量排空 UART 接收 FIFO。
void consoleintr(void) {
    while (*(volatile uint8 *)(UART0 + LSR) & 1) {
        char c = *(volatile uint8 *)UART0;
        // 将回车统一为换行；随后是键盘输入回显，与用户程序 write 的输出不同。
        if (c == '\r') c = '\n';
        consoleputc(c);
        // 未满才入队；满时丢弃新字符，不覆盖未读数据。
        if (input.w - input.r < LAB2_BUF_SIZE)
            input.buf[input.w++ % LAB2_BUF_SIZE] = c;
        // 个人参数为字符流：收到字符即可读取；行模式才等换行。
        if (LAB2_BUF_SEMANTICS == 1 || c == '\n') {
            input.ready = input.w;
            // 用 input.r 的地址标识输入事件，唤醒阻塞在 read 的进程。
            procwakeup(&input.r);
        }
    }
}

// 返回当前可读数据；字符流模式不保证一次 read 对应完整一行。
int consoleread(char *dst, int n) {
    if (!n) return 0;
    // 单核下检查条件和登记睡眠期间保持关中断，避免漏掉一次唤醒。
    while (input.r == input.ready) procsleep(&input.r);
    int copied = 0;
    while (copied < n && input.r != input.ready) {
        // 消费一个字节并推进 r；每个入队字节最多交给用户程序一次。
        char c = input.buf[input.r++ % LAB2_BUF_SIZE];
        dst[copied++] = c;
        if (LAB2_BUF_SEMANTICS == 0 && c == '\n') break;
    }
    return copied;
}