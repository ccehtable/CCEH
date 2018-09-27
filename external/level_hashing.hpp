#ifndef LEVEL_HASHING_H_
#define LEVEL_HASHING_H_

#include <stdint.h>
#include <mutex>
#include <shared_mutex>
#include "src/hash.h"
#include "../util/pair.h"

#define ASSOC_NUM 4

struct Entry {
  // uint8_t token;
  Key_t key;
  Value_t value;
  Entry() {
    key = INVALID;
    value = NONE;
  }
  void* operator new[] (size_t size) {
    void* ret;
    posix_memalign(&ret, 64, size);
    return ret;
  }

  void* operator new (size_t size) {
    void* ret;
    posix_memalign(&ret, 64, size);
    return ret;
  }
};

struct Node {
  Entry slot[ASSOC_NUM];
  void* operator new[] (size_t size) {
    void* ret;
    posix_memalign(&ret, 64, size);
    return ret;
  }

  void* operator new (size_t size) {
    void* ret;
    posix_memalign(&ret, 64, size);
    return ret;
  }
};

class LevelHashing: public Hash {
  private:
    Node *buckets[2];
    uint32_t entry_num;
    uint32_t bucket_num;

    uint32_t levels;
    uint32_t addr_capacity;
    uint32_t total_capacity;
    uint32_t occupied;
    uint64_t f_seed;
    uint64_t s_seed;
    uint32_t resize_num;
    int32_t resizing_lock = 0;
    std::shared_mutex *mutex;
    int nlocks;
    int locksize;

    // uint64_t F_HASH(Key_t& key);
    // uint64_t S_HASH(Key_t& key);
    //
    // inline uint32_t F_IDX(uint64_t hashKey, uint64_t capacity) {
    //   return hashKey % (capacity / 2);
    // }
    // inline uint32_t S_IDX(uint64_t hashKey, uint64_t capacity) {
    //   return (hashKey % (capacity / 2)) + (capacity / 2);
    // }

    void generate_seeds(void);
    void resize(void);

  public:
    LevelHashing(void);
    LevelHashing(size_t);
    ~LevelHashing(void);

    bool InsertOnly(Key_t&, Value_t);
    void Insert(Key_t&, Value_t);
    bool Delete(Key_t&);
    Value_t Get(Key_t&);
    double Utilization(void);
    size_t Capacity(void) {
      return (addr_capacity + addr_capacity/2)*ASSOC_NUM;
    }
};

#endif  // LEVEL_HASHING_H_
