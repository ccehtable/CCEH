#include <iostream>
#include <cassert>
#include <iomanip>
#include <vector>
#include <thread>
#include <algorithm>

#include "util/pair.h"
#include "util/timer.h"
#include "util/fileio.h"

#ifdef LEVEL
#include "external/level_hashing.h"
#elif NLEVEL
#include "external/new_level_hashing.h"
#elif PATH
#include "external/path_hashing.h"
#elif BCH
#include "external/bucketized_cuckoo.h"
#endif

using namespace std;

#define DEBUG

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

uint64_t clflushCount = 0;
size_t splitCount = 0;
uint64_t kWriteLatencyInNS = 0;

int main(void) {
  const size_t kInitialTableSize = 2 * 1024;
  const size_t kInsertSize = 1024*1024*16;
  const size_t kDataSize = kInsertSize;

  Timer t;
  Key_t* keys = new Key_t[kDataSize];

  for (unsigned i = 0; i < kDataSize; ++i) {
    keys[i] = i+1;
  }

  // for (auto dat = 10000; dat < 10000001; dat *= 10) {
#ifdef PATH
    auto table = path_init(11, 8);
#elif LEVEL
    auto table = level_init(12);
#elif NLEVEL
    auto table = level_init(12);
#elif BCH
    auto table = cuckoo_init(9);
    table->cuckoo_loop = 16;
    // Hash* table = new BucketizedCuckooHashing(9);
#endif
    auto dat = 16*1024*1024;
    cout << "Datasize: " << dat << endl;
    clear_cache();
    {
      size_t elapsed = 0;
      // t.Start();
      for (unsigned i = 0; i < dat; ++i) {
#ifdef LEVEL
        while (level_insert(table, keys[i], reinterpret_cast<Value_t>(&keys[i])) != 0) {
          t.Start();
          level_resize(table);
          t.Stop();
      elapsed += t.Get();
        }
#elif NLEVEL
        while (level_insert(table, keys[i], reinterpret_cast<Value_t>(&keys[i])) != 0) {
          t.Start();
          level_resize(table);
          t.Stop();
      elapsed += t.Get();
        }
#elif PATH
        path_insert(table, keys[i], reinterpret_cast<Value_t>(&keys[i]));
#elif BCH
        while (cuckoo_insert(table, keys[i], reinterpret_cast<Value_t>(&keys[i])) != 0) {
          cuckoo_resize(table);
        }
#endif
      }
      // t.Stop();
      cout << ((double)elapsed/1000000000.0) << " seconds to insert " << dat << " number of entries." << endl;
      // cout << t.GetSeconds() << " seconds to insert " << dat << " number of entries." << endl;
      cout << clflushCount << " nubmer of clflush() is called." << endl;
    }

    clear_cache();
    {
      t.Start();
      auto i = 0;
      while (i < dat) {
#ifdef LEVEL
        auto ret = level_query(table, keys[i]);
#elif NLEVEL
        auto ret = level_static_query(table, keys[i]);
#elif PATH
        auto ret = path_query(table, keys[i]);
#elif BCH
        auto ret = cuckoo_query(table, keys[i]);
#endif
        if (ret == NONE) {
          cout << "SOMETHING WRONG" << endl;
        }
        ++i;
      }
      t.Stop();
      cout << t.GetSeconds() << " to retrive " << i << " entries." << endl;
    }
    delete table;
  // }

  return 0;
}
