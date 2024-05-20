#include "aarch64_mmu.h"
#include <iostream> // For debugging purposes

AArch64MMUDriver::AArch64MMUDriver(size_t tlbEntries) : MMU(tlbEntries) {}

AArch64MMUDriver::~AArch64MMUDriver() {}

void AArch64MMUDriver::initialize(PageFaultFunction pageFaultHandler) {
    MMU::initialize(pageFaultHandler);
    pageTable.clear();
    std::cout << "AArch64 MMU initialized" << std::endl;
}

void AArch64MMUDriver::setPageTablePointer(const uintptr_t root) {
    MMU::setPageTablePointer(root);
}

void AArch64MMUDriver::processMemAccess(const MemAccess &access) {
    MMU::processMemAccess(access);
}

uint8_t AArch64MMUDriver::getPageBits(void) const {
    return 14; // 16 KiB pages
}

uint64_t AArch64MMUDriver::getPageSize(void) const {
    return 16 * 1024; // 16 KiB pages
}

uint8_t AArch64MMUDriver::getAddressSpaceBits(void) const {
    return 48; // 48-bit address space
}

bool AArch64MMUDriver::performTranslation(const uint64_t vPage, uint64_t &pPage, bool isWrite) {
    auto it = pageTable.find(vPage);
    if (it != pageTable.end()) {
        pPage = it->second;
        return true;
    } else {
        // Simulate a page fault
        std::cerr << "Page fault at virtual page: " << vPage << std::endl;
        return false;
    }
}

void AArch64MMUDriver::addMapping(uint64_t virtualAddress, uint64_t physicalAddress) {
    uint64_t vPage = virtualAddress >> getPageBits();
    pageTable[vPage] = physicalAddress >> getPageBits();
    std::cout << "Mapping added from VA: " << virtualAddress << " to PA: " << physicalAddress << std::endl;
}

void AArch64MMUDriver::releasePageTable() {
    pageTable.clear();
    std::cout << "AArch64 page table released" << std::endl;
}

bool AArch64MMUDriver::readReferencedBit(uint64_t virtualAddress) {
    uint64_t vPage = virtualAddress >> getPageBits();
    auto it = pageTable.find(vPage);
    if (it != pageTable.end()) {
        // For simplicity, assume referenced bit is set if the entry exists
        return true; // This should be updated to reflect actual reference tracking if available
    }
    return false;
}

bool AArch64MMUDriver::readDirtyBit(uint64_t virtualAddress) {
    uint64_t vPage = virtualAddress >> getPageBits();
    auto it = pageTable.find(vPage);
    if (it != pageTable.end()) {
        // For simplicity, assume dirty bit is set if the entry exists and was written to
        // This should be updated to reflect actual dirty tracking if available
        return true; // Placeholder
    }
    return false;
}
