#include <stdint.h>

typedef struct map_t map_t;

map_t* alloc_map();
void free_map(map_t* m);
void map_put(map_t* m, char* key, void* value);
int map_get(map_t* m, char* key, void** value);
uint32_t map_size(map_t* m);
void forall(map_t* m, void (*fn)(char*, void*));
