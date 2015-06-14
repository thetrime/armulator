// Simple map interface. Initially a linked list, but could make this into a hashtable when I get time
#include <stdlib.h>
#include <string.h>

struct map_entry_t
{
   char* key;
   void* value;
   struct map_entry_t* next;
};

typedef struct map_entry_t map_entry_t;

typedef struct
{
   map_entry_t* entries;
} map_t;


map_t* alloc_map()
{
   map_t* map = malloc(sizeof(map_t));
   map->entries = NULL;
   return map;
}

// FIXME: Does not free the stored values, only keys! Need to pass a function pointer to something to handle freeing stored values
void free_map(map_t* m)
{
   for (map_entry_t* e = m->entries; e; )
   {
      map_entry_t* f = e->next;
      free(e->value);
      free(e);
      e = f;
   }
}

map_entry_t* find_entry(map_t* m, char* key)
{
   for (map_entry_t* e = m->entries; e; e = e->next)
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
      e = malloc(sizeof(map_entry_t));
      e->next = m->entries;
      e->value = value;
      e->key = strdup(key);
      m->entries = e;
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
