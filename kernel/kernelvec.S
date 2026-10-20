.section .text
# stvec 入口至少 4 字节对齐。
.balign 4
.globl kernelvec
kernelvec:
# 沿用当前内核栈，分配 256 字节保持 16 字节对齐。
  addi sp, sp, -256
# 保存除 sp/x0 外全部通用寄存器，防止处理函数破坏被打断代码的现场。
  sd ra, 0(sp)
  sd gp, 8(sp)
  sd tp, 16(sp)
  sd t0, 24(sp)
  sd t1, 32(sp)
  sd t2, 40(sp)
  sd s0, 48(sp)
  sd s1, 56(sp)
  sd a0, 64(sp)
  sd a1, 72(sp)
  sd a2, 80(sp)
  sd a3, 88(sp)
  sd a4, 96(sp)
  sd a5, 104(sp)
  sd a6, 112(sp)
  sd a7, 120(sp)
  sd s2, 128(sp)
  sd s3, 136(sp)
  sd s4, 144(sp)
  sd s5, 152(sp)
  sd s6, 160(sp)
  sd s7, 168(sp)
  sd s8, 176(sp)
  sd s9, 184(sp)
  sd s10, 192(sp)
  sd s11, 200(sp)
  sd t3, 208(sp)
  sd t4, 216(sp)
  sd t5, 224(sp)
  sd t6, 232(sp)
# C 函数处理设备，并保存/恢复陷阱返回地址 sepc 和状态 sstatus。
  call kerneltrap
# 按原偏移恢复寄存器；撤销栈空间即可恢复原 sp。
  ld ra, 0(sp)
  ld gp, 8(sp)
  ld tp, 16(sp)
  ld t0, 24(sp)
  ld t1, 32(sp)
  ld t2, 40(sp)
  ld s0, 48(sp)
  ld s1, 56(sp)
  ld a0, 64(sp)
  ld a1, 72(sp)
  ld a2, 80(sp)
  ld a3, 88(sp)
  ld a4, 96(sp)
  ld a5, 104(sp)
  ld a6, 112(sp)
  ld a7, 120(sp)
  ld s2, 128(sp)
  ld s3, 136(sp)
  ld s4, 144(sp)
  ld s5, 152(sp)
  ld s6, 160(sp)
  ld s7, 168(sp)
  ld s8, 176(sp)
  ld s9, 184(sp)
  ld s10, 192(sp)
  ld s11, 200(sp)
  ld t3, 208(sp)
  ld t4, 216(sp)
  ld t5, 224(sp)
  ld t6, 232(sp)
  addi sp, sp, 256
# 返回被打断的 S 态代码。
  sret
