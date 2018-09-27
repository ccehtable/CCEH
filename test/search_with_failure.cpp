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
  const size_t kInitialTableSize = 16*1024*1024;
  const size_t kPreloadedAmount = kInitialTableSize*0.9;
  const size_t kDataSize = kPreloadedAmount*40;

  Timer t;

  Key_t* keys = new Key_t[kDataSize];
  vector<Key_t*> loaded;
  loaded.reserve(kPreloadedAmount);

  for (unsigned i = 0; i < kDataSize; ++i) {
    keys[i] = i+1;
  }

#ifdef LIN
  Hash* table = new LinearHash(kInitialTableSize);
#elif CUC
  Hash* table = new CuckooHash(kInitialTableSize);
#elif EP
  Hash* table = new CCEH(1024*16);
#elif PATH
  Hash* table = new PathHashing(24, 24);
#elif LEVEL
  Hash* table = new LevelHashing(22);
#elif BCH
  Hash* table = new BucketizedCuckooHashing(22);
#endif

  size_t successCount = 0;
  size_t failureCount = 0;
  size_t totalCount = 0;
  size_t i = 0;

  size_t amount = table->Capacity() * 0.9;
  cout << amount << " " << kPreloadedAmount<< endl;
  while (successCount < amount) {
    if (table->InsertOnly(keys[i], reinterpret_cast<Value_t>(&keys[i]))) {
      loaded.push_back(&keys[i]);
      ++successCount;
    } else {
      ++failureCount;
    }
    ++i;
    if (i > kDataSize-5) return 1;
  }
  cout << table->Utilization() << "% of load factor is initialized." << endl;

  totalCount = successCount + failureCount;
  successCount = 0;
  failureCount = 0;

  random_shuffle(loaded.begin(), loaded.end());

  clear_cache();
  
  auto negSucc = 0;
  auto negFail = 0;
  t.Start();
  {
    size_t i = 0;
    while (i < kPreloadedAmount*0.7) {
      if (i % 100 < 10) {
        failureCount ++;
        if (table->Get(keys[totalCount+i]) != NONE) {
          negFail++;
        }
      } else {
        successCount ++;
        if (table->Get(*loaded[i]) == NONE) {
          negSucc++;
        }
      }
      ++i;
    }
  }
  t.Stop();
  auto totalSearch = successCount + failureCount;
  cout << t.GetSeconds() << " to retrive " << totalSearch << " elements with 10% of invalid keys." << endl;
  if (negSucc + negFail != 0) {
    cout << "SOMETHING WRONG..." << endl;
    cout << negSucc << " " << negFail << endl;
  }

  clear_cache();

  successCount = 0;
  failureCount = 0;
  negSucc = 0;
  negFail = 0;
  t.Start();
  {
    size_t i = 0;
    while (i < kPreloadedAmount*0.7) {
      if (i % 100 < 30) {
        failureCount ++;
        if (table->Get(keys[totalCount+i]) != NONE) {
          negFail++;
        }
      } else {
        successCount ++;
        if (table->Get(*loaded[i]) == NONE) {
          negSucc++;
        }
      }
      ++i;
    }
  }
  t.Stop();
  totalSearch = successCount + failureCount;
  cout << t.GetSeconds() << " to retrive " << totalSearch << " elements with 30% of invalid keys." << endl;
  if (negSucc + negFail != 0) {
    cout << "SOMETHING WRONG..." << endl;
    cout << negSucc << " " << negFail << endl;
  }

  clear_cache();

  successCount = 0;
  failureCount = 0;
  negSucc = 0;
  negFail = 0;
  t.Start();
  {
    size_t i = 0;
    while (i < kPreloadedAmount*0.7) {
      if (i % 100 < 50) {
        failureCount ++;
        if (table->Get(keys[totalCount+i]) != NONE) {
          negFail++;
        }
      } else {
        successCount ++;
        if (table->Get(*loaded[i]) == NONE) {
          negSucc++;
        }
      }
      ++i;
    }
  }
  t.Stop();
  totalSearch = successCount + failureCount;
  cout << t.GetSeconds() << " to retrive " << totalSearch << " elements with 50% of invalid keys." << endl;
  if (negSucc + negFail != 0) {
    cout << "SOMETHING WRONG..." << endl;
    cout << negSucc << " " << negFail << endl;
  }


  return 0;
}
