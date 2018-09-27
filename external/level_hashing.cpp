#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "level_hashing.hpp"
#include "../util/hash.h"
#include "../util/persist.h"

using namespace std;

#define F_HASH(key) (h(&key, sizeof(Key_t), f_seed))
#define S_HASH(key) (h(&key, sizeof(Key_t), s_seed))
#define F_IDX(hash, capacity) (hash % (capacity/2))
#define S_IDX(hash, capacity) ((hash % (capacity/2)) + (capacity/2))

// inline uint64_t LevelHashing::F_HASH(Key_t& key) {
//   return (h(&key, sizeof(Key_t), f_seed));
// }
//
// inline uint64_t LevelHashing::S_HASH(Key_t& key) {
//   return (h(&key, sizeof(Key_t), s_seed));
// }

void LevelHashing::generate_seeds(void) {
  srand(time(NULL));

  do
  {
    f_seed = rand();
    s_seed = rand();
    f_seed = f_seed << (rand() % 63);
    s_seed = s_seed << (rand() % 63);
  } while (f_seed == s_seed);
}

LevelHashing::LevelHashing(void){
}

LevelHashing::~LevelHashing(void){
  delete [] mutex;
  delete [] buckets;
}

LevelHashing::LevelHashing(size_t _levels)
  : levels{_levels},
  addr_capacity{pow(2, levels)},
  total_capacity{pow(2, levels) + pow(2, levels-1)},
  occupied{0},
  bucket_num{total_capacity},
  entry_num{bucket_num*ASSOC_NUM},
  resize_num{0}
{
  locksize = 256;
  nlocks = (3*addr_capacity/2)/locksize+1;
  mutex = new std::shared_mutex[nlocks];

  generate_seeds();
  buckets[0] = new Node[addr_capacity];
  buckets[1] = new Node[addr_capacity/2];
}

void LevelHashing::Insert(Key_t& key, Value_t value) {
RETRY:
  while (resizing_lock == 1) {
    asm("nop");
  }
  uint64_t f_hash = F_HASH(key);
  uint64_t s_hash = S_HASH(key);
  uint32_t f_idx = F_IDX(f_hash, addr_capacity);
  uint32_t s_idx = S_IDX(s_hash, addr_capacity);

  int i, j;

  for(i = 0; i < 2; i ++){
    for(j = 0; j < ASSOC_NUM; j ++){
      auto lock1 = INVALID;
      auto lock2 = INVALID;
      {
        std::unique_lock<std::shared_mutex> lock(mutex[f_idx/locksize]);
        if (CAS(&buckets[i][f_idx].slot[j].key, &lock1, SENTINEL)) {
          buckets[i][f_idx].slot[j].value = value;
          mfence();
          buckets[i][f_idx].slot[j].key = key;
          clflush((char *)&buckets[i][f_idx].slot[j], sizeof(Entry));
          auto _occ = occupied;
          while (!CAS(&occupied, &_occ, _occ+1)) {
            _occ = occupied;
          }
          return;
        }
      }
      {
        std::unique_lock<std::shared_mutex> lock(mutex[s_idx/locksize]);
        if (CAS(&buckets[i][s_idx].slot[j].key, &lock2, SENTINEL)) {
          buckets[i][s_idx].slot[j].value = value;
          mfence();
          buckets[i][s_idx].slot[j].key = key;
          clflush((char *)&buckets[i][s_idx].slot[j], sizeof(Entry));
          auto _occ = occupied;
          while (!CAS(&occupied, &_occ, _occ+1)) {
            _occ = occupied;
          }
          return;
        }
      }
    }
    f_idx = F_IDX(f_hash, addr_capacity / 2);
    s_idx = S_IDX(s_hash, addr_capacity / 2);
  }
  auto lock = 0;
  if (CAS(&resizing_lock, &lock, 1)) {
    timer.Start();
    resize();
    timer.Stop();
    breakdown += timer.GetSeconds();
    resizing_lock = 0;
  }
  goto RETRY;
}

bool LevelHashing::InsertOnly(Key_t& key, Value_t value) {
  uint64_t f_hash = F_HASH(key);
  uint64_t s_hash = S_HASH(key);
  uint32_t f_idx = F_IDX(f_hash, addr_capacity);
  uint32_t s_idx = S_IDX(s_hash, addr_capacity);

  int i, j;

  for(i = 0; i < 2; i ++){
    for(j = 0; j < ASSOC_NUM; j ++){
        if (buckets[i][f_idx].slot[j].key == INVALID)
        {

          buckets[i][f_idx].slot[j].value = value;
          mfence();
          buckets[i][f_idx].slot[j].key = key;
          clflush((char*)&buckets[i][f_idx].slot[j], sizeof(Entry));
          occupied++;
          return true;
        }
        if (buckets[i][s_idx].slot[j].key == INVALID)
        {
          buckets[i][s_idx].slot[j].value = value;
          mfence();
          buckets[i][s_idx].slot[j].key = key;
          clflush((char *)&buckets[i][s_idx].slot[j].key, sizeof(Entry));
          occupied++;
          return true;
        }
    }
    f_idx = F_IDX(f_hash, addr_capacity / 2);
    s_idx = S_IDX(s_hash, addr_capacity / 2);
  }

  return false;
}

void LevelHashing::resize(void) {
  std::unique_lock<std::shared_mutex> *lock[nlocks];
  for(int i=0;i<nlocks;i++){
    lock[i] = new std::unique_lock<std::shared_mutex>(mutex[i]);
  }
  std::shared_mutex* old_mutex = mutex;

  int prev_nlocks = nlocks;
  nlocks = nlocks + 2*addr_capacity/locksize+1;
  mutex= new std::shared_mutex[nlocks];

  size_t new_addr_capacity = pow(2, levels + 1);
  auto newBucket = new Node[new_addr_capacity];

  uint32_t old_idx;
  for (old_idx = 0; old_idx < pow(2, levels - 1); old_idx ++) {

    int i, j;
    for(i = 0; i < ASSOC_NUM; i ++){
      if (buckets[1][old_idx].slot[i].key != INVALID)
      {
        Key_t* key = &buckets[1][old_idx].slot[i].key;
        Value_t* value = &buckets[1][old_idx].slot[i].value;

        uint32_t f_idx = F_IDX(F_HASH(*key), new_addr_capacity);
        uint32_t s_idx = S_IDX(S_HASH(*key), new_addr_capacity);

        int insertSuccess = 0;
        for(j = 0; j < ASSOC_NUM; j ++){
          if (newBucket[f_idx].slot[j].key == INVALID)
          {
            newBucket[f_idx].slot[j].value = *value;
            mfence();
            newBucket[f_idx].slot[j].key = *key;
            clflush((char *)&newBucket[f_idx].slot[j], sizeof(Entry));
            insertSuccess = 1;
            break;
          }
          else if (newBucket[s_idx].slot[j].key == INVALID)
          {
            newBucket[s_idx].slot[j].value = *value;
            mfence();
            newBucket[s_idx].slot[j].key = *key;
            clflush((char *)&newBucket[s_idx].slot[j], sizeof(Entry));
            insertSuccess = 1;
            break;
          }
        }
      }
    }
  }

  levels++;

  delete [] buckets[1];
  buckets[1] = buckets[0];
  buckets[0] = newBucket;

  addr_capacity = new_addr_capacity;
  total_capacity = pow(2, levels) + pow(2, levels - 1);

  newBucket = NULL;
  bucket_num = total_capacity;
  entry_num = bucket_num*ASSOC_NUM;

  for(int i=0;i<prev_nlocks;i++){
    delete lock[i];
  }
  delete[] old_mutex;
}

Value_t LevelHashing::Get(Key_t& key) {
  uint64_t f_hash = F_HASH(key);
  uint64_t s_hash = S_HASH(key);
  uint32_t f_idx = F_IDX(f_hash, addr_capacity);
  uint32_t s_idx = S_IDX(s_hash, addr_capacity);
  int i = 0, j;

  for(i = 0; i < 2; i ++){
    {
      std::shared_lock<std::shared_mutex> lock(mutex[f_idx/locksize]);
      for(j = 0; j < ASSOC_NUM; j ++){
        if (buckets[i][f_idx].slot[j].key == key)
        {
          return buckets[i][f_idx].slot[j].value;
        }
      }
    }
    {
      std::shared_lock<std::shared_mutex> lock(mutex[s_idx/locksize]);
      for(j = 0; j < ASSOC_NUM; j ++){
        if (buckets[i][s_idx].slot[j].key == key)
        {
          return buckets[i][s_idx].slot[j].value;
        }
      }
    }
    f_idx = F_IDX(f_hash, addr_capacity/2);
    s_idx = S_IDX(s_hash, addr_capacity/2);
  }

  return NONE;
}

bool LevelHashing::Delete(Key_t& key) {
  return false;
}

double LevelHashing::Utilization(void) {
  size_t sum = 0;
  for (unsigned i = 0; i < addr_capacity; ++i) {
    for(unsigned j = 0; j < ASSOC_NUM; ++j) {
      if (buckets[0][i].slot[j].key != INVALID) {
        sum++;
      }
    }
  }
  for (unsigned i = 0; i < addr_capacity/2; ++i) {
    for(unsigned j = 0; j < ASSOC_NUM; ++j) {
      if (buckets[1][i].slot[j].key != INVALID) {
        sum++;
      }
    }
  }
  return ((double)(sum)/(double)(total_capacity*ASSOC_NUM)*100);
}
