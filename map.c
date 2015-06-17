// Simple map interface. Initially a linked list, but could make this into a hashtable when I get time
#include <stdlib.h>
#include <string.h>

#define INITIAL_MAP_SIZE 65535

// djb2
uint32_t hash(unsigned char *str)
{
    uint32_t hash = 5381;
    int c;    
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}
struct map_entry_t
{
   uint32_t hashkey;
   char* key;
   void* value;
   struct map_entry_t* next;
};

typedef struct map_entry_t map_entry_t;

typedef struct
{
   map_entry_t** entries;
   void (*free_fn)(void*);
   uint32_t size;
   uint32_t usage;
} map_t;

map_t* alloc_map(void (*free_fn)(void*))
{
   map_t* map = malloc(sizeof(map_t));
   map->size = INITIAL_MAP_SIZE;
   map->usage = 0;
   map->free_fn = free_fn;
   map->entries = calloc(sizeof(map_entry_t), map->size);
   return map;
}

void free_map(map_t* m)
{
   for (int i = 0; i < m->size; i++)
   {
      for (map_entry_t* e = m->entries[i]; e; )
      {
         map_entry_t* f = e->next;
         free(e->key);
         m->free_fn(e->value);
         free(e);
         e = f;
      }
   }
}

map_entry_t* find_entry(map_t* m, char* key)
{
   uint32_t hashkey = hash((unsigned char*)key) % m->size;
   for (map_entry_t* e = m->entries[hashkey]; e; e = e->next)
   {
      if (strcmp(e->key, key) == 0)
      {
         return e;
      }
   }
   return NULL;
}

void map_put(map_t* m, char* key, void* value)
{
   map_entry_t* e = find_entry(m, key);
   if (e)
      e->value = value;
   else
   {
      m->usage++;
      // FIXME: Rehash everything here if usage is too high
      uint32_t hashkey = hash((unsigned char*)key) % m->size;
      e = malloc(sizeof(map_entry_t));
      e->next = m->entries[hashkey];
      e->hashkey = hashkey; // For rehashing later
      e->value = value;
      e->key = strdup(key);
      m->entries[hashkey] = e;
   }
}

int map_get(map_t* m, char* key, void** value)
{
   map_entry_t* e = find_entry(m, key);
   if (e == NULL)
      return 0;
   *value = e->value;
   return 1;
}

uint32_t map_size(map_t* m)
{
   return m->usage;
}

void forall(map_t* m, void (*fn)(char*, void*))
{
   for (int i = 0; i < m->size; i++)
   {
      for (map_entry_t* e = m->entries[i]; e; e = e->next)
      {
         fn(e->key, e->value);
      }
   }
}
