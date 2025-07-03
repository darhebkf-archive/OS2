/* pagetables -- A framework to experiment with memory management
 *
 *    physmemmanager.h - Physical Memory Manager
 *
 * Copyright (C) 2017-2020  Leiden University, The Netherlands.
 */

#include "physmemmanager.h"

#include <iostream>
#include <algorithm>

#include <string.h>
#include <sys/mman.h>  /* mmap() */


PhysMemManager::PhysMemManager(const uint64_t pageSize,
                               const uint64_t memorySize)
  : baseAddress(nullptr), pageSize(pageSize), memorySize(memorySize),
    nPages(memorySize / pageSize), nAllocatedPages(0), maxAllocatedPages(0),
    holes()
{
  /* Circuit breaker: avoid too large memory allocations to avoid the user
   * of this program getting into trouble. We limit at 2 GiB.
   */
  if (memorySize > 2ULL * 1024 * 1024 * 1024)
    throw std::runtime_error("automatic protection: attempted to allocate more than 2 GiB of memory.");

  /* Try to allocate "physical" memory. */
  baseAddress = mmap((void *)physMemBase, memorySize,
                     PROT_READ | PROT_WRITE,
                     MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (baseAddress == MAP_FAILED)
    throw std::runtime_error("mmap for physical memory failed: "
                             + std::string(strerror(errno)));

  /* Initialize hole list with one large hole covering all memory */
  holes.emplace_back(0, nPages);

  std::cerr << "BOOT: system memory @ "
            << std::hex << std::showbase << baseAddress
            << " page size of " << pageSize << " bytes, "
            << std::dec
            << (memorySize / pageSize) << " pages available." << std::endl;
}

PhysMemManager::~PhysMemManager()
{
  munmap(baseAddress, memorySize);
}

std::list<Hole>::iterator
PhysMemManager::findFit(size_t count)
{
  /* First-fit algorithm: find the first hole that can accommodate 'count' pages */
  for (auto it = holes.begin(); it != holes.end(); ++it) {
    if (it->count >= count) {
      return it;
    }
  }
  return holes.end();
}

void
PhysMemManager::splitHole(std::list<Hole>::iterator it, uint64_t startPage, uint64_t count)
{
  /* Split a hole after allocating 'count' pages starting at 'startPage' */
  Hole originalHole = *it;
  holes.erase(it);
  
  /* Add hole before allocation if any */
  if (startPage > originalHole.startPage) {
    holes.emplace_back(originalHole.startPage, startPage - originalHole.startPage);
  }
  
  /* Add hole after allocation if any */
  uint64_t endPage = startPage + count;
  uint64_t originalEndPage = originalHole.startPage + originalHole.count;
  if (endPage < originalEndPage) {
    holes.emplace_back(endPage, originalEndPage - endPage);
  }
}

void
PhysMemManager::addHole(uint64_t startPage, uint64_t count)
{
  /* Add a new hole to the list */
  holes.emplace_back(startPage, count);
}

void
PhysMemManager::mergeHoles()
{
  /* Sort holes by start page for efficient merging */
  holes.sort([](const Hole &a, const Hole &b) {
    return a.startPage < b.startPage;
  });
  
  /* Merge adjacent holes */
  auto it = holes.begin();
  while (it != holes.end()) {
    auto next = std::next(it);
    if (next != holes.end() && it->startPage + it->count == next->startPage) {
      /* Merge adjacent holes */
      it->count += next->count;
      holes.erase(next);
    } else {
      ++it;
    }
  }
}

bool
PhysMemManager::allocatePages(size_t count, uintptr_t &addr)
{
  /* Check upfront if pages are available at all. */
  if (nAllocatedPages + count > nPages)
    return false;

  /* Find first hole that fits using first-fit algorithm */
  auto it = findFit(count);
  if (it == holes.end()) {
    return false;
  }
  
  /* Allocate from the beginning of the found hole */
  uint64_t startPage = it->startPage;
  
  /* Calculate physical address */
  addr = (uintptr_t)baseAddress + startPage * pageSize;
  
  /* Update hole list by splitting the hole */
  splitHole(it, startPage, count);
  
  /* Update statistics */
  nAllocatedPages += count;
  maxAllocatedPages = std::max(maxAllocatedPages, nAllocatedPages);

  return true;
}

void
PhysMemManager::releasePages(uintptr_t addr, size_t count)
{
  /* Translate address back to page index. */
  uint64_t startPage = (addr - (uintptr_t)baseAddress) / pageSize;
  
  /* Add the released pages as a new hole */
  addHole(startPage, count);
  
  /* Merge adjacent holes to minimize fragmentation */
  mergeHoles();
  
  /* Update statistics */
  nAllocatedPages -= count;
}

bool
PhysMemManager::allReleased(void) const
{
  return nAllocatedPages == 0;
}

uint64_t
PhysMemManager::getMaxAllocatedPages(void) const
{
  return maxAllocatedPages;
}