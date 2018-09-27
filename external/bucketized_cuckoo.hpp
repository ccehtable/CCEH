#ifndef BUCKETIZED_CUCKOO_HASH_H_
#define BUCKETIZED_CUCKOO_HASH_H_

#include <stdint.h>
#include <mutex>
#include <shared_mutex>
#include "../util/pair.h"
#include "../src/hash.h"

#define ASSOC_NUM 4
#define CUCKOO_MAX_DEP 500

struct Entry {
  // uint8_t token;
  Key_t key;
  Value_t value;

  Entry(void) {
    // token = 0;
    key = INVALID;
    value = NONE;
  }

  void *operator new[] (size_t size) {
    void* ret;
    posix_memalign(&ret, 64, size);
    return ret;
  }

  void *operator new (size_t size) {
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

class BucketizedCuckooHashing: public Hash {
  private:
    Node *buckets;
    uint32_t entry_num;
    uint32_t bucket_num;
    
    uint32_t levels;  // input: the initial capacity is 2^levels
    uint32_t addr_capacity;
    uint32_t total_capacity;
    uint32_t occupied;
    uint32_t cuckoo_loop;  // the maximum number of evictions for an insertion
    uint64_t rehash_num;
    uint64_t movements;
    uint64_t f_seed;
    uint64_t s_seed;
    
    // FILE *log_file; // not used
    uint64_t rand_location;  // rand() for every insertion?
    std::shared_mutex *mutex;
    int nlocks;
    int locksize;

    void generate_seeds(void);
    int rehash(uint32_t, uint32_t);
    bool resize(void);

    uint32_t F_IDX_Re(Key_t&, uint32_t);
    uint32_t S_IDX_Re(Key_t&, uint32_t);
    int resizing_lock = 0;

  public:
    BucketizedCuckooHashing(void);
    BucketizedCuckooHashing(uint32_t);
    ~BucketizedCuckooHashing(void);

    void Insert(Key_t&, Value_t);
    bool InsertOnly(Key_t&, Value_t);
    Value_t Get(Key_t&);
    bool Delete(Key_t&);
    double Utilization(void);
    size_t Capacity(void) {
      return entry_num;
    }

    Value_t FindAnyway(Key_t&);
};



#endif  // BUCKETIZED_CUCKOO_HASH_H_
