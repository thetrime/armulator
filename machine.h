#include <stdint.h>
void map_memory(unsigned char* data, uint32_t address, uint32_t length);
void write_mem(uint8_t count, uint32_t addr, uint32_t value);
uint32_t alloc_page();

typedef struct
{
   uint8_t z, c, n, v, t;
   uint8_t itstate;
   uint32_t next_instruction;
   uint32_t r[16];
} state_t;

extern state_t state;
