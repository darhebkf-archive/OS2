/* pagetables -- A framework to experiment with memory management
 *
 *    mmu.cc - Memory Management Unit component
 *
 * Copyright (C) 2017  Leiden University, The Netherlands.
 */

#include "mmu.h"
#include "settings.h"

#include <iostream>
#include <list>
#include <unordered_map>

TLB::TLB(const size_t nEntries, const MMU &mmu)
  : nEntries(nEntries), mmu(mmu), nLookups(0), nHits(0), nEvictions(0), nFlush(0), nFlushEvictions(0), currentASID(0)
{
}

TLB::~TLB()
{
}

struct TLBEntry
{
  uint64_t vPage;
  uint64_t pPage;
  uint64_t asid;
  bool valid;
  
  TLBEntry() : vPage(0), pPage(0), asid(0), valid(false) {}
  TLBEntry(uint64_t vp, uint64_t pp, uint64_t as) : vPage(vp), pPage(pp), asid(as), valid(true) {}
};

static std::vector<TLBEntry> tlbEntries;
static std::list<size_t> lruOrder;
static std::unordered_map<uint64_t, std::list<size_t>::iterator> lruMap;
static bool tlbInitialized = false;
static uint64_t currentASID = 0;

bool
TLB::lookup(const uint64_t vPage, uint64_t &pPage)
{
  nLookups++;
  
  if (!tlbInitialized) {
    tlbEntries.resize(nEntries);
    tlbInitialized = true;
    return false;
  }
  
  for (size_t i = 0; i < nEntries; ++i) {
    if (tlbEntries[i].valid && tlbEntries[i].vPage == vPage && 
        tlbEntries[i].asid == currentASID) {
      pPage = tlbEntries[i].pPage;
      nHits++;
      
      // Update LRU: move to front
      auto it = lruMap.find(i);
      if (it != lruMap.end()) {
        lruOrder.erase(it->second);
      }
      lruOrder.push_front(i);
      lruMap[i] = lruOrder.begin();
      
      return true;
    }
  }
  
  return false;
}

void
TLB::add(const uint64_t vPage, const uint64_t pPage)
{
  if (!tlbInitialized) {
    tlbEntries.resize(nEntries);
    tlbInitialized = true;
  }
  
  size_t replaceIndex = 0;
  bool foundEmpty = false;
  
  // First try to find an empty slot
  for (size_t i = 0; i < nEntries; ++i) {
    if (!tlbEntries[i].valid) {
      replaceIndex = i;
      foundEmpty = true;
      break;
    }
  }
  
  // If no empty slot, use LRU replacement
  if (!foundEmpty) {
    if (!lruOrder.empty()) {
      replaceIndex = lruOrder.back();
      lruOrder.pop_back();
      lruMap.erase(replaceIndex);
    }
    nEvictions++;
  }
  
  // Add new entry
  tlbEntries[replaceIndex] = TLBEntry(vPage, pPage, currentASID);
  
  // Update LRU: add to front
  lruOrder.push_front(replaceIndex);
  lruMap[replaceIndex] = lruOrder.begin();
}

void
TLB::flush(void)
{
  nFlush++;
  
  if (!tlbInitialized) {
    return;
  }
  
  // Count valid entries before flush for statistics
  int validCount = 0;
  for (size_t i = 0; i < nEntries; ++i) {
    if (tlbEntries[i].valid) {
      validCount++;
    }
  }
  nFlushEvictions += validCount;
  
  // Clear all entries
  for (size_t i = 0; i < nEntries; ++i) {
    tlbEntries[i].valid = false;
  }
  
  lruOrder.clear();
  lruMap.clear();
}

void
TLB::clear(void)
{
  flush();
  nLookups = 0;
  nHits = 0;
  nEvictions = 0;
  nFlush = 0;
  nFlushEvictions = 0;
}

void
TLB::getStatistics(int &nLookups, int &nHits, int &nEvictions,
                   int &nFlush, int &nFlushEvictions) const
{
  nLookups = this->nLookups;
  nHits = this->nHits;
  nEvictions = this->nEvictions;
  nFlush = this->nFlush;
  nFlushEvictions = this->nFlushEvictions;
}

MMU::MMU()
  : root(0x0), pageFaultHandler(), tlb(nullptr), currentASID(0)
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

  // Check TLB first if available
  if (tlb && tlb->lookup(vPage, pPage)) {
    pAddr = makePhysicalAddr(access, pPage);
    return true;
  }
  
  // TLB miss - perform page table walk
  if (performTranslation(vPage, pPage, isWrite))
    {
      // Add to TLB if available
      if (tlb) {
        tlb->add(vPage, pPage);
      }
      
      pAddr = makePhysicalAddr(access, pPage);
      return true;
    }

  return false;
}

void
MMU::setTLB(std::unique_ptr<TLB> tlb_ptr)
{
  tlb = std::move(tlb_ptr);
}

void
MMU::setCurrentASID(uint64_t asid)
{
  currentASID = asid;
  ::currentASID = asid;
}

void
MMU::flushTLB(void)
{
  if (tlb) {
    tlb->flush();
  }
}

void
MMU::getTLBStatistics(int &nLookups, int &nHits, int &nEvictions,
                      int &nFlush, int &nFlushEvictions)
{
  if (tlb) {
    tlb->getStatistics(nLookups, nHits, nEvictions, nFlush, nFlushEvictions);
  } else {
    nLookups = 0;
    nHits = 0;
    nEvictions = 0;
    nFlush = 0;
    nFlushEvictions = 0;
  }
}
