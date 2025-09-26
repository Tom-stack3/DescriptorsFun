# Descriptor fun Kernel Module

Playing with linux kernel GDT, IDT, TSS and similar random stuff on x86-64.

Why? After learning the awesome *OpenSecurityTraining2 Architecture 2001: x86-64 OS Internals* course, I've felt like it.

## Patching the IDT

Implemented in `idtfun.c`.

I wanted to change the IDT. So I looked for what interrupt entry to hook on.
Naturally looked on the first one, INT0. And thought that it actually might be cool to just decide that from now on, division by `0` won't be an exception. Instead, it would just equal to `0`. And that's possible of course, because I'll hook on `INT0` handler. Cool!

A problem I've ran into is KPTI. It causes my Kernel module's memory to not be mapped when in userspace context. Which then caused a `#PF` when an interrupt was invoked from userspace.
I've decided to turn it off. Another solution might have been patching the current `asm_exc_divide_error` (`INT0` handler), with a change to CR3 and a jump to my code, or just running my code there. But I wanted to change the IDT!! So I did the other way :smile:.

## Hooking on Syscalls

Implemented in `syscallfun.c`.

I wanted to hook on syscalls. For that, I've changed `IA32_LSTAR` MSR to point on my handler. This way for every `syscall` opcode, the CPU will jump to my handler.
In my handler I wanted to log the syscall happening in an array of `syscall_stat` structs, then jump to the real handler (original value of `IA32_LSTAR`).

Interesting problems and bugs that I've ran into:
- For logging stuff on the syscall to my array, I had to use a couple of registers. For that, I had to save their original value on the stack. For that, I had to use a stack. I forgot that unlike Interrupt handling, where the CPU switches the stack to point to the specified stack in the IDT entry (some stack from the `TSS`), in `syscall` opcode, the CPU doesn't do that. So your'e left with the user stack if a syscall is called from the user.
To tackle it, the kernel uses `swapgs` to access `IA32_KERNEL_GS_BASE` which points to a kernel struct containing `current` struct. This struct contains a pointer to the `TSS`, in which the kernel stores the current top of the kernel stack.
So for me to use a stack in my handler, I did exactly what the real handler does. `swapgs`, store the user `%rsp` in a temp place in the `TSS`, move `TSS->current top of kernel stack` into `%rsp`.
  - Note that the real kernel syscall handler (`arch/x86/entry/entry_64.S @ SYM_CODE_START(entry_SYSCALL_64)`) uses variable macros and relocatable offsets to access the variables in the kernel's `GS`, such as the spare place in `TSS` to store the top of the kernel stack. I didn't do that. Instead, I debugged the sycall handler and put the hardcoded values in my code. This won't work for another kernel compilation, nor with `KASLR`.
Then right before jumping to the real handler, restore everything back, as if we come from the user.
- I wanted to disable interrupts, in order to make the access to `num_of_syscalls_recorded` counter as atomic as possible.
For that I used `cli`, and before jumping to the real syscall handler, `sti`. The problem is that I got weird double faults in seemingly random kernel functions (now I understand that they were all interrupt handlers). After debugging and seeing what I messed up before jumping back to the real handler, I understood that the syscall handler was called from context, where interrupts were already disabled. So running `sti` messed things up. Instead, I pushed the `EFLAGS` register to the stack with `pushfq`, and restored it with `popfq` (still with using `cli`).
This bug happened before I fixed the stack problem. After working with a real kernel stack (by using kernel's `GS`), I tried and saw that my syscall handler is no longer called with interrupts turned off. Maybe it was because using the user stack caused `#PF` for example, and somehow it caused a syscall (?). Weird... But I'm not going to debug to dig up the answer.

## Random debugging utils

```bash
mount -t 9p -o trans=virtio,version=9p2000.L mods /mnt
insmod /mnt/dscfun/syscallfun.ko
insmod /mnt/dscfun/syscallfun_write.ko # For actually changing the MSR, split for debugging
/mnt/dscfun/make_syscalls.o

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

# syscall handler if syscall num == 309
b *0xffffffff81000080 if $rax == 0x135
b *0x401956

# break on swapgs before sysretq
b *0xffffffff81000166
b *0xffffffff81000170

# syscall_fun_handler
b *0xffffffffc0000100
# syscall_fun_handler (after loading of num_of_syscalls_recorded)
b *0xffffffffc0000117 if $r14 > 0x3e5
# error_entry
b *0xffffffff810017a0

cat /sys/module/syscallfun/sections/.text

cat /proc/kallsyms | grep syscallfun

# Read stats
# ffffffffc0201540 b syscallfun_stats
# ffffffffc0212e88 b num_of_syscalls_recorded
set $stats = (syscall_stat *) 0xffffffffc0201540
set $num_called = (atomic_t *)0xffffffffc0212e88
p $stats[10]
p $num_called->counter
```

```gdb
cd kmodules
add-symbol-file dscfun/syscallfun.ko 0xffffffffc0000000
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
