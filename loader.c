#include "loader.h"
#include "machine.h"

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

typedef struct segment_list_t segment_list_t;

segment_list_t* segment_list = NULL;

struct breakpage_t
{
   uint32_t page_start;
   uint32_t page_end;
   breakpoint_t* breakpoints[VPAGE_SIZE/4];
   struct breakpage_t* next;
};

stub_t* stubs = NULL;

void register_stub(char* stub_name, uint32_t(_stub)())
{
   stub_t* n = malloc(sizeof(stub_t));
   n->next = stubs;
   stubs = n;
   n->stub_name = strdup(stub_name);
   n->_stub = _stub;
}

uint32_t (*find_stub(char* stub_name))()
{
   for (stub_t* s = stubs; s; s = s->next)
   {
      if (strcmp(stub_name, s->stub_name) == 0)
         return s->_stub;
   }
   return NULL;
}

typedef struct breakpage_t breakpage_t;
breakpage_t* break_pages = NULL;
uint32_t current_page_offset = 0;

void bind_sym(sym_t* sym)
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
   if ((current_page_offset % VPAGE_SIZE) == 0) // If the offset is PAGE_SIZE (or 0) then we need a new page
   {
      current_page_offset = 0;
      breakpage_t* page = malloc(sizeof(breakpage_t));
      page->next = break_pages;
      break_pages = page;
      break_pages->page_start = alloc_page();
      break_pages->page_end = break_pages->page_start + VPAGE_SIZE;
   }
   breakpoint_t* breakpoint = malloc(sizeof(breakpoint_t));
   breakpoint->symbol_name = strdup(sym->name);
   breakpoint->_stub = find_stub(sym->name);
   break_pages->breakpoints[current_page_offset/4] = breakpoint;
   printf("%s bind: %s to (Segment %d, offset %016llx, type %d) %08x\n", sym->mode, sym->name, sym->segment, sym->offset, sym->type, address);
   //printf("Writing BREAK to %08x\n", break_pages->page_start + current_page_offset);
   //printf("Writing %08x to %08x\n", break_pages->page_start + current_page_offset, address);
   // Write the address of the stub code to the bound address
   write_mem(4, address, break_pages->page_start + current_page_offset);
   // and write the actual code for the stub to the address of the stub code
   write_mem(4, break_pages->page_start + current_page_offset, BREAK32);
   current_page_offset += 4;
}

void bind_symbols(char* mode, unsigned char* start, unsigned char* end)
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
            bind_sym(&sym);
            sym.offset += 4;
            break;
         case BIND_DO_BIND_ADD_ADDR_ULEB:
            bind_sym(&sym);
            sym.offset += 4 + read_uleb_integer(&op);
            break;
         case BIND_DO_BIND_ADD_ADDR_IMM_SCALED:
            bind_sym(&sym);
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
               bind_sym(&sym);
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

uint32_t load_executable(char* filename)
{
   unsigned char* data;
   struct load_command* command;
   struct mach_header* header;
   FILE* file = fopen(filename, "rb");
   size_t file_length;
   fseek(file, 0, SEEK_END);
   file_length = ftell(file);
   fseek(file, 0, SEEK_SET);
   data = malloc(file_length);
   fread(data, file_length, 1, file);
   fclose(file);
   header = (struct mach_header*)data;
   assert(header->magic == MH_MAGIC);
   command = (struct load_command*)(data + sizeof(struct mach_header));
   uint32_t initial_pc = 0;
   int segment_number = 0;   
   
   for (int i = 0; i < header->ncmds; i++)
   {
      switch(command->cmd)
      {
         case LC_SEGMENT:
         {            
            struct segment_command* c = (struct segment_command*)command;
            printf("Got segment: %s (with %d sections) mapped to %08x\n", c->segname, c->nsects, c->vmaddr);
            for (int j = 0; j < c->nsects; j++)
            {
               unsigned char* chunk = NULL;
               struct section* s = (struct section*)(((char*)command) + sizeof(struct segment_command) + j*sizeof(struct section));
               printf("   Got section %s in segment %s (mapped to %08x)\n", s->sectname, c->segname, s->addr);
               if ((strcmp(c->segname, "__TEXT") == 0) && (strcmp(s->sectname, "__text") == 0))
               {
                  initial_pc = s->addr;
               }
               chunk = calloc(s->size, 1);
               if (!((s->flags & S_ZEROFILL) == S_ZEROFILL))
                 memcpy(chunk, &data[s->offset], s->size);
               map_memory(chunk, s->addr, s->size);
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
            for (int j = 0; j < c->nsyms; j++)
            {
               struct nlist* index_ptr = (struct nlist*)(&data[c->symoff + j*sizeof(struct nlist)]);
               if (index_ptr->n_type & N_EXT)
               {
                  // External symbol
                  //printf("External Symbol %d (section: %d, type %d): %s\n", index_ptr->n_un.n_strx, index_ptr->n_sect, index_ptr->n_type, &data[c->stroff + index_ptr->n_un.n_strx]);
               }
            }

            break;
         }
         case LC_DYSYMTAB:
         {
            struct dysymtab_command* c = (struct dysymtab_command*)command;
            printf("Got dysymtab containing %d indirect symbols, %d undefined symbols, %d local symbols\n", c->nindirectsyms, c->nundefsym, c->nlocalsym);
            break;
         }
         case LC_DYLD_INFO_ONLY:
         {
            struct dyld_info_command* c = (struct dyld_info_command*)command;
            printf("Binding symbols\n");
            bind_symbols("external", &data[c->bind_off], &data[c->bind_off + c->bind_size]);
            printf("Binding lazy symbols\n");
            bind_symbols("lazy", &data[c->lazy_bind_off], &data[c->lazy_bind_off + c->lazy_bind_size]);
            break;
         }
         case LC_CODE_SIGNATURE:
            printf("Code signature\n");
            break;
         default:
            printf("Got other type %d\n", command->cmd);
      }
      command = (struct load_command*)((char*)command + command->cmdsize);
   }
   return initial_pc;
}

breakpoint_t* find_breakpoint(uint32_t pc)
{
   for (breakpage_t* page = break_pages; page; page = page->next)
   {
      if (page->page_start <= pc && page->page_end >= pc)
      {
         return page->breakpoints[(pc - page->page_start)/4];
      }
   }
   assert(0 && "Illegal breakpoint");
}

