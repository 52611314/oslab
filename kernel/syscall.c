#include "types.h"
#include "proc.h"
#include "syscall.h"
#include "console.h"
// 系统调用 ABI：a7 是编号，a0-a2 是本阶段使用的前三个参数。
static uint64 argaddr(int n) {
  switch (n) {
  case 0: return myproc()->tf.a0;
  case 1: return myproc()->tf.a1;
  default: return myproc()->tf.a2;
  }
}
// 将参数按整数解释，例如 fd、长度和退出码。
static int argint(int n) { return (int)argaddr(n); }
// 有界复制用户字符串，逐字节检查地址，必须在容量内遇到终止符。
static int argstr(int n, char *dst, int size) {
  uint64 addr = argaddr(n);
  for (int i = 0; i < size; i++) {
    if (!userrange(addr + i, 1)) return -1;
    char c = *(char *)(addr + i);
    // 赠送 gets 保留换行，sh 直接交给 exec，因此将行尾也视为程序名终止。
    if (!c || c == '\n' || c == '\r') { dst[i] = 0; return 0; }
    dst[i] = c;
  }
  return -1;
}
// 分发七个必需调用；未知或未实现编号沿用默认返回值 -1。
void syscall(void) {
  struct proc *p = myproc();
  long result = -1;
  switch (p->tf.a7) {
  case SYS_fork: result = procfork(); break;
  case SYS_exit: procexit(argint(0));
  case SYS_wait: result = procwait(argaddr(0)); break;
  case SYS_exec: {
    // Lab2 只按名称加载内嵌程序，课件允许此阶段忽略 argv。
    char name[64];
    if (argstr(0, name, sizeof(name)) == 0) result = procexec(name);
    break;
  }
  case SYS_getpid: result = p->pid; break;
  case SYS_read:
  case SYS_write: {
    int fd = argint(0), n = argint(2);
    uint64 addr = argaddr(1);
    // 拒绝负长度及用户执行区外的内存，然后才进行输入输出。
    if (n < 0 || !userrange(addr, (uint64)n)) break;
    if (p->tf.a7 == SYS_read) {
      // 只实现标准输入 fd=0，无输入时 consoleread 让进程睡眠。
      if (fd == 0) result = consoleread((char *)addr, n);
    } else if (fd == 1 || fd == 2) {
      // 标准输出/错误 fd=1/2，逐字符调用 Lab1 串口输出。
      for (int i = 0; i < n; i++) consoleputc(((char *)addr)[i]);
      result = n;
    }
    break;
  }
  default: break;
  }
  // 返回值写入保存的 a0，trampoline 恢复后用户代码即可获得结果。
  p->tf.a0 = (uint64)result;
}
