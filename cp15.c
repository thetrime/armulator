#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
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
#define REVIDR      0x0006
#define ID_PFR0     0x0010
#define ID_PFR1     0x0011
#define ID_DFR0     0x0012
#define ID_AFR0     0x0013
#define ID_MMFR0    0x0014
#define ID_MMFR1    0x0015
#define ID_MMFR2    0x0016
#define ID_MMFR3    0x0017
#define ID_ISAR0    0x0020
#define ID_ISAR1    0x0021
#define ID_ISAR2    0x0022
#define ID_ISAR3    0x0023
#define ID_ISAR4    0x0023
#define ID_ISAR5    0x0023
#define CCSIDR      0x0100
#define CLIDR       0x0101
#define AIDR        0x0107
#define CSSELR      0x0200
#define VPIDR       0x0400
#define VMPIDR      0x0405

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

#define abort(...) {printf(__VA_ARGS__); assert(0);}

int cp15_accept(uint32_t anything)
{
   return 1; // FIXME: Maybe should be more selective than this
}

uint32_t cp15_read(uint8_t size, uint8_t CRn, uint8_t opc1, uint8_t CRm, uint8_t opc2)
{
   crn_t* p1 = crn[CRn];
   if (p1 == NULL) abort("Coprocessor 15 has no CRn %d when trying to read path %08x\n", CRn, CRn << 24 | opc1 << 16 | CRm << 8 | opc2);
   opc1_t* p2 = p1->opc1[opc1];
   if (p2 == NULL) abort("Coprocessor 15 has no opc1 %d when trying to read path %08x\n", opc1, CRn << 24 | opc1 << 16 | CRm << 8 | opc2);
   crm_t* p3 = p2->crm[CRm];
   if (p3 == NULL) abort("Coprocessor 15 has no CRm %d when trying to read path %08x\n", CRm, CRn << 24 | opc1 << 16 | CRm << 8 | opc2);
   opc2_t* p4 = p3->opc2[opc2];
   if (p4 == NULL) abort("Coprocessor 15 has no opc2 %d when trying to read path %08x\n", opc2, CRn << 24 | opc1 << 16 | CRm << 8 | opc2);
   printf("   Value for %x%x%x%x is %08x\n", CRn, opc1, CRm, opc2, p4->value);
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
   printf("Configured %x%x%x%x to be %08x\n", path>>12, path>>8&0xf, path>>4&0xf, path&0xf, value);
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
   create_register(MIDR,       0x410FC073); // Pretend we are a genuine ARM Cortex A7
   // CTR, TCMTR
   create_register(TLBTR,      0x00000000); // We have a unified TLB
   alias_register (0x0004, MIDR);
   alias_register (0x0006, MIDR);
   alias_register (0x0007, MIDR);


   create_coprocessor(15, cp15_read, cp15_accept);
}
