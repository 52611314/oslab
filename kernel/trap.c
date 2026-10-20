#include "types.h"
#include "riscv.h"
#include "memlayout.h"
#include "course_sid.h"
#include "proc.h"
#include "console.h"
#include "printf.h"
extern void kernelvec(void), uservec(void), userret(uint64 satp);
// stimecmp 是下一次中断期限，按个人 LAB2_TICK 增加 time 计数值。
static void nexttick(void) {
  w_stimecmp(r_time() + 1000000UL * LAB2_TICK);
}
// start.c 已授权 S 态时钟，这里安装入口并开启相应中断来源。
void trapinit(void) {
  w_stvec((uint64)kernelvec); // 内核陷阱入口，先保存寄存器。
  nexttick();                // 设置首个期限，建立周期性中断。
  w_sie(SIE_SEIE | SIE_STIE); // 外部与时钟来源，全局开关另由 sstatus 控制。
}
// scause 最高位表示中断；低位 5 是 S 态时钟，9 是 S 态外部中断。
static int devintr(void) {
  uint64 cause = r_scause();
  if (cause == ((1UL << 63) | 5)) {
    // 重设中断期限，不切换时间片。
    nexttick();
    return 1;
  }
  if (cause == ((1UL << 63) | 9)) {
    // claim 取待处理源；处理后写回同一 irq 编号表示 complete。
    uint irq = *(volatile uint *)PLIC_SCLAIM(0);
    if (irq == UART0_IRQ) consoleintr();
    if (irq) *(volatile uint *)PLIC_SCLAIM(0) = irq;
    return 1;
  }
  return 0;
}
// kernelvec 已保存通用寄存器； 并处理设备。
void kerneltrap(void) {
  uint64 epc = r_sepc(), status = r_sstatus();//保存 sepc、sstatus现场
  if (!devintr()) {
    printf("kernel trap cause=0x%x epc=0x%x val=0x%x\n",
           r_scause(), epc, r_stval());
    for (;;) asm volatile("wfi");
  }
  w_sepc(epc);
  w_sstatus(status);
}
// uservec 保存用户寄存器后跳转
void usertrap(void) {
  // 陷入内核态
  w_stvec((uint64)kernelvec);
  struct proc *p = myproc();
  // 现场复制到私有 tf
  p->tf = *(struct trapframe *)LAB2_TRAPFRAME;
  p->tf.epc = r_sepc(); // 补上由硬件记录的sepc
  if (r_scause() == 8) {
    // 如果是系统调用，跳过4字节指令
    p->tf.epc += 4;
    syscall();
  // 中断不推进 epc；无法识别的用户异常结束该进程。
  } else if (!devintr()) {
    printf("user trap pid=%d cause=0x%x epc=0x%x val=0x%x\n",
           p->pid, r_scause(), p->tf.epc, r_stval());
    procexit(1);
  }
  usertrapret();
}
// 准备返回用户态，也是新进程首次被调度的入口。
void usertrapret(void) {
  // 切换 stvec 和重建返回现场期间不能被中断打断。
  intr_off();
  struct proc *p = myproc();
  //uservec 恢复内核地址模式、栈、入口及 hart 编号。
  p->tf.kernel_satp = 0;
  p->tf.kernel_sp = (uint64)(p->kstack + sizeof(p->kstack));
  p->tf.kernel_trap = (uint64)usertrap;
  p->tf.kernel_hartid = 0;
  // 私有用户现场复制回固定地址，供 userret 汇编恢复。
  *(struct trapframe *)LAB2_TRAPFRAME = p->tf;
  w_stvec((uint64)uservec);
  // SPP=0 返回 U 态，SPIE=1 使 sret 后 S 态中断全局开关就绪。
  w_sstatus((r_sstatus() & ~SSTATUS_SPP) | SSTATUS_SPIE);
  w_sepc(p->tf.epc);
  // 参数是 satp 而非 trapframe 地址，0 代表继续使用 Bare 模式。
  userret(0);
  for (;;) asm volatile("wfi");
}
