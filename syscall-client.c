#include "syscall.h"

int main () {
    int counter;

    for (counter = 0; 1 == 1; counter++) {
        int result;
        result = syscall0(37);
        result = result;
    }
}
