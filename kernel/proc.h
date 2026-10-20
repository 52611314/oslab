#ifndef PROC_H
#define PROC_H
#include "lab2.h"

struct trapframe {
  uint64 kernel_satp, kernel_sp, kernel_trap, epc, kernel_hartid;
  uint64 ra, sp, gp, tp, t0, t1, t2, s0, s1;
  uint64 a0, a1, a2, a3, a4, a5, a6, a7;
  uint64 s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
  uint64 t3, t4, t5, t6;
};
// 内核函数切换现场：ra、sp、s0-s11 的顺序与 swtch.S 一致。
struct context {
  uint64 ra, sp, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
};
// 空闲、可运行、正在运行、等待事件、已退出待回收。
enum procstate { UNUSED, RUNNABLE, RUNNING, SLEEPING, ZOMBIE };
struct proc {
  enum procstate state;       // 生命周期状态，由调度、睡眠、唤醒和退出代码更新。
  int pid, status;            // 进程编号与退出码。
  struct proc *parent;        // 父进程，供 wait/exit 配合使用。
  void *chan;                // 等待事件标识，相同地址代表同一事件。
  struct context context;    // 该进程的内核函数执行现场。
  struct trapframe tf;       // 该进程的用户寄存器和返回地址。
  uchar kstack[4096] __attribute__((aligned(16))); // 独立内核栈。
  uchar image[LAB2_USER_SIZE]; // 用户内存快照，包含代码、数据、BSS 和用户栈。
};
void procinit(void);
void userinit(void);
void scheduler(void) __attribute__((noreturn));
struct proc *myproc(void);
int procfork(void);
int procexec(const char *name);
int procwait(uint64 status);
void procexit(int status) __attribute__((noreturn));
void procsleep(void *chan);
void procwakeup(void *chan);
void swtch(struct context *old, struct context *next);
#endif
