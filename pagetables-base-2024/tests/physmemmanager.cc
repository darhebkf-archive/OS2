#define BOOST_TEST_MODULE PhysMemManager
#include <boost/test/unit_test.hpp>

#include "os/physmemmanager.h"
#include <vector>
#include <set>
#include <random>
#include <utility>


BOOST_AUTO_TEST_SUITE(physmemmanager_test)

BOOST_AUTO_TEST_CASE( basic_allocation )
{
  const uint64_t pageSize = 4096;
  const uint64_t memorySize = 16 * pageSize; // 16 pages
  PhysMemManager manager(pageSize, memorySize);
  
  uintptr_t addr;
  // Test single page allocation
  BOOST_CHECK(manager.allocatePages(1, addr));
  BOOST_CHECK(addr >= physMemBase);
  BOOST_CHECK(addr < physMemBase + memorySize);
  
  // Test releasing the page
  manager.releasePages(addr, 1);
  BOOST_CHECK(manager.allReleased());
}

BOOST_AUTO_TEST_CASE( multiple_allocations )
{
  const uint64_t pageSize = 4096;
  const uint64_t memorySize = 32 * pageSize;
  PhysMemManager manager(pageSize, memorySize);
  
  std::vector<uintptr_t> addrs;
  
  // Allocate multiple single pages
  for (int i = 0; i < 10; i++) {
    uintptr_t addr;
    BOOST_CHECK(manager.allocatePages(1, addr));
    addrs.push_back(addr);
  }
  
  // Check that addresses are unique and properly aligned
  std::set<uintptr_t> uniqueAddrs(addrs.begin(), addrs.end());
  BOOST_CHECK_EQUAL(uniqueAddrs.size(), addrs.size());
  
  for (auto addr : addrs) {
    BOOST_CHECK_EQUAL(addr % pageSize, 0);
  }
  
  // Release all pages
  for (auto addr : addrs) {
    manager.releasePages(addr, 1);
  }
  BOOST_CHECK(manager.allReleased());
}

BOOST_AUTO_TEST_CASE( multi_page_allocation )
{
  const uint64_t pageSize = 4096;
  const uint64_t memorySize = 64 * pageSize;
  PhysMemManager manager(pageSize, memorySize);
  
  uintptr_t addr1, addr2, addr3;
  
  // Allocate chunks of different sizes
  BOOST_CHECK(manager.allocatePages(5, addr1));
  BOOST_CHECK(manager.allocatePages(10, addr2));
  BOOST_CHECK(manager.allocatePages(3, addr3));
  
  // Check addresses don't overlap
  BOOST_CHECK(addr2 >= addr1 + 5 * pageSize || addr2 + 10 * pageSize <= addr1);
  BOOST_CHECK(addr3 >= addr1 + 5 * pageSize || addr3 + 3 * pageSize <= addr1);
  BOOST_CHECK(addr3 >= addr2 + 10 * pageSize || addr3 + 3 * pageSize <= addr2);
  
  // Release and check
  manager.releasePages(addr1, 5);
  manager.releasePages(addr2, 10);
  manager.releasePages(addr3, 3);
  BOOST_CHECK(manager.allReleased());
}

BOOST_AUTO_TEST_CASE( out_of_memory )
{
  const uint64_t pageSize = 4096;
  const uint64_t memorySize = 10 * pageSize;
  PhysMemManager manager(pageSize, memorySize);
  
  uintptr_t addr;
  
  // Try to allocate more than available
  BOOST_CHECK(!manager.allocatePages(11, addr));
  
  // Allocate all memory
  BOOST_CHECK(manager.allocatePages(10, addr));
  
  // Try to allocate one more page
  uintptr_t addr2;
  BOOST_CHECK(!manager.allocatePages(1, addr2));
  
  // Release and try again
  manager.releasePages(addr, 10);
  BOOST_CHECK(manager.allocatePages(1, addr2));
}

BOOST_AUTO_TEST_CASE( hole_merging )
{
  const uint64_t pageSize = 4096;
  const uint64_t memorySize = 20 * pageSize;
  PhysMemManager manager(pageSize, memorySize);
  
  uintptr_t addr1, addr2, addr3;
  
  // Create fragmentation
  BOOST_CHECK(manager.allocatePages(5, addr1));
  BOOST_CHECK(manager.allocatePages(5, addr2));
  BOOST_CHECK(manager.allocatePages(5, addr3));
  
  // Release non-adjacent blocks first
  manager.releasePages(addr1, 5);
  manager.releasePages(addr3, 5);
  
  // At this point we have holes: [0-4], [10-14], and [15-19]
  // After merging, this becomes [0-4], [10-19]
  // So we can allocate 9 contiguous pages but not 10
  uintptr_t largeAddr;
  BOOST_CHECK(manager.allocatePages(9, largeAddr));
  manager.releasePages(largeAddr, 9);  // Release it for next test
  
  // Release middle block - holes should merge
  manager.releasePages(addr2, 5);
  
  // Now after merging we should have one big hole [0-19]
  // We already allocated 5 pages earlier, so we have 15 pages free
  BOOST_CHECK(manager.allocatePages(15, largeAddr));
}

BOOST_AUTO_TEST_CASE( first_fit_behavior )
{
  const uint64_t pageSize = 4096;
  const uint64_t memorySize = 30 * pageSize;
  PhysMemManager manager(pageSize, memorySize);
  
  std::vector<uintptr_t> addrs;
  
  // Allocate all memory in chunks
  for (int i = 0; i < 6; i++) {
    uintptr_t addr;
    BOOST_CHECK(manager.allocatePages(5, addr));
    addrs.push_back(addr);
  }
  
  // Release every other block to create holes
  manager.releasePages(addrs[0], 5);  // Hole at beginning
  manager.releasePages(addrs[2], 5);  // Hole in middle
  manager.releasePages(addrs[4], 5);  // Hole near end
  
  // First-fit should allocate from the first hole
  uintptr_t newAddr;
  BOOST_CHECK(manager.allocatePages(3, newAddr));
  BOOST_CHECK_EQUAL(newAddr, addrs[0]);  // Should reuse first hole
  
  // The remaining 2 pages from the first hole are added to the end of the list
  // So the next allocation will use the second hole (at addrs[2])
  uintptr_t newAddr2;
  BOOST_CHECK(manager.allocatePages(2, newAddr2));
  BOOST_CHECK_EQUAL(newAddr2, addrs[2]);  // Uses second hole, not remainder of first
}

BOOST_AUTO_TEST_CASE( max_allocated_tracking )
{
  const uint64_t pageSize = 4096;
  const uint64_t memorySize = 100 * pageSize;
  PhysMemManager manager(pageSize, memorySize);
  
  BOOST_CHECK_EQUAL(manager.getMaxAllocatedPages(), 0);
  
  uintptr_t addr1, addr2, addr3;
  
  // Allocate some pages
  BOOST_CHECK(manager.allocatePages(10, addr1));
  BOOST_CHECK_EQUAL(manager.getMaxAllocatedPages(), 10);
  
  BOOST_CHECK(manager.allocatePages(20, addr2));
  BOOST_CHECK_EQUAL(manager.getMaxAllocatedPages(), 30);
  
  // Release some
  manager.releasePages(addr1, 10);
  BOOST_CHECK_EQUAL(manager.getMaxAllocatedPages(), 30);  // Max doesn't decrease
  
  // Allocate more
  BOOST_CHECK(manager.allocatePages(25, addr3));
  BOOST_CHECK_EQUAL(manager.getMaxAllocatedPages(), 45);
}

BOOST_AUTO_TEST_CASE( stress_test_fragmentation )
{
  const uint64_t pageSize = 4096;
  const uint64_t memorySize = 1024 * pageSize;
  PhysMemManager manager(pageSize, memorySize);
  
  std::vector<std::pair<uintptr_t, size_t>> allocations;
  std::mt19937 gen(42);  // Fixed seed for reproducibility
  std::uniform_int_distribution<> sizeDist(1, 10);
  std::uniform_int_distribution<> actionDist(0, 1);
  
  // Perform random allocations and deallocations
  for (int i = 0; i < 1000; i++) {
    if (allocations.empty() || actionDist(gen) == 0) {
      // Try to allocate
      size_t size = sizeDist(gen);
      uintptr_t addr;
      if (manager.allocatePages(size, addr)) {
        allocations.push_back({addr, size});
      }
    } else {
      // Deallocate random allocation
      std::uniform_int_distribution<> indexDist(0, allocations.size() - 1);
      int index = indexDist(gen);
      manager.releasePages(allocations[index].first, allocations[index].second);
      allocations.erase(allocations.begin() + index);
    }
  }
  
  // Clean up remaining allocations
  for (auto& [addr, size] : allocations) {
    manager.releasePages(addr, size);
  }
  
  BOOST_CHECK(manager.allReleased());
}

BOOST_AUTO_TEST_SUITE_END()
