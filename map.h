typedef struct map_t map_t;

map_t* alloc_map();
void free_map(map_t* m);
void map_put(map_t* m, char* key, void* value);
int map_get(map_t* m, char* key, void** value);
