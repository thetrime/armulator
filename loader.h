#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

typedef struct
{
   char* symbol_name;
   uint32_t (*_stub)();
} breakpoint_t;

breakpoint_t* find_breakpoint(uint32_t pc);
uint32_t load_executable(char* filename);

struct stub_t
{
   struct stub_t* next;
   char* stub_name;
   uint32_t (*_stub)();
};

typedef struct stub_t stub_t;

void register_stub(char* stub_name, uint32_t(*_stub)());

#define VPAGE_SIZE 4096
