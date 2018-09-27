#include <iostream>
#include <cassert>
#include <iomanip>
#include <vector>
#include <thread>
#include <algorithm>

#include "util/pair.h"
#include "util/timer.h"
#include "util/fileio.h"

#include "external/level_hashing.h"

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
size_t splitCount = 0;
uint64_t kWriteLatencyInNS = 700;

int main(void) {
  const size_t kInitialTableSize = 16 * 1024;
  const size_t kInsertSize = 1024*1024*16;
  const size_t kDataSize = kInsertSize;

  Timer t, t2;
  Key_t* keys = new Key_t[kDataSize];

  for (unsigned i = 0; i < kDataSize; ++i) {
    keys[i] = i+1;
  }

  // for (auto dat = 10000; dat < 10000001; dat *= 10) {
    auto table = level_init(12);
    auto dat = 16*1024*1024;
    cout << "Datasize: " << dat << endl;
    clear_cache();
    {
      size_t elapsed = 0;
      t.Start();
      for (unsigned i = 0; i < dat; ++i) {
        while (level_insert(table, keys[i], reinterpret_cast<Value_t>(&keys[i])) != 0) {
          t2.Start();
          level_resize(table);
          t2.Stop();
      elapsed += t2.Get();
        }
      }
      t.Stop();
      cout << t.GetSeconds() << " seconds to insert " << dat << " number of entries." << endl;
      cout << ((double)elapsed/1000000000.0) << " seconds to rehash" << endl;
      cout << clflushCount << " nubmer of clflush() is called." << endl;
    }

    /*
    clear_cache();
    {
      t.Start();
      auto i = 0;
      while (i < dat) {
#ifdef LEVEL
        auto ret = level_query(table, keys[i]);
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
    */
    delete table;
  // }

  return 0;
}
