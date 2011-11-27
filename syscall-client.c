#include "syscall.h"

int main () {
    int counter;

    for (counter = 0; counter < 1; counter++) {
        int result;
        result = syscall1(SYS_NUM_ECHO, 37);
        result = result;
    }

    return 0;
}
