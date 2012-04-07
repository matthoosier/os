#ifndef __VM_DEFS_H__
#define __VM_DEFS_H__

/*! \file kernel/vm-def.h */

#include <stdint.h>
#include <sys/types.h>

#include <sys/decls.h>

BEGIN_DECLS

/**
 * \private
 * \memberof Page
 *
 * Don't use this directly. Use VIRTUAL_HEAP_START and HEAP_SIZE
 */
extern char __HeapStart;

/**
 * \private
 * \memberof Page
 *
 * Don't use this directly. Use VIRTUAL_HEAP_START and HEAP_SIZE
 */
extern char __RamEnd;

/**
 * \brief   Holds a physical memory address. Can be converted to
 *          a virtual memory address by using #P2V.
 */
typedef uintptr_t PhysAddr_t;

/**
 * \brief   Holds a virtual memory address. Can be converted to
 *          a physical memory address by using #V2P.
 */
typedef uintptr_t VmAddr_t;

/**
 * \private
 * \memberof Page
 *
 * The kernel code's run address is 2GB higher than its load address.
 */
extern void * __KernelStart[];

/**
 * \brief   The constant offset by which kernel code refers to
 *          itself. That is, kernel code physically occupying
 *          address 0 will be mapped to accessible at virtual
 *          address KERNEL_MODE_OFFSET.
 */
#define KERNEL_MODE_OFFSET ((uint32_t)&__KernelStart)

/**
 * \def     V2P(_vmaddr)
 * \brief   Translate a kernel (not user!) virtual address to physical
 *          address.
 *
 * Because these convertors must be usable from both early (real mode)
 * and normal kernel (protected mode), they must be implemented as macros
 * to prevent a symbol (which would live only in one mode or the other's
 * addressing scheme) from being generated.
 */
#ifdef __GNUC__
    #define V2P(_vmaddr)                \
            ({                          \
            VmAddr_t a = (_vmaddr);     \
            a - KERNEL_MODE_OFFSET;     \
            })
#else
    #define V2P(_vmaddr) ((_vmaddr) - KERNEL_MODE_OFFSET)
#endif

/**
 * \def     P2V(_physaddr)
 * \brief   Translate a physical address to a kernel virtual address.
 *
 * Because these convertors must be usable from both early (real mode)
 * and normal kernel (protected mode), they must be implemented as macros
 * to prevent a symbol (which would live only in one mode or the other's
 * addressing scheme) from being generated.
 */
#ifdef __GNUC__
    #define P2V(_physaddr)              \
            ({                          \
            PhysAddr_t a = (_physaddr); \
            a + KERNEL_MODE_OFFSET;     \
            })
#else
    #define P2V(_physaddr) ((_physaddr) + KERNEL_MODE_OFFSET)
#endif

/**
 * \brief   Base address in kernel virtual memory at which
 *          the heap (memory not assigned to specific link-time
 *          data structures and/or executable code) begins
 */
#define VIRTUAL_HEAP_START ((VmAddr_t)&__HeapStart)

/**
 * \brief   The magnitude in bytes of the kernel heap
 */
#define HEAP_SIZE ((size_t)((VmAddr_t)&__RamEnd) - VIRTUAL_HEAP_START)

END_DECLS

#endif /* __VM_DEFS_H__ */

