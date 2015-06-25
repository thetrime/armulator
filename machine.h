#include <stdint.h>
void map_memory(unsigned char* data, uint32_t address, uint32_t length);
void write_mem(uint8_t count, uint32_t addr, uint32_t value);
uint32_t read_mem(uint8_t count, uint32_t addr);
uint32_t alloc_page();
#define NUMARGS(...)  (sizeof((int[]){__VA_ARGS__})/sizeof(int))
#define execute_function(...)  _execute_function(NUMARGS(__VA_ARGS__), __VA_ARGS__)
// The above lets you call execute_function(...) without passing the number of args specifically - the preprocessor will count them
uint32_t _execute_function(int argc, ...);

typedef struct
{
   uint8_t z, c, n, v, t;
   uint8_t itstate;
   uint32_t next_instruction;
   uint32_t r[16];
} state_t;

extern state_t state;

