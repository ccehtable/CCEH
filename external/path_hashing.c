#include "path_hashing.h"
#include "util/persist.h"
#include "util/hash.h"

#define FIRST_HASH(hash, capacity) (hash % (capacity / 2))
#define SECOND_HASH(hash, capacity) ((hash % (capacity / 2)) + (capacity / 2))
#define F_IDX() FIRST_HASH(                       \
    h(&key, sizeof(key), path->f_seed), \
    path->addr_capacity)
#define S_IDX() SECOND_HASH(                       \
    h(&key, sizeof(key), path->s_seed), \
    path->addr_capacity)
    
uint64_t F_HASH(path_hash *path, Key_t& key) {
    return (h(&key, sizeof(key), path->f_seed));
}

uint64_t S_HASH(path_hash *path, Key_t& key) {
    return (h(&key, sizeof(key), path->s_seed));
}

uint32_t F_IDX_Re(uint64_t hashKey, uint64_t capacity) {
    return hashKey % (capacity / 2);
}

uint32_t S_IDX_Re(uint64_t hashKey, uint64_t capacity) {
    return (hashKey % (capacity / 2)) + (capacity / 2);
}   

void generate_seeds(path_hash *path)
{
    srand(time(NULL));
    
    do
    {
        path->f_seed = rand();
        path->s_seed = rand();
        path->f_seed = path->f_seed << (rand() % 63);
        path->s_seed = path->s_seed << (rand() % 63);
    } while (path->f_seed == path->s_seed);
}

path_hash *path_init(uint32_t levels, uint32_t reserved_levels)
{
    
    path_hash *path = (path_hash*) malloc(sizeof(path_hash));
    if (!path)
    {
        printf("The Initialization Fails1!\n");
        exit(1);
    }
    
    path->levels = levels;
    path->reserved_levels = reserved_levels;
    path->addr_capacity = pow(2, levels - 1);
    path->total_capacity = pow(2, levels) - pow(2, levels - reserved_levels);
    path->size = 0;
    generate_seeds(path);
    path->nodes = (path_node*) malloc(path->total_capacity*sizeof(path_node));
    
    if (!path->nodes)
    {
        printf("The Initialization Fails2!\n");
        exit(1);
    }
    
    printf("The Initialization succeeds!\n");
    printf("The number of levels in path hashing: %d\n", path->levels);
    printf("The number of reserved levels in path hashing: %d\n", path->reserved_levels);
    printf("The number of addressable cells in path hashing: %d\n", path->addr_capacity);
    printf("The total number of cells in path hashing: %d\n", path->total_capacity);
    
    return path;
}

int path_resize(path_hash *path) {

    if (!path)
    {
        printf("The Resize Fails1!\n");
        exit(1);
    }

    uint32_t old_total_capacity =  path->total_capacity;
    path_node *oldBucket = path->nodes;
    path->levels ++;
    path->addr_capacity = pow(2, path->levels-1);
    path->total_capacity = pow(2, path->levels) - pow(2, path->levels - path->reserved_levels);
    path->nodes = (path_node*) malloc(path->total_capacity*sizeof(path_node));
    if (!path->nodes) {
        printf("The Resize Failed in calloc path->nodes!\n");
        exit(1);
    }

    uint32_t old_idx;
    for (old_idx = 0; old_idx < old_total_capacity; old_idx ++) {

        int i, j;
            if (oldBucket[old_idx].token == 1)
            {
                Key_t key = oldBucket[old_idx].key;
                Value_t value = oldBucket[old_idx].value;

                uint32_t f_idx = F_IDX_Re(F_HASH(path, key), path->addr_capacity);
                uint32_t s_idx = S_IDX_Re(S_HASH(path, key), path->addr_capacity);

                uint32_t sub_f_idx = f_idx;
                uint32_t sub_s_idx = s_idx;
                uint32_t capacity = 0;
    
                int insertSuccess = 0;
                for(i = 0; i < path->reserved_levels; i ++){

                        if (path->nodes[f_idx].token == 0)
                        {
                            memcpy(&path->nodes[f_idx].key, &key, sizeof(Key_t));
                            memcpy(&path->nodes[f_idx].value, &value, sizeof(Value_t));
                            clflush((char*)&path->nodes[f_idx].key, sizeof(Key_t) + sizeof(Value_t));
                            mfence();
                            path->nodes[f_idx].token = 1;
                            clflush((char*)&path->nodes[f_idx].token, sizeof(uint8_t));
                            mfence();
                            insertSuccess = 1;
                            break;
                        }
                        else if (path->nodes[s_idx].token == 0)
                        {
                            memcpy(&path->nodes[s_idx].key, &key, sizeof(Key_t));
                            memcpy(&path->nodes[s_idx].value, &value, sizeof(Value_t));
                            clflush((char*)&path->nodes[s_idx].key, sizeof(Key_t) + sizeof(Value_t));
                            mfence();
                            path->nodes[s_idx].token = 1;
                            clflush((char*)&path->nodes[s_idx].token, sizeof(uint8_t));
                            mfence();
                            insertSuccess = 1;
                            break;
                        }
                    
                    sub_f_idx = sub_f_idx/2;
                    sub_s_idx = sub_s_idx/2;            
                    capacity = (int)pow(2, path->levels) - (int)pow(2, path->levels - i - 1);
        
                    f_idx = sub_f_idx + capacity;
                    s_idx = sub_s_idx + capacity; 
                }
            }
    }

    free(oldBucket);
    return 1;
}

uint8_t path_insert(path_hash *path, Key_t& key, Value_t value)
{
    uint32_t f_idx = F_IDX();
    uint32_t s_idx = S_IDX();
    
    uint32_t sub_f_idx = f_idx;
    uint32_t sub_s_idx = s_idx;
    uint32_t capacity = 0;
    
    int i;      
    for(i = 0; i < path->reserved_levels; i ++){
        if (!path->nodes[f_idx].token)               
        {
            memcpy(&path->nodes[f_idx].key, &key, sizeof(Key_t));
            memcpy(&path->nodes[f_idx].value, &value, sizeof(Value_t));
            clflush((char*)&path->nodes[f_idx].key, sizeof(Key_t) + sizeof(Value_t));
            mfence();
            path->nodes[f_idx].token = 1;
            clflush((char*)&path->nodes[f_idx].token, sizeof(uint8_t));
            mfence();
            path->size++;
            return 0;
        }
        else if (!path->nodes[s_idx].token)
        {
            memcpy(&path->nodes[s_idx].key, &key, sizeof(Key_t));
            memcpy(&path->nodes[s_idx].value, &value, sizeof(Value_t));
            clflush((char*)&path->nodes[s_idx].key, sizeof(Key_t) + sizeof(Value_t));
            mfence();
            path->nodes[s_idx].token = 1;
            clflush((char*)&path->nodes[s_idx].token, sizeof(uint8_t));
            mfence();
            path->size++;
            return 0;
        }
        
        sub_f_idx = sub_f_idx/2;
        sub_s_idx = sub_s_idx/2;            
        capacity = (int)pow(2, path->levels) - (int)pow(2, path->levels - i - 1);
        
        f_idx = sub_f_idx + capacity;
        s_idx = sub_s_idx + capacity;   
    }
    
    if(path_resize(path)){
        return 0;
    }
    printf("Insertion fails: %s\n", key);
    return 1;
}

uint8_t path_delete(path_hash *path, Key_t& key)
{
    uint32_t f_idx = F_IDX();
    uint32_t s_idx = S_IDX();

    uint32_t sub_f_idx = f_idx;
    uint32_t sub_s_idx = s_idx;
    uint32_t capacity = 0;

    int i;
    for(i = 0; i < path->reserved_levels; i ++){
        if (path->nodes[f_idx].token)            
        {
            if(path->nodes[f_idx].key == key){
                path->nodes[f_idx].token = 0;
                path->size--;
                return 0;
            }
        }
        if (path->nodes[s_idx].token)
        {
            if(path->nodes[s_idx].key == key){
                path->nodes[s_idx].token = 0;
                path->size--;
                return 0;
            }
        }
            
        sub_f_idx = sub_f_idx/2;
        sub_s_idx = sub_s_idx/2;            
        capacity = (int)pow(2, path->levels) - (int)pow(2, path->levels - i - 1);
            
        f_idx = sub_f_idx + capacity;
        s_idx = sub_s_idx + capacity;       
    }

    printf("The key does not exist: %d\n", key);
    return 1;   
}

Value_t path_query(path_hash *path, Key_t& key)
{   
    uint32_t f_idx = F_IDX();
    uint32_t s_idx = S_IDX();

    uint32_t sub_f_idx = f_idx;
    uint32_t sub_s_idx = s_idx;
    uint32_t capacity = 0;

    int i;
    for(i = 0; i < path->reserved_levels; i ++){
        if (path->nodes[f_idx].token)                
        {
            if(path->nodes[f_idx].key == key){
                return path->nodes[f_idx].value;
            }
        }
        if (path->nodes[s_idx].token )
        {
            if(path->nodes[s_idx].key == key){
                return path->nodes[s_idx].value;
            }
        }
        
        sub_f_idx = sub_f_idx/2;
        sub_s_idx = sub_s_idx/2;            
        capacity = pow(2, path->levels) - pow(2, path->levels - i - 1);
            
        f_idx = sub_f_idx + capacity;
        s_idx = sub_s_idx + capacity;           
    }
    
    printf("The key does not exist: %d\n", key);   
    return NULL;
}











