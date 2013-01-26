#include <stdint.h>
#include <muos/process.h>

void _start (void) __attribute__((naked));
void sling (void);

uint8_t stack[4096] __attribute__((aligned(4096))) = { 0 };

void sling (void) {

    int argc = 0;
    char * argv[] = { "", 0 };
    char * envp[] = { "", 0 };

    /* Prototype of main() */
    int main (int argc, char * argv[], char * envp[]);

    main(argc, argv, envp);
    Exit();
}

void _start (void) {
    asm volatile(
        "ldr sp, =stack     \n"
        "add sp, sp, %[sz]  \n"
        "ldr v1, =sling     \n"
        "blx v1             \n"
        "0: b 0b            \n"
        :
        : [sz] "i" (sizeof(stack))
    );
}
