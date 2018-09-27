#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include "util/pair.h"

// #include "hash.h"
// #include "cflush.h"
// #include "../../../src/lib/pmalloc.h"       // path to pmalloc.h in Quartz

#define ASSOC_NUM 4
#define KEY_LEN sizeof(Key_t)
#define VALUE_LEN sizeof(Value_t)
#define CUCKOO_MAX_DEP 16


typedef struct entry{
    uint8_t token;                        
    Key_t key;
    Value_t value;
    // uint8_t key[KEY_LEN];
    // uint8_t value[VALUE_LEN];
} entry;

typedef struct cuckoo_node
{
    entry slot[ASSOC_NUM];
} cuckoo_node;

typedef struct cuckoo_hash {
    cuckoo_node *buckets;
    uint32_t entry_num;
    uint32_t bucket_num;
    
    uint32_t levels;
    uint32_t addr_capacity;       
    uint32_t total_capacity;                
    uint32_t occupied;                     
    uint32_t cuckoo_loop;
    uint64_t rehash_num;
    uint64_t movements;
    uint64_t f_seed;
    uint64_t s_seed;
    
    FILE *log_file;
    uint64_t rand_location;
} cuckoo_hash;

cuckoo_hash *cuckoo_init(uint32_t levels);                                      

uint8_t cuckoo_insert(cuckoo_hash *cuckoo, Key_t& key, Value_t value);         

static uint8_t rehash(cuckoo_hash *cuckoo, uint32_t idx, uint32_t loop);

Value_t cuckoo_query(cuckoo_hash *cuckoo, Key_t& key);

uint8_t cuckoo_delete(cuckoo_hash *cuckoo, Key_t& key);

int cuckoo_resize(cuckoo_hash *cuckoo);

