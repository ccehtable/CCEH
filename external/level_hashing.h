#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include "util/pair.h"

#define ASSOC_NUM 4
#define KEY_LEN 16
#define VALUE_LEN 15

typedef struct entry{
    uint8_t token;                  
    // uint8_t key[KEY_LEN];
    // uint8_t value[VALUE_LEN];
    Key_t key;
    Value_t value;
} entry;

typedef struct level_node
{
    entry slot[ASSOC_NUM];
} level_node;

typedef struct level_hash {
    level_node *buckets[2];
    uint32_t entry_num;
    uint32_t bucket_num;
    
    uint32_t levels;
    uint32_t addr_capacity;         
    uint32_t total_capacity;        
    uint32_t occupied;              
    uint64_t f_seed;
    uint64_t s_seed;
    uint32_t resize_num;
} level_hash;

level_hash *level_init(uint32_t levels);                                        

uint8_t level_insert(level_hash *level, Key_t& key, Value_t value);          

Value_t level_query(level_hash *level, Key_t& key);

int level_delete(level_hash *level, Key_t& key);

int level_resize(level_hash *level);

