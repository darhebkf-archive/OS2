/* pagetables -- A framework to experiment with memory management
 *
 *    aarch64/mmu.cc - AArch64 4-level page table implementation
 *                     Hardware MMU part.
 *
 * Copyright (C) 2024  Leiden University, The Netherlands.
 */

#include "aarch64.h"
using namespace AArch64;

#include <stdexcept>
#include <memory>

/*
 * MMU part. The MMU performs a 4-level page table walk for AArch64
 * and attempts the translation from a virtual to a physical address.
 */

AArch64MMU::AArch64MMU()
{
  // Initialize TLB with configurable size (default 64 entries)
  setTLB(std::make_unique<TLB>(64, *this));
}

AArch64MMU::~AArch64MMU()
{
}

bool
AArch64MMU::performTranslation(const uint64_t vPage,
                               uint64_t &pPage,
                               bool isWrite)
{
  /* Validate alignment of page table root */
  if ((root & (pageTableAlign - 1)) != 0)
    throw std::runtime_error("Unaligned page table access");

  /* Calculate virtual address from page number */
  uint64_t vAddr = vPage << pageBits;
  
  /* Extract indices for each level of the page table */
  uint64_t l0_idx = L0_INDEX(vAddr);
  uint64_t l1_idx = L1_INDEX(vAddr);
  uint64_t l2_idx = L2_INDEX(vAddr);
  uint64_t l3_idx = L3_INDEX(vAddr);

  /* Start with L0 table (root) */
  SimpleTableEntry *l0_table = reinterpret_cast<SimpleTableEntry *>(root);
  
  /* L0 -> L1 translation */
  if (!l0_table[l0_idx].valid || l0_table[l0_idx].type != 1)
    return false;
  
  SimpleTableEntry *l1_table = reinterpret_cast<SimpleTableEntry *>
    (l0_table[l0_idx].physicalPageNum << pageBits);
  
  /* L1 -> L2 translation */
  if (!l1_table[l1_idx].valid || l1_table[l1_idx].type != 1)
    return false;
  
  SimpleTableEntry *l2_table = reinterpret_cast<SimpleTableEntry *>
    (l1_table[l1_idx].physicalPageNum << pageBits);
  
  /* L2 -> L3 translation */
  if (!l2_table[l2_idx].valid || l2_table[l2_idx].type != 1)
    return false;
  
  SimpleTableEntry *l3_table = reinterpret_cast<SimpleTableEntry *>
    (l2_table[l2_idx].physicalPageNum << pageBits);
  
  /* L3 -> final page translation */
  if (!l3_table[l3_idx].valid)
    return false;

  /* Set the physical page number */
  pPage = l3_table[l3_idx].physicalPageNum;
  
  /* Set the referenced bit (access flag) */
  l3_table[l3_idx].referenced = 1;
  
  /* Set the dirty bit if this is a write access */
  if (isWrite)
    l3_table[l3_idx].dirty = 1;

  return true;
}