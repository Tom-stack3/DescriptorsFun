#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/desc_defs.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Me");
MODULE_DESCRIPTION("Fun with Descriptors...");

void div0_handler(void);
// Copied this weird syntex from: tools/testing/selftests/x86/check_initial_reg_state.c
asm (
    ".pushsection .text\n\t"
    ".type div0_handler, @function\n\t"
    ".global div0_handler\n\t"
    ".p2align 4\n\t"
    "div0_handler:\n\t"
    // // Zero RAX and RDX to make the Quotient and Remainder are zeroed out
    "xorq %rax, %rax\n\t"
    "xorq %rdx, %rdx\n\t"
    "iretq\n\t"
    ".size div0_handler, . - div0_handler\n\t"
    ".popsection");

// Vol. 2B 4-651
// In 64-bit mode, the operand size fixed at 8+2 bytes. The instruction stores 8-byte base and 2-byte limit values.
struct idtr_val
{
    uint16_t idt_limit;
    uint64_t idt_base;
} __attribute__((packed));

static struct idtr_val original_idtr_val = {0};
static struct idtr_val patched_idtr_val = {0};
// 0x100 because there can be at max 256 interrupt gate descriptors
static gate_desc original_idt[0x100] = {0};
static gate_desc patched_idt[0x100] __aligned(PAGE_SIZE) = {0};

// Took from: arch/x86/boot/compressed/idt_64.c
static void load_idt(const struct idtr_val *dtr)
{
    asm volatile("lidt %0" ::"m"(*dtr));
}

static void print_idt_entries(void)
{
    int i;
    for (i = 0; i <= original_idtr_val.idt_limit / sizeof(gate_desc); i++)
    {
        struct idt_data entry;
        entry.vector = i;
        entry.segment = original_idt[i].segment;
        entry.bits = original_idt[i].bits;
        entry.addr = (void *)((u64)original_idt[i].offset_low |
                              ((u64)original_idt[i].offset_middle << 16) |
                              ((u64)original_idt[i].offset_high << 32));

        pr_info("IDT[%u] -> addr=%px type=%u dpl=%u present=%u\n",
                entry.vector,
                entry.addr,
                entry.bits.type,
                entry.bits.dpl,
                entry.bits.p);
    }
}

static void save_and_parse_original_idtr(void)
{
    struct idtr_val idtr_read = {0};
    asm volatile(
        "sidt %0\n"
        : "=m"(idtr_read));
    pr_info("dscfun: 0x%x 0x%llx\n", idtr_read.idt_limit, idtr_read.idt_base);
    original_idtr_val = idtr_read;

    memcpy(original_idt, (const void *)idtr_read.idt_base, idtr_read.idt_limit + 1);

    print_idt_entries();
}

// static void try_to_div_by_0(void)
// {
//     int result;

//     __asm__ __volatile__(
//         "mov $10, %%eax\n\t"
//         "xor %%ebx, %%ebx\n\t"
//         "div %%ebx\n\t"
//         : "=a"(result)
//         :
//         : "ebx");

//     printk("Result: %d\n", result);
// }

static void patch_idt_entry_0(void)
{
    memcpy(patched_idt, (const void *)original_idt, original_idtr_val.idt_limit + 1);

    // Patch the Divide by zero interrupt handler (INT0 - Divide error)
    // Copied code from: arch/x86/boot/compressed/idt_64.c -> set_idt_entry
    unsigned long address = (unsigned long)div0_handler;

    patched_idt[0].offset_low = (u16)(address & 0xffff);
    patched_idt[0].offset_middle = (u16)((address >> 16) & 0xffff);
    patched_idt[0].offset_high = (u32)(address >> 32);

    patched_idtr_val = original_idtr_val;
    patched_idtr_val.idt_base = (uint64_t) &patched_idt[0];
    load_idt(&patched_idtr_val);
}

static int __init dscfun_init(void)
{
    pr_info("dscfun: module loaded\n");
    save_and_parse_original_idtr();
    patch_idt_entry_0();
    pr_info("dscfun: Changed IDTR to point at patched\n");

    // save_and_parse_original_idtr();
    // try_to_div_by_0();
    pr_info("dscfun: Great success!\n");

    return 0;
}

static void __exit dscfun_exit(void)
{
    pr_info("dscfun: module unloaded\n");
}

module_init(dscfun_init);
module_exit(dscfun_exit);
