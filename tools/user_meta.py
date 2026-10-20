"""Keep real ELF entry/BSS metadata separate from the supplied table ABI."""
# 编译时工具：保留真实入口和含 BSS 的大小，不改变赠送表 ABI。
import pathlib
import struct
import sys
print("/* Generated from user ELF files. */")
print("struct usermeta { const char *name; uint64 entry, memsize; };")
print("static const struct usermeta user_meta[] = {")
# 命令行传入程序名；Makefile 将输出写入 kernel/user_meta.h。
for name in sys.argv[1:]:
    data = pathlib.Path(f"user-flat/{name}.elf").read_bytes()
    # 按小端 ELF64 格式解码，检查 RISC-V 机器编号 243。
    hdr = struct.unpack_from("<16sHHIQQQIHHHHHH", data)
    if hdr[0][:6] != b"\x7fELF\x02\x01" or hdr[2] != 243:
        raise SystemExit(f"not a little-endian RISC-V ELF64: {name}")
    # e_entry 为真实入口；后三项定位程序头，不假定入口等于二进制首地址。
    entry, phoff, phsize, phcount = hdr[4], hdr[5], hdr[9], hdr[10]
    loads = []
    for i in range(phcount):
        ph = struct.unpack_from("<IIQQQQQQ", data, phoff + i * phsize)
        # PT_LOAD=1，p_memsz 包含不占文件字节的 BSS，供内存范围检查使用。
        if ph[0] == 1:
            loads.append((ph[3], ph[3] + ph[6]))
    # 计算可加载段覆盖范围，检查约定物理基址与入口合法性。
    base = min(p[0] for p in loads)
    end = max(p[1] for p in loads)
    if base != 0x81000000 or not base <= entry < end:
        raise SystemExit(f"incorrect bare-mode user address: {name}")
    # 输出名字、入口和内存占用，供 procexec 加载程序。
    print(f'  {{"{name}", 0x{entry:x}UL, {end - base}UL}},')
print("};")
