#include <stdint.h>

int coproc_accept(uint8_t coprocessor, uint32_t instruction);
uint32_t coproc_read(uint8_t size, uint8_t coprocessor, uint8_t CRn, uint8_t opc1, uint8_t CRm, uint8_t opc2);
void configure_coprocessors();

void create_coprocessor(uint8_t id, uint32_t (*read)(uint8_t size, uint8_t CRn, uint8_t opc1, uint8_t CRm, uint8_t opc2), int (*accept)(uint32_t instruction));
