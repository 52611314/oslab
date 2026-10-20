#ifndef CONSOLE_H
#define CONSOLE_H
void consoleinit(void);
void consoleputc(char c);
void uartputc_sync(char c);
// UART 接收中断入口：读取、回显、入队，并唤醒等待输入的进程。
void consoleintr(void);
// read(0, ...) 的实现：无输入时睡眠，有输入时复制就绪数据。
int consoleread(char *dst, int n);
#endif