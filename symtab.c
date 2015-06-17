#include "map.h"
#include "machine.h"
#include <stdlib.h>
#include <stdio.h>

map_t* symtab = NULL;

struct entry_binding_t
{
   uint32_t target;
   struct entry_binding_t* next;
};

typedef struct entry_binding_t entry_binding_t;

typedef struct
{
   uint32_t value;
   entry_binding_t* bindings;
} symtab_entry_t;

void bind_symbol(uint32_t target, uint32_t value)
{
   write_mem(4, target, value);
}

void free_symtab_entry(void* entryt)
{
   symtab_entry_t* entry = (symtab_entry_t*) entryt;
   while (entry->bindings)
   {
      entry_binding_t* b = entry->bindings;
      entry->bindings = b->next;
      free(b);
   }
   free(entry);
}

void found_symbol(char* symbol_name, uint32_t value)
{
   symtab_entry_t* entry;
   if (symtab == NULL)
      symtab = alloc_map(free_symtab_entry);
   //printf("   Found symbol %s at %08x\n", symbol_name, value);
   if (map_get(symtab, symbol_name, (void**)&entry) == 0)
   {
      entry = malloc(sizeof(symtab_entry_t));
      entry->value = value;
      entry->bindings = NULL;
      map_put(symtab, symbol_name, entry);
   }
   else
   {
      entry->value = value;
      while (entry->bindings)
      {
         entry_binding_t* b = entry->bindings;
         bind_symbol(b->target, value);
         entry->bindings = b->next;
         free(b);
      }
   }
}

void need_symbol(char* symbol_name, uint32_t target)
{
   if (symtab == NULL)
      symtab = alloc_map(free_symtab_entry);
   symtab_entry_t* entry;
   if (map_get(symtab, symbol_name, (void**)&entry) == 0)
   {
      //printf("   Need to find symbol %s to fill in stub at %08x\n", symbol_name, target);
      entry = malloc(sizeof(symtab_entry_t));
      entry->value = 0;
      entry->bindings = malloc(sizeof(entry_binding_t));
      entry->bindings->next = NULL;
      entry->bindings->target = target;
      map_put(symtab, symbol_name, entry);
   }
   else
   {
      //printf("  Request for symbol %s to fill in stub at %08x ---> We already have this symbol! %08x\n", symbol_name, target, entry->value);
      bind_symbol(target, entry->value);
   }
}

uint32_t undefined_count;

void check_entry(char* key, void* entry)
{
   if (((symtab_entry_t*)entry)->value == 0)
   {
      undefined_count++;
      printf("Undefined symbol: %s\n", key);
   }
   else
   {
      //printf("Symbol %s bound to address %08x\n", key, ((symtab_entry_t*)entry)->value);
   }
}

void dump_symtab()
{
   printf("Symtab has %d entries in it\n", map_size(symtab));
   undefined_count = 0;
   forall(symtab, check_entry);
   if (undefined_count > 0)
   {
      printf("There were %d undefined symbols detected\n", undefined_count);
      exit(-3);
   }
}
