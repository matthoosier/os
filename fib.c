#include "fib.h"

unsigned int fibonacci (unsigned int x)
{
    unsigned int res1;
    unsigned int res2;

    if (x < 2) {
        return 1;
    } else {
        res1 = fibonacci(x - 1);
        res2 = fibonacci(x - 2);
        return res1 + res2;
    }
}
