#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include "util/pair.h"

#define KEY_LEN 16
#define VALUE_LEN 15

typedef struct path_node
{
    uint8_t token;                  // the token signifies whether the node is empty
    // uint8_t key[KEY_LEN];
    // uint8_t value[VALUE_LEN];
    Key_t key;
    Value_t value;
} path_node;


typedef struct path_hash
{
    uint32_t levels;                //  the number of levels of the complete binary tree in path hashing
    uint32_t reserved_levels;       //  the number of reserved levels in path hashing  
    uint32_t addr_capacity;         //  the number of addressed cells in path hashing, i.e., the number of cells in the top level 
    uint32_t total_capacity;        //  the total number of cells in path hashing
    uint32_t size;                  //  the number of stored items in path hashing
    uint64_t f_seed;
    uint64_t s_seed;
    path_node *nodes;
} path_hash;

path_hash *path_init(uint32_t levels, uint32_t reserved_levels);             // Initialize a path hashing

uint8_t path_insert(path_hash *path, Key_t& key, Value_t value);          // Insert an item in path hashing

uint8_t path_delete(path_hash *path, Key_t& key);                          // Delete an item in path hashing

Value_t path_query(path_hash *path, Key_t& key);                          // Query an item in path hashing



