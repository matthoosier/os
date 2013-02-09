#ifndef __ADDRESS_SPACE_HPP__
#define __ADDRESS_SPACE_HPP__

#include <new>

#include <kernel/list.hpp>
#include <kernel/mmu-defs.h>
#include <kernel/mmu.hpp>
#include <kernel/slaballocator.hpp>
#include <kernel/smart-ptr.hpp>
#include <kernel/vm.hpp>

/**
 * @brief   A sequence of pages that will be mapped into a
 *          single range of address space in a process
 *
 * @class VmArea address-space.hpp kernel/address-space.hpp
 */
class VmArea : public RefCounted
{
public:
    /**
     * @brief       Performs all allocation of pages up front.
     *
     * @exception   std::bad_alloc if sufficient pages to provide
     *              <tt>aLength</tt> bytes of storage can't be
     *              reserved
     */
    VmArea (size_t aLength) throw (std::bad_alloc);

    void * operator new (size_t size) throw (std::bad_alloc)
    {
        return sSlab.AllocateWithThrow();
    }

    void operator delete (void * mem)
    {
        sSlab.Free(mem);
    }

    /**
     * @brief   Fetch a handle to the beginning of the sequence
     *          of pages contained in this VM area
     */
    List<Page, &Page::list_link>::Iterator GetPages ();

    /**
     * @brief   Fetch the number of pages contained in the region
     *
     * The total byte length of the region will be
     * <tt>GetPageCount() * #PAGE_SIZE</tt>.
     */
    size_t GetPageCount ();

private:
    virtual ~VmArea ();

private:
    static SyncSlabAllocator<VmArea> sSlab;

    List<Page, &Page::list_link> mPages;

    size_t mPageCount;

    /**
     * @brief   For privileged access to destructor
     */
    friend class RefPtr<VmArea>;
};

/**
 * @brief   A linear range of pages in the virtual memory of
 *          some address space
 *
 * @class Mapping address-space.hpp kernel/address-space.hpp
 */
class Mapping
{
public:
    Mapping (VmAddr_t aBaseAddress, Prot_t aProtection);

    virtual ~Mapping ();

    virtual bool Map (RefPtr<TranslationTable> aPageTable) = 0;

    virtual void Unmap (RefPtr<TranslationTable> aPageTable) = 0;

    VmAddr_t GetBaseAddress ();

    virtual size_t GetLength () = 0;

    bool Intersects (VmAddr_t aBaseAddress, size_t aLength)
    {
        if (aBaseAddress + aLength <= mBaseAddress ||
            aBaseAddress >= mBaseAddress + GetLength())
        {
            return false;
        }
        else
        {
            return true;
        }
            
    }

protected:
    void Unmap (RefPtr<TranslationTable> aPageTable, size_t aPageCount);

public:
    ListElement mLink;

protected:
    VmAddr_t mBaseAddress;

    Prot_t mProtection;

    bool mMapped;
};

/**
 * @brief   A linear range of addresses backed by general-purpose
 *          pages in virtual memory
 *
 * @class BackedMapping address-space.hpp kernel/address-space.hpp
 */
class BackedMapping : public Mapping
{
public:
    BackedMapping (VmAddr_t aBaseAddress,
                   Prot_t aProtection,
                   RefPtr<VmArea> aRegion);

    virtual ~BackedMapping ();

    void * operator new (size_t size) throw (std::bad_alloc)
    {
        return sSlab.AllocateWithThrow();
    }

    void operator delete (void * mem)
    {
        sSlab.Free(mem);
    }

    virtual bool Map (RefPtr<TranslationTable> aPageTable);

    virtual void Unmap (RefPtr<TranslationTable> aPageTable);

    /**
     * @brief   Fetch the length in bytes of the mapping
     */
    virtual size_t GetLength ();

private:
    static SyncSlabAllocator<BackedMapping> sSlab;

    RefPtr<VmArea> mRegion;
};

/**
 * @brief   A linear range of addresses backed by physical IO
 *          memory
 *
 * @class PhysicalMapping address-space.hpp kernel/address-space.hpp
 */
class PhysicalMapping : public Mapping
{
public:
    PhysicalMapping (VmAddr_t aVirtualAddress,
                     PhysAddr_t aPhysicalAddress,
                     size_t aLength,
                     Prot_t aProtection);

    virtual ~PhysicalMapping ();

    void * operator new (size_t size) throw (std::bad_alloc)
    {
        return sSlab.AllocateWithThrow();
    }

    void operator delete (void * mem)
    {
        sSlab.Free(mem);
    }

    virtual bool Map (RefPtr<TranslationTable> aPageTable);

    virtual void Unmap (RefPtr<TranslationTable> aPageTable);

    virtual size_t GetLength ();

private:
    static SyncSlabAllocator<PhysicalMapping> sSlab;

    PhysAddr_t mPhysicalAddress;

    size_t mLength;
};

/**
 * @brief   Aggregation of all the virtual memory entries mapped
 *          into a process
 *
 * It is the AddressSpace that owns the pagetable of a process.
 *
 * @class PhysicalMapping address-space.hpp kernel/address-space.hpp
 */
class AddressSpace
{
public:
    AddressSpace ();

    ~AddressSpace ();

    void * operator new (size_t size) throw (std::bad_alloc)
    {
        return sSlab.AllocateWithThrow();
    }

    void operator delete (void * mem)
    {
        sSlab.Free(mem);
    }

    /**
     * Allocate new physical pages of RAM and insert them into the
     * indicated address range of virtual memory
     */
    bool CreateBackedMapping (VmAddr_t aVirtualAddress, size_t aLength);

    /**
     * Allocate a stack
     */
    bool CreateStack (size_t aLength,
                      VmAddr_t & aBaseAddress,
                      size_t & aAdjustedLength);

    /**
     * Insert some peripheral memory into the indicated address
     * range of virtual memory
     */
    bool CreatePhysicalMapping (PhysAddr_t aPhysicalAddress,
                                size_t aLength,
                                VmAddr_t & aVirtualAddress);

    /**
     * Request that the heap be extended by an additional number
     * of bytes
     */
    bool ExtendHeap (size_t aAdditionalLength,
                     VmAddr_t & aOldEnd,
                     VmAddr_t & aNewEnd);

    RefPtr<TranslationTable> GetPageTable ();

private:
    /**
     * The non-inclusive static upper bound on the address range
     * usable by mappings (mmap's, code, read-write data, bss, ...)
     */
    VmAddr_t mMappingsCeiling;

    /**
     * The base address to be used on the next mapping installed into
     * this address space
     */
    VmAddr_t mMappingsNextBase;

    /**
     * The non-inclusive static upper bound on the address range
     * usable as stack by this address space
     */
    VmAddr_t mStacksCeiling;

    /**
     * The base address to be used on the next stack installed
     * into this address space
     */
    VmAddr_t mStacksNextBase;

    /**
     * The non-inclusive static upper bound on the address range
     * usable as heap by this address space
     */
    VmAddr_t mHeapCeiling;

    /**
     * The base address to be used on the next heap chunk installed
     * into this address space
     */
    VmAddr_t mHeapNextBase;

    /**
     * All the current code, data, mmap, read-only, etc mappings
     * installed into this address space
     */
    List<Mapping, &Mapping::mLink> mMappings;

    /**
     * All the stacks currently installed into this address space
     */
    List<Mapping, &Mapping::mLink> mStacks;

    /**
     * All the heap chunks currently installed into this
     * address space
     */
    List<Mapping, &Mapping::mLink> mHeap;

    /**
     * @brief   Pagetable implementing the MMU gymnastics for this
     *          virtual address space
     */
    RefPtr<TranslationTable> mPageTable;

    static SyncSlabAllocator<AddressSpace> sSlab;
};

#endif /* __ADDRESS_SPACE_HPP__ */
