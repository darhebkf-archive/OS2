/* pagetables -- A framework to experiment with memory management
 *
 *    mmu.cc - Memory Management Unit component
 *
 * Copyright (C) 2017  Leiden University, The Netherlands.
 */

#include "mmu.h"
#include "settings.h"

#include <iostream>

MMU::MMU()
  : root(0x0), pageFaultHandler()
{
}

MMU::~MMU()
{
  int nLookups{}, nHits{}, nEvictions{}, nFlush{}, nFlushEvictions{};
  getTLBStatistics(nLookups, nHits, nEvictions, nFlush, nFlushEvictions);

  std::cerr << std::dec << std::endl
            << "TLB Statistics (since last reset):" << std::endl
            << "# lookups: " << nLookups << std::endl
            << "# hits: " << nHits
            << " (" << (((float)nHits/nLookups)*100.) << "%)" << std::endl
            << "# line evictions: " << nEvictions << std::endl
            << "# flushes: " << nFlush << std::endl
            << "# line evictions due to flush: " << nFlushEvictions << std::endl;
}

void
MMU::initialize(PageFaultFunction _pageFaultHandler)
{
  pageFaultHandler = _pageFaultHandler;
}

void
MMU::setPageTablePointer(const uintptr_t _root)
{
  root = _root;
}

void
MMU::processMemAccess(const MemAccess &access)
{
  if (root == 0x0)
    throw std::runtime_error("MMU: page table pointer is NULL, cannot continue.");

  if (LogMemoryAccesses)
    std::cerr << "MMU: memory access: " << access << std::endl;

  uint64_t pAddr = 0x0;
  while (not getTranslation(access, pAddr))
    {
      pageFaultHandler(access.addr);
    }

  if (LogMemoryAccesses)
    std::cerr << "MMU: translated virtual "
        << std::hex << std::showbase << access.addr
        << " to physical " << pAddr << std::endl;
}

uint64_t
MMU::makePhysicalAddr(const MemAccess &access, const uint64_t pPage)
{
  uint64_t pAddr = pPage << getPageBits();
  pAddr |= access.addr & (getPageSize() - 1);

  return pAddr;
}

bool
MMU::getTranslation(const MemAccess &access, uint64_t &pAddr)
{
  /* Strip off (zero out) unused sign-extension bits in virtual address */
  const uint64_t vAddr = access.addr & ((1UL << getAddressSpaceBits()) - 1);

  const uint64_t vPage = vAddr >> getPageBits();
  uint64_t pPage = 0;
  bool isWrite = (access.type == MemAccessType::Store ||
                  access.type == MemAccessType::Modify);

  /* TODO: your TLB likely needs to be glued in somewhere in this method */

  if (performTranslation(vPage, pPage, isWrite))
    {
      pAddr = makePhysicalAddr(access, pPage);
      return true;
    }

  return false;
}

void
MMU::getTLBStatistics(int &nLookups, int &nHits, int &nEvictions,
                      int &nFlush, int &nFlushEvictions)
{
  /* TODO: connect this to your TLB implementation to fill in the
   * appropriate values for the statistics.
   */
}
