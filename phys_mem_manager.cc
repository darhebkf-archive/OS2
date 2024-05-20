#include "phys_mem_manager.h"

PhysMemManager::PhysMemManager() {
    // Initialize with a single large hole
    holes.push_back({0, 1ULL << 40}); // Example: 1 TB of physical memory
}

uint64_t PhysMemManager::allocate(uint64_t size) {
    for (auto it = holes.begin(); it != holes.end(); ++it) {
        if (it->size >= size) {
            uint64_t address = it->start;
            if (it->size > size) {
                it->start += size;
                it->size -= size;
            } else {
                holes.erase(it);
            }
            return address;
        }
    }
    throw std::bad_alloc();
}

void PhysMemManager::release(uint64_t address, uint64_t size) {
    // Find the correct place to insert the released memory
    auto it = holes.begin();
    for (; it != holes.end(); ++it) {
        if (it->start > address) {
            break;
        }
    }
    holes.insert(it, {address, size});
    // Merge adjacent holes
    auto prev = it == holes.begin() ? it : std::prev(it);
    if (prev != holes.end() && prev->start + prev->size == address) {
        prev->size += size;
        holes.erase(it);
        it = prev;
    }
    auto next = std::next(it);
    if (next != holes.end() && it->start + it->size == next->start) {
        it->size += next->size;
        holes.erase(next);
    }
}
