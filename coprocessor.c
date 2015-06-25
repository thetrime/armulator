#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cp15.h"


typedef struct
{
   uint32_t (*read)(uint8_t size, uint8_t CRn, uint8_t opc1, uint8_t CRm, uint8_t opc2);
   int (*accept)(uint32_t instruction);
} coprocessor_t;

coprocessor_t* coprocessors[16];

uint32_t coproc_read(uint8_t size, uint8_t coprocessor, uint8_t CRn, uint8_t opc1, uint8_t CRm, uint8_t opc2)
{
   coprocessor_t* cp = coprocessors[coprocessor];
   if (cp == NULL)
      assert(0 && "No such coprocessor");
   return cp->read(size, CRn, opc1, CRm, opc2);
}

int coproc_accept(uint8_t coprocessor, uint32_t instruction)
{
    coprocessor_t* cp = coprocessors[coprocessor];
   if (cp == NULL)
      assert(0 && "No such coprocessor");
   return cp->accept(instruction);
}

void create_coprocessor(uint8_t id,
                        uint32_t (*read)(uint8_t size, uint8_t CRn, uint8_t opc1, uint8_t CRm, uint8_t opc2),
                        int (*accept)(uint32_t instruction))
{
   coprocessors[id] = malloc(sizeof(coprocessor_t));
   coprocessors[id]->read = read;
   coprocessors[id]->accept = accept;
   
}

void configure_coprocessors()
{
   memset(coprocessors, 0, sizeof(coprocessors));
   // Create CP15. See page D12-2526
   cp15_init();
}
