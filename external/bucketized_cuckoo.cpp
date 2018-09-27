#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "bucketized_cuckoo.hpp"
#include "../util/hash.h"
#include "../util/persist.h"

#define FIRST_HASH(hash, capacity) (hash % (capacity / 2))
#define SECOND_HASH(hash, capacity) ((hash % (capacity / 2)) + (capacity / 2))
#define F_IDX() FIRST_HASH(                       \
    h((void *)&key, sizeof(Key_t), f_seed), \
    addr_capacity)
#define S_IDX() SECOND_HASH(                       \
    h((void *)&key, sizeof(Key_t), s_seed), \
    addr_capacity)


using namespace std;


uint32_t BucketizedCuckooHashing::F_IDX_Re(Key_t& key, uint32_t capacity) {
  return (h((void *)&key, sizeof(Key_t), f_seed)) % (capacity / 2);
}
uint32_t BucketizedCuckooHashing::S_IDX_Re(Key_t& key, uint32_t capacity) {
  return (h((void *)&key, sizeof(Key_t), s_seed)) % (capacity / 2) + (capacity / 2);
}


void BucketizedCuckooHashing::generate_seeds(void) {
  srand(time(NULL));

  do
  {
    f_seed = rand();
    s_seed = rand();
    f_seed = f_seed << (rand() % 63);
    s_seed = s_seed << (rand() % 63);
  } while (f_seed == s_seed);
}

BucketizedCuckooHashing::BucketizedCuckooHashing(void) {
}

BucketizedCuckooHashing::BucketizedCuckooHashing(uint32_t _levels) 
{
  levels = _levels;
  addr_capacity = pow(2, levels) + pow(2, levels-1);
  total_capacity = pow(2, levels) + pow(2, levels-1);
  occupied = 0;
  rehash_num = 0;
  movements = 0;
  buckets = new Node[total_capacity];
  bucket_num = total_capacity;
  entry_num = bucket_num*ASSOC_NUM;
  cuckoo_loop = 16;

  locksize = 256;
  nlocks = (addr_capacity)/locksize+1;
  mutex = new shared_mutex[nlocks];

  generate_seeds();
}

BucketizedCuckooHashing::~BucketizedCuckooHashing(void) {
  delete[] buckets;
}

void BucketizedCuckooHashing::Insert(Key_t& key, Value_t value)
{
RETRY:
  while (resizing_lock == 1) {
    asm("nop");
  }
  uint32_t f_idx = F_IDX();
  uint32_t s_idx = S_IDX();
  int j;

  for (;;)
  {
    auto lock1 = INVALID;
    for(j = 0; j < ASSOC_NUM; j ++){
      {
        unique_lock<shared_mutex> lock(mutex[(f_idx)/locksize]);
        if (CAS(&buckets[f_idx].slot[j].key, &lock1, SENTINEL))
        {
          buckets[f_idx].slot[j].value = value;
          mfence();
          buckets[f_idx].slot[j].key = key;
          clflush((char*)&buckets[f_idx].slot[j], sizeof(Entry));
          auto oc = occupied;
          while (!CAS(&occupied, &oc, oc+1)) {
            oc = occupied;
          }
          return;
        }
      }
    }
    for(j = 0; j < ASSOC_NUM; j ++){
      {
        unique_lock<shared_mutex> lock(mutex[(s_idx)/locksize]);
        if (CAS(&buckets[s_idx].slot[j].key, &lock1, SENTINEL))
        {
          buckets[s_idx].slot[j].value = value;
          mfence();
          buckets[s_idx].slot[j].key = key;
          clflush((char*)&buckets[s_idx].slot[j], sizeof(Entry));
          auto oc = occupied;
          while (!CAS(&occupied, &oc, oc+1)) {
            oc = occupied;
          }
          return;
        }
      }
    }

    timer.Start();
    auto ret = rehash(f_idx, cuckoo_loop);
    timer.Stop();
    breakdown += timer.GetSeconds();
    if (ret == 2) break;
  }
  int res_lock = 0;
  if (CAS(&resizing_lock, &res_lock, 1)) {
    resize();
    resizing_lock = 0;
  }
  goto RETRY;
}

bool BucketizedCuckooHashing::InsertOnly(Key_t& key, Value_t value)
{
  uint32_t f_idx = F_IDX();
  uint32_t s_idx = S_IDX();
  int j;

  for (;;)
  {
    for(j = 0; j < ASSOC_NUM; j ++){
      unique_lock<shared_mutex> lock(mutex[(f_idx)/locksize]);
      if (buckets[f_idx].slot[j].key == INVALID)
      {
        buckets[f_idx].slot[j].value = value;
        mfence();
        buckets[f_idx].slot[j].key = key;
        clflush((char*)&buckets[f_idx].slot[j], sizeof(Entry));
        auto _occ = occupied;
        while (!CAS(&occupied, &_occ, _occ+1)) {
          _occ = occupied;
        }
        return true;
      }
    }
    for(j = 0; j < ASSOC_NUM; j ++){
      unique_lock<shared_mutex> lock(mutex[(s_idx)/locksize]);
      if (buckets[s_idx].slot[j].key == INVALID)
      {
        buckets[s_idx].slot[j].value = value;
        mfence();
        buckets[s_idx].slot[j].key = key;
        clflush((char*)&buckets[s_idx].slot[j], sizeof(Entry));
        auto _occ = occupied;
        while (!CAS(&occupied, &_occ, _occ+1)) {
          _occ = occupied;
        }
        return true;
      }
    }
    return false;
  }
}

Value_t BucketizedCuckooHashing::Get(Key_t& key)
{
  uint32_t f_idx = F_IDX();
  uint32_t s_idx = S_IDX();
  int j;

  for(j = 0; j < ASSOC_NUM; j ++){
    {
      shared_lock<shared_mutex> lock(mutex[f_idx/locksize]);
      if (buckets[f_idx].slot[j].key == key) {
        return buckets[f_idx].slot[j].value;
      }
    }
    {
      shared_lock<shared_mutex> lock(mutex[s_idx/locksize]);
      if (buckets[s_idx].slot[j].key == key) {
        return buckets[s_idx].slot[j].value;
      }
    }
  }
  return NULL;

  }

  bool BucketizedCuckooHashing::Delete(Key_t& key) {
    return false;
  }

  bool BucketizedCuckooHashing::resize(void) {
    uint32_t newCapacity = pow(2, levels + 1) + pow(2, levels);
    uint32_t oldCapacity = pow(2, levels) + pow(2, levels - 1);

    unique_lock<shared_mutex> *lock[nlocks];
    for(int i=0; i<nlocks; i++){
      lock[i] = new std::unique_lock<std::shared_mutex>(mutex[i]);
    }
    shared_mutex* old_mutex = mutex;
    mutex = new shared_mutex[newCapacity/locksize+1];
    int prev_nlocks = nlocks;
    nlocks = newCapacity/locksize+1;

    addr_capacity = newCapacity;
    Node* newBucket = new Node[addr_capacity];
    Node* oldBucket = buckets;
    buckets = newBucket;
    levels++;
    total_capacity = newCapacity;
    bucket_num = total_capacity;
    entry_num = bucket_num*ASSOC_NUM;
    generate_seeds();

    if (!newBucket) {
      printf("The resize fails when callocing newBucket!\n");
      exit(1);
    }

    uint32_t old_idx;
    for (old_idx = 0; old_idx < oldCapacity; old_idx ++) {

      int i, j;
      for(i = 0; i < ASSOC_NUM; i ++){
        if (oldBucket[old_idx].slot[i].key != INVALID)
        {
          Key_t key = oldBucket[old_idx].slot[i].key;
          Value_t value = oldBucket[old_idx].slot[i].value;

          uint32_t f_idx = F_IDX_Re(key, addr_capacity);
          uint32_t s_idx = S_IDX_Re(key, addr_capacity);
          int insertSuccess = 0;

          for(;;){
            for(j = 0; j < ASSOC_NUM; j ++){
              if (buckets[f_idx].slot[j].key == INVALID)
              {
                buckets[f_idx].slot[j].value = value;
                mfence();
                buckets[f_idx].slot[j].key = key;
                clflush((char*)&buckets[f_idx].slot[j], sizeof(Entry));
                insertSuccess = 1;
                break;
              }
              else if (buckets[s_idx].slot[j].key == INVALID)
              {
                buckets[s_idx].slot[j].value = value;
                mfence();
                buckets[s_idx].slot[j].key = key;
                clflush((char*)&buckets[s_idx].slot[j], sizeof(Entry));
                insertSuccess = 1;
                break;
              }
            }
            if (insertSuccess)
              break;
            if(!rehash(f_idx, cuckoo_loop) == 2){
              printf("Rehash fails when resizing!\n");
              exit(1);
            }
          }
        }
      }
    }

    delete [] oldBucket;
    return true;
  }

int BucketizedCuckooHashing::rehash(uint32_t idx, uint32_t loop)
{
  rehash_num ++;
  auto rand_location = rand();
  Key_t key = buckets[idx].slot[rand_location%ASSOC_NUM].key;
  uint32_t f_idx = F_IDX();
  uint32_t s_idx = S_IDX();
  uint32_t n_idx = s_idx;
  Entry *log_node = new Entry;

  if (idx == s_idx)
  {
    n_idx = f_idx;
  }

  int j;
  for(j = 0; j < ASSOC_NUM; j ++){
    auto lock = INVALID;
    if (CAS(&buckets[n_idx].slot[j].key, &lock, SENTINEL)) {
      movements ++;
      log_node->key = buckets[idx].slot[rand_location%ASSOC_NUM].key;
      log_node->value = buckets[idx].slot[rand_location%ASSOC_NUM].value;
      clflush((char*)&log_node, sizeof(Entry));
      mfence();
      delete log_node;

      buckets[n_idx].slot[j].value = buckets[idx].slot[rand_location%ASSOC_NUM].value;
      mfence();
      buckets[n_idx].slot[j].key = buckets[idx].slot[rand_location%ASSOC_NUM].key;
      clflush((char*)&buckets[n_idx].slot[j], sizeof(Entry));
      buckets[idx].slot[rand_location%ASSOC_NUM].key = INVALID;
      clflush((char*)&buckets[idx].slot[rand_location%ASSOC_NUM].key, sizeof(Key_t));
      return 0;
    }
  }

  if (loop == 0)
  {
    return 2;
  }

  else if (rehash(n_idx, loop - 1) == 1)
  {
    return 1;
  }
}

double BucketizedCuckooHashing::Utilization(void) {
  size_t sum = 0;
  for (size_t i = 0; i < total_capacity; ++i) {
    for (unsigned j = 0; j < ASSOC_NUM; ++j) {
      if (buckets[i].slot[j].key != INVALID) {
        sum++;
      }
    }
  }
  cout << "levels: " << levels << endl;
  cout << "rehash_num: " << rehash_num << endl;
  cout << "movements: " << movements << endl;
  return ((double)sum/(double)entry_num*100);
}

Value_t BucketizedCuckooHashing::FindAnyway(Key_t& key) {
  for (size_t i = 0; i < total_capacity; ++i) {
    for (unsigned j = 0; j < ASSOC_NUM; ++j) {
      if (buckets[i].slot[j].key == key) {
        return buckets[i].slot[j].value;
      }
    }
  }
  return NONE;
}
