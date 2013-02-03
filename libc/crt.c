#include <stdint.h>
#include <muos/process.h>

void _start (void) __attribute__((naked));
void sling (void);

uint8_t stack[4096] __attribute__((aligned(4096))) = { 0 };

typedef void (*VoidFunc) (void);

void sling (void) {

    int argc = 0;
    char * argv[] = { "", 0 };
    char * envp[] = { "", 0 };

    /* Prototype of main() */
    int main (int argc, char * argv[], char * envp[]);

    extern char __init_array_start;
    extern char __init_array_end;

    /* Run global constructors (C++, some standard C library stuff, ...) */
    VoidFunc * ctor_func_ptr;

    for (ctor_func_ptr = (VoidFunc *)&__init_array_start;
         ctor_func_ptr < (VoidFunc *)&__init_array_end;
         ctor_func_ptr++)
    {
        (*ctor_func_ptr)();
    }

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

#if defined(__ARM_EABI__)
    void *__dso_handle = &__dso_handle;
#endif
