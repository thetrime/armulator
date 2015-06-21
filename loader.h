#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/nlist.h>

typedef struct
{
   char* symbol_name;
   uint32_t (*handler)();
} breakpoint_t;

breakpoint_t* find_breakpoint(uint32_t pc);
void load_executable(char* filename);
void parse_executable(unsigned char* data, char* filename);

struct stub_t
{
   struct stub_t* next;
   char* stub_name;
   uint32_t (*_stub)();
};

typedef struct stub_t stub_t;

void register_stub(char* stub_name, uint32_t(*_stub)());
void prepare_loader();

#define VPAGE_SIZE 4096
