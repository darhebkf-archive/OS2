/* pagetables -- A framework to experiment with memory management
 *
 *    physmemmanager.h - Physical Memory Manager
 *
 * Copyright (C) 2017-2020  Leiden University, The Netherlands.
 */

#include "physmemmanager.h"

#include <iostream>

#include <string.h>
#include <sys/mman.h>  /* mmap() */


PhysMemManager::PhysMemManager(const uint64_t pageSize,
                               const uint64_t memorySize)
  : baseAddress(nullptr), pageSize(pageSize), memorySize(memorySize),
    nPages(memorySize / pageSize), nAllocatedPages(0), maxAllocatedPages(0),
    allocated(nPages, false)
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

bool
PhysMemManager::allocatePages(size_t count, uintptr_t &addr)
{
  /* Check upfront if pages are available at all. */
  if (nAllocatedPages + count > nPages)
    return false;

  size_t index = 0;
  while (index < nPages)
    {
      if (allocated[index] == false)
        {
          /* Test if a sequence of "count" free pages is available. */
          size_t start = index;
          for ( ; index < start + count and index < nPages; ++index)
            {
              if (allocated[index] == true)
                break;
            }

          /* Only if we managed to move index "count" pages, the test
           * succeeded.
           */
          if (index == start + count)
            {
              /* Mark pages as allocated. */
              for (size_t i = start; i < index; ++i)
                allocated[i] = true;

              addr = (uintptr_t)baseAddress + start * pageSize;
              nAllocatedPages += count;
              maxAllocatedPages = std::max(maxAllocatedPages, nAllocatedPages);

              return true;
            }
        }

      ++index;
    }

  return false;
}

void
PhysMemManager::releasePages(uintptr_t addr, size_t count)
{
  /* Translate address back to index. */
  uint64_t index = (addr - (uintptr_t)baseAddress) / pageSize;

  for (size_t i = index; i < index + count; ++i)
    allocated[i] = false;

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
