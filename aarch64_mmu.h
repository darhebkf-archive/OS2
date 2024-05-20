#ifndef AARCH64_MMU_DRIVER_H
#define AARCH64_MMU_DRIVER_H

#include "include/mmu.h"
#include <unordered_map>

class AArch64MMUDriver : public MMU {
public:
    AArch64MMUDriver(size_t tlbEntries);
    virtual ~AArch64MMUDriver();

    void initialize(PageFaultFunction pageFaultHandler) override;
    void setPageTablePointer(const uintptr_t root) override;
    void processMemAccess(const MemAccess &access) override;

    uint8_t getPageBits(void) const override;
    uint64_t getPageSize(void) const override;
    uint8_t getAddressSpaceBits(void) const override;

    bool performTranslation(const uint64_t vPage, uint64_t &pPage, bool isWrite) override;

private:
    std::unordered_map<uint64_t, uint64_t> pageTable; // Simple page table for demonstration
};

#endif // AARCH64_MMU_DRIVER_H
