#ifndef __MUOS_COMPILER_H__
#define __MUOS_COMPILER_H__

#define COMPILER_PREPROC_CONCAT_(a, b) a##b
#define COMPILER_PREPROC_CONCAT(a, b) COMPILER_PREPROC_CONCAT_(a, b)

/* Provoke compile-time failure if "e" does not evaluate non-zero. */
#define COMPILER_ASSERT(e)                                              \
        enum {                                                          \
            /* Encode the line number in the name. */                   \
            COMPILER_PREPROC_CONCAT(assert_line, __LINE__) = 1/(!!(e))  \
        }

#endif /* __MUOS_COMPILER_H__ */
