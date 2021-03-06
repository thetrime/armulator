#include "loader.h"
#include "machine.h"
#include "map.h"
#include "symtab.h"
#include "dyld_cache.h"
#include "function_map.h"
#include <unistd.h>

//#define printf(...) (void)0

/* Problem:
   
   I would like to be able to call arbitrary OSX-hosted libraries, but the pointers are in the wrong address space.
   For example, suppose the ARM code calls pthread_mutex_init(&foo)
   The value passed as an argument might be 0x3000, which the VM maps to host-space 0x5000000. 
   Sure, we could map the argument (if we knew it was a pointer, which we may not), but what if it actually
   does something like this:
   pthread_mutex_init(foo*) { foo->self = foo; }
   Then if we attempt to access foo->self in ARM-land again, we are going to get 0x500000 and not 0x3000!
   This will obviously be a problem for anything which returns a linked list...

   One possible solution is to make read_mem and write_mem a bit more clever. If we can guarantee that the address
   spaces are indepdenent (that is, the ARM space does not overlap with the Intel space), then we could have
   read_mem and write_mem translate out-of-scope addresses directly. So, 0x500000 will be mapped to 0x50000.
   However, that only solves the problem in one direction; if we pass a linked list TO an OSX function, we are going
   to have a bad time.

   The only guaranteed way to solve these problems is to either:
       1) Implement the function myself, possibly calling the underlying function for gathering the logic
       2) Use the ARM version of the library. Eventually I will have to implement the kernel functions myself, but the 
          address space is separate there anyway
   
 */



#define BIND_DONE 0x0
#define BIND_SET_DYLIB_ORDINAL_IMM 0x1
#define BIND_SET_DYLIB_ORDINAL_ULEB 0x2
#define BIND_SET_DYLIB_SPECIAL_IMM 0x3
#define BIND_SET_SYMBOL_TRAILING_FLAGS_IMM 0x4
#define BIND_SET_TYPE_IMM 0x5
#define BIND_SET_ADDEND_SLEB 0x6
#define BIND_SET_SEGMENT_AND_OFFSET_ULEB 0x7
#define BIND_ADD_ADDR_ULEB 0x8
#define BIND_DO_BIND 0x9
#define BIND_DO_BIND_ADD_ADDR_ULEB 0xA
#define BIND_DO_BIND_ADD_ADDR_IMM_SCALED 0xB
#define BIND_DO_BIND_ULEB_TIMES_SKIPPING_ULEB 0xC


#define BREAK32 0xe1200070
#define BREAK16 0x00be

int32_t read_sleb_integer(unsigned char** p)
{
   (*p)++;
   int32_t result = 0;
   uint8_t shift = 0;
   uint64_t x = 0;
   while(1)
   {
      x = **p;
      result |= ((x & 127) << shift);
      if (!(x & 128))
         break;
      shift += 7;
      (*p)++;
   }
   if ((shift < 32) && ((x & 64)))
      result |= -(1 << shift);
   return result;

}

uint64_t read_uleb_integer(unsigned char** p)
{
   (*p)++;
   uint64_t result = 0;
   uint8_t shift = 0;
   while(1)
   {
      uint64_t x = **p;
      result |= (uint64_t)((x & 127) << shift);
      if (!(x & 128))
         break;
      shift+=7;
      (*p)++;
   }
   return result;
}


struct segment_list_t
{
   int segment_number;
   uint32_t base_address;
   struct segment_list_t* next;   
};
typedef struct segment_list_t segment_list_t;

struct section_list_t
{
   int section_number;
   uint32_t base_address;
   uint32_t flags;
   uint32_t size;
   struct section_list_t* next;   
};
typedef struct section_list_t section_list_t;


typedef struct
{
   char* mode;
   uint32_t library_ordinal;
   uint8_t type;
   uint64_t offset;
   uint8_t segment;
   int32_t addend;
   char* name;
} sym_t;


uint32_t next_break = 0xa0000000;
map_t* breakpoints = NULL;

void register_stub(char* stub_name, uint32_t(_stub)())
{
   // Make a breakpoint. Not sure where to put this, so lets just say we start at 0xa0000000?
   if (next_break % VPAGE_SIZE == 0)
   {
      unsigned char* page = malloc(VPAGE_SIZE);
      map_memory(page, next_break, VPAGE_SIZE);
   }
   write_mem(4, next_break, BREAK32);
   found_symbol(stub_name, next_break);
   breakpoint_t* breakpoint = malloc(sizeof(breakpoint_t));
   breakpoint->symbol_name = strdup(stub_name);
   breakpoint->handler = _stub;
   uint32_t* key = malloc(sizeof(uint32_t));
   *key = next_break;
   map_put(breakpoints, key, breakpoint);
   next_break += 4;
}


uint32_t current_page_offset = 0;

void bind_sym(char* current_file, segment_list_t* segment_list, sym_t* sym)
{
   uint32_t address = 0;
   for (segment_list_t* node = segment_list; node; node = node->next)
   {
      if (node->segment_number == sym->segment)
      {
         address = sym->offset + node->base_address;
         break;
      }
   }
   need_symbol(sym->name, address);
}

void bind_symbols(char* current_file, segment_list_t* segment_list, char* mode, unsigned char* start, unsigned char* end)
{
   unsigned char* op;
   sym_t sym;
   sym.name = NULL;
   sym.mode = mode;
   for(op = start; op < end; op++)
   {
      switch(((*op) >> 4) & 15)
      {
         case BIND_DONE:                    
            break;
         case BIND_SET_DYLIB_ORDINAL_IMM:
            sym.library_ordinal = (*op) & 15;
            break;
         case BIND_SET_DYLIB_ORDINAL_ULEB:
            sym.library_ordinal = read_uleb_integer(&op);
            break;
         case BIND_SET_DYLIB_SPECIAL_IMM:
            if ((*op & 15) == 0)
               sym.library_ordinal = 0;
            else
               sym.library_ordinal = 0xf0 | (*op & 15);
            break;
         case BIND_SET_SYMBOL_TRAILING_FLAGS_IMM:
         {
            // flags are in the low nibble
            op++;
            if (sym.name)
               free(sym.name);
            sym.name = strdup((const char*)op);
            op += strlen(sym.name);
            break;
         }
         case BIND_SET_TYPE_IMM:
            sym.type = (*op) & 15;
            break;
         case BIND_SET_SEGMENT_AND_OFFSET_ULEB:
            sym.segment = (*op) & 15;
            sym.offset = read_uleb_integer(&op);
            break;
         case BIND_SET_ADDEND_SLEB:
            sym.addend = read_sleb_integer(&op);
            break;
         case BIND_DO_BIND:
            bind_sym(current_file, segment_list, &sym);
            sym.offset += 4;
            break;
         case BIND_DO_BIND_ADD_ADDR_ULEB:
            bind_sym(current_file, segment_list, &sym);
            sym.offset += 4 + read_uleb_integer(&op);
            break;
         case BIND_DO_BIND_ADD_ADDR_IMM_SCALED:
            bind_sym(current_file, segment_list, &sym);
            sym.offset += 4 + (4 * ((*op) & 15));
            break;                     
         case BIND_ADD_ADDR_ULEB:
         {
            uint64_t offset = read_uleb_integer(&op);
            sym.offset += offset;            
            break;
         }
         case BIND_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
         {
            uint32_t count = read_uleb_integer(&op);
            uint32_t skip = read_uleb_integer(&op);
            for (int j = 0; j < count; j++)
            {
               bind_sym(current_file, segment_list, &sym);
               sym.offset += 4 + skip;
            }
         }
         break;
         default:
            printf("Opcode not implemented: %d\n", ((*op) >> 4));
            assert(0);                     
      }
   }
}

char* sim_chroot = "armv7_5";

char* find_dylib(char* base, char* suggested_path)
{
   char* filename = malloc(strlen(suggested_path) + strlen(sim_chroot) + 2);
   sprintf(filename, "%s%s", sim_chroot, suggested_path);
   if (access(filename, F_OK ) != -1)
      return filename;
   printf("Failed to find file at %s\n", filename);
   free(filename);
   return NULL;
}

struct loaded_dylib_t
{
   struct loaded_dylib_t* next;
   char* dylib_name;
};

typedef struct loaded_dylib_t loaded_dylib_t;
loaded_dylib_t* loaded_dylibs = NULL;

int load_dylib(char* base, char* filename)
{
   // First mark it as loaded to avoid cycles
   loaded_dylib_t* x = malloc(sizeof(loaded_dylib_t));
   x->dylib_name = strdup(filename);
   x->next = loaded_dylibs;
   loaded_dylibs = x;
   
   if (try_cache(filename))
      return 1;
   
   char* dylib_name = find_dylib(base, filename);
   if (dylib_name != NULL)
   {
      printf("Found at %s\n", dylib_name);
      load_executable(dylib_name);
      free(dylib_name);
      return 1;
   }
   else
   {
      printf("Failed to find dylib\n");
      return 0;
   }
}


#define byteswap32(x) ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) | (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))

void free_segment_list(segment_list_t* segment_list)
{
   for(segment_list_t* x = segment_list; x;)
   {
      segment_list_t* y = x->next;
      free(x);
      x = y;
   }
}

void free_section_list(section_list_t* section_list)
{
   for(section_list_t* x = section_list; x;)
   {
      section_list_t* y = x->next;
      free(x);
      x = y;
   }
}

void parse_export_tree(unsigned char* trie_root, unsigned char* trie, unsigned char* trie_end, unsigned char* buffer, unsigned char* root_buffer, uint64_t base_address)
{
   assert(trie < trie_end);
   uint8_t terminal_size = *trie;
   if (terminal_size > 127)
      assert(0);
   if (terminal_size != 0)
   {
      // This is a terminal
      uint64_t flags = read_uleb_integer(&trie);
      printf("Terminal: >%s< flags %016llx\n", root_buffer, flags);
      if ((flags & EXPORT_SYMBOL_FLAGS_REEXPORT) == EXPORT_SYMBOL_FLAGS_REEXPORT)
      {
         // FIXME: Not implemented
         return;
      }
      uint64_t address = read_uleb_integer(&trie);
      if ((flags & EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER) == EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER)
      {
         // Finally, we know that this symbol needs to have its resolver run
         uint64_t resolver = read_uleb_integer(&trie);
         printf("      Symbol requires a resolver to be run at %016llx, or call the stub at %016llx\n", resolver + base_address, address + base_address);
         // Run resolver here and advise that we have found a symbol by calling found_symbol.
         uint32_t resolved = execute_function(resolver+base_address);
         found_symbol((char*)root_buffer, resolved);
      }
      else
      {
         // advise that we have found a symbol by calling found_symbol.
         found_symbol((char*)root_buffer, address+base_address);
      }
   }
   else
   {
      trie++;
      uint8_t child_count = *trie++;
      unsigned char* tmp = buffer;
      for (int i = 0; i < child_count; i++)
      {
         buffer = tmp;
         while(*trie)
         {
            *buffer++ = *trie++;
         }
         *buffer = *trie;
         uint64_t offset = read_uleb_integer(&trie);
         trie++;
         assert(offset != 0);
         parse_export_tree(trie_root, trie_root + offset, trie_end, buffer, root_buffer, base_address);
      }      
   }
}

void parse_executable(unsigned char* data, uint32_t offset, char* filename)
{
   struct nlist* symbol_table = NULL;
   char* string_table = NULL;
   uint32_t* indirect_table = NULL;
   uint32_t indirect_count = 0;

   segment_list_t* segment_list = NULL;
   section_list_t* section_list = NULL;   
   struct load_command* command;
   struct mach_header* header;
   header = (struct mach_header*)data;
   uint64_t text_segment = 0;
      
   assert(header->magic == MH_MAGIC);
   command = (struct load_command*)(data + sizeof(struct mach_header));
   uint32_t initial_pc = 0;
   int segment_number = 0;
   int section_number = 1;
   
   for (int i = 0; i < header->ncmds; i++)
   {
      switch(command->cmd)
      {
         case LC_SEGMENT:
         {            
            struct segment_command* c = (struct segment_command*)command;
            printf("Got segment: %s (with %d sections) mapped to %08x (file offset is %08x)\n", c->segname, c->nsects, c->vmaddr, c->fileoff);
            if (strcmp(c->segname, "__TEXT") == 0)
               text_segment = c->vmaddr;
            for (int j = 0; j < c->nsects; j++)
            {
               unsigned char* chunk = NULL;
               struct section* s = (struct section*)(((char*)command) + sizeof(struct segment_command) + j*sizeof(struct section));
               printf("   Got section #%d (%s) in segment %s (mapped to %08x), file location %08x\n", section_number, s->sectname, c->segname, s->addr, s->offset);
               if ((strcmp(c->segname, "__TEXT") == 0) && (strcmp(s->sectname, "__text") == 0))
               {
                  initial_pc = s->addr;                  
               }
               chunk = calloc(s->size, 1);
               if (!((s->flags & 0xff) == S_ZEROFILL))
               {
                  uint64_t file_base = 0;
                  // I do not understand this; some of the sections have fileoff relative to the start of the image, but some are absolute from the start of the file
                  // there seems to be no flags indicating which is which
                  if (c->fileoff < offset)
                     file_base = s->offset;
                  else
                     file_base = s->offset - offset;
                  // Print the message with the position relative to the start of the file, for the sake of sanity
                  printf("Mapping data from absolute file location %016llx to memory address %08x\n", file_base + offset, s->addr);
                  memcpy(chunk, &data[file_base], s->size);
               }
               map_memory(chunk, s->addr, s->size);
               section_list_t* section = malloc(sizeof(section_list_t));
               section->next = section_list;
               section->base_address = s->addr;
               section->section_number = section_number;
               section->flags = s->flags;
               section->size = s->size;
               section_number++;
               section_list = section;
               if (s->reserved1 != 0)
                  printf("Section %s has reserved1: %08x and base %08x\n", s->sectname, s->reserved1, s->addr);               
            }
            segment_list_t* node = malloc(sizeof(segment_list_t));
            node->next = segment_list;
            node->segment_number = segment_number;
            node->base_address = c->vmaddr;
            segment_number++;
            segment_list = node;
            break;
         }
         case LC_SYMTAB:
         {
            struct symtab_command* c = (struct symtab_command*)command;
            printf("Got symtab containing %d symbols\n", c->nsyms);
            symbol_table = (struct nlist*)(&data[c->symoff - offset]);
            string_table = (char*)&data[c->stroff - offset];
#ifdef WITH_FUNCTION_LABELS
            for (int j = 0; j < c->nsyms; j++)
            {
               struct nlist* index_ptr = &symbol_table[j];
               if ((index_ptr->n_type & N_TYPE) == N_SECT)
               {
                  found_function(filename, &data[c->stroff + index_ptr->n_un.n_strx - offset], index_ptr->n_value);
                  printf("%s provides symbol %s at address %08x with type %02x and attributes %04x section is #%d\n", filename, &data[c->stroff + index_ptr->n_un.n_strx - offset], index_ptr->n_value, index_ptr->n_type, index_ptr->n_desc, index_ptr->n_sect);
               }
            }
#endif
            break;
         }
         case LC_DYSYMTAB:
         {
            struct dysymtab_command* c = (struct dysymtab_command*)command;
            printf("Got dysymtab containing %d indirect symbols, %d undefined symbols, %d local symbols\n", c->nindirectsyms, c->nundefsym, c->nlocalsym);
            indirect_table = (uint32_t*)&data[c->indirectsymoff - offset];
            indirect_count = c->nindirectsyms;
            break;
         }
         case LC_DYLD_INFO_ONLY:
         {
            struct dyld_info_command* c = (struct dyld_info_command*)command;
            unsigned char tmp_buffer[4096];
            printf("Exports from %s start at 0x%x and are %d long\n", filename, c->export_off, c->export_size);
            tmp_buffer[0] = 0;
            parse_export_tree(&data[c->export_off - offset], &data[c->export_off - offset], &data[c->export_off + c->export_size - offset], (unsigned char*)&tmp_buffer, (unsigned char*)&tmp_buffer, text_segment);
            printf("Binding symbols from %s (%d bytes of binding opcodes)\n", filename, c->bind_size);
            bind_symbols(filename, segment_list, "external", &data[c->bind_off - offset], &data[c->bind_off + c->bind_size - offset]);
            printf("Binding lazy symbols from  %s (%d bytes of binding opcodes)\n", filename, c->lazy_bind_size);
            bind_symbols(filename, segment_list, "lazy", &data[c->lazy_bind_off - offset], &data[c->lazy_bind_off + c->lazy_bind_size - offset]);            
            
            break;
         }
         case LC_ID_DYLIB:
         {
            struct dylib_command* c = (struct dylib_command*)command;            
            printf("This dylib is %s (compatibility version %d.%d.%d, current version %d.%d.%d)\n", ((unsigned char*)command) + c->dylib.name.offset, (c->dylib.compatibility_version >> 16) & 0xffff, (c->dylib.compatibility_version >> 8) & 0xff, (c->dylib.compatibility_version >> 0) & 0xff, (c->dylib.current_version >> 16) & 0xffff, (c->dylib.current_version >> 8) & 0xff, (c->dylib.current_version >> 0) & 0xff);
            break;
         }
         case LC_CODE_SIGNATURE:
            printf("Code signature. Ignored\n");
            break;
         case LC_UNIXTHREAD:
         {
            uint32_t* base = ((uint32_t*)((char*)command + sizeof(struct thread_command)));
            //uint32_t flavor = base[0];
            uint32_t count = base[1];
            assert(count == 17); // Ordinary threadstate
            state.r[0] = base[2];
            state.r[1] = base[3];
            state.r[2] = base[4];
            state.r[3] = base[5];
            state.r[4] = base[6];
            state.r[5] = base[7];
            state.r[6] = base[8];
            state.r[7] = base[9];
            state.r[8] = base[10];
            state.r[9] = base[11];
            state.r[10] = base[12];
            state.r[11] = base[13];
            state.r[12] = base[14];
            //state.r[13] = base[15];   // Do not destroy the stack!
            state.r[14] = base[16];
            state.r[15] = base[17];
            //state.cspr = base[??];
            break;
         }
         case LC_THREAD:
            printf("Thread state given. Not implemented\n");
            break;
         case LC_REEXPORT_DYLIB: // Fallthrough
            printf("Reexporting....\n");
         case LC_LOAD_DYLIB:
         {
            struct dylib_command* c = (struct dylib_command*)command;
            uint8_t dylib_already_loaded = 0;
            for (loaded_dylib_t* x = loaded_dylibs; x; x = x->next)
            {
               if (strcmp(x->dylib_name, ((char*)command) + c->dylib.name.offset) == 0)
               {
                  dylib_already_loaded = 1;
                  break;
               }
            }
            if (!dylib_already_loaded)
            {
               printf("Load dylib %s (compatibility version %d.%d.%d, current version %d.%d.%d)\n", ((unsigned char*)command) + c->dylib.name.offset, (c->dylib.compatibility_version >> 16) & 0xffff, (c->dylib.compatibility_version >> 8) & 0xff, (c->dylib.compatibility_version >> 0) & 0xff, (c->dylib.current_version >> 16) & 0xffff, (c->dylib.current_version >> 8) & 0xff, (c->dylib.current_version >> 0) & 0xff);
               assert(load_dylib(filename, ((char*)command) + c->dylib.name.offset));
            }
            break;
         }
         case LC_LOAD_DYLINKER:
         {
            struct dylinker_command* c = (struct dylinker_command*)command;
            printf("Executable has requested dylinker %s\n", ((unsigned char*)command) + c->name.offset);
            break;
         }
         case LC_UUID:
         {
            struct uuid_command* c = (struct uuid_command*)command;
            printf("UUID is ");
            for (int j = 0; j < 16; j++)
            {
               printf("%02X", c->uuid[j]);
               if (j == 3 || j == 5 || j == 7 || j == 9)
                  printf("-");
            }
            printf("\n");
            break;
         }
         case LC_FUNCTION_STARTS:
         {
            printf("Function start table. Ignored\n");
            break;
         }
         case LC_VERSION_MIN_IPHONEOS:
         {
            struct version_min_command* c = (struct version_min_command*)command;
            printf("Executable requires version %d.%d.%d or greater, and was compiled with SDK %d.%d.%d\n", (c->version >> 16) & 0xffff, (c->version >> 8) & 0xff, (c->version >> 0) & 0xff, (c->sdk >> 16) & 0xffff, (c->sdk >> 8) & 0xff, (c->sdk >> 0) & 0xff);
            break;
         }
         default:
            printf("Got other type 0x%x\n", command->cmd);
      }
      command = (struct load_command*)((char*)command + command->cmdsize);
   }

   // Ok, all loaded. Only now can we process the indirect symbols!
   printf("Resolving indirect symbols for %s\n", filename);
   for (section_list_t* section = section_list; section; section = section->next)
   {
      if (section->flags == S_LAZY_SYMBOL_POINTERS)
      {
         assert(indirect_table != NULL);             // Must have a dsymtab or we will not have a good time
         assert(symbol_table != NULL);               // Must have a symtab or we will not have a good time either
         uint32_t indirect_symbols_this_section = section->size / sizeof(uint32_t);
         for (int j = 0; j < indirect_symbols_this_section; j++)
         {            
            if (indirect_table[j] == INDIRECT_SYMBOL_ABS)
               continue;
            if (indirect_table[j] == (INDIRECT_SYMBOL_ABS | INDIRECT_SYMBOL_LOCAL))
               continue;
            if (indirect_table[j] == INDIRECT_SYMBOL_LOCAL)
               continue;         
            struct nlist* ptr = &symbol_table[indirect_table[j]];
            printf("%s needs symbol %s at %08lx\n", filename, &string_table[ptr->n_un.n_strx], section->base_address + (sizeof(uint32_t) * j));
            need_symbol(&string_table[ptr->n_un.n_strx], section->base_address + (sizeof(uint32_t) * j));
         }
      }
      if (section->flags == S_MOD_INIT_FUNC_POINTERS)
      {
         // This section contains just pointers to bits of code we have to execute to initialize it
         uint32_t initializers_this_section = section->size / sizeof(uint32_t);
         for (int j = 0; j < initializers_this_section; j++)
         {
            printf("Running initializer at %08x in binary %s\n", section->base_address + 4*j, filename);
            uint32_t address = read_mem(4, section->base_address + 4*j);
            _execute_function(1, read_mem(4, section->base_address + 4*j));
         }
      }
   }
   free_segment_list(segment_list);
   free_section_list(section_list);  
}


void load_executable(char* filename)
{
   unsigned char* data;
   FILE* file = NULL;
   size_t file_length;
   struct mach_header* header;
   uint32_t base = 0;

   if (try_cache(filename))
     return; // try_cache will load it for us
   
   file = fopen(filename, "rb");
   assert(file);
   fseek(file, 0, SEEK_END);
   file_length = ftell(file);
   fseek(file, 0, SEEK_SET);
   data = malloc(file_length);
   fread(data, file_length, 1, file);
   fclose(file);

   header = (struct mach_header*)data;
   if (header->magic == FAT_CIGAM)
   {
      // Universal binary. Lets find the armv7 one
      struct fat_header* fat = (struct fat_header*)data;
      printf("Universal binary supporting %d different CPU architectures\n", byteswap32(fat->nfat_arch));
      uint32_t count = byteswap32(fat->nfat_arch);
      uint8_t arch_found = 0;
      for (int i = 0; i < count && !arch_found; i++)
      {
         struct fat_arch* arch = (struct fat_arch*)(data + sizeof(struct fat_header) + (i*(sizeof(struct fat_arch))));
         if (arch->cputype == byteswap32(CPU_TYPE_ARM))
         {
            header = (struct mach_header*)&data[byteswap32(arch->offset)];
            base = byteswap32(arch->offset);
            arch_found = 1;
            break;
         }
      }
      assert(arch_found);
   }
   parse_executable(&data[base], 0, filename);
   free(data);
}



breakpoint_t* find_breakpoint(uint32_t pc)
{
   breakpoint_t* breakpoint;
   if (map_get(breakpoints, &pc, (void**)&breakpoint))
   {
      printf("  *** %s\n", breakpoint->symbol_name);
      return breakpoint;
   }
   assert(0 && "Illegal breakpoint");
}

uint32_t hash_uint32(void* x)
{
   return *((uint32_t*)x);
}

int comparator_uint32(void* x, void* y)
{
   return *((uint32_t*)x) - *((uint32_t*)y);
}

void free_breakpoint(void* ptr)
{
   breakpoint_t* breakpoint = (breakpoint_t*)ptr;
   free(breakpoint->symbol_name);
}

void prepare_loader()
{
   breakpoints = alloc_map(free_breakpoint, hash_uint32, comparator_uint32);
   unsigned char* hypervisor_break = malloc(4);
   printf("Mapping memory to 0xfffffff0\n");
   map_memory(hypervisor_break, 0xfffffff0, 4);
   write_mem(4, 0xfffffff0, BREAK32);
   load_dyld_cache("dyld_shared_cache_armv7");

   unsigned char* tls = calloc(2048, 1);
   map_memory(tls, 0x80000000, 2048);
   
}
