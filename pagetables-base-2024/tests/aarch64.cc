/* pagetables -- A framework to experiment with memory management
 *
 *    tests/aarch64.cc - unit tests for AArch64 page table implementation.
 *
 * Copyright (C) 2024  Leiden University, The Netherlands.
 */

#define BOOST_TEST_MODULE AArch64
#include <boost/test/unit_test.hpp>

#include "arch/include/aarch64.h"
#include "os/physmemmanager.h"
#include "oskernel.h"
using namespace AArch64;

#include <memory>
#include <vector>

constexpr static uint64_t MemorySize = (1 * 1024 * 1024) << 10;

BOOST_AUTO_TEST_SUITE(aarch64_test)

/*
 * Test AArch64MMU class separately
 */

struct AArch64MMUFixture
{
  AArch64MMU mmu;
  SimpleTableEntry *l0_table;
  SimpleTableEntry *l1_table;
  SimpleTableEntry *l2_table; 
  SimpleTableEntry *l3_table;

  AArch64MMUFixture()
    : mmu(), l0_table(nullptr), l1_table(nullptr), l2_table(nullptr), l3_table(nullptr)
  {
    /* Allocate and clear L0 page table (2 entries) */
    l0_table = new (std::align_val_t{ pageSize }, std::nothrow) SimpleTableEntry[L0_ENTRIES];
    std::fill_n(l0_table, L0_ENTRIES, SimpleTableEntry{ 0 });

    /* Allocate and clear L1 page table (2048 entries) */
    l1_table = new (std::align_val_t{ pageSize }, std::nothrow) SimpleTableEntry[L1_ENTRIES];
    std::fill_n(l1_table, L1_ENTRIES, SimpleTableEntry{ 0 });

    /* Allocate and clear L2 page table (2048 entries) */
    l2_table = new (std::align_val_t{ pageSize }, std::nothrow) SimpleTableEntry[L2_ENTRIES];
    std::fill_n(l2_table, L2_ENTRIES, SimpleTableEntry{ 0 });

    /* Allocate and clear L3 page table (2048 entries) */
    l3_table = new (std::align_val_t{ pageSize }, std::nothrow) SimpleTableEntry[L3_ENTRIES];
    std::fill_n(l3_table, L3_ENTRIES, SimpleTableEntry{ 0 });

    mmu.setPageTablePointer(reinterpret_cast<uintptr_t>(l0_table));
  }

  ~AArch64MMUFixture()
  {
    operator delete[](l0_table, std::align_val_t{ pageSize }, std::nothrow);
    operator delete[](l1_table, std::align_val_t{ pageSize }, std::nothrow);
    operator delete[](l2_table, std::align_val_t{ pageSize }, std::nothrow);
    operator delete[](l3_table, std::align_val_t{ pageSize }, std::nothrow);
  }

  void setupValidMapping(uint64_t vPage, uint64_t pPage)
  {
    uint64_t vAddr = vPage << pageBits;
    
    uint64_t l0_idx = L0_INDEX(vAddr);
    uint64_t l1_idx = L1_INDEX(vAddr);
    uint64_t l2_idx = L2_INDEX(vAddr);
    uint64_t l3_idx = L3_INDEX(vAddr);

    /* Set up L0 -> L1 mapping */
    l0_table[l0_idx].valid = 1;
    l0_table[l0_idx].type = 1;
    l0_table[l0_idx].physicalPageNum = reinterpret_cast<uintptr_t>(l1_table) >> pageBits;

    /* Set up L1 -> L2 mapping */
    l1_table[l1_idx].valid = 1;
    l1_table[l1_idx].type = 1;
    l1_table[l1_idx].physicalPageNum = reinterpret_cast<uintptr_t>(l2_table) >> pageBits;

    /* Set up L2 -> L3 mapping */
    l2_table[l2_idx].valid = 1;
    l2_table[l2_idx].type = 1;
    l2_table[l2_idx].physicalPageNum = reinterpret_cast<uintptr_t>(l3_table) >> pageBits;

    /* Set up L3 -> physical page mapping */
    l3_table[l3_idx].valid = 1;
    l3_table[l3_idx].type = 0;
    l3_table[l3_idx].physicalPageNum = pPage;
  }

  AArch64MMUFixture(const AArch64MMUFixture &) = delete;
  AArch64MMUFixture &operator=(const AArch64MMUFixture &) = delete;
};

/* Test empty page table */
BOOST_FIXTURE_TEST_CASE( empty_page_table, AArch64MMUFixture )
{
  uint64_t pPage;
  BOOST_CHECK( mmu.performTranslation(0, pPage, false) == false );
  BOOST_CHECK( mmu.performTranslation(1, pPage, false) == false );
  BOOST_CHECK( mmu.performTranslation(0xFFFF, pPage, false) == false );
}

/* Test valid translation */
BOOST_FIXTURE_TEST_CASE( valid_translation, AArch64MMUFixture )
{
  uint64_t vPage = 0x12345;
  uint64_t expectedPPage = 0xABCDE;
  
  setupValidMapping(vPage, expectedPPage);
  
  uint64_t actualPPage;
  BOOST_CHECK( mmu.performTranslation(vPage, actualPPage, false) == true );
  BOOST_CHECK_EQUAL( actualPPage, expectedPPage );
}

/* Test referenced bit setting */
BOOST_FIXTURE_TEST_CASE( referenced_bit_test, AArch64MMUFixture )
{
  uint64_t vPage = 0x1000;
  uint64_t pPage = 0x2000;
  
  setupValidMapping(vPage, pPage);
  
  /* Initially referenced bit should be 0 */
  uint64_t vAddr = vPage << pageBits;
  uint64_t l3_idx = L3_INDEX(vAddr);
  BOOST_CHECK_EQUAL( l3_table[l3_idx].referenced, 0 );
  
  /* After translation, referenced bit should be set */
  uint64_t resultPPage;
  BOOST_CHECK( mmu.performTranslation(vPage, resultPPage, false) == true );
  BOOST_CHECK_EQUAL( l3_table[l3_idx].referenced, 1 );
}

/* Test dirty bit setting on write */
BOOST_FIXTURE_TEST_CASE( dirty_bit_test, AArch64MMUFixture )
{
  uint64_t vPage = 0x1000;
  uint64_t pPage = 0x2000;
  
  setupValidMapping(vPage, pPage);
  
  uint64_t vAddr = vPage << pageBits;
  uint64_t l3_idx = L3_INDEX(vAddr);
  
  /* Initially dirty bit should be 0 */
  BOOST_CHECK_EQUAL( l3_table[l3_idx].dirty, 0 );
  
  /* Read access should not set dirty bit */
  uint64_t resultPPage;
  BOOST_CHECK( mmu.performTranslation(vPage, resultPPage, false) == true );
  BOOST_CHECK_EQUAL( l3_table[l3_idx].dirty, 0 );
  
  /* Write access should set dirty bit */
  BOOST_CHECK( mmu.performTranslation(vPage, resultPPage, true) == true );
  BOOST_CHECK_EQUAL( l3_table[l3_idx].dirty, 1 );
}

/* Test architecture parameters */
BOOST_AUTO_TEST_CASE( architecture_parameters )
{
  AArch64MMU mmu;
  
  BOOST_CHECK_EQUAL( mmu.getPageBits(), 14 );  /* 16 KiB pages */
  BOOST_CHECK_EQUAL( mmu.getPageSize(), 16384 );
  BOOST_CHECK_EQUAL( mmu.getAddressSpaceBits(), 48 );
}

/*
 * Test TLB functionality
 */

BOOST_AUTO_TEST_CASE( tlb_basic_functionality )
{
  AArch64MMU mmu;
  TLB tlb(4, mmu);  /* 4-entry TLB for testing */
  
  /* Test initial state */
  uint64_t pPage;
  BOOST_CHECK( tlb.lookup(0x1000, pPage) == false );
  
  /* Add entry and test lookup */
  tlb.add(0x1000, 0x2000);
  BOOST_CHECK( tlb.lookup(0x1000, pPage) == true );
  BOOST_CHECK_EQUAL( pPage, 0x2000 );
  
  /* Test miss */
  BOOST_CHECK( tlb.lookup(0x3000, pPage) == false );
}

BOOST_AUTO_TEST_CASE( tlb_lru_replacement )
{
  AArch64MMU mmu;
  TLB tlb(2, mmu);  /* 2-entry TLB for testing LRU */
  
  /* Fill TLB */
  tlb.add(0x1000, 0x2000);
  tlb.add(0x3000, 0x4000);
  
  /* Both should be present */
  uint64_t pPage;
  BOOST_CHECK( tlb.lookup(0x1000, pPage) == true );
  BOOST_CHECK( tlb.lookup(0x3000, pPage) == true );
  
  /* Access first entry to make it most recently used */
  BOOST_CHECK( tlb.lookup(0x1000, pPage) == true );
  
  /* Add third entry - should evict 0x3000 (least recently used) */
  tlb.add(0x5000, 0x6000);
  
  /* 0x1000 should still be there, 0x3000 should be evicted */
  BOOST_CHECK( tlb.lookup(0x1000, pPage) == true );
  BOOST_CHECK( tlb.lookup(0x3000, pPage) == false );
  BOOST_CHECK( tlb.lookup(0x5000, pPage) == true );
}

BOOST_AUTO_TEST_CASE( tlb_flush )
{
  AArch64MMU mmu;
  TLB tlb(4, mmu);
  
  /* Add entries */
  tlb.add(0x1000, 0x2000);
  tlb.add(0x3000, 0x4000);
  
  /* Verify they exist */
  uint64_t pPage;
  BOOST_CHECK( tlb.lookup(0x1000, pPage) == true );
  BOOST_CHECK( tlb.lookup(0x3000, pPage) == true );
  
  /* Flush and verify all entries are gone */
  tlb.flush();
  BOOST_CHECK( tlb.lookup(0x1000, pPage) == false );
  BOOST_CHECK( tlb.lookup(0x3000, pPage) == false );
}

BOOST_AUTO_TEST_CASE( tlb_statistics )
{
  AArch64MMU mmu;
  TLB tlb(4, mmu);
  
  int nLookups, nHits, nEvictions, nFlush, nFlushEvictions;
  tlb.getStatistics(nLookups, nHits, nEvictions, nFlush, nFlushEvictions);
  
  /* Initial statistics should be zero */
  BOOST_CHECK_EQUAL( nLookups, 0 );
  BOOST_CHECK_EQUAL( nHits, 0 );
  BOOST_CHECK_EQUAL( nEvictions, 0 );
  BOOST_CHECK_EQUAL( nFlush, 0 );
  BOOST_CHECK_EQUAL( nFlushEvictions, 0 );
  
  /* Add entry and test statistics */
  tlb.add(0x1000, 0x2000);
  
  uint64_t pPage;
  tlb.lookup(0x1000, pPage);  /* Hit */
  tlb.lookup(0x3000, pPage);  /* Miss */
  
  tlb.getStatistics(nLookups, nHits, nEvictions, nFlush, nFlushEvictions);
  BOOST_CHECK_EQUAL( nLookups, 2 );
  BOOST_CHECK_EQUAL( nHits, 1 );
}

/*
 * Test PhysMemManager with hole list
 */

BOOST_AUTO_TEST_CASE( physmem_basic_allocation )
{
  PhysMemManager manager(pageSize, 10 * pageSize);
  
  uintptr_t addr1, addr2;
  
  /* Test basic allocation */
  BOOST_CHECK( manager.allocatePages(1, addr1) == true );
  BOOST_CHECK( manager.allocatePages(2, addr2) == true );
  
  /* Addresses should be different */
  BOOST_CHECK( addr1 != addr2 );
  
  /* Test release */
  manager.releasePages(addr1, 1);
  manager.releasePages(addr2, 2);
  
  BOOST_CHECK( manager.allReleased() == true );
}

BOOST_AUTO_TEST_CASE( physmem_hole_merging )
{
  PhysMemManager manager(pageSize, 10 * pageSize);
  
  uintptr_t addr1, addr2, addr3;
  
  /* Allocate three contiguous blocks */
  BOOST_CHECK( manager.allocatePages(2, addr1) == true );
  BOOST_CHECK( manager.allocatePages(2, addr2) == true );
  BOOST_CHECK( manager.allocatePages(2, addr3) == true );
  
  /* Release middle block first */
  manager.releasePages(addr2, 2);
  
  /* Release first block - should merge with middle */
  manager.releasePages(addr1, 2);
  
  /* Release last block - should merge into one large hole */
  manager.releasePages(addr3, 2);
  
  /* Should be able to allocate all 6 pages as one block */
  uintptr_t largeAddr;
  BOOST_CHECK( manager.allocatePages(6, largeAddr) == true );
  
  manager.releasePages(largeAddr, 6);
  BOOST_CHECK( manager.allReleased() == true );
}

BOOST_AUTO_TEST_CASE( physmem_first_fit )
{
  PhysMemManager manager(pageSize, 10 * pageSize);
  
  uintptr_t addr1, addr2, addr3, addr4;
  
  /* Create fragmented memory */
  BOOST_CHECK( manager.allocatePages(2, addr1) == true );
  BOOST_CHECK( manager.allocatePages(3, addr2) == true );
  BOOST_CHECK( manager.allocatePages(2, addr3) == true );
  
  /* Release first and third blocks, creating two holes */
  manager.releasePages(addr1, 2);  /* 2-page hole */
  manager.releasePages(addr3, 2);  /* 2-page hole */
  
  /* Request 2 pages - should get first hole (first-fit) */
  BOOST_CHECK( manager.allocatePages(2, addr4) == true );
  BOOST_CHECK_EQUAL( addr4, addr1 );  /* Should reuse first hole */
  
  /* Clean up */
  manager.releasePages(addr2, 3);
  manager.releasePages(addr4, 2);
  BOOST_CHECK( manager.allReleased() == true );
}

BOOST_AUTO_TEST_CASE( physmem_out_of_memory )
{
  PhysMemManager manager(pageSize, 3 * pageSize);  /* Only 3 pages */
  
  uintptr_t addr1, addr2;
  
  /* Allocate all memory */
  BOOST_CHECK( manager.allocatePages(3, addr1) == true );
  
  /* Should fail to allocate more */
  BOOST_CHECK( manager.allocatePages(1, addr2) == false );
  
  /* Release and try again */
  manager.releasePages(addr1, 3);
  BOOST_CHECK( manager.allocatePages(1, addr2) == true );
  
  manager.releasePages(addr2, 1);
  BOOST_CHECK( manager.allReleased() == true );
}

BOOST_AUTO_TEST_SUITE_END()