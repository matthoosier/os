#ifndef __VM_H__
#define __VM_H__

#include <stdint.h>
#include "decls.h"

BEGIN_DECLS

/* Don't use this directly. Use VIRTUAL_HEAP_START and HEAP_SIZE */
extern char __HeapStart;

/* Don't use this directly. Use VIRTUAL_HEAP_START and HEAP_SIZE */
extern char __RamEnd;

typedef uintptr_t physaddr_t;
typedef uintptr_t vmaddr_t;

/*
 * The kernel code's run address is 3GB higher than its load address.
 */
extern void * __KernelStart[];
#define KERNEL_MODE_OFFSET ((uint32_t)&__KernelStart)

/*
 * Translate a kernel (not user!) virtual address to physical address.
 *
 * Because these convertors must be usable from both early (real mode)
 * and normal kernel (protected mode), they must be implemented as macros
 * to prevent a symbol (which would live only in one mode or the other's
 * addressing scheme) from being generated.
 */
#define V2P(_vmaddr) ((_vmaddr) - KERNEL_MODE_OFFSET)

/*
 * Translate a physical address to a kernel virtual address.
 *
 * Because these convertors must be usable from both early (real mode)
 * and normal kernel (protected mode), they must be implemented as macros
 * to prevent a symbol (which would live only in one mode or the other's
 * addressing scheme) from being generated.
 */
#define P2V(_physaddr) ((_physaddr) + KERNEL_MODE_OFFSET)

#define VIRTUAL_HEAP_START ((vmaddr_t)&__HeapStart)
#define HEAP_SIZE ((size_t)((vmaddr_t)&__RamEnd) - VIRTUAL_HEAP_START)

/* Initialize page allocator mechanism */
extern void vm_init();

/* Returns back the virtual memory address of a newly allocated page */
extern void * vm_page_alloc ();

/* Release the page starting at virtual memory address 'page_address' */
extern void vm_page_free (void * page_address);

END_DECLS

#endif /* __VM_H__ */
