#include "level_hashing.h"
#include "util/persist.h"
#include "util/hash.h"

uint64_t F_HASH(level_hash *level, Key_t& key) {
    return (h(&key, sizeof(Key_t), level->f_seed));
}

uint64_t S_HASH(level_hash *level, Key_t& key) {
    return (h(&key, sizeof(Key_t), level->s_seed));
}

uint32_t F_IDX(uint64_t hashKey, uint64_t capacity) {
    return hashKey % (capacity / 2);
}

uint32_t S_IDX(uint64_t hashKey, uint64_t capacity) {
    return (hashKey % (capacity / 2)) + (capacity / 2);
}

void generate_seeds(level_hash *level)
{
    srand(time(NULL));

    do
    {
        level->f_seed = rand();
        level->s_seed = rand();
        level->f_seed = level->f_seed << (rand() % 63);
        level->s_seed = level->s_seed << (rand() % 63);
    } while (level->f_seed == level->s_seed);
}

level_hash *level_init(uint32_t levels)
{
    level_hash *level = (level_hash*)malloc(sizeof(level_hash));
    if (!level)
    {
        printf("The Initialization Fails1!\n");
        exit(1);
    }

    level->levels = levels;
    level->addr_capacity = pow(2, levels);
    level->total_capacity = pow(2, levels) + pow(2, levels - 1);
    level->occupied = 0;
    generate_seeds(level);
    level->buckets[0] = (level_node*)malloc(pow(2, levels)*sizeof(level_node));
    level->buckets[1] = (level_node*)malloc(pow(2, levels - 1)*sizeof(level_node));

    level->bucket_num = level->total_capacity;
    level->entry_num = level->bucket_num*ASSOC_NUM;
    level->resize_num = 0;

    if (!level->buckets[0] || !level->buckets[1])
    {
        printf("The Initialization Fails2!\n");
        exit(1);
    }

    printf("The Initialization succeeds!\n");
    printf("The number of addressable cells in level hashing: %d\n", level->addr_capacity);
    printf("The total number of cells in level hashing: %d\n", level->total_capacity);
    printf("Buckets: %d\n", level->bucket_num);
    printf("Entries: %d\n", level->entry_num);
    return level;
}

int level_resize(level_hash *level) {
    if (!level)
    {
        printf("The Resize Fails!\n");
        exit(1);
    }

    level->addr_capacity = pow(2, level->levels + 1);
    level_node *newBucket = (level_node*)malloc(level->addr_capacity*sizeof(level_node));
    if (!newBucket) {
        printf("The resize fails when callocing newBucket!\n");
        exit(1);
    }

    uint32_t old_idx;
    for (old_idx = 0; old_idx < pow(2, level->levels - 1); old_idx ++) {

        int i, j;
        for(i = 0; i < ASSOC_NUM; i ++){
            if (level->buckets[1][old_idx].slot[i].token == 1)
            {
                Key_t key = level->buckets[1][old_idx].slot[i].key;
                Value_t value = level->buckets[1][old_idx].slot[i].value;

                uint32_t f_idx = F_IDX(F_HASH(level, key), level->addr_capacity);
                uint32_t s_idx = S_IDX(S_HASH(level, key), level->addr_capacity);

                int insertSuccess = 0;
                for(j = 0; j < ASSOC_NUM; j ++){
                    if (newBucket[f_idx].slot[j].token == 0)
                    {
                        memcpy(&newBucket[f_idx].slot[j].key, &key, sizeof(Key_t));
                        memcpy(&newBucket[f_idx].slot[j].value, &value, sizeof(Value_t));
                        clflush((char *)&newBucket[f_idx].slot[j].key, sizeof(Key_t) + sizeof(Value_t));
                        mfence();
                        newBucket[f_idx].slot[j].token = 1;
                        clflush((char *)&newBucket[f_idx].slot[j].token, sizeof(uint8_t));
                        mfence();
                        insertSuccess = 1;
                        break;
                    }
                    else if (newBucket[s_idx].slot[j].token == 0)
                    {
                        memcpy(&newBucket[s_idx].slot[j].key, &key, sizeof(Key_t));
                        memcpy(&newBucket[s_idx].slot[j].value, &value, sizeof(Value_t));
                        clflush((char *)&newBucket[s_idx].slot[j].key, sizeof(Key_t) + sizeof(Value_t));
                        mfence();
                        newBucket[s_idx].slot[j].token = 1;
                        clflush((char *)&newBucket[s_idx].slot[j].token, sizeof(uint8_t));
                        mfence();
                        insertSuccess = 1;
                        break;
                    }
                }
            }
        }

    }

    level->levels++;
    level->total_capacity = pow(2, level->levels) + pow(2, level->levels - 1);

    /** pfree(level->buckets[1], pow(2, level->levels - 2)*sizeof(level_node)); */
    free(level->buckets[1]);
    level->buckets[1] = level->buckets[0];
    level->buckets[0] = newBucket;

    newBucket = NULL;
    level->bucket_num = level->total_capacity;
    level->entry_num = level->bucket_num*ASSOC_NUM;

    return 1;
}


Value_t level_query(level_hash *level, Key_t& key)
{
    uint64_t f_hash = F_HASH(level, key);
    uint64_t s_hash = S_HASH(level, key);
    uint32_t f_idx = F_IDX(f_hash, level->addr_capacity);
    uint32_t s_idx = S_IDX(s_hash, level->addr_capacity);
    int i, j;

    for(i = 0; i < 2; i ++){

        for(j = 0; j < ASSOC_NUM; j ++){
            if (level->buckets[i][f_idx].slot[j].token == 1&&level->buckets[i][f_idx].slot[j].key == key)
            {
                return level->buckets[i][f_idx].slot[j].value;
            }
            else if (level->buckets[i][s_idx].slot[j].token == 1&&level->buckets[i][s_idx].slot[j].key == key)
            {
                return level->buckets[i][s_idx].slot[j].value;
            }
        }
        f_idx = f_hash%(level->addr_capacity / 4);
        s_idx = s_hash%(level->addr_capacity / 4) + (level->addr_capacity / 4);
    }
    return NULL;
}

int level_delete(level_hash *level, Key_t& key)
{
    uint64_t f_hash = F_HASH(level, key);
    uint64_t s_hash = S_HASH(level, key);
    uint32_t f_idx = F_IDX(f_hash, level->addr_capacity);
    uint32_t s_idx = S_IDX(s_hash, level->addr_capacity);
    int i, j;
    for(i = 0; i < 2; i ++){
        for(j = 0; j < ASSOC_NUM; j ++){
            if (level->buckets[i][f_idx].slot[j].token == 1&&level->buckets[i][f_idx].slot[j].key == key)
            {
                level->buckets[i][f_idx].slot[j].token = 0;
                level->occupied--;
                return 0;
            }
            else if (level->buckets[i][s_idx].slot[j].token == 1&&level->buckets[i][s_idx].slot[j].key == key)
            {
                level->buckets[i][s_idx].slot[j].token = 0;
                level->occupied--;
                return 0;
            }
        }
        f_idx = F_IDX(f_hash, level->addr_capacity / 2);
        s_idx = S_IDX(s_hash, level->addr_capacity / 2);
    }
    return 1;
}

uint8_t level_insert(level_hash *level, Key_t& key, Value_t value)
{
    uint64_t f_hash = F_HASH(level, key);
    uint64_t s_hash = S_HASH(level, key);
    uint32_t f_idx = F_IDX(f_hash, level->addr_capacity);
    uint32_t s_idx = S_IDX(s_hash, level->addr_capacity);

    int i, j;

    for(i = 0; i < 2; i ++){
        for(j = 0; j < ASSOC_NUM; j ++){
            if (level->buckets[i][f_idx].slot[j].token == 0)
            {
                memcpy(&level->buckets[i][f_idx].slot[j].key, &key, sizeof(Key_t));
                memcpy(&level->buckets[i][f_idx].slot[j].value, &value, sizeof(Value_t));
                clflush((char*)&level->buckets[i][f_idx].slot[j].key, sizeof(Key_t) + sizeof(Value_t));
                mfence();
                level->buckets[i][f_idx].slot[j].token = 1;
                clflush((char *)&level->buckets[i][f_idx].slot[j].token, sizeof(uint8_t));
                mfence();
                level->occupied ++;

                return 0;
            }
            else if (level->buckets[i][s_idx].slot[j].token == 0)
            {
                memcpy(&level->buckets[i][s_idx].slot[j].key, &key, sizeof(Key_t));
                memcpy(&level->buckets[i][s_idx].slot[j].value, &value, sizeof(Value_t));
                clflush((char *)&level->buckets[i][s_idx].slot[j].key, sizeof(Key_t) + sizeof(Value_t));
                mfence();
                level->buckets[i][s_idx].slot[j].token = 1;
                clflush((char *)&level->buckets[i][s_idx].slot[j].token, sizeof(uint8_t));
                mfence();
                level->occupied ++;

                return 0;
            }
        }
        f_idx = F_IDX(f_hash, level->addr_capacity / 2);
        s_idx = S_IDX(s_hash, level->addr_capacity / 2);
    }
    
    return 1;
}
