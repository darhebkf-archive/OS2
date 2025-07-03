/* pagetables -- A framework to experiment with memory management
 *
 *    aarch64.h - AArch64 4-level page table implementation with 16 KiB granule
 *
 * Copyright (C) 2024  Leiden University, The Netherlands.
 */

#ifndef __ARCH_AARCH64__
#define __ARCH_AARCH64__

#include "mmu.h"
#include "oskernel.h"

#include <map>
#include <unordered_map>

namespace AArch64 {

/* AArch64 addressing specification:
 * - 48-bit virtual addresses → 48-bit physical addresses
 * - 16 KiB page size (14-bit page offset)
 * - Address breakdown: 1 bit + 11 bits + 11 bits + 11 bits + 14 bits = 48 bits
 * - First level: 2 entries, other levels: 2048 entries each
 */
const static uint64_t addressSpaceBits = 48;
const static uint64_t pageBits = 14; /* 16 KiB / page */
const static uint64_t pageSize = 1UL << pageBits;

/* Page table level definitions */
const static uint64_t L0_BITS = 1;   /* Level 0: 1 bit, 2 entries */
const static uint64_t L1_BITS = 11;  /* Level 1: 11 bits, 2048 entries */
const static uint64_t L2_BITS = 11;  /* Level 2: 11 bits, 2048 entries */
const static uint64_t L3_BITS = 11;  /* Level 3: 11 bits, 2048 entries */

const static uint64_t L0_ENTRIES = 1UL << L0_BITS;
const static uint64_t L1_ENTRIES = 1UL << L1_BITS;
const static uint64_t L2_ENTRIES = 1UL << L2_BITS;
const static uint64_t L3_ENTRIES = 1UL << L3_BITS;

/* Extract page table indices from virtual address */
#define L0_INDEX(vaddr) (((vaddr) >> (L1_BITS + L2_BITS + L3_BITS + pageBits)) & ((1UL << L0_BITS) - 1))
#define L1_INDEX(vaddr) (((vaddr) >> (L2_BITS + L3_BITS + pageBits)) & ((1UL << L1_BITS) - 1))
#define L2_INDEX(vaddr) (((vaddr) >> (L3_BITS + pageBits)) & ((1UL << L2_BITS) - 1))
#define L3_INDEX(vaddr) (((vaddr) >> pageBits) & ((1UL << L3_BITS) - 1))

/* Page table alignment - AArch64 requires 16 KiB alignment for page tables */
const static uint64_t pageTableAlign = pageSize;

/* 64-bit page table entry format for AArch64 */
struct __attribute__ ((__packed__)) TableEntry
{
  uint8_t valid : 1;        /* Valid bit */
  uint8_t type : 1;         /* 0=block, 1=table (for L0-L2), ignored for L3 */
  uint8_t attrIndx : 3;     /* Memory attribute index */
  uint8_t ns : 1;           /* Non-secure bit */
  uint8_t ap : 2;           /* Access permissions */
  uint8_t sh : 2;           /* Shareability field */
  uint8_t af : 1;           /* Access flag (referenced bit) */
  uint8_t ng : 1;           /* Not global bit */
  uint16_t reserved1 : 4;   /* Reserved bits */
  
  /* For AArch64, we support 48-bit physical addresses */
  uint64_t physicalPageNum : 34;  /* Physical page number (bits 47:14) */
  
  uint8_t reserved2 : 7;    /* Reserved bits */
  uint8_t pxn : 1;          /* Privileged execute never */
  uint8_t uxn : 1;          /* Unprivileged execute never */
  uint8_t ignored : 4;      /* Software-defined bits */
  uint8_t pbha : 4;         /* Page-based hardware attributes */
};

/* For simplicity in our implementation, we'll also define a more manageable entry format */
struct SimpleTableEntry
{
  uint64_t valid : 1;
  uint64_t type : 1;        /* 0=invalid/page, 1=table */
  uint64_t reserved : 10;
  uint64_t physicalPageNum : 34;  /* Physical page number */
  uint64_t referenced : 1;   /* Access flag */
  uint64_t dirty : 1;       /* Modified bit (software managed) */
  uint64_t padding : 16;
};

/*
 * MMU hardware part (arch/aarch64/mmu.cc)
 */

class AArch64MMU : public MMU
{
  private:
    /* TLB support is now implemented in base MMU class */
    
  public:
    AArch64MMU();
    virtual ~AArch64MMU();

    virtual uint8_t getPageBits(void) const override
    {
      return pageBits;
    }

    virtual uint64_t getPageSize(void) const override
    {
      return pageSize;
    }

    virtual uint8_t getAddressSpaceBits(void) const override
    {
      return addressSpaceBits;
    }

    virtual bool performTranslation(const uint64_t vPage,
                                    uint64_t &pPage,
                                    bool isWrite) override;
};

/*
 * OS driver part (arch/aarch64/driver.cc)
 */

class AArch64MMUDriver : public MMUDriver
{
  protected:
    /* Map PID to L0 page table root */
    std::unordered_map<uint64_t, SimpleTableEntry *> pageTables;
    
    /* Track allocated bytes for page tables */
    uint64_t bytesAllocated;
    
    /* Reference to kernel for memory allocation */
    OSKernel *kernel;  /* no ownership */

    /* Helper methods for page table management */
    SimpleTableEntry* allocatePageTableLevel(void);
    void releasePageTableLevel(SimpleTableEntry *table, int level);
    SimpleTableEntry* getOrCreateTable(SimpleTableEntry *parent, uint64_t index);
    
  public:
    AArch64MMUDriver();
    virtual ~AArch64MMUDriver() override;

    virtual void      setHostKernel(OSKernel *kernel) override;
    virtual uint64_t  getPageSize(void) const override;

    virtual void      allocatePageTable(const uint64_t PID) override;
    virtual void      releasePageTable(const uint64_t PID) override;
    virtual uintptr_t getPageTable(const uint64_t PID) override;

    virtual void      setMapping(const uint64_t PID,
                                 uintptr_t vAddr,
                                 PhysPage &pPage) override;

    virtual void      setPageValid(PhysPage &pPage,
                                   bool setting) override;

    virtual uint64_t  getBytesAllocated(void) const override;

    /* Disallow objects from being copied, since it has a pointer member. */
    AArch64MMUDriver(const AArch64MMUDriver &driver) = delete;
    void operator=(const AArch64MMUDriver &driver) = delete;
};

} /* namespace AArch64 */

#endif /* __ARCH_AARCH64__ */