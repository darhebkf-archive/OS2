#ifndef OS_KERNEL_H
#define OS_KERNEL_H

#include "phys_mem_manager.h"
#include <unordered_map>
#include <vector>

class OSKernel {
public:
    void allocatePage(uint64_t processID, uint64_t virtualAddress);
    void terminateProcess(uint64_t processID);

private:
    std::unordered_map<uint64_t, std::vector<uint64_t>> processPages;
    PhysMemManager physMemManager;
};

#endif // OS_KERNEL_H
