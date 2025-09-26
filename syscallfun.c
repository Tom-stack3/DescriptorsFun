#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/atomic.h>
#include <linux/preempt.h>

#define MAX_SYSCALLS_TO_RECORD 1000
#define IA32_EFER_MSR (0xc0000080)
#define IA32_EFER_SYSCALL_ENABLE_BIT_INDEX (0)
#define IA32_STAR_MSR (IA32_EFER_MSR + 1)
#define IA32_LSTAR_MSR (IA32_STAR_MSR + 1)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Me");
MODULE_DESCRIPTION("Fun with Syscalls...");

struct syscall_called_stat_struct
{
    uint64_t syscall_number; // stored at %rax
    uint64_t next_rip;       // stored at %rcx
    uint64_t rflags;         // stored at %r11
    uint64_t arg0;           // %rdi
    uint64_t arg1;           // %rsi
    uint64_t arg2;           // %rdx
    uint64_t arg3;           // %r10
    uint64_t arg4;           // %r8
    uint64_t arg5;           // %r9
} __attribute__((packed));

typedef struct syscall_called_stat_struct syscall_stat;

static uint64_t original_lstar_val = 0;

syscall_stat stats[MAX_SYSCALLS_TO_RECORD] asm("syscallfun_stats") = {0}; // Keep global to be accessible
static atomic_t num_of_syscalls_recorded = ATOMIC_INIT(0);

void syscall_fun_handler(void);

// Copied this weird syntex from: tools/testing/selftests/x86/check_initial_reg_state.c
// Looked on SWAPGS for accessing Kernel stack at: arch/x86/entry/entry_64.S @ SYM_CODE_START(entry_SYSCALL_64)
asm(
    ".pushsection .text\n\t"
    ".extern syscallfun_stats\n\t"
    ".extern num_of_syscalls_recorded\n\t"
    ".extern original_lstar_val\n\t"
    ".type syscall_fun_handler, @function\n\t"
    ".global syscall_fun_handler\n\t"
    ".p2align 4\n\t"
    "syscall_fun_handler:\n\t"
    "endbr64\n\t" // For CET if enabled
    // Switch to kernel stack
    "swapgs\n\t"
    "movq %rsp, %gs:0xffffffff834f5014\n\t" // Save user %rsp to TSS.sp2 (just a per-CPU space available)
    "movq %gs:0xffffffff83504010, %rsp\n\t" // Get the current per-CPU kernel stack
    // Save EFLAGS register
    "pushfq\n\t"
    // Save callee-saved registers
    "pushq %rbp\n\t"
    "pushq %rbx\n\t"
    "pushq %r12\n\t"
    "pushq %r13\n\t"
    "pushq %r14\n\t"
    "pushq %r15\n\t"

    // Disable interrupts to avoid races
    "cli\n\t"
    "movl num_of_syscalls_recorded(%rip), %r14d\n\t"
    "cmpl $1000, %r14d\n\t"
    "jae 2f\n\t" // if >= MAX_SYSCALLS_TO_RECORD, skip logging

    // Calculate the address to write to
    "lea syscallfun_stats(%rip), %r13\n\t"
    "imul $72, %r14, %r14\n\t"
    "addq %r14, %r13\n\t"

    // Copy all fields to the struct
    "movq %rax, (%r13)\n\t"   // syscall_number
    "movq %rcx, 8(%r13)\n\t"  // next_rip
    "movq %r11, 16(%r13)\n\t" // rflags
    "movq %rdi, 24(%r13)\n\t" // arg0
    "movq %rsi, 32(%r13)\n\t" // arg1
    "movq %rdx, 40(%r13)\n\t" // arg2
    "movq %r10, 48(%r13)\n\t" // arg3
    "movq %r8, 56(%r13)\n\t"  // arg4
    "movq %r9, 64(%r13)\n\t"  // arg5

    // Increment the atomic counter
    "lock; incl num_of_syscalls_recorded(%rip)\n\t"
    "2:\n\t"
    // Restore callee-saved registers
    "popq %r15\n\t"
    "popq %r14\n\t"
    "popq %r13\n\t"
    "popq %r12\n\t"
    "popq %rbx\n\t"
    "popq %rbp\n\t"
    "popfq\n\t"

    // Switch back to previous stack
    "movq %rsp, %gs:0xffffffff83504010\n\t" // Save the current per-CPU kernel stack
    "movq %gs:0xffffffff834f5014, %rsp\n\t" // Get user %rsp from TSS.sp2 (just a per-CPU space available)
    "swapgs\n\t"
    // Jump to real syscall handler
    "jmp *original_lstar_val(%rip)\n\t"
    ".size syscall_fun_handler, . - syscall_fun_handler\n\t"
    ".popsection");

static uint64_t read_msr(uint32_t msr_index)
{
    unsigned int upper = 0;
    unsigned int lower = 0;
    asm volatile(
        "movl %2, %%ecx\n\t"
        "rdmsr\n\t"
        "movl %%edx, %0\n\t"
        "movl %%eax, %1"
        : "=r"(upper), "=r"(lower)
        : "r"(msr_index)
        : "rax", "rcx", "rdx");
    return ((uint64_t)upper << 32) | lower;
}

static void write_msr(uint32_t msr_index, uint64_t value)
{
    pr_info("syscallfun: Writing to MSR %x value: 0x%llx\n", msr_index, value);
    unsigned int upper = value >> 32;
    unsigned int lower = value & 0xFFFFFFFF;
    asm volatile(
        "movl %2, %%ecx\n\t"
        "movl %0, %%edx\n\t"
        "movl %1, %%eax\n\t"
        "wrmsr"
        :
        : "r"(upper), "r"(lower), "r"(msr_index)
        : "rax", "rcx", "rdx");
}

static unsigned int is_syscall_opcode_supported(void)
{
    return (unsigned int)(((read_msr(IA32_EFER_MSR)) >> IA32_EFER_SYSCALL_ENABLE_BIT_INDEX) & 1);
}

static int __init syscallfun_init(void)
{
    pr_info("syscallfun: module loaded\n");

    if (!is_syscall_opcode_supported())
    {
        pr_err("syscallfun: SYSCALL not supported\n");
        return 1;
    }
    pr_info("syscallfun: IA32_STAR_MSR: 0x%llx\n", read_msr(IA32_STAR_MSR));
    original_lstar_val = read_msr(IA32_LSTAR_MSR);
    pr_info("syscallfun: IA32_LSTAR_MSR: 0x%llx\n", original_lstar_val);

    pr_info("syscallfun: Great success!\n");

    atomic_set(&num_of_syscalls_recorded, 0);
    pr_info("syscallfun: stats[0].syscall_number: %llu\n", stats[0].syscall_number);

    write_msr(IA32_LSTAR_MSR, (unsigned long long)syscall_fun_handler);

    return 0;
}

static void __exit syscallfun_exit(void)
{
    write_msr(IA32_LSTAR_MSR, original_lstar_val);

    pr_info("syscallfun: module unloaded\n");
}

module_init(syscallfun_init);
module_exit(syscallfun_exit);
