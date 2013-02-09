#include <muos/arch.h>

#include <kernel/address-space.hpp>
#include <kernel/assert.h>
#include <kernel/math.hpp>
#include <kernel/minmax.hpp>
#include <kernel/vm-defs.h>

SyncSlabAllocator<VmArea> VmArea::sSlab;

VmArea::VmArea (size_t aLength) throw (std::bad_alloc)
    : mPageCount(0)
{
    assert(aLength % PAGE_SIZE == 0);

    while (mPageCount * PAGE_SIZE < aLength)
    {
        Page * page = Page::Alloc();

        if (!page)
            throw std::bad_alloc();

        mPages.Append(page);
        mPageCount++;
    }
}

VmArea::~VmArea ()
{
    while (!mPages.Empty())
    {
        Page::Free(mPages.PopLast());
    }
}

size_t VmArea::GetPageCount ()
{
    return mPageCount;
}

List<Page, &Page::list_link>::Iterator VmArea::GetPages ()
{
    return mPages.Begin();
}

SyncSlabAllocator<BackedMapping> BackedMapping::sSlab;

Mapping::Mapping (VmAddr_t aBaseAddress,
                  Prot_t aProtection)
    : mBaseAddress(aBaseAddress)
    , mProtection(aProtection)
    , mMapped(false)
{
    assert(aBaseAddress % PAGE_SIZE == 0);
}

Mapping::~Mapping ()
{
    assert(!mMapped);
}

void Mapping::Unmap (RefPtr<TranslationTable> aPageTable,
                     size_t aPageCount)
{
    VmAddr_t virt = mBaseAddress;

    while (aPageCount > 0) {
        bool unmapped = aPageTable->UnmapPage(virt);
        assert(unmapped);
        virt += PAGE_SIZE;
        aPageCount--;
    }
}

VmAddr_t Mapping::GetBaseAddress ()
{
    return mBaseAddress;
}

BackedMapping::BackedMapping (VmAddr_t aBaseAddress,
                              Prot_t aProtection,
                              RefPtr<VmArea> aRegion)
    : Mapping(aBaseAddress, aProtection)
    , mRegion(aRegion)
{
}

BackedMapping::~BackedMapping ()
{
}

bool BackedMapping::Map (RefPtr<TranslationTable> aPageTable)
{
    assert(!mMapped);

    VmAddr_t virt = mBaseAddress;
    size_t numPagesLeft = mRegion->GetPageCount();
    List<Page, &Page::list_link>::Iterator pageIter = mRegion->GetPages();

    while (numPagesLeft > 0) {
        Page * p = *pageIter;
        bool mapped = aPageTable->MapPage(virt, V2P(p->base_address), mProtection);
        if (!mapped) {
            assert(false);
            Mapping::Unmap(aPageTable, mRegion->GetPageCount() - numPagesLeft);
            return false;
        }

        virt += PAGE_SIZE;
        numPagesLeft--;
        ++pageIter;
    }

    mMapped = true;
    return true;
}

void BackedMapping::Unmap (RefPtr<TranslationTable> aPageTable)
{
    assert(mMapped);
    Mapping::Unmap(aPageTable, mRegion->GetPageCount());
    mMapped = false;
}

size_t BackedMapping::GetLength ()
{
    return mRegion->GetPageCount() * PAGE_SIZE;
}

SyncSlabAllocator<PhysicalMapping> PhysicalMapping::sSlab;

PhysicalMapping::PhysicalMapping (VmAddr_t aVirtualAddress,
                                  PhysAddr_t aPhysicalAddress,
                                  size_t aLength,
                                  Prot_t aProtection)
    : Mapping(aVirtualAddress, aProtection)
    , mPhysicalAddress(aPhysicalAddress)
    , mLength(aLength)
{
    assert(aPhysicalAddress % PAGE_SIZE == 0);
    assert(aLength % PAGE_SIZE == 0);
}

PhysicalMapping::~PhysicalMapping ()
{
}

bool PhysicalMapping::Map (RefPtr<TranslationTable> aPageTable)
{
    assert(!mMapped);

    VmAddr_t virt = mBaseAddress;
    PhysAddr_t phys = mPhysicalAddress;
    size_t numPagesLeft = mLength / PAGE_SIZE;

    while (numPagesLeft > 0) {
        bool mapped = aPageTable->MapPage(virt, phys, mProtection);
        if (!mapped) {
            assert(false);
            Mapping::Unmap(aPageTable, (mLength / PAGE_SIZE) - numPagesLeft);
            return false;
        }

        virt += PAGE_SIZE;
        phys += PAGE_SIZE;
        numPagesLeft--;
    }

    mMapped = true;
    return true;
}

void PhysicalMapping::Unmap (RefPtr<TranslationTable> aPageTable)
{
    assert(mMapped);
    Mapping::Unmap(aPageTable, mLength / PAGE_SIZE);
    mMapped = false;
}

size_t PhysicalMapping::GetLength ()
{
    return mLength;
}

SyncSlabAllocator<AddressSpace> AddressSpace::sSlab;

AddressSpace::AddressSpace ()
    : mPageTable(new TranslationTable())
{
    /*
    User-accessible addresses run from 0 through KERNEL_MODE_OFFSET
    */

    /*
    Mappings (text, bss, readonly, rw, physical mmaps) get one quarter
    of the user-visible address range
    */
    mMappingsNextBase = 0;
    mMappingsCeiling = Math::RoundDown(KERNEL_MODE_OFFSET / 4, PAGE_SIZE);

    /*
    Stacks get the next one quarter of the user-visible address range
    */
    mStacksNextBase = mMappingsCeiling;
    mStacksCeiling = mMappingsCeiling * 2;

    /*
    The remainder (the top half) of the user-visible address range
    is for the heap
    */
    mHeapNextBase = mStacksCeiling;
    mHeapCeiling = Math::RoundDown(KERNEL_MODE_OFFSET, PAGE_SIZE);
}

AddressSpace::~AddressSpace ()
{
    Mapping * mapping;

    while (!mHeap.Empty()) {
        mapping = mHeap.PopFirst();
        mapping->Unmap(mPageTable);
        delete mapping;
    }

    while (!mStacks.Empty()) {
        mapping = mStacks.PopFirst();
        mapping->Unmap(mPageTable);
        delete mapping;
    }

    while (!mMappings.Empty()) {
        mapping = mMappings.PopFirst();
        mapping->Unmap(mPageTable);
        delete mapping;
    }
}

RefPtr<TranslationTable> AddressSpace::GetPageTable ()
{
    return mPageTable;
}

bool AddressSpace::CreateBackedMapping (VmAddr_t aVirtualAddress,
                                        size_t aLength)
{
    assert(aVirtualAddress % PAGE_SIZE == 0);
    assert(aLength % PAGE_SIZE == 0);

    RefPtr<VmArea> area;
    BackedMapping * mapping;

    if (aVirtualAddress + aLength > mMappingsCeiling) {
        return false;
    }

    // Make sure the new proposed mapping won't collide with any
    // allocated address range
    for (List<Mapping, &Mapping::mLink>::Iterator i = mMappings.Begin();
         i; ++i)
    {
        if (i->Intersects(aVirtualAddress, aLength)) {
            return false;
        }
    }

    try {
        area.Reset(new VmArea(Math::RoundUp(aLength, PAGE_SIZE)));
        mapping = new BackedMapping(aVirtualAddress, PROT_USER_READWRITE, area);
    }
    catch (std::bad_alloc) {
        assert(false);
        return false;
    }

    if (!mapping->Map(mPageTable)) {
        delete mapping;
        return false;
    }

    mMappings.Append(mapping);

    if (mMappingsNextBase < aVirtualAddress + aLength) {
        mMappingsNextBase = Math::RoundUp(aVirtualAddress + aLength,
                                          PAGE_SIZE);
    }

    return true;
}

bool AddressSpace::CreatePhysicalMapping (PhysAddr_t aPhysicalAddress,
                                          size_t aLength,
                                          VmAddr_t & aVirtualAddress)
{
    assert(aPhysicalAddress % PAGE_SIZE == 0);

    PhysicalMapping * map;

    if (mMappingsNextBase + aLength > mMappingsCeiling) {
        // Not enough address range left to satisfy this
        return false;
    }

    try {
        map = new PhysicalMapping(mMappingsNextBase, aPhysicalAddress,
                                  aLength, PROT_USER_READWRITE);
    } catch (std::bad_alloc) {
        return false;
    }

    if (map->Map(mPageTable)) {
        aVirtualAddress = mMappingsNextBase;
        mMappingsNextBase += aLength;
        mMappings.Append(map);
        return true;
    }
    else {
        delete map;
        return false;
    }
}

bool AddressSpace::CreateStack (size_t aLength,
                                VmAddr_t & aBaseAddress,
                                size_t & aAdjustedLength)
{
    RefPtr<VmArea> area;
    BackedMapping * map;

    size_t actual_len = Math::RoundUp(aLength, PAGE_SIZE);

    if (mStacksNextBase + actual_len > mStacksCeiling) {
        return false;
    }

    try {
        area.Reset(new VmArea(actual_len));
        map = new BackedMapping(mStacksNextBase, PROT_USER_READWRITE, area);
    }
    catch (std::bad_alloc) {
        delete map;
        return false;
    }

    if (!map->Map(mPageTable)) {
        delete map;
        return false;
    }

    aBaseAddress = mStacksNextBase;
    aAdjustedLength = actual_len;
    mStacksNextBase += actual_len;
    return true;
}

bool AddressSpace::ExtendHeap (size_t aAdditionalLength,
                               VmAddr_t & aOldEnd,
                               VmAddr_t & aNewEnd)
{
    assert(aAdditionalLength % PAGE_SIZE == 0);

    if (aAdditionalLength == 0) {
        aOldEnd = aNewEnd = mHeapNextBase;
        return true;
    }

    if (mHeapNextBase + aAdditionalLength > mHeapCeiling) {
        return false;
    }

    RefPtr<VmArea> area;
    BackedMapping * map;

    try {
        area.Reset(new VmArea(aAdditionalLength));
        map = new BackedMapping(mHeapNextBase, PROT_USER_READWRITE, area);
    }
    catch (std::bad_alloc) {
        return false;
    }

    if (!map->Map(mPageTable)) {
        delete map;
        return false;
    }

    aOldEnd = mHeapNextBase;
    aNewEnd = mHeapNextBase + aAdditionalLength;
    mHeapNextBase = aNewEnd;
    mMappings.Append(map);
    return true;
}
