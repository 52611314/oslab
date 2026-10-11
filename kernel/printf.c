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
#include "console.h"
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)
// 输出一个字符
static void putc(char c) {
    consoleputc(c);
}

// 输出字符串
static void puts(const char *s) {
    while (*s) {
        putc(*s++);
    }
}

// 输出无符号整数，base 10 或 16
static void printuint(uint64 x, int base) {
    char buf[32];
    int i = 0;
    if (x == 0) {
        putc('0');
        return;
    }
    while (x > 0) {
        int d = x % base;
        buf[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        x /= base;
    }
    while (i > 0) {
        putc(buf[--i]);
    }
}

// 输出有符号整数
static void printint(int64 x) {
    if (x < 0) {
        putc('-');
        printuint((uint64)(-x), 10);
    } else {
        printuint((uint64)x, 10);
    }
}

// 最小 printf，支持 %d %x %s %c %% %l
void printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            putc(*p);
            continue;
        }
        p++;
        switch (*p) {
        case 'd':
            printint(va_arg(ap, int));
            break;
        case 'x':
            printuint(va_arg(ap, uint64), 16);
            break;
        case 's': {
            char *s = va_arg(ap, char*);
            if (s == 0) s = "(null)";
            puts(s);
            break;
        }
        case 'c':
            putc((char)va_arg(ap, int));
            break;
        case '%':
            putc('%');
            break;
        case 'l':
            p++;
            if (*p == 'd') {
                // 按 int64 输出
                int64 v = va_arg(ap, int64);
                if (v < 0) {
                    putc('-');
                    printuint((uint64)(-v), 10);
                } else {
                    printuint((uint64)v, 10);
                }
            } else if (*p == 'u') {
                printuint(va_arg(ap, uint64), 10);
            } else {
                putc('%');
                putc('l');
                putc(*p);
            }
            break;
        default:
            putc('%');
            putc(*p);
            break;
        }
    }
    va_end(ap);
}