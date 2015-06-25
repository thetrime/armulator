#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "coprocessor.h"

//                    VVVV  These comments are read vertically: CRn, opc1, CRm, opc2
//                     o o
//                    CpCp
//                    RcRc
//                    n1m2
#define MIDR        0x0000
#define CTR         0x0001
#define TCMTR       0x0002
#define TLBTR       0x0003
#define MPIDR       0x0005
#define DACR        0x3000
#define CP15WFI     0x7004
#define CP15ISB     0x7054
#define CDSR        0x70A6
#define TCMSR       0x9020
#define CBOR        0x9080
#define PRRR        0xA020
#define NMRR        0xA021
#define ISR         0xC010
#define FCSEIDR     0xD000
#define TPIDRURO    0xD003
#define CONTEXTIDR  0xD001

typedef struct
{
   uint32_t value;
} opc2_t;

typedef struct
{
   opc2_t* opc2[8];
} crm_t;

typedef struct
{
   crm_t* crm[15];
} opc1_t;

typedef struct
{
   opc1_t* opc1[8];
} crn_t;

crn_t* crn[16];

int cp15_accept(uint32_t anything)
{
   return 1; // FIXME: Maybe should be more selective than this
}

uint32_t cp15_read(uint8_t size, uint8_t CRn, uint8_t opc1, uint8_t CRm, uint8_t opc2)
{
   crn_t* p1 = crn[CRn];
   assert(p1 != NULL);
   opc1_t* p2 = p1->opc1[opc1];
   assert(p2 != NULL);
   crm_t* p3 = p2->crm[CRm];
   assert(p3 != NULL);
   opc2_t* p4 = p3->opc2[opc2];
   assert(p4 != NULL);
   return p4->value;
}

void create_register(uint16_t path, uint32_t value)
{
   if (crn[path >> 12] == NULL)
      crn[path >> 12] = malloc(sizeof(crn_t));
   if (crn[path >> 12]->opc1[(path >> 8) & 0xf] == NULL)
      crn[path >> 12]->opc1[(path >> 8) & 0xf] = malloc(sizeof(opc1_t));
   if (crn[path >> 12]->opc1[(path >> 8) & 0xf]->crm[(path >> 4) & 0xf] == NULL)
      crn[path >> 12]->opc1[(path >> 8) & 0xf]->crm[(path >> 4) & 0xf] = malloc(sizeof(crm_t));
   if (crn[path >> 12]->opc1[(path >> 8) & 0xf]->crm[(path >> 4) & 0xf]->opc2[path & 0xf] == NULL)
      crn[path >> 12]->opc1[(path >> 8) & 0xf]->crm[(path >> 4) & 0xf]->opc2[path & 0xf] = malloc(sizeof(opc2_t));
   crn[path >> 12]->opc1[(path >> 8) & 0xf]->crm[(path >> 4) & 0xf]->opc2[path & 0xf]->value = value;
}

void alias_register(uint16_t alias, uint32_t actual)
{
   // FIXME: Does not actually check this register exists
   opc2_t* actual_node = crn[actual >> 12]->opc1[(actual >> 8) & 0xf]->crm[(actual >> 4) & 0xf]->opc2[actual & 0xf];
   
   if (crn[alias >> 12] == NULL)
      crn[alias >> 12] = malloc(sizeof(crn_t));
   if (crn[alias >> 12]->opc1[(alias >> 8) & 0xf] == NULL)
      crn[alias >> 12]->opc1[(alias >> 8) & 0xf] = malloc(sizeof(opc1_t));
   if (crn[alias >> 12]->opc1[(alias >> 8) & 0xf]->crm[(alias >> 4) & 0xf] == NULL)
      crn[alias >> 12]->opc1[(alias >> 8) & 0xf]->crm[(alias >> 4) & 0xf] = malloc(sizeof(crm_t));
   crn[alias >> 12]->opc1[(alias >> 8) & 0xf]->crm[(alias >> 4) & 0xf]->opc2[alias & 0xf] = actual_node;
}

void cp15_init()
{
   memset(crn, sizeof(crn), 0);

   create_register(TPIDRURO,   0x80000000); // Initial value is actually undefined... but the OS may set it?
   create_coprocessor(15, cp15_read, cp15_accept);

   
   /* Actually this is ARMv6 T-T   
   create_register(MIDR,   0x0000);
   create_register(CTR,    0x0000);
   create_register(TCMTR,  0x0000);
   create_register(TLBTR,  0x0000);
   create_register(MDIPR,  0x0000);
   alias_register (0x0004, MIDR);
   alias_register (0x0006, MIDR);
   alias_register (0x0007, MIDR);
   // CPUID registers - there are 64 of these?!
   for (int i = 0x0010; i < 0x0080; i+=0x0010)
      for (int j = 0; j < 8; j++)
         create_register(i|j, 0x0000);   
   // System control registers have no name
   create_register(0x01000, 0x0000);
   create_register(0x01001, 0x0000);
   create_register(0x01002, 0x0000);
   // Security extensions registers have no names either
   create_register(0x01010, 0x0000);
   create_register(0x01011, 0x0000);
   create_register(0x01012, 0x0000);
   // Translation table base registers
   create_register(0x01010, 0x0000);
   create_register(0x01011, 0x0000);
   create_register(0x01012, 0x0000);
   create_register(DACR,    0x0000);
   // Fault status
   create_register(0x5000,  0x0000);
   create_register(0x5001,  0x0000);
   // Fault address
   create_register(0x6000,  0x0000);
   create_register(0x6001,  0x0000);
   create_register(0x6002,  0x0000);
   create_register(CP15WFI, 0x0000);
   // Cache and branch predictor maintenance
   create_register(0x7050,  0x0000);
   create_register(0x7051,  0x0000);
   create_register(0x7052,  0x0000);
   create_register(0x7056,  0x0000);
   create_register(0x7057,  0x0000);
   create_register(CP15ISB, 0x0000);
   // Cache maintenance
   create_register(0x7060,  0x0000);
   create_register(0x7061,  0x0000);
   create_register(0x7062,  0x0000);
   create_register(0x7070,  0x0000);
   create_register(0x7071,  0x0000);
   create_register(0x7072,  0x0000);
   create_register(0x70A0,  0x0000);
   create_register(0x70A1,  0x0000);
   create_register(0x70A2,  0x0000);
   create_register(0x70A3,  0x0000);
   // Data barrier
   create_register(0x70A4,  0x0000);
   create_register(0x70A5,  0x0000);
   create_register(CDSR,    0x0000);
   // More cache maintenance
   create_register(0x70B0,  0x0000);
   create_register(0x70B1,  0x0000);
   create_register(0x70B2,  0x0000);
   // Block transfer support
   create_register(0x70C4,  0x0000);
   create_register(0x70C5,  0x0000);
   // Cache prefetch
   create_register(0x70D1,  0x0000);
   // Yet More cache maintenance
   create_register(0x70D0,  0x0000);
   create_register(0x70D1,  0x0000);
   create_register(0x70D2,  0x0000);
   create_register(0x70D3,  0x0000);
   // TLB maintenance
   create_register(0x8050,  0x0000);
   create_register(0x8051,  0x0000);
   create_register(0x8052,  0x0000);
   create_register(0x8060,  0x0000);
   create_register(0x8061,  0x0000);
   create_register(0x8062,  0x0000);
   create_register(0x8070,  0x0000);
   create_register(0x8071,  0x0000);
   create_register(0x8072,  0x0000);
   // Cache lockdown C and TCM region
   create_register(0x9000,  0x0000);
   create_register(0x9001,  0x0000);
   create_register(0x9010,  0x0000);
   create_register(0x9011,  0x0000);
   // TCM non-secure access control
   create_register(0x9012,  0x0000);
   create_register(0x9013,  0x0000);
   create_register(TCMSR,   0x0000);
   // Cache lockdown D
   create_register(0x9050,  0x0000);
   create_register(0x9051,  0x0000);
   create_register(0x9060,  0x0000);
   create_register(0x9061,  0x0000);
   create_register(CBOR,    0x0000);
   // TLB lockdown registers
   create_register(0xA000,  0x0000);
   create_register(0xA001,  0x0000);
   create_register(PRRR,    0x0000);
   create_register(NMRR,    0x0000);
   // TLB lockdown operations and operations
   create_register(0xA040,  0x0000);
   create_register(0xA041,  0x0000);
   create_register(0xA080,  0x0000);
   create_register(0xA081,  0x0000);
   // DMA for TCM
   for (int i = 0x0000; i < 0x0800; i+= 0x0100)
      for (int j = 0x0000; j < 0x0090; j += 0x0010)
         for (int k = 0x0000; k < 0x0008; k+= 0x0001)
            create_register(i|j|k|0xB000, 0x0000);
   // Also one more block where j = 0x00F0
   for (int i = 0x0000; i < 0x0800; i+= 0x0100)
      for (int k = 0x0000; k < 0x0008; k+= 0x0001)
         create_register(i|k|0xB0F0, 0x0000);
   // Security extensions
   create_register(0xC000,  0x0000);
   create_register(0xC001,  0x0000);
   create_register(ISR,     0x0000);
   create_register(FCSEIDR, 0x0000);
   create_register(CONTEXTIDR, 0x0000);
   // Software thread ID registers
   create_register(0xD002,  0x0000);
   create_register(0xD003,  0x0000);
   create_register(0xD004,  0x0000);
   // Implementation defined
   for (int i = 0x0000; i < 0x0800; i+= 0x0100)
      for (int j = 0x0000; j <= 0x00F0; j += 0x0010)
         for (int k = 0x0000; k < 0x0008; k+= 0x0001)
            create_register(i|j|k|0xF000, 0x0000);   
   */
}
