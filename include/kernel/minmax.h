#ifndef __MINMAX_H__
#define __MINMAX_H__

#define MIN(_a, _b)     \
    ({                  \
    typeof(_a) a = _a;  \
    typeof(_b) b = _b;  \
    a < b ? a : b;      \
    })

#define MAX(_a, _b)     \
    ({                  \
    typeof(_a) a = _a;  \
    typeof(_b) b = _b;  \
    a > b ? a : b;      \
    })

#endif /* __MINMAX_H__ */
