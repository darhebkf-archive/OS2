#ifndef TLB_H
#define TLB_H

#include <unordered_map>
#include <list>
#include <cstdint>

class TLB {
public:
    TLB(size_t numEntries);
    void addEntry(uint64_t virtualAddress, uint64_t physicalAddress, uint64_t asid);
    bool lookup(uint64_t virtualAddress, uint64_t& physicalAddress, uint64_t asid);
    void flush();
    void flushASID(uint64_t asid);

private:
    struct TLBEntry {
        uint64_t virtualAddress;
        uint64_t physicalAddress;
        uint64_t asid;
    };

    size_t numEntries;
    std::list<TLBEntry> entries;
    std::unordered_map<uint64_t, std::list<TLBEntry>::iterator> cache;
};

#endif // TLB_H
