#include <iostream>
#include <cassert>
#include <iomanip>
#include <vector>
#include <thread>
#include <algorithm>

#include "util/pair.h"
#include "util/timer.h"
#include "util/fileio.h"

#ifdef LIN
#include "src/linear_hash.h"
#elif CUC
#include "src/cuckoo_hash.h"
#elif EP
#include "src/CCEH.h"
#elif PATH
#include "external/path_hashing.hpp"
#elif LEVEL
#include "external/level_hashing.hpp"
#elif BCH
#include "external/bucketized_cuckoo.hpp"
#endif

using namespace std;

// #define DEBUG

void clear_cache() {
#ifndef DEBUG
  const size_t size = 1024*1024*512;
  int* dummy = new int[size];
  for (int i=100; i<size; i++) {
    dummy[i] = i;
  }

  for (int i=100;i<size-(1024*1024);i++) {
    dummy[i] = dummy[i-rand()%100] + dummy[i+rand()%100];
  }

  delete[] dummy;
#endif
}

uint32_t clflushCount = 0;
uint64_t kWriteLatencyInNS = 0;
size_t somecount = 0;

int main(void) {
  const size_t kInitialTableSize = 16*1024;
  const size_t kPreloadedAmount = kInitialTableSize*0.5;
  const size_t kNumThreads = 1;
  const size_t kInsertSize = 1024*1024*16;
  const size_t kDataSize = kInsertSize*20;

  Timer t;

  Key_t* keys = new Key_t[kDataSize];
  vector<Key_t*> loaded;
  loaded.reserve(kPreloadedAmount);

  vector<size_t> threadReturns(kNumThreads);

  for (unsigned i = 0; i < kDataSize; ++i) {
    keys[i] = i+1;
  }

#ifdef LIN
  Hash* table = new LinearHash(kInitialTableSize);
#elif CUC
  Hash* table = new CuckooHash(kInitialTableSize);
#elif EP
  Hash* table = new CCEH(kInitialTableSize/Segment::kNumSlot);
#elif PATH
  Hash* table = new PathHashing(14, 8);
#elif LEVEL
  Hash* table = new LevelHashing(12);
#elif BCH
  Hash* table = new BucketizedCuckooHashing(12);
#endif

  size_t successCount = 0;
  size_t failureCount = 0;
  size_t totalCount = 0;
  size_t i = 0;

  /*
  while (successCount < kPreloadedAmount) {
    if (table->InsertOnly(keys[i], reinterpret_cast<Value_t>(&keys[i]))) {
      loaded.push_back(&keys[i]);
      ++successCount;
    } else {
      ++failureCount;
    }
    ++i;
  }
  cout << table->Utilization() << "% of load factor is initialized." << endl;

  totalCount = successCount + failureCount;
  successCount = 0;
  failureCount = 0;

  clear_cache();

  random_shuffle(loaded.begin(), loaded.end());
  
  {
    // Search Evaluation - Multi-threaded
    auto search = [&table, &loaded, &threadReturns](unsigned from, unsigned to, unsigned id) {
      size_t success = 0;
      for (unsigned i = from; i < to; ++i) {
        auto v = table->Get(*loaded[i]);
        if (v != NONE) {
          success++;
        }
      }
      threadReturns[id] = success;
    };
    vector<thread> searchingThreads;

    const size_t kChunkSize = kPreloadedAmount/kNumThreads;

    t.Start();
    for (unsigned t = 0; t < kNumThreads-1; ++t) {
      searchingThreads.emplace_back
        (thread(search, kChunkSize*t, kChunkSize*(t+1), t));
    }
    searchingThreads.emplace_back
      (thread(search, kChunkSize*(kNumThreads-1), kPreloadedAmount, kNumThreads-1));

    for (auto& t: searchingThreads) t.join();
    t.Stop();
    cout << t.GetSeconds() << " seconds to retrieve pre-loaded entries." << endl;

    size_t sum = 0;
    for (auto& v: threadReturns) sum += v;
    cout << sum << " number of successful search out of " << kPreloadedAmount << "." << endl;
  }

  */
  clear_cache();
  clflushCount = 0;

  {
    // Insert Evaluation - Multi-threaded
    auto insert = [&table, &keys, &threadReturns](unsigned from, unsigned to, unsigned id)  {
      for (unsigned i = from; i < to; ++i) {
        table->Insert(keys[i], reinterpret_cast<const char*>(&keys[i]));
      }
    };
    vector<thread> insertingThreads;
    const size_t kChunkSize = kInsertSize/kNumThreads;

    t.Start();
    for (unsigned t = 0; t < kNumThreads-1; ++t) {
      insertingThreads.emplace_back(
          thread(insert, totalCount+kChunkSize*t, totalCount+kChunkSize*(t+1), t));
    }
    insertingThreads.emplace_back(
        thread(insert, totalCount+kChunkSize*(kNumThreads-1), totalCount+kInsertSize, kNumThreads-1));

    for (auto& t: insertingThreads) t.join();
    t.Stop();

    cout << t.GetSeconds() << " seconds to insert new addtional entries." << endl;
  }

  /*
#ifdef DEBUG
  for (unsigned i = 0; i < kInsertSize; i++) {
    auto v = table->Get(keys[i+totalCount]);
    if (v == NONE) {
      ++failureCount;
#ifdef BCH
      if (((BucketizedCuckooHashing*)table)->FindAnyway(keys[i+totalCount]) != NONE) {
        cout << "IT IS THERE." << endl;
      } else {
        cout << "NO...." << endl;
        return 1;
      }
#endif
    } else {
      ++successCount;
    }
  }
#endif
*/

  clear_cache();
  t.Start();
  {
    // Search Evaluation - Multi-threaded
    auto search = [&table, &keys, &threadReturns](unsigned from, unsigned to, unsigned id) {
      size_t success = 0;
      for (unsigned i = from; i < to; ++i) {
        auto v = table->Get(keys[i]);
        if (v != NONE) {
          success++;
        }
        // } else {
        //   cout << "A" << endl;
        // }
      }
      threadReturns[id] = success;
    };
    vector<thread> searchingThreads;

    const size_t kChunkSize = kInsertSize/kNumThreads;

    for (unsigned t = 0; t < kNumThreads-1; ++t) {
      searchingThreads.emplace_back
        (thread(search, totalCount+kChunkSize*t, totalCount+kChunkSize*(t+1), t));
    }
    searchingThreads.emplace_back
      (thread(search, totalCount+kChunkSize*(kNumThreads-1), totalCount+kInsertSize, kNumThreads-1));

    for (auto& t: searchingThreads) t.join();
    t.Stop();
    cout << t.GetSeconds() << " seconds to retrieve additional entries." << endl;

    size_t sum = 0;
    for (auto& v: threadReturns) sum += v;
    cout << sum << " number of successful search out of " << kInsertSize << "." << endl;
  }

  cout << clflushCount << " number of clflush is called." << endl;
  cout << table->Utilization() << "% of space is used" << endl;
  cout << table->breakdown << endl;
  cout << somecount << endl;

  return 0;
}
