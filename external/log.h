#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#include "util/pair.h"
#include "util/persist.h"


typedef struct log_entry{                
  Key_t key;
  Value_t value;
    // uint8_t key[KEY_LEN];
    // uint8_t value[VALUE_LEN];
    uint8_t flag;
} log_entry;

typedef struct level_log
{ 
    uint64_t log_length;
    log_entry* entry;
    uint64_t current;
}level_log;

level_log* log_create(uint64_t log_length);

void log_write(level_log *log, Key_t& key, Value_t value);
void log_clean(level_log *log);
