/* pagetables -- A framework to experiment with memory management
 *
 *    aarch64/driver.cc - AArch64 4-level page table implementation
 *                        OS driver part.
 *
 * Copyright (C) 2024  Leiden University, The Netherlands.
 */

#include "aarch64.h"
using namespace AArch64;

#include <iostream>
#include <cstring>

/*
 * Helper functions for page table management
 */

static inline void
initTableEntry(SimpleTableEntry &entry, const uintptr_t address, bool isTable)
{
  std::memset(&entry, 0, sizeof(entry));
  entry.physicalPageNum = address >> pageBits;
  entry.valid = 1;
  entry.type = isTable ? 1 : 0;
  entry.referenced = 0;
  entry.dirty = 0;
}

static inline uintptr_t
getAddress(const SimpleTableEntry &entry)
{
  return entry.physicalPageNum << pageBits;
}

/*
 * AArch64MMUDriver implementation
 */

AArch64MMUDriver::AArch64MMUDriver()
  : pageTables(), bytesAllocated(0), kernel(nullptr)
{
}

AArch64MMUDriver::~AArch64MMUDriver()
{
  if (!pageTables.empty())
    std::cerr << "AArch64MMUDriver: error: kernel did not release all page tables."
              << std::endl;
}

void
AArch64MMUDriver::setHostKernel(OSKernel *kernel)
{
  this->kernel = kernel;
}

uint64_t
AArch64MMUDriver::getPageSize(void) const
{
  return pageSize;
}

SimpleTableEntry*
AArch64MMUDriver::allocatePageTableLevel(void)
{
  /* Calculate size based on maximum entries per level (L1/L2/L3 have 2048 entries) */
  size_t tableSize = L1_ENTRIES * sizeof(SimpleTableEntry);
  
  SimpleTableEntry *table = reinterpret_cast<SimpleTableEntry *>
    (kernel->allocateMemory(tableSize, pageTableAlign));
  
  bytesAllocated += tableSize;
  
  /* Initialize all entries to invalid */
  std::memset(table, 0, tableSize);
  
  return table;
}

void
AArch64MMUDriver::releasePageTableLevel(SimpleTableEntry *table, int level)
{
  if (!table)
    return;
    
  size_t tableSize;
  size_t numEntries;
  
  /* Determine table size based on level */
  if (level == 0) {
    numEntries = L0_ENTRIES;
  } else {
    numEntries = L1_ENTRIES; /* L1, L2, L3 all have same number of entries */
  }
  
  tableSize = numEntries * sizeof(SimpleTableEntry);
  
  /* For non-leaf levels, recursively release child tables */
  if (level < 3) {
    for (size_t i = 0; i < numEntries; ++i) {
      if (table[i].valid && table[i].type == 1) {
        SimpleTableEntry *childTable = reinterpret_cast<SimpleTableEntry *>
          (getAddress(table[i]));
        releasePageTableLevel(childTable, level + 1);
      }
    }
  }
  
  kernel->releaseMemory(table, tableSize);
}

SimpleTableEntry*
AArch64MMUDriver::getOrCreateTable(SimpleTableEntry *parent, uint64_t index)
{
  if (!parent[index].valid) {
    /* Allocate new page table level */
    SimpleTableEntry *newTable = allocatePageTableLevel();
    initTableEntry(parent[index], reinterpret_cast<uintptr_t>(newTable), true);
    return newTable;
  } else if (parent[index].type == 1) {
    /* Return existing table */
    return reinterpret_cast<SimpleTableEntry *>(getAddress(parent[index]));
  } else {
    /* This should not happen in a well-formed page table */
    throw std::runtime_error("Invalid page table entry: expected table but found page");
  }
}

void
AArch64MMUDriver::allocatePageTable(const uint64_t PID)
{
  /* Allocate L0 table (root) - it only has 2 entries */
  size_t l0Size = L0_ENTRIES * sizeof(SimpleTableEntry);
  SimpleTableEntry *l0_table = reinterpret_cast<SimpleTableEntry *>
    (kernel->allocateMemory(l0Size, pageTableAlign));
  
  bytesAllocated += l0Size;
  
  /* Initialize L0 table */
  std::memset(l0_table, 0, l0Size);
  
  pageTables[PID] = l0_table;
}

void
AArch64MMUDriver::releasePageTable(const uint64_t PID)
{
  auto it = pageTables.find(PID);
  if (it != pageTables.end()) {
    releasePageTableLevel(it->second, 0);
    pageTables.erase(it);
  }
}

uintptr_t
AArch64MMUDriver::getPageTable(const uint64_t PID)
{
  auto it = pageTables.find(PID);
  if (it == pageTables.end())
    return 0x0;
  
  return reinterpret_cast<uintptr_t>(it->second);
}

void
AArch64MMUDriver::setMapping(const uint64_t PID,
                             uintptr_t vAddr,
                             PhysPage &pPage)
{
  /* Ensure unused address bits are zero */
  vAddr &= (1UL << addressSpaceBits) - 1;
  
  /* Extract indices for each level */
  uint64_t l0_idx = L0_INDEX(vAddr);
  uint64_t l1_idx = L1_INDEX(vAddr);
  uint64_t l2_idx = L2_INDEX(vAddr);
  uint64_t l3_idx = L3_INDEX(vAddr);
  
  auto it = pageTables.find(PID);
  if (it == pageTables.end())
    throw std::runtime_error("Page table not found for PID");
  
  SimpleTableEntry *l0_table = it->second;
  
  /* Walk/create the page table hierarchy */
  SimpleTableEntry *l1_table = getOrCreateTable(l0_table, l0_idx);
  SimpleTableEntry *l2_table = getOrCreateTable(l1_table, l1_idx);
  SimpleTableEntry *l3_table = getOrCreateTable(l2_table, l2_idx);
  
  /* Set the final page mapping in L3 table */
  initTableEntry(l3_table[l3_idx], pPage.addr, false);
  
  /* Store pointer to page table entry in PhysPage for quick access */
  pPage.driverData = &l3_table[l3_idx];
}

void
AArch64MMUDriver::setPageValid(PhysPage &pPage, bool setting)
{
  SimpleTableEntry *entry = reinterpret_cast<SimpleTableEntry *>(pPage.driverData);
  if (!entry)
    throw std::runtime_error("Invalid page table entry pointer");
  
  if (!entry->valid && setting)
    throw std::runtime_error("Cannot validate an invalid page table entry");
  
  entry->valid = setting ? 1 : 0;
}

uint64_t
AArch64MMUDriver::getBytesAllocated(void) const
{
  return bytesAllocated;
}