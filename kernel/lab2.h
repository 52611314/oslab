#ifndef LAB2_H
#define LAB2_H

/* Bare 模式的物理布局，不启用页表。
 * 用户程序共用 64 KiB 执行区，切换时由 proc.c 保存和恢复内存快照。
 * trapframe 固定在 QEMU 128 MiB RAM 的末页，供 trampoline 汇编访问。
 */
#define LAB2_USER_BASE 0x81000000
#define LAB2_USER_SIZE 65536
#define LAB2_TRAPFRAME 0x87fff000

// 汇编只使用地址常量，C 函数声明不能送入汇编器。
#ifndef __ASSEMBLER__
void trapinit(void);
void usertrap(void);
void usertrapret(void) __attribute__((noreturn));
void syscall(void);
int userrange(uint64 addr, uint64 len);
#endif
#endif
