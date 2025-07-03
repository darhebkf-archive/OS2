/* pagetables -- A framework to experiment with memory management
 *
 *    physmemmanager.h - Physical Memory Manager
 *
 * Copyright (C) 2017-2020  Leiden University, The Netherlands.
 */

#ifndef __PHYSMEMMANAGER_H__
#define __PHYSMEMMANAGER_H__

#include "settings.h"

#include <vector>
#include <list>

/* We assume that the RAM starts at 16GB in the physical address space.
 */
const static uint64_t physMemBase = 16UL*1024*1024*1024;


struct Hole
{
  uint64_t startPage;
  uint64_t count;
  
  Hole(uint64_t start, uint64_t cnt) : startPage(start), count(cnt) {}
};

class PhysMemManager
{
  protected:
    void *baseAddress;
    const uint64_t pageSize;
    const uint64_t memorySize;

    uint64_t nPages;
    uint64_t nAllocatedPages;
    uint64_t maxAllocatedPages;
    
    /* Hole list: maintains free memory regions */
    std::list<Hole> holes;
    
    /* Helper methods for hole management */
    void mergeHoles();
    std::list<Hole>::iterator findFit(size_t count);
    void addHole(uint64_t startPage, uint64_t count);
    void removeHole(std::list<Hole>::iterator it);
    void splitHole(std::list<Hole>::iterator it, uint64_t startPage, uint64_t count);

  public:
    PhysMemManager(const uint64_t pageSize, const uint64_t memorySize);
    ~PhysMemManager();

    bool      allocatePages(size_t count, uintptr_t &addr);
    void      releasePages(uintptr_t addr, size_t count);

    bool      allReleased(void) const;
    uint64_t  getMaxAllocatedPages(void) const;

    PhysMemManager(const PhysMemManager &) = delete;
    PhysMemManager &operator=(const PhysMemManager &) = delete;
};

#endif /* __PHYSMEMMANAGER_H__ */
