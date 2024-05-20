#include "tlb.h"

TLB::TLB(size_t numEntries) : numEntries(numEntries) {}

void TLB::addEntry(uint64_t virtualAddress, uint64_t physicalAddress, uint64_t asid) {
    auto it = cache.find(virtualAddress);
    if (it != cache.end()) {
        entries.erase(it->second);
        cache.erase(it);
    }
    if (entries.size() == numEntries) {
        auto last = entries.back();
        cache.erase(last.virtualAddress);
        entries.pop_back();
    }
    entries.push_front({virtualAddress, physicalAddress, asid});
    cache[virtualAddress] = entries.begin();
}

bool TLB::lookup(uint64_t virtualAddress, uint64_t& physicalAddress, uint64_t asid) {
    auto it = cache.find(virtualAddress);
    if (it != cache.end() && it->second->asid == asid) {
        physicalAddress = it->second->physicalAddress;
        entries.splice(entries.begin(), entries, it->second);
        return true;
    }
    return false;
}

void TLB::flush() {
    entries.clear();
    cache.clear();
}

void TLB::flushASID(uint64_t asid) {
    for (auto it = entries.begin(); it != entries.end(); ) {
        if (it->asid == asid) {
            cache.erase(it->virtualAddress);
            it = entries.erase(it);
        } else {
            ++it;
        }
    }
}
