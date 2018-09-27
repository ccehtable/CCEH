#include "bucketized_cuckoo.h"
#include "util/persist.h"
#include "util/hash.h"

#define FIRST_HASH(hash, capacity) (hash % (capacity / 2))
#define SECOND_HASH(hash, capacity) ((hash % (capacity / 2)) + (capacity / 2))
#define F_IDX() FIRST_HASH(                       \
    h((void *)&key, sizeof(Key_t), cuckoo->f_seed), \
    cuckoo->addr_capacity)
#define S_IDX() SECOND_HASH(                       \
    h((void *)&key, sizeof(Key_t), cuckoo->s_seed), \
    cuckoo->addr_capacity)

/* Memory fence */
#define asm_mfence()                \
({                      \
    __asm__ __volatile__ ("mfence":::"memory");    \
})

uint32_t F_IDX_Re(cuckoo_hash *cuckoo, Key_t& key, uint32_t capacity) {
    return (h((void *)&key, sizeof(key), cuckoo->f_seed)) % (capacity / 2);
}
uint32_t S_IDX_Re(cuckoo_hash *cuckoo, Key_t& key, uint32_t capacity) {
    return (h((void *)&key, sizeof(key), cuckoo->s_seed)) % (capacity / 2) + (capacity / 2);
}

void generate_seeds(cuckoo_hash *cuckoo)
{
    srand(time(NULL));
    
    do
    {
        cuckoo->f_seed = rand();
        cuckoo->s_seed = rand();
        cuckoo->f_seed = cuckoo->f_seed << (rand() % 63);
        cuckoo->s_seed = cuckoo->s_seed << (rand() % 63);
    } while (cuckoo->f_seed == cuckoo->s_seed);
}

cuckoo_hash *cuckoo_init(uint32_t levels)
{
    /** cuckoo_hash *cuckoo = pmalloc(sizeof(cuckoo_hash)); */
    cuckoo_hash *cuckoo = (cuckoo_hash*) malloc(sizeof(cuckoo_hash));
    if (!cuckoo)
    {
        printf("The Initialization Fails!\n");
        exit(1);
    }
    
    cuckoo->levels = levels;
    cuckoo->addr_capacity = pow(2, levels) + pow(2, levels - 1);
    cuckoo->total_capacity = pow(2, levels) + pow(2, levels - 1);
    cuckoo->occupied = 0;
    generate_seeds(cuckoo);
    cuckoo->rehash_num = 0;
    cuckoo->movements = 0;
    /** cuckoo->buckets = pmalloc(cuckoo->total_capacity*sizeof(cuckoo_node)); */
    cuckoo->buckets = (cuckoo_node*)malloc(cuckoo->total_capacity*sizeof(cuckoo_node));
    cuckoo->bucket_num = cuckoo->total_capacity;
    cuckoo->entry_num = cuckoo->bucket_num*ASSOC_NUM;
    
    if (!cuckoo->buckets)
    {
        printf("The Initialization Fails!\n");
        exit(1);
    }
    
    printf("The Initialization succeeds!\n");
    printf("The number of addressable cells in cuckoo hashing: %d\n", cuckoo->addr_capacity);
    printf("The total number of cells in cuckoo hashing: %d\n", cuckoo->total_capacity);
    printf("Buckets: %d\n", cuckoo->bucket_num); 
    printf("Entries: %d\n", cuckoo->entry_num);     
    return cuckoo; 
} 

int cuckoo_resize(cuckoo_hash *cuckoo) {

    uint32_t newCapacity = pow(2, cuckoo->levels + 1) + pow(2, cuckoo->levels);
    uint32_t oldCapacity = pow(2, cuckoo->levels) + pow(2, cuckoo->levels - 1);

    cuckoo->addr_capacity = newCapacity;                    
    /** cuckoo_node *newBucket = pmalloc(cuckoo->addr_capacity*sizeof(cuckoo_node));     */
    cuckoo_node *newBucket = (cuckoo_node*) malloc(cuckoo->addr_capacity*sizeof(cuckoo_node));    
    cuckoo_node *oldBucket = cuckoo->buckets;
    cuckoo->buckets = newBucket;
    cuckoo->levels++;
    cuckoo->total_capacity = newCapacity;
    cuckoo->bucket_num = cuckoo->total_capacity;
    cuckoo->entry_num = cuckoo->bucket_num*ASSOC_NUM;
    generate_seeds(cuckoo);

    if (!newBucket) {
        printf("The resize fails when callocing newBucket!\n");
        exit(1);
    }

    uint32_t old_idx;
    for (old_idx = 0; old_idx < oldCapacity; old_idx ++) {

        int i, j;
        for(i = 0; i < ASSOC_NUM; i ++){
            if (oldBucket[old_idx].slot[i].token == 1)
            {
                /** uint8_t *key = oldBucket[old_idx].slot[i].key; */
                /** uint8_t *value = oldBucket[old_idx].slot[i].value; */
                Key_t key = oldBucket[old_idx].slot[i].key;
                Value_t value = oldBucket[old_idx].slot[i].value;

                uint32_t f_idx = F_IDX_Re(cuckoo, key, cuckoo->addr_capacity);
                uint32_t s_idx = S_IDX_Re(cuckoo, key, cuckoo->addr_capacity);
                int insertSuccess = 0;

                for(;;){
                    for(j = 0; j < ASSOC_NUM; j ++){
                        if (cuckoo->buckets[f_idx].slot[j].token == 0)
                        {
                            /** memcpy(cuckoo->buckets[f_idx].slot[j].key, key, KEY_LEN); */
                            /** memcpy(cuckoo->buckets[f_idx].slot[j].value, value, VALUE_LEN); */
                            memcpy(&cuckoo->buckets[f_idx].slot[j].key, &key, KEY_LEN);
                            memcpy(&cuckoo->buckets[f_idx].slot[j].value, &value, VALUE_LEN);
                            /** pflush((uint64_t *)&cuckoo->buckets[f_idx].slot[j].key); */
                            /** pflush((uint64_t *)&cuckoo->buckets[f_idx].slot[j].value); */
                            clflush((char*)&cuckoo->buckets[f_idx].slot[j].key, sizeof(Key_t));
                            clflush((char*)&cuckoo->buckets[f_idx].slot[j].value, sizeof(Value_t));
                            /** asm_mfence(); */
                            mfence();
                            cuckoo->buckets[f_idx].slot[j].token = 1;
                            /** pflush((uint64_t *)&cuckoo->buckets[f_idx].slot[j].token); */
                            clflush((char*)&cuckoo->buckets[f_idx].slot[j].token, sizeof(uint8_t));
                            /** asm_mfence(); */
                            mfence();
                            insertSuccess = 1;
                            break;
                        }
                        else if (cuckoo->buckets[s_idx].slot[j].token == 0)
                        {
                            /** memcpy(cuckoo->buckets[s_idx].slot[j].key, key, KEY_LEN); */
                            /** memcpy(cuckoo->buckets[s_idx].slot[j].value, value, VALUE_LEN); */
                            memcpy(&cuckoo->buckets[s_idx].slot[j].key, &key, KEY_LEN);
                            memcpy(&cuckoo->buckets[s_idx].slot[j].value, &value, VALUE_LEN);
                            /** pflush((uint64_t *)&cuckoo->buckets[s_idx].slot[j].key); */
                            /** pflush((uint64_t *)&cuckoo->buckets[s_idx].slot[j].value); */
                            clflush((char*)&cuckoo->buckets[s_idx].slot[j].key, sizeof(Key_t));
                            clflush((char*)&cuckoo->buckets[s_idx].slot[j].value, sizeof(Value_t));
                            /** asm_mfence(); */
                            mfence();
                            cuckoo->buckets[s_idx].slot[j].token = 1;
                            /** pflush((uint64_t *)&cuckoo->buckets[s_idx].slot[j].token); */
                            /** asm_mfence(); */
                            mfence();
                            insertSuccess = 1;
                            break;
                        }
                    }
                    if (insertSuccess)
                        break;
                    if(rehash(cuckoo, f_idx, cuckoo->cuckoo_loop) == 2){
                      if (rehash(cuckoo, s_idx, cuckoo->cuckoo_loop) == 2) {
                        printf("Rehash fails when resizing!\n");
                        exit(1);
                      }
                    }
                }
                oldBucket[old_idx].slot[i].token == 0;
            }
        }
    }

    /** pfree(oldBucket, oldCapacity*sizeof(cuckoo_node)); */
    free(oldBucket);
    return 1;
}


uint8_t cuckoo_insert(cuckoo_hash *cuckoo, Key_t& key, Value_t value)
{
    uint32_t f_idx = F_IDX();
    uint32_t s_idx = S_IDX();
    /** uint8_t key_old[KEY_LEN]; */
    /** uint8_t value_old[VALUE_LEN]; */
    Key_t key_old;
    Value_t value_old;
    int j;      

    for (;;)
    {
        for(j = 0; j < ASSOC_NUM; j ++){
            if (!cuckoo->buckets[f_idx].slot[j].token)               
            {
                /** memcpy(cuckoo->buckets[f_idx].slot[j].key, key, KEY_LEN); */
                /** memcpy(cuckoo->buckets[f_idx].slot[j].value, value, VALUE_LEN); */
                memcpy(&cuckoo->buckets[f_idx].slot[j].key, &key, KEY_LEN);
                memcpy(&cuckoo->buckets[f_idx].slot[j].value, &value, VALUE_LEN);
                /** pflush((uint64_t *)&cuckoo->buckets[f_idx].slot[j].key); */
                /** pflush((uint64_t *)&cuckoo->buckets[f_idx].slot[j].value); */
                clflush((char*)&cuckoo->buckets[f_idx].slot[j].key, sizeof(Key_t));
                clflush((char*)&cuckoo->buckets[f_idx].slot[j].value, sizeof(Value_t));
                /** asm_mfence(); */
                mfence();
                cuckoo->buckets[f_idx].slot[j].token = 1;
                /** pflush((uint64_t *)&cuckoo->buckets[f_idx].slot[j].token); */
                clflush((char*)&cuckoo->buckets[f_idx].slot[j].token, sizeof(uint8_t));
                /** asm_mfence(); */
                mfence();
                cuckoo->occupied ++;
                return 0;
            }
        }
        for(j = 0; j < ASSOC_NUM; j ++){
            if (!cuckoo->buckets[s_idx].slot[j].token)
            {
                /** memcpy(cuckoo->buckets[s_idx].slot[j].key, key, KEY_LEN); */
                /** memcpy(cuckoo->buckets[s_idx].slot[j].value, value, VALUE_LEN); */
                memcpy(&cuckoo->buckets[s_idx].slot[j].key, &key, KEY_LEN);
                memcpy(&cuckoo->buckets[s_idx].slot[j].value, &value, VALUE_LEN);
                
                /** pflush((uint64_t *)&cuckoo->buckets[s_idx].slot[j].key); */
                /** pflush((uint64_t *)&cuckoo->buckets[s_idx].slot[j].value); */
                clflush((char*)&cuckoo->buckets[s_idx].slot[j].key, sizeof(Key_t));
                clflush((char*)&cuckoo->buckets[s_idx].slot[j].value, sizeof(Value_t));
                /** asm_mfence(); */
                mfence();
                cuckoo->buckets[s_idx].slot[j].token = 1;
                /** pflush((uint64_t *)&cuckoo->buckets[s_idx].slot[j].token); */
                clflush((char*)&cuckoo->buckets[s_idx].slot[j].token, sizeof(uint8_t));
                /** asm_mfence(); */
                mfence();
                cuckoo->occupied ++;
                return 0;
            }
        }
        
        if(rehash(cuckoo, f_idx, cuckoo->cuckoo_loop) == 2){
                    return 1;
        }
        
    }
    return 1;
}


static uint8_t rehash(cuckoo_hash *cuckoo, uint32_t idx, uint32_t loop)
{
    if (loop == 0)
    {
        return 2;
    }
    
    cuckoo->rehash_num ++;
    uint64_t rand_location = cuckoo->rand_location;
    Key_t key = cuckoo->buckets[idx].slot[rand_location%ASSOC_NUM].key;
    /** uint8_t *key = cuckoo->buckets[idx].slot[rand_location%ASSOC_NUM].key; */
    uint32_t f_idx = F_IDX();
    uint32_t s_idx = S_IDX();
    uint32_t n_idx = s_idx;
    entry *log_node = (entry *)malloc(sizeof(entry));
    /** entry *log_node = (entry *)pmalloc(sizeof(entry)); */

    if (idx == s_idx)
    {
        n_idx = f_idx;
    }
    
    int j;      
    for(j = 0; j < ASSOC_NUM; j ++){
        if (!cuckoo->buckets[n_idx].slot[j].token){             
            cuckoo->movements ++;
            /** memcpy(log_node->key, cuckoo->buckets[idx].slot[rand_location%ASSOC_NUM].key, KEY_LEN); */
            /** memcpy(log_node->value, cuckoo->buckets[idx].slot[rand_location%ASSOC_NUM].value, VALUE_LEN);            */
            memcpy(&log_node->key, &cuckoo->buckets[idx].slot[rand_location%ASSOC_NUM].key, KEY_LEN);
            memcpy(&log_node->value, &cuckoo->buckets[idx].slot[rand_location%ASSOC_NUM].value, VALUE_LEN);           
            /** pflush((uint64_t *)&log_node->key); */
            /** pflush((uint64_t *)&log_node->value); */
            clflush((char*)&log_node->key, sizeof(Key_t));
            clflush((char*)&log_node->value, sizeof(Value_t));
            /** asm_mfence(); */
            mfence();
            log_node->token = 1;
            /** pflush((uint64_t *)&log_node->token); */
            clflush((char*)&log_node->token, sizeof(uint8_t));
            /** asm_mfence();        */
            mfence();
            /** pfree(log_node, sizeof(entry));  */
            free(log_node);
    
            /** memcpy(cuckoo->buckets[n_idx].slot[j].key, cuckoo->buckets[idx].slot[rand_location%ASSOC_NUM].key, KEY_LEN); */
            /** memcpy(cuckoo->buckets[n_idx].slot[j].value, cuckoo->buckets[idx].slot[rand_location%ASSOC_NUM].value, VALUE_LEN); */
            memcpy(&cuckoo->buckets[n_idx].slot[j].key, &cuckoo->buckets[idx].slot[rand_location%ASSOC_NUM].key, KEY_LEN);
            memcpy(&cuckoo->buckets[n_idx].slot[j].value, &cuckoo->buckets[idx].slot[rand_location%ASSOC_NUM].value, VALUE_LEN);
            /** pflush((uint64_t *)&cuckoo->buckets[n_idx].slot[j].key); */
            /** pflush((uint64_t *)&cuckoo->buckets[n_idx].slot[j].value); */
            clflush((char*)&cuckoo->buckets[n_idx].slot[j].key, sizeof(Key_t));
            clflush((char*)&cuckoo->buckets[n_idx].slot[j].value, sizeof(Value_t));
            /** asm_mfence(); */
            mfence();
            cuckoo->buckets[n_idx].slot[j].token = 1;
            /** asm_mfence(); */
            mfence();
            /** pflush((uint64_t *)&cuckoo->buckets[n_idx].slot[j].token); */
            clflush((char*)&cuckoo->buckets[n_idx].slot[j].token, sizeof(uint8_t));
            cuckoo->buckets[idx].slot[rand_location%ASSOC_NUM].token = 0;
            /** pflush((uint64_t *)&cuckoo->buckets[idx].slot[rand_location%ASSOC_NUM].token); */
            clflush((char*)&cuckoo->buckets[idx].slot[rand_location%ASSOC_NUM].token, sizeof(uint8_t));
            /** asm_mfence(); */
            mfence();
            return 0;
        }
    }       
    
    if (rehash(cuckoo, n_idx, loop - 1) == 1)
    {
            return 1;
    }
}

Value_t cuckoo_query(cuckoo_hash *cuckoo, Key_t& key)
{
    uint32_t f_idx = F_IDX();
    uint32_t s_idx = S_IDX();
    int j;

    for(j = 0; j < ASSOC_NUM; j ++){
        if (cuckoo->buckets[f_idx].slot[j].token &&
            /** strcmp(cuckoo->buckets[f_idx].slot[j].key, key) == 0) { */
            cuckoo->buckets[f_idx].slot[j].key == key) {
            return cuckoo->buckets[f_idx].slot[j].value;
        }
        
        if (cuckoo->buckets[s_idx].slot[j].token &&
            /** strcmp(cuckoo->buckets[s_idx].slot[j].key, key) == 0) { */
            cuckoo->buckets[s_idx].slot[j].key == key) {
            return cuckoo->buckets[s_idx].slot[j].value;
        }
    }
    return NULL;

}

uint8_t cuckoo_delete(cuckoo_hash *cuckoo, Key_t& key)
{
    uint32_t f_idx = F_IDX();
    uint32_t s_idx = S_IDX();
    int j;

    for(j = 0; j < ASSOC_NUM; j ++){
        if (cuckoo->buckets[f_idx].slot[j].token == 1 &&
            /** strcmp(cuckoo->buckets[f_idx].slot[j].key, key) == 0) { */
            cuckoo->buckets[f_idx].slot[j].key == key) {
            cuckoo->buckets[f_idx].slot[j].token = 0;
            clflush((char*)&cuckoo->buckets[f_idx].slot[j].token, sizeof(uint8_t));
            /** asm_mfence(); */
            mfence();
            cuckoo->occupied --;
            return 0;
        }
    }
    for(j = 0; j < ASSOC_NUM; j ++){
         if (cuckoo->buckets[s_idx].slot[j].token == 1 &&
            /** strcmp(cuckoo->buckets[s_idx].slot[j].key, key) == 0) { */
            cuckoo->buckets[s_idx].slot[j].key == key) {
            cuckoo->buckets[s_idx].slot[j].token = 0;
            /** pflush((uint64_t *)&cuckoo->buckets[s_idx].slot[j].token); */
            clflush((char*)&cuckoo->buckets[s_idx].slot[j].token, sizeof(uint8_t));
            /** asm_mfence(); */
            mfence();
            cuckoo->occupied --;
            return 0;
        }
    }
    
    return 1;
}
