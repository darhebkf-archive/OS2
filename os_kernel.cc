#include "os_kernel.h"

void OSKernel::allocatePage(uint64_t processID, uint64_t virtualAddress) {
    uint64_t physicalAddress = physMemManager.allocate(1); // Allocate one page
    processPages[processID].push_back(physicalAddress);
    // Add the mapping to the page table (not shown)
}

void OSKernel::terminateProcess(uint64_t processID) {
    auto it = processPages.find(processID);
    if (it != processPages.end()) {
        for (uint64_t address : it->second) {
            physMemManager.release(address, 1); // Release one page
        }
        processPages.erase(it);
    }
}
