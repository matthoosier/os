#ifndef __DECLS_H__
#define __DECLS_H__

#ifdef __cplusplus

    #define BEGIN_DECLS extern "C" {
    #define END_DECLS   }

#else

    #define BEGIN_DECLS
    #define END_DECLS

#endif

#endif /* __DECLS_H__ */
