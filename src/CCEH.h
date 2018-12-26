#ifndef CCEH_H_
#define CCEH_H_

#include <cstring>
#include <vector>
#include "util/pair.h"
#include "src/hash.h"

const size_t kSegmentBits = 8;
const size_t kMask = pow(2, kSegmentBits)-1;
// const size_t kMask = 256-1;  // (2^8-1)
const size_t kShift = kSegmentBits;
const size_t kSegmentSize = pow(2, kSegmentBits) * 16 * 4;

struct Segment {
  // static const size_t kSegmentSize = 256; // 4 - 1
  // static const size_t kSegmentSize = 1024; // 16 - 1
  // static const size_t kSegmentSize = 4*1024; // 64 - 1
  // static const size_t kSegmentSize = 16*1024; // 256 - 1
  // static const size_t kSegmentSize = 64*1024; // 1024 - 1
  // static const size_t kSegmentSize = 256*1024; // 4096 - 1
  static const size_t kNumSlot = kSegmentSize/sizeof(Pair);

  Segment(void)
  : local_depth{0}
  { }

  Segment(size_t depth)
  :local_depth{depth}
  { }

  ~Segment(void) {
  }

  void* operator new(size_t size) {
    void* ret;
    posix_memalign(&ret, 64, size);
    return ret;
  }

  void* operator new[](size_t size) {
    void* ret;
    posix_memalign(&ret, 64, size);
    return ret;
  }

  int Insert(Key_t&, Value_t, size_t, size_t);
  void Insert4split(Key_t&, Value_t, size_t);
  bool Put(Key_t&, Value_t, size_t);
  Segment** Split(void);

  Pair _[kNumSlot];
  size_t local_depth;
  int64_t sema = 0;
  size_t pattern = 0;
  size_t numElem(void); 
};

struct Directory {
  static const size_t kDefaultDirectorySize = 1024;
  Segment** _;
  size_t capacity;
  bool lock;
  int sema = 0 ;

  Directory(void) {
    capacity = kDefaultDirectorySize;
    _ = new Segment*[capacity];
    lock = false;
    sema = 0;
  }

  Directory(size_t size) {
    capacity = size;
    _ = new Segment*[capacity];
    lock = false;
    sema = 0;
  }

  ~Directory(void) {
    delete [] _;
  }

  bool Acquire(void) {
    bool unlocked = false;
    return CAS(&lock, &unlocked, true);
  }

  bool Release(void) {
    bool locked = true;
    return CAS(&lock, &locked, false);
  }
  
  void SanityCheck(void*);
  void LSBUpdate(int, int, int, int, Segment**);
};

class CCEH : public Hash {
  public:
    CCEH(void);
    CCEH(size_t);
    ~CCEH(void);
    void Insert(Key_t&, Value_t);
    bool InsertOnly(Key_t&, Value_t);
    bool Delete(Key_t&);
    Value_t Get(Key_t&);
    Value_t FindAnyway(Key_t&);
    double Utilization(void);
    size_t Capacity(void);
    bool Recovery(void);

    void* operator new(size_t size) {
      void *ret;
      posix_memalign(&ret, 64, size);
      return ret;
    }

  private:
    size_t global_depth;
    Directory dir;
};

#endif  // EXTENDIBLE_PTR_H_
