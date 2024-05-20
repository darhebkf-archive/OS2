#ifndef PHYS_MEM_MANAGER_H
#define PHYS_MEM_MANAGER_H

#include <list>
#include <cstdint>

struct MemoryHole {
    uint64_t start;
    uint64_t size;
};

class PhysMemManager {
public:
    PhysMemManager();
    uint64_t allocate(uint64_t size);
    void release(uint64_t address, uint64_t size);

private:
    std::list<MemoryHole> holes;
};

#endif // PHYS_MEM_MANAGER_H
