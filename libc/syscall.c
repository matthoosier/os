#include <stdint.h>

#include <muos/syscall.h>

int syscall0 (unsigned int number)
{
    int result;

    /*
    Note that LR_svc gets trashed by the SWI instruction.

    This means that executing a syscall when already in supervisor mode will
    appear to destroy the link register.

    Work around by declaring LR in the clobber list, so that the compiler
    will automatically reload a saved copy of LR from the stack when
    exiting the assembly sequence.
    */
    asm volatile(
        "mov r8, %[number]  \n"
        "swi 0              \n"
        "mov %[result], r0  \n"
        : [result]"=r" (result)     /* Outputs  */
        : [number]"r" (number)      /* Inputs   */
        : "memory",                 /* Clobbers */
          "lr",
          "r8"
    );

    return result;
}

int syscall1 (unsigned int number, int arg0)
{
    int result;

    /* See notes in syscall0() about the gymnastics with LR. */
    asm volatile(
        "mov r0, %[arg0]    \n"
        "mov r8, %[number]  \n"
        "swi 0              \n"
        "mov %[result], r0  \n"
        : [result]"=r" (result)     /* Outputs  */
        : [number]"r" (number),     /* Inputs   */
          [arg0]"r" (arg0)
        : "memory",                 /* Clobbers */
          "lr",
          "r0",
          "r8"
    );

    return result;
}

int syscall2 (unsigned int number, int arg0, int arg1)
{
    int result;

    /* See notes in syscall0() about the gymnastics with LR. */
    asm volatile(
        "mov r0, %[arg0]    \n"
        "mov r1, %[arg1]    \n"
        "mov r8, %[number]  \n"
        "swi 0              \n"
        "mov %[result], r0  \n"
        : [result]"=r" (result)     /* Outputs  */
        : [number]"r" (number),     /* Inputs   */
          [arg0]"r" (arg0),
          [arg1]"r" (arg1)
        : "memory",                 /* Clobbers */
          "lr",
          "r0",
          "r1",
          "r8"
    );

    return result;
}

int syscall3 (unsigned int number, int arg0, int arg1, int arg2)
{
    int result;

    /* See notes in syscall0() about the gymnastics with LR. */
    asm volatile(
        "mov r0, %[arg0]    \n"
        "mov r1, %[arg1]    \n"
        "mov r2, %[arg2]    \n"
        "mov r8, %[number]  \n"
        "swi 0              \n"
        "mov %[result], r0  \n"
        : [result]"=r" (result)     /* Outputs  */
        : [number]"r" (number),     /* Inputs   */
          [arg0]"r" (arg0),
          [arg1]"r" (arg1),
          [arg2]"r" (arg2)
        : "memory",                 /* Clobbers */
          "lr",
          "r0",
          "r1",
          "r2",
          "r8"
    );

    return result;
}

int syscall4 (unsigned int number, int arg0, int arg1, int arg2, int arg3)
{
    int result;

    /* See notes in syscall0() about the gymnastics with LR. */
    asm volatile(
        "mov r0, %[arg0]    \n"
        "mov r1, %[arg1]    \n"
        "mov r2, %[arg2]    \n"
        "mov r3, %[arg3]    \n"
        "mov r8, %[number]  \n"
        "swi 0              \n"
        "mov %[result], r0  \n"
        : [result]"=r" (result)     /* Outputs  */
        : [number]"r" (number),     /* Inputs   */
          [arg0]"r" (arg0),
          [arg1]"r" (arg1),
          [arg2]"r" (arg2),
          [arg3]"r" (arg3)
        : "memory",                 /* Clobbers */
          "lr",
          "r0",
          "r1",
          "r2",
          "r3",
          "r8"
    );

    return result;
}

int syscall5 (unsigned int number, int arg0, int arg1, int arg2, int arg3, int arg4)
{
    int result;

    /* See notes in syscall0() about the gymnastics with LR. */
    asm volatile(
        "mov r0, %[arg0]    \n"
        "mov r1, %[arg1]    \n"
        "mov r2, %[arg2]    \n"
        "mov r3, %[arg3]    \n"
        "mov r4, %[arg4]    \n"
        "mov r8, %[number]  \n"
        "swi 0              \n"
        "mov %[result], r0  \n"
        : [result]"=r" (result)     /* Outputs  */
        : [number]"r" (number),     /* Inputs   */
          [arg0]"r" (arg0),
          [arg1]"r" (arg1),
          [arg2]"r" (arg2),
          [arg3]"r" (arg3),
          [arg4]"r" (arg4)
        : "memory",                 /* Clobbers */
          "lr",
          "r0",
          "r1",
          "r2",
          "r3",
          "r4",
          "r8"
    );

    return result;
}
