#include "types.h"
#include "riscv.h"
#include "proc.h"
#include "printf.h"
#include "user_meta.h"

static struct proc procs[2];
// 当前运行进程；调度器空闲时为 0。
static struct proc *current;
// 调度器自己的内核现场，用于从进程睡眠或退出处返回。
static struct context schedctx;
static int nextpid = 1;
// Makefile.upgrade 生成的内嵌表，包含二进制起止地址和程序名。
extern uint64 _uprog_table[];
// 按字节复制，用于加载程序以及保存和恢复用户内存。
static void copy(void *dst, const void *src, uint64 n) {
  uchar *d = dst;
  const uchar *s = src;
  while (n--) *d++ = *s++;
}
// 清零结构体或用户执行区，使未初始化数据从零开始。
static void zero(void *dst, uint64 n) {
  uchar *d = dst;
  while (n--) *d++ = 0;
}
// 精确比较程序名。
static int same(const char *a, const char *b) {
  while (*a && *a == *b) { a++; b++; }
  return *a == *b;
}
// 选空闲槽，设置 pid 和首次内核现场；由调用者设置可运行状态。
static struct proc *allocproc(void) {
  for (int i = 0; i < 2; i++) {
    struct proc *p = &procs[i];
    if (p->state != UNUSED) continue;
    zero(p, sizeof(*p));
    p->pid = nextpid++;
    // 首次 swtch 到新进程时，ret 跳到 usertrapret，sp 切到该进程内核栈顶。
    p->context.ra = (uint64)usertrapret;
    p->context.sp = (uint64)(p->kstack + sizeof(p->kstack));
    return p;
  }
  return 0;
}
// 检查用户地址与长度；使用减法判断长度，避免 addr+len 溢出。
int userrange(uint64 addr, uint64 len) {
  return addr >= LAB2_USER_BASE && addr <= LAB2_USER_BASE + LAB2_USER_SIZE &&
         len <= LAB2_USER_BASE + LAB2_USER_SIZE - addr;
}
// 单核实现通过 current 获取当前进程。
struct proc *myproc(void) { return current; }
// 清零后各进程槽的 state 为 UNUSED。
void procinit(void) {
  zero(procs, sizeof(procs));
  zero(&schedctx, sizeof(schedctx));
}
// exec 替换用户程序，保留当前进程的 pid、父子关系及内核栈。
int procexec(const char *name) {
  uint64 *row = _uprog_table;
  while (row[0] && row[1]) {
    if (same(name, (char *)(row + 2))) break;
    // 每项表后紧跟二进制，下一项位于二进制末尾的 8 字节对齐处。
    row = (uint64 *)((row[1] + 7) & ~7UL);
  }
  if (!row[0] || !row[1]) return -1;
  // 平铺二进制没有 ELF 头，另查编译时生成的真实入口与含 BSS 的内存大小。
  const struct usermeta *meta = 0;
  for (uint i = 0; i < sizeof(user_meta) / sizeof(user_meta[0]); i++)
    if (same(name, user_meta[i].name)) { meta = &user_meta[i]; break; }
  uint64 size = row[1] - row[0];
  // 执行区末尾预留 4 KiB 用户栈
  if (!meta || size > LAB2_USER_SIZE - 4096 ||
      meta->memsize > LAB2_USER_SIZE - 4096) return -1;
  // 清空整个执行区，复制二进制内容。
  zero((void *)LAB2_USER_BASE, LAB2_USER_SIZE);
  copy((void *)LAB2_USER_BASE, (void *)row[0], size);
  // 重建用户寄存器
  zero(&current->tf, sizeof(current->tf));
  current->tf.epc = meta->entry;
  current->tf.sp = LAB2_USER_BASE + LAB2_USER_SIZE;
  return 0;
}
// 建立第一个用户进程，加载 sh 并保存其初始内存快照。
void userinit(void) {
  current = allocproc();
  if (!current || procexec("sh") < 0) {
    printf("lab2: cannot load sh\n");
    for (;;) asm volatile("wfi");
  }
  // image 保存用户内存；下面的 state 则记录生命周期状态，两者含义不同。
  copy(current->image, (void *)LAB2_USER_BASE, LAB2_USER_SIZE);
  current->state = RUNNABLE;
  current = 0;
}
// 最小单核调度
void scheduler(void) {
  for (;;) {
    // 修改状态、current 和用户内存期间关中断
    intr_off();
    int ran = 0;
    for (int i = 0; i < 2; i++) {
      struct proc *p = &procs[i];
      if (p->state != RUNNABLE) continue;
      ran = 1;
      current = p;
      // 运行前恢复该进程用户内存，登记为正在运行。
      copy((void *)LAB2_USER_BASE, p->image, LAB2_USER_SIZE);
      p->state = RUNNING;
      // 保存调度器 ra/sp/s0-s11，恢复进程内核现场；实际保存指令在 swtch.S。
      swtch(&schedctx, &p->context);
      // 进程睡眠或退出切回后，保存它的最新用户内存。
      copy(p->image, (void *)LAB2_USER_BASE, LAB2_USER_SIZE);
      current = 0;
    }
    // 无可运行进程时开中断并等待，UART 中断可唤醒等待读取的进程。
    if (!ran) {
      intr_on();
      asm volatile("wfi");
    }
  }
}
void procsleep(void *chan) {
  // 调用者关中断检查条件；先登记事件和 SLEEPING，再切回调度器。
  current->chan = chan;
  current->state = SLEEPING;
  swtch(&current->context, &schedctx);
  // 被唤醒并重新调度后，swtch 返回这里，清除本次等待事件。
  current->chan = 0;
}
// 将等待同一事件的进程标记为 RUNNABLE，实际运行由 scheduler 选择。
void procwakeup(void *chan) {
  for (int i = 0; i < 2; i++)
    if (procs[i].state == SLEEPING && procs[i].chan == chan)
      procs[i].state = RUNNABLE;
}
// 复制用户寄存器和用户内存；父子进程各有独立内核栈与内核现场。
int procfork(void) {
  struct proc *child = allocproc();
  if (!child) return -1;
  child->parent = current;
  child->tf = current->tf;
  // fork 双返回值：子进程 a0=0，父进程在本函数末尾得到子 pid。
  child->tf.a0 = 0;
  child->tf.kernel_sp = (uint64)(child->kstack + sizeof(child->kstack));
  copy(child->image, (void *)LAB2_USER_BASE, LAB2_USER_SIZE);
  child->state = RUNNABLE;
  return child->pid;
}
// 回收已退出子进程；有未退出子进程则睡眠；无子进程则返回 -1。
int procwait(uint64 status) {
  if (status && !userrange(status, sizeof(int))) return -1;
  for (;;) {
    int children = 0;
    for (int i = 0; i < 2; i++) {
      struct proc *child = &procs[i];
      if (child->state == UNUSED || child->parent != current) continue;
      children = 1;
      if (child->state == ZOMBIE) {
        int pid = child->pid;
        if (status) *(int *)status = child->status;
        // wait 完成回收，释放此槽供下一次 fork 复用。
        child->state = UNUSED;
        return pid;
      }
    }
    if (!children) return -1;
    // 用父进程地址作为等待事件，与子进程 exit 的唤醒地址一致。
    procsleep(current);
  }
}
// 保留退出码并变为 ZOMBIE，唤醒父进程后切回调度器，不再返回用户态。
void procexit(int status) {
  current->status = status;
  current->state = ZOMBIE;
  if (current->parent) procwakeup(current->parent);
  swtch(&current->context, &schedctx);
  for (;;) asm volatile("wfi");
}
