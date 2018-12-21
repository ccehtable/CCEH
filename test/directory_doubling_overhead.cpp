#include <iostream>
#include <cassert>
#include <cstring>
#include "util/timer.h"
#include "util/persist.h"

using namespace std;

void* aligned_malloc(size_t size) {
  void* ret;
  posix_memalign(&ret, 64, size);
  return ret;
}

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

uint64_t kWriteLatencyInNS = 0;
uint64_t clflushCount = 0;

int main (void) {
  const size_t dir_size = 65536;
  void* lsb_ori[dir_size/2];
  void* msb_ori[dir_size/2];
  void* lsb_copy[dir_size];
  void* msb_copy[dir_size];
  Timer t;

  clear_cache();
  t.Start();
  for (int i = 0; i < dir_size/2; ++i) {
    msb_copy[2*i] = msb_ori[i];
    msb_copy[2*i+1] = msb_ori[i];
  }
  t.Stop();
  cout << t.Get() << " nsec for msb copy" << endl;
  clear_cache();
  t.Start();
  // for (int i = 0; i < dir_size/2; ++i) {
  //   lsb_copy[i] = lsb_ori[i];
  //   lsb_copy[i+dir_size/2] = lsb_ori[i];
  // }
  memcpy(&lsb_copy[0], &lsb_ori[0], sizeof(void*)*dir_size/2);
  memcpy(&lsb_copy[dir_size/2], &lsb_ori[0], sizeof(void*)*dir_size/2);
  t.Stop();
  cout << t.Get() << " nsec for lsb copy" << endl;

  clear_cache();
  t.Start();
  clflush((char*)&lsb_copy[0], sizeof(void*)*dir_size);
  t.Stop();
  cout << t.Get() << " nsec for clflush()" << endl;

  return 0;
}
