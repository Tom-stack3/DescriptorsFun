# Descriptor fun Kernel Module

Playing with linux kernel GDT, IDT, TSS and similar random stuff on x86-64.

Why? After learning the awesome *OpenSecurityTraining2 Architecture 2001: x86-64 OS Internals* course, I've felt like it.

## Patching the IDT

I wanted to change the IDT. So I looked for what interrupt entry to hook on.
Naturally looked on the first one, INT0. And thought that it actually might be cool to just decide that from now on, division by `0` won't be an exception. Instead, it would just equal to `0`. And that's possible of course, because I'll hook on `INT0` handler. Cool!

A problem I've ran into is KPTI. It causes my Kernel module's memory to not be mapped when in userspace context. Which then caused a `#PF` when an interrupt was invoked from userspace.
I've decided to turn it off. Another solution might have been patching the current `asm_exc_divide_error` (`INT0` handler), with a change to CR3 and a jump to my code, or just running my code there. But I wanted to change the IDT!! So I did the other way :smile:.


## Random debugging utils

```bash
mount -t 9p -o trans=virtio,version=9p2000.L mods /mnt
insmod /mnt/dscfun/dscfun.ko
/mnt/dscfun/zero.o
```

```bash
cd $TOP/build/linux-x86-basic
gdb vmlinux
# GDB:
target remote :1234
b asm_exc_divide_error

b *0x401815
b asm_exc_page_fault
x/8gx $rsp
```

```gdb
add-symbol-file ../../kmodules/dscfun/dscfun.ko 0xffffffffc0000000
```

Found the PF!!!
```bash
(gdb) x/8gx $rsp
0xfffffe0000002fa0:     0x0000000000000010	0xffffffffc00000a0
0xfffffe0000002fb0:     0x0000000000000010	0x0000000000010046
0xfffffe0000002fc0:     0xfffffe0000002fd8	0x0000000000000000
0xfffffe0000002fd0:     0x000000000000000b	0x0000000000401815

0xffffffff81001280
0x0000000000401815
```


```bash
grep "cpu_insecure\|cpu_meltdown\|kaiser" /proc/cpuinfo
```
