.DEFAULT_GOAL := kernel/kernel
# lab1 骨架 Makefile(课程提供;新增源文件时把它加进 OBJS)
CC   = riscv64-unknown-elf-gcc
LD   = riscv64-unknown-elf-ld
CFLAGS = -Wall -Werror -O -std=gnu99 -mcmodel=medany -ffreestanding \
         -nostdlib -fno-common -ggdb -march=rv64gc -fno-stack-protector -fno-pie
QEMU = qemu-system-riscv64

# 将 Lab2 进程、系统调用、陷阱入口、上下文切换和内嵌程序链接进内核。
OBJS = kernel/entry.o kernel/start.o kernel/console.o kernel/printf.o kernel/main.o \
       kernel/proc.o kernel/syscall.o kernel/trap.o kernel/kernelvec.o \
       kernel/swtch.o kernel/trampoline.o kernel/userimg.o

.PHONY: kernel qemu
kernel: kernel/kernel

kernel/kernel: $(OBJS) kernel/kernel.ld
	$(LD) -T kernel/kernel.ld -o $@ $(OBJS)

%.o: %.c kernel/riscv.h kernel/course_sid.h
	$(CC) $(CFLAGS) -Ikernel -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -Ikernel -c -o $@ $<

# 复用课程构建规则，并在依赖展开前加入两项官方测试。
include Makefile.upgrade
UPROGS += badecall bufstorm

kernel/syscall.o kernel/console.o kernel/trap.o: kernel/console.h
kernel/proc.o: kernel/user_meta.h
kernel/proc.o kernel/syscall.o kernel/trap.o kernel/console.o kernel/main.o: kernel/proc.h kernel/lab2.h

# 从标准输入编译，使 kernel/bare 的物理地址适配头优先，赠送汇编保持原样。
# 将 trampsec 标记为可装载代码，使其进入最终镜像。
kernel/trampoline.o: kernel/trampoline.S kernel/bare/memlayout.h kernel/lab2.h
	$(CC) $(CFLAGS) -Ikernel/bare -Ikernel -x assembler-with-cpp -c -o $@ - < $<
	$(OBJCOPY) --set-section-flags trampsec=alloc,load,readonly,code $@

# 不改赠送 user.ld，通过链接参数将用户代码定位到物理执行区。
user-flat/%.bin: LD = riscv64-unknown-elf-ld --section-start=.text=0x81000000

# 生成入口与内存大小表，写完临时文件再替换，避免半成品。
kernel/user_meta.h: tools/user_meta.py $(addprefix user-flat/,$(UPROGS:=.bin))
	python3 tools/user_meta.py $(UPROGS) > $@.tmp
	mv $@.tmp $@

# 验收环境固定(排雷环节禁止改动本行以下内容)
qemu: kernel/kernel
	$(QEMU) -machine virt -bios none -kernel kernel/kernel -nographic

clean:
	rm -f kernel/*.o kernel/kernel
