#include <stdint.h>

typedef struct map_t map_t;

map_t* alloc_map(void (*free_fn)(void*), uint32_t (*hash_fn)(void* ptr), int (*comparator)(void*, void*));
map_t* alloc_char_map(void (*free_fn)(void*));
void free_map(map_t* m);
void map_put(map_t* m, void* key, void* value);
int map_get(map_t* m, void* key, void** value);
uint32_t map_size(map_t* m);
void forall(map_t* m, void (*fn)(void*, void*));
