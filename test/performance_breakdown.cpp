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
uint64_t kWriteLatencyInNS = 700;
size_t somecount = 0;

int main(void) {
  const size_t kInitialTableSize = 16 * 1024;
  const size_t kInsertSize = 1024*1024*16;
  const size_t kDataSize = kInsertSize;

  Timer t;
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


  Key_t* keys = new Key_t[kDataSize];

  for (unsigned i = 0; i < kDataSize; ++i) {
    keys[i] = i+1;
  }

  clear_cache();
  {
    t.Start();
    for (unsigned i = 0; i < kInsertSize; ++i) {
      table->Insert(keys[i], reinterpret_cast<Value_t>(&keys[i]));
    }
    t.Stop();
    cout << t.GetSeconds() << " seconds to insert " << kInsertSize << " number of entries." << endl;
    cout << clflushCount << " nubmer of clflush() is called." << endl;
    cout << table->Utilization() << "% of load factor is initialized." << endl;
    cout << table->breakdown << " seconds to resize the table." << endl;
  }

  /*
  clear_cache();
  {
    t.Start();
    auto i = 0;
    while (i < kInsertSize) {
      auto ret = table->Get(keys[i]);
      if (ret == NONE) {
        cout << "SOMETHING WRONG" << endl;
      }
      ++i;
    }
    t.Stop();
    cout << t.GetSeconds() << " to retrive " << i << " entries." << endl;
  }
  */

  return 0;
}
