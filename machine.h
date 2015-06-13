#include <stdint.h>
void map_memory(unsigned char* data, uint32_t address, uint32_t length);
void write_mem(uint8_t count, uint32_t addr, uint32_t value);
uint32_t alloc_page();
