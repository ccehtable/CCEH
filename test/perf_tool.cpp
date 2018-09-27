#include <iostream>
#include <cassert>
#include <iomanip>
#include <vector>
#include <thread>
#include <algorithm>

#include "util/pair.h"
#include "util/timer.h"
#include "util/fileio.h"
#ifdef PCM
#include "cpucounters.h"
#endif

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

uint32_t clflushCount = 0;
size_t somecount = 0;
uint64_t kWriteLatencyInNS = 0;

int main(void) {
#ifdef PCM
  PCM *m = PCM::getInstance();
  PCM::ErrorCode returnResult = m->program();
  if (returnResult != PCM::Success) {
    cout << "Intel's PCM couldn't start." << endl;
    cout << "Error Code: " << returnResult << endl;
    exit(1);
  }
#endif

  const size_t kInitialTableSize = 16 * 1024;
  const size_t kInsertSize = 1024*1024*16;
  const size_t kDataSize = kInsertSize;

  Timer t;
  Key_t* keys = new Key_t[kDataSize];

  for (unsigned i = 0; i < kDataSize; ++i) {
    keys[i] = i+1;
  }

  for (auto iter = 0; iter < 1; ++iter) {
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

    clear_cache();
    {
#ifdef PCM
      SystemCounterState before_sstate = getSystemCounterState();
#else
      cout << "> " << endl;
      getchar();
#endif
      t.Start();
      for (unsigned i = 0; i < kInsertSize; ++i) {
        table->Insert(keys[i], reinterpret_cast<Value_t>(&keys[i]));
      }
      t.Stop();
#ifdef PCM
      SystemCounterState after_sstate = getSystemCounterState();
      cout << "Instructions per clock:" << getIPC(before_sstate,after_sstate) << endl;
      cout << "Bytes read:" << getBytesReadFromMC(before_sstate,after_sstate) << endl;
      cout << "L2cache misses:" << getL2CacheMisses(before_sstate,after_sstate) << endl;
      cout << "L3cache misses:" << getL3CacheMisses(before_sstate,after_sstate) << endl;
#else
      cout << "< " << endl;
      getchar();
#endif
      cout << t.GetSeconds() << " seconds to insert " << kInsertSize << " number of entries." << endl;
    }

    clear_cache();
    {
#ifdef PCM
      SystemCounterState before_sstate = getSystemCounterState();
#else
      cout << ">" << endl;
      getchar();
#endif
      t.Start();
      auto i = 0;
      while (i < kInsertSize) {
        auto ret = table->Get(keys[i]);
        if (ret == NONE) {
          asm("nop");
          // cout << "SOMETHING WRONG" << endl;
        }
        ++i;
      }
      t.Stop();
#ifdef PCM
      SystemCounterState after_sstate = getSystemCounterState();
      cout << "Instructions per clock:" << getIPC(before_sstate,after_sstate) << endl;
      cout << "Bytes read:" << getBytesReadFromMC(before_sstate,after_sstate) << endl;
      cout << "L2cache misses:" << getL2CacheMisses(before_sstate,after_sstate) << endl;
      cout << "L3cache misses:" << getL3CacheMisses(before_sstate,after_sstate) << endl;
#else
      cout << "<" << endl;
      getchar();
#endif
      cout << t.GetSeconds() << " to retrive " << i << " entries." << endl;
    }
    delete table;
  }
#ifdef PCM
  m->cleanup();
#endif

  return 0;
}
