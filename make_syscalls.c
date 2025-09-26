#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>

// Compile with: gcc -static -g make_syscalls.c -o make_syscalls.o

void do_getcpu(void)
{
    unsigned int cpu = 0;
    unsigned int node = 0;
    long ret;

    // Didn't use the libc wrapper because it uses VDSO...

    asm volatile(
        "movq %[nr], %%rax\n\t"    // syscall number → rax
        "movq %[pcpu], %%rdi\n\t"  // arg0: &cpu → rdi
        "movq %[pnode], %%rsi\n\t" // arg1: &node → rsi
        "xorq %%rdx, %%rdx\n\t"    // arg2: NULL → rdx
        "syscall\n\t"              // enter kernel
        "movq %%rax, %[ret]\n\t"   // save return value
        : [ret] "=r"(ret)
        : [nr] "i"(__NR_getcpu),
          [pcpu] "r"(&cpu),
          [pnode] "r"(&node)
        : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory");

    if (ret < 0)
    {
        errno = -ret;
        perror("getcpu syscall");
        return;
    }

    printf("cpu: %u node: %u\n", cpu, node);
}

int main(void)
{
    printf("do_getcpu at: %p\n", do_getcpu);

    int i = 0;
    for (; i < 1; i++)
    {
        do_getcpu();
    }
}