#include "syscall.h"

void _start () {
    int counter;

    for (counter = 0; 1 == 1; counter++) {
        int result;
        result = syscall0(37);
        result = result;
    }
}
