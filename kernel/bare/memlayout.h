/* 仅在编译赠送 trampoline 时使用的适配头文件。
 * 继承原始定义后，将 TRAPFRAME 替换为 Bare 模式的物理地址。
 * Makefile 使本文件优先匹配；原始 memlayout.h 和 trampoline.S 保持原样。
 */
#include "../memlayout.h"
#include "../lab2.h"
#undef TRAPFRAME
#define TRAPFRAME LAB2_TRAPFRAME
