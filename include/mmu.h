/* pagetables -- A framework to experiment with memory management
 *
 *    mmu.h - Memory Management Unit component
 *
 * Copyright (C) 2017  Leiden University, The Netherlands.
 */

#ifndef __MMU_H__
#define __MMU_H__

#include <functional>

#include "process.h" /* for MemAccess */


using PageFaultFunction = std::function<void(uintptr_t)>;

class MMU;

class TLB
{
  protected:
    /* Number of entries in TLB */
    const size_t nEntries;

    /* Reference to MMU; to be filled by initializer list in constructor */
    const MMU &mmu;

    /* TLB statistics */
    int nLookups;
    int nHits;
    int nEvictions;
    int nFlush;
    int nFlushEvictions;

  public:
    TLB(const size_t nEntries, const MMU &mmu);
    ~TLB();

    /* This method should lookup the virtual page number to a physical page number */
    bool lookup(const uint64_t vPage, uint64_t &pPage);

    /* This method should store a physical page number for a given virtual page number */
    void add(const uint64_t vPage, const uint64_t pPage);

    /* This method should flush all TLB entries upon a context switch */
    void flush(void);

    /* This method should clear the entire state of the TLB */
    void clear(void);

    /* This method should yield all TLB statistics */
    void getStatistics(int &nLookups, int &nHits, int &nEvictions,
                       int &nFlush, int &nFlushEvictions) const;
};

class MMU
{
  protected:
    uintptr_t root;
    PageFaultFunction pageFaultHandler;

  public:
    MMU();
    virtual ~MMU();

    void initialize(PageFaultFunction pageFaultHandler);
    void setPageTablePointer(const uintptr_t root);
    void processMemAccess(const MemAccess &access);

    uint64_t makePhysicalAddr(const MemAccess &access, const uint64_t pPage);
    bool getTranslation(const MemAccess &access, uint64_t &pAddr);

    /* This method is used to acquire statistics from the TLB implementation */
    void getTLBStatistics(int &nLookups, int &nHits,
                          int &nEvictions,
                          int &nFlush, int &nFlushEvictions);

    /* These methods should return the architecture's page size / bits. */
    virtual uint8_t getPageBits(void) const = 0;
    virtual uint64_t getPageSize(void) const = 0;
    virtual uint8_t getAddressSpaceBits(void) const = 0;

    /* An implementation of the "performTranslation" method should translate
     * the given virtual page *number* to a physical page number. We
     * consider a page number to be the address with the page offset
     * shifted away. We use page numbers instead of full addresses to
     * simplify the implementation of a TLB.
     */
    virtual bool performTranslation(const uint64_t vPage,
                                    uint64_t &pPage,
                                    bool isWrite) = 0;
};


#endif /* __MMU_H__ */
