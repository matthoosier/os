#include <stddef.h>

int main () {
    /* Test out basic abnormal termination handler in kernel */
    *((char *)NULL) = '\0';
    return 0;
}
