#include <iostream>
#include <cassert>
#include <iomanip>
#include <vector>
#include <thread>
#include <algorithm>

#include "util/pair.h"
#include "util/timer.h"
#include "util/fileio.h"

#include "src/CCEH.h"

#define PERF

#ifdef PERF
#include "cpucounters.h"
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
int64_t kWriteLatencyInNS = 0;
size_t somecount = 0;

int main(void) {
#ifdef PERF
  PCM *m = PCM::getInstance();
  PCM::ErrorCode returnResult = m->program();
  if (returnResult != PCM::Success) {
    cout << "Intel's PCM couldn't start." << endl;
    cout << "Error Code: " << returnResult << endl;
    exit(1);
  }
#endif
  const size_t kInitialTableSize = 64 * 1024;
  const size_t kInsertSize = 1024*1024*16;
  const size_t kDataSize = kInsertSize;

  Timer t;

  Key_t* keys = new Key_t[kDataSize];

  for (unsigned i = 0; i < kDataSize; ++i) {
    keys[i] = i+1;
  }

  Hash* table = new CCEH(kInitialTableSize/Segment::kNumSlot);

  clear_cache();
  {
#ifdef PERF
      SystemCounterState before_sstate = getSystemCounterState();
#endif
    t.Start();
    for (int i = kInsertSize-1; i >= 0; --i) {
      table->Insert(keys[i], reinterpret_cast<Value_t>(&keys[i]));
    }
    t.Stop();
#ifdef PERF
      SystemCounterState after_sstate = getSystemCounterState();
      cout << "Instructions per clock:" << getIPC(before_sstate,after_sstate) << endl;
      cout << "Bytes read:" << getBytesReadFromMC(before_sstate,after_sstate) << endl;
      cout << "L2cache misses:" << getL2CacheMisses(before_sstate,after_sstate) << endl;
      cout << "L2cahce miss ratio:" << getL2CacheHitRatio(before_sstate,after_sstate) << endl;
      cout << "L3cache misses:" << getL3CacheMisses(before_sstate,after_sstate) << endl;
      cout << "L3cahce miss ratio:" << getL3CacheHitRatio(before_sstate,after_sstate) << endl;
#endif
    cout << t.GetSeconds() << " to insert " << kInsertSize << " number of entries." << endl;
    cout << clflushCount << " nubmer of clflush() is called." << endl;
    cout << table->breakdown << endl;
    cout << table->Utilization() << "% of load factor is initialized." << endl;
  }

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
#ifdef PERF
  m->cleanup();
#endif

  return 0;
}
