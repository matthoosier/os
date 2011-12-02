#ifndef __COMPILER_H__
#define __COMPILER_H__

#define ASSERT_CONCAT_(a, b) a##b
#define ASSERT_CONCAT(a, b) ASSERT_CONCAT_(a, b)

/* Provoke compile-time failure if "e" does not evaluate non-zero. */
#define COMPILER_ASSERT(e)                                      \
        enum {                                                  \
            /* Encode the line number in the name. */           \
            ASSERT_CONCAT(assert_line, __LINE__) = 1/(!!(e))    \
        }

#endif /* __COMPILER_H__ */
