#include "dyld_cache.h"
#include "loader.h"
#include "map.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

map_t* cache_map;
unsigned char* cache_data;

void load_dyld_cache(char* filename)
{
   cache_map = alloc_char_map(free);
   FILE* file = fopen(filename, "rb");
   size_t file_length;
   
   fseek(file, 0, SEEK_END);
   file_length = ftell(file);
   fseek(file, 0, SEEK_SET);
   cache_data = malloc(file_length);
   fread(cache_data, file_length, 1, file);
   fclose(file);

   struct dyld_cache_header* header = (struct dyld_cache_header*)cache_data;
   assert(strcmp(header->magic, "dyld_v1   armv7") == 0);
   struct dyld_cache_image_info* images = (struct dyld_cache_image_info*)&cache_data[header->imagesOffset];
   struct dyld_cache_mapping_info* map_info = (struct dyld_cache_mapping_info*)&cache_data[header->mappingOffset];
   printf("Cache is located at %p and is 0x%08zx bytes long\n", cache_data, file_length);
   for (int i = 0; i < header->mappingCount; i++)
      printf("Mapping: %016llx-%016llx -> %016llx\n", (&map_info[i])->address, (&map_info[i])->address+(&map_info[i])->size, (&map_info[i])->fileOffset);
   for (int i = 0; i < header->imagesCount; i++)
   {
      struct dyld_cache_image_info* image = &images[i];
      uint64_t* file_offset = NULL;
      for (int j = 0; j < header->mappingCount; j++)
      {
         struct dyld_cache_mapping_info* mapping = &map_info[j];
         if (image->address >= mapping->address && image->address <= mapping->address + mapping->size)
         {
            file_offset = malloc(sizeof(uint64_t));
            *file_offset = mapping->fileOffset + (image->address - mapping->address);
            //*file_offset = mapping->address;
            break;
         }
      }
      assert(file_offset != NULL);
      printf("Found image %s in cache at %016llx -> %016llx\n", &cache_data[image->pathFileOffset], image->address, *file_offset);
      map_put(cache_map, strdup((char*)&cache_data[image->pathFileOffset]), file_offset);
   }
   //free(cache);   Not until much later? Maybe never, since who knows what our executable may end up trying to load in the future
}


int try_cache(char* filename)
{
   uint64_t* address;
   if (map_get(cache_map, filename, (void**)&address))
   {
      printf("--- Cache hit for %s! (%08x)\n", filename, *address);
      parse_executable(&cache_data[*address], *address, filename);
      return 1;      
   }
   printf(" --- Cache miss for %s\n", filename);
   return 0;
}


// Problem: All the values in the structures are relative to the start of the file, not the value of data.
// Need to call parse_executable with 2 values: The base offset (from the start of the FAT archive or the file if no FAT)
//                                            : The data pointer to the file
//
