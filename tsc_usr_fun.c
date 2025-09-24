#include <stdio.h>
#include <unistd.h>

#define RDTSCP_SUPPORT_CPUID_INDEX (0x80000001)
#define RDTSCP_SUPPORT_BIT (27)
#define CR4_TSD_BIT_INDEX (2)

// Compile with: gcc -static -g tsc_usr_fun.c -o tsc_usr_fun

static unsigned int is_rdtscp_supported(void)
{
    unsigned int is_supported;
    asm volatile(
        "movl %1, %%eax\n\t"
        "cpuid\n\t"
        "movl %%edx, %0"
        : "=r"(is_supported)
        : "r"(RDTSCP_SUPPORT_CPUID_INDEX)
        : "rax", "rbx", "rcx", "rdx");
    is_supported = (is_supported >> (RDTSCP_SUPPORT_BIT - 1)) & 1;
    return is_supported;
}

static unsigned int is_rdtscp_supported_by_user(void)
{
    // Should run only ring 0...
    unsigned int is_supported;
    unsigned long long cr4_content;
    asm volatile(
        "movq %%cr4, %0"
        : "=r"(cr4_content));
    is_supported = (unsigned int)((cr4_content >> (CR4_TSD_BIT_INDEX)) & 1);
    return is_supported;
}

struct rdtscp_output
{
    unsigned long long timestamp;
    unsigned int logical_processor_id;
} __attribute__((packed));

static struct rdtscp_output read_rdtscp(void)
{
    struct rdtscp_output out = {0};
    unsigned int upper = 0;
    unsigned int lower = 0;
    asm volatile(
        "rdtscp\n\t"
        "movl %%edx, %0\n\t"
        "movl %%eax, %1\n\t"
        "movl %%ecx, %2\n\t"
        : "=r"(upper), "=r"(lower), "=m"(out.logical_processor_id)
        :
        : "eax", "ecx", "edx");
    out.timestamp = ((unsigned long long)upper << 32) | lower;
    return out;
}

static void read_and_print_rdtscp_output(int times)
{
    struct rdtscp_output out = {0};
    int i = 0;
    for (; i < 10; i++)
    {
        out = read_rdtscp();
        printf("%lu, Processor ID: %u\n", out.timestamp, out.logical_processor_id);
    }
}

int main()
{
    printf("%u\n", is_rdtscp_supported());
    read_and_print_rdtscp_output(10);
    read_and_print_rdtscp_output(10);
    read_and_print_rdtscp_output(10);
    sleep(1); // Sleep to see if core will be replaced...
    read_and_print_rdtscp_output(10);

    return 0;
}
