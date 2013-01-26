#ifndef __MMU_HPP__
#define __MMU_HPP__

#include <stdint.h>
#include <stdbool.h>

#include <new>

#include <sys/decls.h>

#include <kernel/assert.h>
#include <kernel/list.hpp>
#include <kernel/mmu-defs.h>
#include <kernel/slaballocator.hpp>
#include <kernel/smart-ptr.hpp>
#include <kernel/tree-map.hpp>
#include <kernel/vm.hpp>

class SecondlevelTable;

/**
 * \brief   Data structure used to encapsulate the hardware translation-
 *          table descriptors used by the MMU to perform virtual-to-physical
 *          memory translations.
 *
 * The actual descriptor installed into the Translation Table Base Register
 * (on ARM) is pointed to in virtual memory by the #firstlevel_ptes field of
 * this class.
 *
 * \class TranslationTable mmu.hpp kernel/mmu.hpp
 */
class TranslationTable
{
public:
    typedef TreeMap<VmAddr_t, struct SecondlevelTable *> SparseSecondlevelMap_t;

public:

    void * operator new (size_t size) throw (std::bad_alloc)
    {
        assert(size == sizeof(TranslationTable));
        return sSlab.AllocateWithThrow();
    }

    void operator delete (void * mem) throw ()
    {
        sSlab.Free(mem);
    }

    TranslationTable () throw (std::bad_alloc);
    ~TranslationTable ();

    bool MapPage (
            VmAddr_t virt,
            PhysAddr_t phys,
            Prot_t prot
            );

    bool MapNextPage (
            VmAddr_t * pVirt,
            PhysAddr_t phys,
            Prot_t prot
            );

    bool UnmapPage (
            VmAddr_t virt
            );

    bool MapSection (
            VmAddr_t virt,
            PhysAddr_t phys,
            Prot_t prot
            );

    bool UnmapSection (
            VmAddr_t virt
            );

    static void SetKernel (TranslationTable * table);
    static TranslationTable * GetKernel ();

    static void SetUser (TranslationTable * table);
    static TranslationTable * GetUser ();

    static ssize_t CopyWithAddressSpaces (
            TranslationTable *  source_tt,
            const void *        source_buf,
            size_t              source_len,
            TranslationTable *  dest_tt,
            void *              dest_buf,
            size_t              dest_len
            );

public:
    /**
     * \brief   Provides the storage pointed at by #firstlevel_ptes below
     */
    PagePtr firstlevel_ptes_pages;

    /**
     * \brief   Points to a pt_firstlevel_t[4096].
     *
     * First element must be 16KB-aligned.
     */
    pt_firstlevel_t * firstlevel_ptes;

    /**
     * Sparse map of beginning virtual address of each section
     * to the SecondlevelTable instance that fills in the individual
     * pages for that section.
     *
     * Each struct contained in this list controls the mappings for
     * 1MB worth of individual pages.
     *
     * A sample couple entries in this map might be:
     *
     *    First section (0x00000000 - 0x000fffff):
     *        0x00000000 -> (SecondlevelTable *)<struct address>
     *    ...
     *
     *    Fifteenth section (0x00e00000 - 0x00efffff)
     *        0x00e00000 -> (SecondlevelTable *)<struct address>
     */
    ScopedPtr<SparseSecondlevelMap_t> sparse_secondlevel_map;

    /**
     * Virtual address of the first byte of the first page in the virtual
     * address space, that is not mapped.
     */
    VmAddr_t first_unmapped_page;

private:
    static SyncSlabAllocator<TranslationTable> sSlab;
};

/**
 * \brief   Wrapper for individual page descriptors
 *
 * \class SecondlevelTable mmu.hpp kernel/mmu.hpp
 */
class SecondlevelTable
{
public:
    void * operator new (size_t size) throw (std::bad_alloc)
    {
        assert(size == sizeof(SecondlevelTable));
        return sSlab.AllocateWithThrow();
    }

    void operator delete (void * mem) throw ()
    {
        sSlab.Free(mem);
    }

    SecondlevelTable () throw (std::bad_alloc);
    ~SecondlevelTable () throw ();

public:
    /*
     * The array of individual pagetable entries used by the MMU.
     *
     * Allocated separately because of strict 1KB alignment-boundary
     * requirements. Logically it's just part of this data structure
     */
    struct SecondlevelPtes * ptes;

    /*
     * Utility field for inserting into whatever list is needed
     */
    ListElement link;

    unsigned int num_mapped_pages;

private:
    static SyncSlabAllocator<SecondlevelTable> sSlab;
};

/**
 * \brief   Individual page descriptors
 *
 * \class SecondlevelPtes mmu.hpp kernel/mmu.hpp
 */
struct SecondlevelPtes
{
public:
    friend class SecondlevelTable;

    /*!
     * Each element describes one 4KB page.
     *
     * 256 of these for each 1MB section mapping.
     * First element out of each 256 must be 1KB-aligned
     */
    pt_secondlevel_t ptes[256];

private:
    static SyncSlabAllocator<SecondlevelPtes> sSlab;
};

BEGIN_DECLS

/**
 * \brief   Backward-compatible C shim for calling TranslationTable#SetUser()
 *          from assembly code.
 */
void TranslationTableSetUser (TranslationTable *);

/**
 * \brief   Backward-compatible C shim for calling TranslationTable#GetUser()
 *          from assembly code.
 */
TranslationTable * TranslationTableGetUser ();

END_DECLS

#endif /* __MMU_HPP__ */
