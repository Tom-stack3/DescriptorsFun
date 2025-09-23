#include <stdio.h>

// Compile with: gcc -static zero.c -o zero.o

int main()
{
    int result;

    __asm__ __volatile__(
        "movl $10, %%eax\n\t"
        "xorl %%ebx, %%ebx\n\t"
        "divl %%ebx\n\t"
        : "=r"(result)
        :
        : "ebx", "ebx");

    printf("Result: %d\n", result);
    return 0;
}
