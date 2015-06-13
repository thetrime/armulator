#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "arm.h"
#include "loader.h"
#include "stubs.h"
typedef enum
{
   LDR_I = 0,
   ADD_I,
   ADD_R,
   BIC_I,
   MOV_R,
   CMP_I,
   B,
   BL,
   BLX,
   PUSH,
   ADD_SPI,
   SUB_SPI,
   MOV_I,
   MOVT,
   LDRB_I,
   CBZ,
   CBNZ,
   POP,
   STR_I,
   CMP_R,
   EOR_I,
   TST_I,
   LDR_L,
   BKPT,
   STRB_I,
   IT,
   BX
} opcode_t;

char* opcode_name[] = {"ldr", "add", "add", "bic", "mov", "cmp", "b", "bl", "blx", "push", "add", "sub", "mov", "movt", "ldrb", "cbz", "cbnz", "pop", "str", "cmp", "eor", "tst", "ldr", "bkpt", "strb", "it", "bx"};

typedef enum
{
   LSL = 0, LSR, ASR, RRX, ROR
} shift_type_t;

char* shift_name[] = {"lsl", "lsr", "asr", "rrx", "ror"};

char* reg_name[] = {"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc"};

struct page_table_t
{
   unsigned char* data;
   uint32_t address;
   uint32_t length;
   struct page_table_t* next;
};

typedef struct page_table_t page_table_t;

page_table_t* page_tables = NULL;

#define PC r[15]
#define SP r[13]
#define LR r[14]
#define CURRENT_PC32 (state.PC-4)
#define CURRENT_PC16 (state.PC-2)

#define LOAD_PC(p) {state.next_instruction = p & ~1; state.t = (p & 1);}

#define UNKNOWN 0xdeadbeef

#define ILLEGAL_OPCODE assert(0 && "Illegal opcode")
#define NOT_DECODED(t) assert(0 && t " Not decoded")
#define UNPREDICTABLE assert(0 && "Unpredictable")
#define UNDEFINED assert(0 && "Undefined")
#define DECODED return 1

#define CHECK_CONDITION  {if (!condition_passed(instruction.condition)) continue;}


struct
{
   uint8_t z, c, n, v, t;
   uint8_t itstate;
   uint32_t next_instruction;
   uint32_t r[16];
} state;

unsigned char stack[1024 * 1024];


unsigned char* map_addr(uint32_t addr)
{
   for (page_table_t* t = page_tables; t; t = t->next)
   {
      if (addr >= t->address && addr < (t->address + t->length))
         return &t->data[addr - t->address];
   }
   printf("Attempted to read from unmapped address %08x\n", addr);
   assert(0 && "memory access violation");
}

void map_memory(unsigned char* data, uint32_t address, uint32_t length)
{
   printf("Adding page for %08x to %08x\n", address, address+length);
   page_table_t* new_table = malloc(sizeof(page_table_t));
   new_table->next = page_tables;
   page_tables = new_table;
   new_table->data = data;
   new_table->address = address;
   new_table->length = length;
}

uint32_t next_page = 0x80000000;

uint32_t alloc_page()
{
   uint32_t address = next_page;
   next_page += VPAGE_SIZE;
   unsigned char* page = malloc(VPAGE_SIZE);
   map_memory(page, address, VPAGE_SIZE);
   return address;
}

typedef struct
{
   uint8_t condition;
   opcode_t opcode;
   uint8_t setflags;
   uint32_t source_address;
   union
   {
      struct
      {
         uint8_t t, n, index, add, wback;
         int32_t imm32;        
      } LDR_I;
      struct
      {
         uint8_t t, n, index, add, wback;
         int32_t imm32;        
      } STR_I;
      struct
      {
         uint8_t t, n, index, add, wback;
         int32_t imm32;        
      } STRB_I;
      struct
      {
         uint8_t n, d;
         int32_t imm32;
      } ADD_I;
      struct
      {
         uint8_t n, d, c;
         int32_t imm32;
      } EOR_I;
      struct
      {
         uint8_t n, c;
         int32_t imm32;
      } TST_I;      
      struct
      {
         uint8_t n, d, m;
         shift_type_t shift_t;
         int32_t shift_n;
      } ADD_R;
      struct
      {
         uint8_t n, d, c;
         int32_t imm32;         
      } BIC_I;
      struct
      {
         uint8_t d, m;
      } MOV_R;
      struct
      {
         uint8_t n;
         uint32_t imm32;
      } CMP_I;
      struct
      {
         uint32_t imm32;         
      } B;
      struct
      {
         uint8_t t;
         int32_t imm32;
      } BL;
      struct
      {
         uint8_t t;
         int32_t imm32;
      } BLX;
      struct
      {
         uint8_t unaligned_allowed;
         uint16_t registers;
      } PUSH;
      struct
      {
         uint8_t unaligned_allowed;
         uint16_t registers;
      } POP;
      struct
      {
         uint8_t d;
         uint32_t imm32;
      } ADD_SPI;
      struct
      {
         uint8_t d;
         uint32_t imm32;
      } SUB_SPI;
      struct
      {
         uint8_t c, d;
         uint32_t imm32;
      } MOV_I;
      struct
      {
         uint8_t d;
         uint16_t imm16;
      } MOVT;
      struct
      {
         uint8_t t, n, index, add, wback;
         uint32_t imm32;
      } LDRB_I;
      struct
      {
         uint8_t n;
         uint32_t imm32;
      } CBZ;
      struct
      {
         uint8_t n;
         uint32_t imm32;
      } CBNZ;
      struct
      {
         uint8_t n;
         uint8_t m;
         shift_type_t shift_t;
         int32_t shift_n;
      } CMP_R;
      struct
      {
         uint8_t t, add;
         uint32_t imm32;
      } LDR_L;
      struct
      {
         uint8_t firstcond;
         uint8_t mask;
      } IT;
      struct
      {
         uint8_t m;
      } BX;
   };
} instruction_t;


uint32_t read_mem(uint8_t count, uint32_t addr)
{
   //printf("Reading from %08x\n", addr);
   unsigned char* physical = map_addr(addr);
   if (count == 4)
      return physical[0] | physical[1] << 8 | physical[2] << 16 | physical[3] << 24;
   else if (count == 1)
      return physical[0];
   else if (count == 2)
      return physical[0] | physical[1] << 8;
   else
      assert(0 && "Bad read size");
}

void write_mem(uint8_t count, uint32_t addr, uint32_t value)
{
   unsigned char* physical = map_addr(addr);
   if (count == 4)
   {
      //printf("Writing 0x%08x to %08x\n", value, addr);
      physical[0] = value & 0xff;
      physical[1] = (value >> 8) & 0xff;
      physical[2] = (value >> 16) & 0xff;
      physical[3] = (value >> 24) & 0xff;
   }
   else if (count == 1)
   {
      //printf("Writing 0x%02x to %08x\n", value, addr);
      physical[0] = value & 0xff;
   }
   else if (count == 2)
   {
      //printf("Writing 0x%04x to %08x\n", value, addr);
      physical[0] = value & 0xff;
      physical[1] = (value >> 8) & 0xff;
   }
   else
      assert(0 && "Bad write size");
}

int32_t SignExtend(uint8_t N, int32_t value, uint8_t length)
{
   if (value & (1 << (N-1)))
      return ((0xffffffff << N) | value) & (0xffffffff >> length);
   return value;
}

void AddWithCarry(uint32_t x, uint32_t y, uint8_t carry_in, uint32_t* result, uint8_t* carry_out, uint8_t* overflow_out)
{
   uint64_t unsigned_sum = x + y + carry_in;
   int64_t signed_sum = x + y + carry_in;
   *result = (unsigned_sum & 0xffffffff);
   *carry_out = (unsigned_sum >> 32);
   *overflow_out = ((int64_t)*result) != signed_sum;
}

void RRX_C(uint8_t N, uint32_t value, uint32_t shift, uint8_t carry_in, uint32_t* result, uint8_t* carry_out)
{
   *carry_out = (value & 1);
   *result = carry_in << (N-1) | (value >> 1);
}

void ROR_C(uint8_t N, uint32_t value, uint32_t shift, uint8_t carry_in, uint32_t* result, uint8_t* carry_out)
{
   assert(shift != 0);
   uint32_t m = shift % N;
   *result = (value >> m) | (value << (N-m));
   *carry_out = (*result >> (N-1)) & 1;   
}

void ASR_C(uint8_t N, uint32_t value, uint32_t shift, uint8_t carry_in, uint32_t* result, uint8_t* carry_out)
{
   assert(shift > 0);
   int32_t extended_value = SignExtend(N, value, shift+N);
   *result = extended_value >> shift;
   *carry_out = (extended_value >> (shift-1)) & 1;
}

void LSR_C(uint8_t N, uint32_t value, uint32_t shift, uint8_t carry_in, uint32_t* result, uint8_t* carry_out)
{
   assert(shift > 0);
   *result = value >> shift;
   *carry_out = (value >> (shift-1)) & 1;
}

void LSL_C(uint8_t N, uint32_t value, uint32_t shift, uint8_t carry_in, uint32_t* result, uint8_t* carry_out)
{
   assert(shift > 0);
   *result = value << shift;
   *carry_out = (*result >> 31);
}

void Shift_C(uint8_t N, uint32_t value, shift_type_t type, uint32_t shift, uint8_t carry_in, uint32_t* result, uint8_t* carry_out)
{
   assert(!(type == RRX && shift != 1));
   if (shift == 0)
   {
      *result = value;
      *carry_out = carry_in;
   }
   else if (type == LSL)
      LSL_C(N, value, shift, carry_in, result, carry_out);
   else if (type == LSR)
      LSR_C(N, value, shift, carry_in, result, carry_out);
   else if (type == ASR)
      ASR_C(N, value, shift, carry_in, result, carry_out);
   else if (type == ROR)
      ROR_C(N, value, shift, carry_in, result, carry_out);
   else if (type == RRX)
      RRX_C(N, value, shift, carry_in, result, carry_out);
   
}

void Shift(uint8_t N, uint32_t value, shift_type_t type, uint32_t shift, uint8_t carry_in, uint32_t* result)
{
   uint8_t carry_out;
   Shift_C(N, value, type, shift, carry_in, result, &carry_out);   
}


void DecodeImmShift(uint8_t type, uint8_t imm5, shift_type_t* shift_t, int32_t* shift_n)
{
   if (type == 0)
   {
      *shift_t = LSL;
      *shift_n = imm5;
   }
   else if (type == 1)
   {
      *shift_t = LSR;
      *shift_n = (imm5 == 0)?32:imm5;
   }
   else if (type == 2)
   {
      *shift_t = ASR;
      *shift_n = (imm5 == 0)?32:imm5;
   }
   else if (type == 3)
   {
      if (imm5 == 0)
      {
         *shift_t = RRX;
         *shift_n = 1;
      }
      else
      {
         *shift_t = ROR;
         *shift_n = imm5;
      }
   }

}

void initialize_state()
{
   // Map some memory for the stack which will grow DOWN from 0xd0000000
   // Also reserve some space above here for passing args. Really this should be much much smaller!
   map_memory(stack, 0xd0000000-(512*1024), 1024*1024);
   // Set up the registers
   state.SP = 0xd0000000;
   // Copy in argc and argv...?
   write_mem(4, state.SP, 0);
   state.SP -= 4;
   
   state.t = 0;
}

int decode_instruction(instruction_t* instruction)
{
   //printf("instruction at = %08x, SP = %08x)\n", state.next_instruction, state.SP);
   instruction->source_address = state.next_instruction;
   if (state.t == 0) // ARM mode
   {
      uint32_t word = read_mem(4, state.next_instruction);
      state.PC = state.next_instruction + 8;
      state.next_instruction += 4;
      instruction->condition = ((word >> 28) & 15);
      uint8_t op1 = (word >> 25) & 7;
      uint8_t op = (word >> 4) & 1;
      if (instruction->condition != 15)
      {
         if ((op1 & 0b110) == 0b000)
         {
            // Data processing and misc
            op1 = (word >> 20) & 31;
            op = (word >> 25) & 1;
            uint8_t imm5 = (word >> 7) & 31;
            uint8_t op2 = (word >> 4) & 15;
            if (op == 0)
            {
               if (((op1 & 0b11001) != 0b10000) && ((op2 & 1) == 0))
               {
                  // Data-processing (register)
                  op = (word >> 20) & 31;
                  op2 = (word >> 5) & 3;
                  if ((op & 0b11110) == 0b01000)
                  {
                     instruction->opcode = ADD_R;
                     instruction->ADD_R.d = (word >> 12) & 15;
                     instruction->ADD_R.n = (word >> 16) & 15;
                     instruction->ADD_R.m = word & 15;
                     instruction->setflags = (word >> 20) & 1;
                     DecodeImmShift((word >> 5) & 3, (word >> 7) & 31, &instruction->ADD_R.shift_t, &instruction->ADD_R.shift_n);
                     DECODED;
                  }
                  else if ((op & 0b11110) == 0b11010)
                  {
                     if (op2 == 0)
                     {
                        if (imm5 == 0)
                        {
                           instruction->opcode = MOV_R;
                           instruction->setflags = (word >> 20) & 1;
                           instruction->MOV_R.d = (word >> 12) & 15;
                           instruction->MOV_R.m = word & 15;
                           DECODED;
                        }
                        else
                        {
                           NOT_DECODED("LSL_I");
                        }
                     }
                  }
                  assert(0);
               }
               else if (((op1 & 0b11001) != 0b10000) && ((op2 & 0b1001) == 1))
               {
                  // Data-processing (register-shifted register)
                  assert(0);
               }
               else if (((op1 & 0b11001) == 0b10000) && ((op2 & 0b1000) == 0))
               {
                  // Miscellaneous instructions
                  uint8_t op = (word >> 21) & 3;
                  uint8_t op1 = (word >> 16) & 15;
                  uint8_t op2 = (word >> 4) & 7;
                  uint8_t B = (word >> 9) & 1;
                  if (op2 == 0)
                  {
                     if (B == 1)
                     {
                        if ((op & 1) == 0)
                        {
                           NOT_DECODED("MRS_B");
                        }
                        else
                        {
                           NOT_DECODED("MSR_B");
                        }
                     }
                     else // B == 0
                     {
                        if ((op & 1) == 0)
                        {
                           NOT_DECODED("MRS");
                        }
                        else if (op == 1)
                        {
                           if ((op1 & 0b0011) == 0)
                           {
                              NOT_DECODED("MSR_R");
                           }
                           else  if (((op1 & 0b0011) == 0b01) || ((op1 & 0b0010) == 0b0010))
                           {
                              NOT_DECODED("MSR_R");
                           }
                        }
                        else if (op == 3)
                           NOT_DECODED("MSR_R");
                     }
                  }
                  else if (op2 == 1)
                  {
                     if (op == 1)
                        NOT_DECODED("BX");
                     else if (op == 3)
                        NOT_DECODED("CLZ");                     
                  }
                  else if (op2 == 2 && op == 1)
                     NOT_DECODED("BXJ");
                  else if (op2 == 3)
                     NOT_DECODED("BLX");
                  else if (op2 == 5)
                  {
                     // Saturating addition/subtraction
                     assert(0);
                  }
                  else if (op2 == 6 && op == 3)
                     NOT_DECODED("ERET");
                  else if (op2 == 7)
                  {
                     if (op == 1)
                     {
                        instruction->opcode = BKPT;
                        DECODED;
                     }
                     else if (op == 2)
                        NOT_DECODED("HVC");
                     else if (op == 3)
                        NOT_DECODED("SMC");
                  }
                  ILLEGAL_OPCODE;
               }
               else if (((op2 & 0b11001) == 0b10000) && ((op2 & 0b1001) == 0b1000))
               {
                  // Halfword multiply and multiply accumulate
                  assert(0);
               }
               else if (((op1 & 0b10000) == 0) && (op2 == 0b1001))
               {
                  // Multiply and multiply accumulate
                  assert(0);
               }
               else if (((op1 & 0b10000) == 0b10000) && (op2 == 0b1001))
               {
                  // Synchronization
                  assert(0);
               }
               else if ((((op1 & 0b10010) != 0b00010) && (op2 == 0b1011 || ((op2 & 0b1101) == 0b1101))) || (((op1 & 0b10010) == 0b00010) && ((op2 & 0b1101) == 0b1101)))
               {
                  // exta load/store
                  assert(0);
               }
               else if ((((op1 & 0b10010) == 0b00010) && op2 == 0b1011) || (((op1 & 0b10011) == 0b00011) && ((op2 & 0b1101) == (0b1101))))
               {
                  // extra load/store unprivileged
                  assert(0);
               }
            }
            else
            {
               if ((op1 & 0b11001) != 0b10000)
               {
                  // Data processing (immediate)
                  op = (word >> 20) & 31;
                  uint8_t rn = (word >> 16) & 15;
                  if ((op & 0b11110) == 0b01000)
                  {
                     if (rn != 15)
                     {
                        instruction->opcode = ADD_I;
                        instruction->ADD_I.d = (word >> 12) & 15;
                        instruction->ADD_I.n = rn;
                        instruction->setflags = (word >> 20) & 1;
                        instruction->ADD_I.imm32 = ARMExpandImm(word & 0xfff);
                        DECODED;
                     }
                     else
                     {
                        NOT_DECODED("ADR");
                     }
                  }
                  else if ((op & 0b11110) == 0b11100)
                  {
                     instruction->opcode = BIC_I;
                     instruction->BIC_I.d = (word >> 12) & 15;
                     instruction->BIC_I.n = (word >> 16) & 15;
                     instruction->setflags = (word >> 20) & 1;
                     instruction->BIC_I.imm32 = ARMExpandImm_C(word & 0xfff, state.c, &instruction->BIC_I.c);
                     DECODED;
                  }
                  else if (op == 0b10101)
                  {
                     instruction->opcode = CMP_I;
                     instruction->CMP_I.n = (word >> 16) & 15;
                     instruction->CMP_I.imm32 = ARMExpandImm(word & 0xfff);
                     DECODED;
                  }
                  
                  assert(0);
               }
               else if (op1 == 0b10000)
               {
                  // 16-bit immediate load, MOV(immediate)
                  assert(0);
               }
               else if (op1 == 0b10100)
               {
                  // High halfword 16-bit immediate load, MOVT
                  assert(0);
               }
               else if ((op1 & 0b11011) == 0b10010)
               {
                  // MSR (immediate) and hints
                  assert(0);
               }
            }               
            assert(0 && "Illegal opcode");
         }
         else if ((op1 & 0b010) == 0b010)
         {
            // Load store word and unsigned byte (and media instruction)            
            // load/store word and unsigned byte
            uint8_t A = (word >> 25) & 1;
            uint8_t B = (word >> 4) & 1;
            uint8_t op1 = (word >> 20) & 31;
            uint8_t Rn = (word >> 16) & 15;
            
            if (A == 1 && B == 1)
            {
               // Media instruction
               assert(0 && "Media not implemented");
            }

            if (A == 0 && ((op1 & 0b00101) == 0) && !((op1 & 0b10111) == 0b00010))
              NOT_DECODED("STR_I");
            else if (A == 1 && ((op1 & 0b00101) == 0) && !((op1 & 0b10111) == 0b00010))
              NOT_DECODED("STR_R");
            else if ((A == 0 && ((op1 & 0b10111) == 0b00010)) || (A == 1 && ((op1 & 0b10111) == 0b00010) && B == 0))
              NOT_DECODED("STRT");
            else if (A == 0 && ((op1 & 0b00101) == 0b00001) && !((op1 & 0b10111) == 0b00011))
            {
               if (Rn != 15)
               {
                  uint8_t P = (word >> 24) & 1;
                  uint8_t W = (word >> 24) & 1;
                  uint8_t U = (word >> 23) & 1;
                  uint16_t imm12 = word & 0xfff;
                  uint8_t Rn = (word >> 16) & 15;
                  assert(!(P == 0 && W == 1));
                  if (Rn == 13 && P == 0 && U == 1 && W == 0 && imm12 == 4)
                     assert("Should be POP");
                  instruction->opcode = LDR_I;
                  instruction->LDR_I.t = (word >> 12) & 15;
                  instruction->LDR_I.n = (word >> 16) & 15;
                  instruction->LDR_I.imm32 = imm12;
                  instruction->LDR_I.index = (P == 1);
                  instruction->LDR_I.add = (U == 1);
                  instruction->LDR_I.wback = (P == 0 || W == 1);
                  if (instruction->LDR_I.wback && (instruction->LDR_I.n == instruction->LDR_I.t))
                     UNPREDICTABLE;
                  DECODED;
               }
               else
               {
                  uint8_t P = (word >> 24) & 1;
                  uint8_t U = (word >> 23) & 1;
                  uint8_t W = (word >> 21) & 1;
                  instruction->opcode = LDR_L;
                  instruction->LDR_L.t = (word >> 12) & 15;
                  instruction->LDR_L.imm32 = (word & 0xfff);
                  instruction->LDR_L.add = (U == 1);
                  if (P == W)
                     UNPREDICTABLE;
                  if (P == 0 && W == 1)
                     assert("Should be LDRT");
                  DECODED;
               }
            }
            else if (A == 1 && ((op1 & 0b00101) == 0b00001) && !((op1 & 0b10111) == 0b00011) && B == 0)
               NOT_DECODED("LDR_R");
            else if ((A == 0 && ((op1 & 0b10111) == 0b00011)) || (A == 1 && ((op1 & 0b10111) == 0b00011) && B == 0))
               NOT_DECODED("LDRT");
            else if (A == 0 && ((op1 & 0b00101) == 0b00101) && ((op1 & 0b10111) != 0b00110))
               NOT_DECODED("STRB_I");
            else if (A == 1 && ((op1 & 0b00101) == 0b00101) && ((op1 & 0b10111) != 0b00110) && B == 0)
               NOT_DECODED("STRB_R");
            else if ((A == 0 && ((op1 & 0b10111) == 0b00110)) || ((A == 1 && ((op1 & 0b10111) == 0b00110) && B == 0)))
               NOT_DECODED("STBRT");
            else if (A == 0 && ((op1 & 0b00101) == 0b00101) && !((op1 & 0b10111) == 0b00111))
            {
               if (Rn != 15)
                  NOT_DECODED("LDRB_I");
               else
                  NOT_DECODED("LDRB_L");
            }
            else if (A == 1 && ((op1 & 0b00101) == 0b00101) && !((op1 & 0b10111) == 0b00111) && B == 0)
               NOT_DECODED("LDRB_R");
            else if ((A == 0 && ((op & 0b10111) == 0b00111)) || (A == 1 && ((op1 & 0b10111) == 0b10111) && B == 0))
               NOT_DECODED("LDRBT");                                                                                
         }
         else if ((op1 & 0b110) ==  0b100)
         {
            // B, BL, block data transfer
            op = (word >> 20) & 0x3f;
            if ((op & 0b110000) == 0b100000)
            {
               instruction->opcode = B;
               instruction->B.imm32 = SignExtend(24, (word & 0xffffff) << 2, 32);
               DECODED;
            }
            else if ((op & 0b110000) == 0b110000)
            {               
               instruction->opcode = BL;
               instruction->BL.t = 0;
               instruction->B.imm32 = SignExtend(24, (word & 0xffffff) << 2, 32);
               DECODED;
            }
            assert(0);
         }
         else if ((op1 & 0b110) == (0b110))
         {
            // Coprocessor
            assert(0);
         }         
      }
      else
      {
         // Unconditonal instruction
         uint8_t op1 = (word >> 20) & 255;
         //uint8_t rn = (word >> 16) & 15;
         //uint8_t op = (word >> 4) & 1;
         if ((op1 & 0b10000000) == 0)
         {
            // Memory hints, SIMD, and misc
            assert(0);
         }
         else if ((op1 & 0b11100101) == 0b10000100)
         {
            NOT_DECODED("SRS");
         }
         else if ((op1 & 0b11100101) == 0b10000001)
         {
            NOT_DECODED("RFE");
         }
         else if ((op1 & 0b11100000) == 0b10100000)
         {
            instruction->opcode = BLX;
            instruction->BL.t = 1;
            instruction->BL.imm32 = SignExtend(24, ((word & 0xffffff) << 2) | ((word >> 23) & 2), 32);
            DECODED;
         }
         assert(0);
      }            
   }
   else if (state.t) // THUMB node
   {
      instruction->condition = 14; // By default thumb instructions are always executed
      uint16_t word = read_mem(2, state.next_instruction);
      state.PC = state.next_instruction + 4;
      state.next_instruction += 2;
      if ((word >> 11 == 0b11101) || (word >> 11 == 0b11110) || (word >> 11 == 0b11111))
      {
         // 32-bit thumb
         uint16_t word2 = read_mem(2, state.next_instruction);
         // Do NOT change state.PC here for a 2-cycle decode! 
         state.next_instruction += 2;
         uint8_t op1 = (word >> 11) & 3;
         uint8_t op2 = (word >> 4) & 127;
         uint8_t op = (word2 >> 15) & 1;
         if (op1 == 1)
         {
            if ((op2 & 0b1100100) == 0)
            {
               // Load/store multiple
               uint8_t op = (word >> 7) & 3;
               uint8_t L = (word >> 4) & 1;
               uint8_t W = (word >> 5) & 1;
               uint8_t Rn = word & 15;
               if (op == 0)
               {
                  if (L == 0)
                  {
                     NOT_DECODED("SRS");
                  }
                  else
                  {
                     NOT_DECODED("RFE");  
                  }
               }
               else if (op == 1)
               {
                  if (L == 0)
                  {
                     NOT_DECODED("STM");
                  }
                  else
                  {
                     if (!(W == 1 && Rn == 0b1101))
                     {
                        NOT_DECODED("LDM");
                     }
                     else
                     {  // T2
                        instruction->opcode = POP;
                        instruction->POP.registers = word2;
                        instruction->POP.unaligned_allowed = 0;
                        assert(BitCount(instruction->POP.registers) >= 2);
                        assert((word2 >> 14) != 3);
                        // FIXME: Check IT stuff
                        DECODED;
                     }
                  }
               }
               else if (op == 2)
               {
                  if (L == 0)
                  {
                     if (!(W == 1 && Rn == 0b1101))
                     {
                        NOT_DECODED("STMDB");
                     }
                     else
                     {
                        instruction->opcode = PUSH; // T2
                        instruction->PUSH.registers = word2;
                        instruction->PUSH.unaligned_allowed = 0;
                        assert(BitCount(instruction->PUSH.registers) >= 2);
                        DECODED;
                     }
                  }
                  else
                  {
                     NOT_DECODED("LDMDB");
                  }
               }
               else if (op == 3)
               {
                  if (L == 0)
                  {
                     NOT_DECODED("SRS");
                  }
                  else
                  {
                     NOT_DECODED("RFE");
                  }
               }
               assert(0);
            }
            else if ((op2 & 0b1100100) == 0b0000100)
            {
               // Load/store dual
               assert(0);
            }
            else if ((op2 & 0b1100000) == 0b0100000)
            {
               // Data processing shifted register
               assert(0);
            }
            else if ((op2 & 0b1000000) == 0b1000000)
            {
               // Coprocessor, SIMD, FP
               assert(0);
            }
         }         
         else if (op1 == 2)
         {            
            if (((op2 & 0b0100000) == 0b0000000) && op == 0)
            {
               // Data processing modified immediate
               uint8_t op = (word >> 5) & 15;
               uint8_t Rn = word & 15;
               uint8_t RdS = (((word2 >> 8) & 15) << 1) | ((word >> 4) & 1);
               if (op == 0 && RdS != 31)
               {
                  NOT_DECODED("AND_I");
               }
               else if (op == 0 && RdS == 31)
               {
                  instruction->opcode = TST_I;
                  instruction->TST_I.n = word & 15;
                  instruction->TST_I.imm32 = ThumbExpandImm_C((word2 & 255) | ((word2 >> 4) & 0x700) | ((word << 1) & 0x800), state.c, &instruction->TST_I.c);
                  assert(instruction->TST_I.n != 13 && instruction->TST_I.n != 15);
                  DECODED;
               }
               else if (op == 1)
               {
                  NOT_DECODED("BIC_I");
               }
               else if (op == 2 && Rn != 15)
               {
                  NOT_DECODED("ORR_I");
               }
               else if (op == 2 && Rn == 15)
               {
                  NOT_DECODED("MOV_I");
               }
               else if (op == 3 && Rn != 15)
               {
                  NOT_DECODED("ORN_I");
               }
               else if (op == 3 && Rn == 15)
               {
                  NOT_DECODED("MVN_I");
               }
               else if (op == 4 && RdS != 31)
               {
                  instruction->opcode = EOR_I;
                  instruction->EOR_I.d = (word2 >> 8) & 15;
                  instruction->EOR_I.n = word & 15;
                  instruction->setflags = (word >> 4) & 1;
                  instruction->EOR_I.imm32 = ThumbExpandImm_C((word2 & 255) | ((word2 >> 4) & 0x700) | ((word << 1) & 0x800), state.c, &instruction->EOR_I.c);
                  assert(!(instruction->EOR_I.d == 13 || (instruction->EOR_I.d == 15 && instruction->setflags == 0) || instruction->EOR_I.n == 13 || instruction->EOR_I.n == 15));
                  DECODED;
               }
               else if (op == 4 && RdS == 31)
               {
                  NOT_DECODED("TEQ_I");
               }
               else if (op == 8 && RdS != 31)
               {
                  NOT_DECODED("ADD_I");
               }
               else if (op == 8 && RdS == 15)
               {
                  NOT_DECODED("CMN_I");
               }
               else if (op == 10)
               {
                  NOT_DECODED("ADC_I");
               }
               else if (op == 11)
               {
                  NOT_DECODED("SBC_I");
               }
               else if (op == 13 && RdS != 31)
               {
                  NOT_DECODED("SUB_I");
               }
               else if (op == 13 && RdS == 31)
               {
                  NOT_DECODED("CMP_I");
               }
               else if (op == 14)
               {
                  NOT_DECODED("RSB_I");
               }

                  
               assert(0);
            }
            if (((op2 & 0b0100000) == 0b0100000) && op == 0)
            {
               // Data processing plain binary immediate
               uint8_t op = (word >> 4) & 31;
               uint8_t Rn = word & 15;
               if (op == 0)
               {
                  if (Rn != 15)
                  {
                     NOT_DECODED("ADD");
                  }
                  else
                  {
                     NOT_DECODED("ADR");
                  }
               }
               else if (op == 0b00100)
               {
                  instruction->opcode = MOV_I; // T3
                  instruction->MOV_I.d = (word2 >> 8) & 15;
                  instruction->setflags = 0; // Carry is not given for this encoding, but that doesnt matter since S is always false
                  instruction->MOV_I.imm32 = (word2 & 255) | ((word2 >>4) & 0x700) | ((word << 1) & 0x800) | ((word & 15) << 12);
                  DECODED;
               }
               else if (op == 0b01010)
               {
                  if (Rn != 15)
                  {
                     NOT_DECODED("SUB");
                  }
                  else
                  {
                     NOT_DECODED("ADR");
                  }
               }
               else if (op == 0b01100)
               {
                  instruction->opcode = MOVT;
                  instruction->MOVT.d = (word2 >> 8) & 15;
                  instruction->MOVT.imm16 = ((word2 & 255) | ((word2 >> 4) & 0x700) | ((word << 1) & 0x800) | ((word & 15) << 12));
                  assert(instruction->MOVT.d != 13 && instruction->MOVT.d != 15);
                  DECODED;
               }
               else if (op == 0b10000 || op == 0b10010) // FIXME: Second case only applies if word:[14:12, 7:6] != 0
               {
                  NOT_DECODED("SSAT");
               }
               else if (op == 0b10010) // FIXME: only applies if word:[14:12, 7:6] == 0
               {
                  NOT_DECODED("SSAT16");
               }
               else if (op == 0b10100)
               {
                  NOT_DECODED("SBFX");
               }
               else if (op == 0b10110)
               {
                  if (Rn != 15)
                  {
                     NOT_DECODED("BFI");
                  }
                  else
                  {
                     NOT_DECODED("BFC");
                  }
               }
               else if (op == 0b11000 || op == 0b11010) // FIXME: Second case only applies if word:[14:12, 7:6] != 0
               {
                  NOT_DECODED("USAT");
               }
               else if (op == 0b11010) // FIXME: only applies if word:[14:12, 7:6] == 0
               {
                  NOT_DECODED("USAT16");
               }
               else if (op == 0b11100)
               {
                  NOT_DECODED("UBFX");
               }
               assert(0 && "Illegal opcode");
            }
            else if (op == 1)
            {
               // Branch and miscellaneous control
               uint8_t op1 = (word2 >> 12) & 7;
               uint8_t op = (word >> 4) & 127;
               uint8_t imm8 = word2 & 255;
               if ((op1 & 0b101) == 0)
               {
                  if ((op & 0b0111000) != 0b0111000)
                  {
                     // B (T3)
                     instruction->opcode = B;
                     instruction->B.imm32 = SignExtend(20, ((word2 & 0x7ff) << 1) | ((word & 63) << 12) | ((word2 << 7) & 0x20000) | ((word2 << 6) & 0x40000) | ((word << 9) & 0x80000), 32);
                     instruction->condition = (word >> 6) & 15;
                     DECODED;
                  }
                  else if (((imm8 & 0b00100000) == 0b00100000) && ((op & 0b1111110) == 0b0111000))
                  {
                     NOT_DECODED("MSR_B");
                  }
                  else if (((imm8 & 0b00100000) == 0b00000000) && (op == 0b0111000) && ((op2 & 0b0011) == 0))
                  {
                     NOT_DECODED("MSR_R");
                  }
                  else if (((imm8 & 0b00100000) == 0b00000000) && (op == 0b0111000) && (((op2 & 0b0011) == 0b0001) || ((op2 & 0b0010) == 0b0010)))
                  {
                     NOT_DECODED("MSR_RS"); // Probably not that useful for me
                  }
                  else if (op == 0b0111010)
                  {
                     // CPS, and hints
                     assert(0);
                  }
                  else if (op == 0b0111011)
                  {
                     // Misc control
                     assert(0);
                  }
                  else if (op == 0b01111000)
                  {
                     NOT_DECODED("BXJ");
                  }
                  else if (imm8 == 0 && op == 0b0111101)
                  {
                     NOT_DECODED("ERET");
                  }
                  else if (imm8 != 0 && op == 0b0111101)
                  {
                     NOT_DECODED("SUBS_PC/LR");
                  }
                  else if ((imm8 & 0b00100000) == 0b00100000 && ((op & 0b1111110) == 0b0111110))
                  {
                     NOT_DECODED("MRS_B");
                  }
                  else if ((imm8 & 0b00100000) == 0b00000000 && (op == 0b0111110))
                  {
                     NOT_DECODED("MRS");
                  }
                  else if ((imm8 & 0b00100000) == 0b00000000 && (op == 0b0111111))
                  {
                     NOT_DECODED("MRS_S");
                  }
               }
               if (op1 == 0)
               {
                  if (op == 0b1111110)
                  {
                     NOT_DECODED("HVC");
                  }
                  else if (op == 0b1111111)
                  {
                     NOT_DECODED("SMC");
                  }
               }
               if ((op1 & 0b101) == 0b001)
               {  // T4
                  uint8_t S = (word >> 10) & 1;
                  uint8_t J1 = (word2 >> 13) & 1;
                  uint8_t J2 = (word2 >> 11) & 1;
                  uint8_t I1 = ~(J1 ^ S) & 1;
                  uint8_t I2 = ~(J2 ^ S) & 1;
                  instruction->opcode = B;
                  instruction->B.imm32 = SignExtend(25, (S << 23) | (I1 << 22) | (I2 << 21) | ((word & 0x3ff) << 12) | ((word2 & 0x7ff) << 1), 32);
                  if (state.itstate != 0) UNPREDICTABLE;
                  DECODED;
               }
               if (op1 == 0b010)
               {
                  assert(0 && "Permanently UNDEFINED");
               }
               if ((op1 & 0b101) == 0b100)
               {
                  // T2
                  uint8_t S = (word >> 10) & 1;
                  uint8_t J1 = (word2 >> 13) & 1;
                  uint8_t J2 = (word2 >> 11) & 1;
                  uint8_t I1 = ~(J1 ^ S) & 1;
                  uint8_t I2 = ~(J2 ^ S) & 1;
                  instruction->opcode = BLX;
                  instruction->BL.t = 0;
                  assert((word2 & 1) != 1);
                  instruction->BL.imm32 = SignExtend(25, (S << 23) | (I1 << 22) | (I2 << 21) | ((word & 0x3ff) << 12) | ((word2 & 0x7ff) << 1), 32);
                  DECODED;
               }
               if ((op1 & 0b101) == 0b101)
               {
                  // T1
                  uint8_t S = (word >> 10) & 1;
                  uint8_t J1 = (word2 >> 13) & 1;
                  uint8_t J2 = (word2 >> 11) & 1;
                  uint8_t I1 = ~(J1 ^ S) & 1;
                  uint8_t I2 = ~(J2 ^ S) & 1;
                  instruction->opcode = BL;
                  instruction->BL.t = 1;
                  instruction->BL.imm32 = SignExtend(24, (S << 23) | (I1 << 22) | (I2 << 21) | ((word & 0x3ff) << 12) | ((word2 & 0x7ff) << 1), 32);
                  DECODED;
               }
            }
            assert(0 && "Illegal opcode");
         }
         else if (op1 == 3)
         {
            if ((op2 & 0b1110001) == 0b0000000)
            {
               // Store single
               uint8_t op1 = (word >> 5) & 3;
               uint8_t op2 = (word2 >> 6) & 63;
               if ((op1 == 0 && (((op2 & 0b100100) == 0b100100) || ((op2 & 0b111100) == 0b110000))) || (op1 == 0b100))
               {
                  NOT_DECODED("STRB_I");
               }
               else if (op1 == 0 && op2 == 0)
               {
                  NOT_DECODED("STRB_R");
               }
               else if (op1 == 0 && ((op2 & 0b111100) == 0b1111000))
               {
                  NOT_DECODED("STRBT");
               }
               else if ((op1 == 1 && (((op1 & 0b100100) == 0b100100) || ((op2 & 0b111100) == 0b110000))) || (op1 == 0b101))
               {
                  NOT_DECODED("STRH_I");
               }
               else if (op1 == 0b001 && op2 == 0)
               {
                  NOT_DECODED("STRH_R");
               }
               else if (op1 == 0b1 && ((op2 & 0b111100) == 0b111100))
               {
                  NOT_DECODED("STRHT");
               }
               else if ((op1 == 0b010 && (((op2 & 0b100100) == 0b100100) || ((op2 & 0b111100) == 0b110000))))
               {  // T4
                  instruction->opcode = STR_I;
                  instruction->STR_I.t = (word2 >> 12) & 15;
                  instruction->STR_I.n = word & 15;
                  instruction->STR_I.imm32 = word2 & 255;
                  instruction->STR_I.index = ((word2 >> 10) & 1) == 1;
                  instruction->STR_I.add = ((word2 >> 9) & 1) == 1;
                  instruction->STR_I.wback = ((word2 >> 8) & 1) == 1;
                  if ((instruction->STR_I.t == 15) || (instruction->STR_I.wback && (instruction->STR_I.t == instruction->STR_I.n)))
                     UNPREDICTABLE;
                  if ((instruction->STR_I.n == 15 ) || ((((word2 >> 10) & 1) == 0) && (((word2 >> 8) & 1) == 0)))
                     UNDEFINED;
                  DECODED;
               }
               else if (op1 == 0b110)
               {  // T3
                  NOT_DECODED("STR_I"); 
               }
               else if (op1 == 0b010 && op2 == 0)
               {
                  NOT_DECODED("STR_R");
               }
               else if (op1 == 0b010 && ((op2 & 0b111100) == 0b111000))
               {
                  NOT_DECODED("STRT");
               }
               ILLEGAL_OPCODE;
            }
            else if ((op2 & 0b1100111) == 0b0000001)
            {
               // Load byte, memory hints
               assert(0);
            }
            else if ((op2 & 0b1100111) == 0b0000011)
            {
               // Load halfword, memory hints
               assert(0);
            }
            else if ((op2 & 0b1100111) == 0b0000101)
            {
               // Load word
               uint8_t op1 = (word >> 7) & 3;
               uint8_t op2 = (word2 >> 6) & 63;
               uint8_t Rn = word & 15;
               if (op1 == 0 && op2 == 0 && Rn != 15)
               {
                  NOT_DECODED("LDR_R");
               }
               else if ((op1 == 0 && Rn != 15 && (((op2 & 0b100100) == 0b100100) || ((op2 & 0b111100) == 0b110000))) || (op1 == 1 && Rn != 15))
               {
                  if (((word >> 7) & 1) == 1)
                  { // T3
                     instruction->opcode = LDR_I;
                     instruction->LDR_I.t = (word2 >> 12) & 15;
                     instruction->LDR_I.n = word & 15;
                     instruction->LDR_I.imm32 = word2 & 0xfff;
                     instruction->LDR_I.index = 1;
                     instruction->LDR_I.add = 1;
                     instruction->LDR_I.wback = 0;
                     // Check itstate
                     DECODED;
                  }
                  else
                  { // T4
                     NOT_DECODED("LDR_I");
                  }
                  NOT_DECODED("LDR_I");
               }
               else if (op1 == 0 && ((op2 & 0b111100) == 0b111000) && Rn != 15)
               {
                  NOT_DECODED("LDRT");
               }
               else if ((op1 & 0b10) == 0 && Rn == 15)
               {  // T2
                  instruction->opcode = LDR_L;
                  instruction->LDR_L.t = (word2 >> 12) & 15;
                  instruction->LDR_L.imm32 = word2 & 0xfff;
                  instruction->LDR_L.add = ((word >> 7) & 1) == 1;
                  if (instruction->LDR_L.t == 15)
                  { // FIXME: Must be last if in ITblock
                  }
                  DECODED;
               }               
               assert(0 && "Illegal opcode");
            }
            else if ((op2 & 0b1100111) == 0b0000111)
            {
               // Explicitly Undefined
               assert(0 && "Undefined");
            }
            else if ((op2 & 0b1110001) == 0b0010000)
            {
               // Advanced SIMD, or structure load/store
               assert(0);
            }
            else if ((op2 & 0b1110000) == 0b0100000)
            {
               // Data processing register
               assert(0);
            }
            else if ((op2 & 0b1111000) == 0b0110000)
            {
               // Multiply, Multiply accumulate, and ABS
               assert(0);
            }
            else if ((op2 & 0b1111000) == 0b0111000)
            {
               // Long multiply, long multiply accumulate and divide
               assert(0);
            }
            else if ((op2 & 0b1000000) == 0b1000000)
            {
               // Coprocessor, Advanced SIMD and FP
               assert(0);
            }
         }         
         assert(0 && "Illegal opcode");
      }
      else
      {
         // 16-bit thumb
         uint8_t opcode = (word >> 10) & 63;
         if ((opcode & 0b110000) == 0)
         {
            // Shift (immediate), add, subtract, move, compare
            opcode = (word >> 9) & 31;
            if ((opcode & 0b11100) == 0)
            {
               NOT_DECODED("LSL_I");
            }
            else if ((opcode & 0b11100) == 0b00100)
            {
               NOT_DECODED("LSR_I");
            }
            else if ((opcode & 0b11100) == 0b01000)
            {
               NOT_DECODED("ASR_I");
            }
            else if (opcode == 0b01100)
            {
               NOT_DECODED("ADD_R");
            }
            else if (opcode == 0b01101)
            {
               NOT_DECODED("SUB_R");
            }
            else if (opcode == 0b01110)
            {
               NOT_DECODED("ADD_I");
            }
            else if (opcode == 0b01111)
            {
               NOT_DECODED("SUB_I");
            }
            else if ((opcode & 0b11100) == 0b10000)
            {
               instruction->opcode = MOV_I;
               instruction->MOV_I.d = (word >> 8) & 7;
               instruction->MOV_I.c = state.c;
               instruction->MOV_I.imm32 = word & 255;
               instruction->setflags = (state.itstate == 0);
               DECODED;
            }
            else if ((opcode & 0b11100) == 0b10100)
            {
               instruction->opcode = CMP_I;
               instruction->CMP_I.n = (word >> 8) & 7;
               instruction->CMP_I.imm32 = word & 255;
               DECODED;
            }
            else if ((opcode & 0b11100) == 0b11000)
            {
               NOT_DECODED("ADD_I");
            }
            else if ((opcode & 0b11100) == 0b11100)
            {
               NOT_DECODED("SUB_I");
            }
            ILLEGAL_OPCODE;
         }
         else if (opcode == 0b010000)
         {
            // Data-processing
            uint8_t opcode = (word >> 6) & 15;
            switch(opcode)
            {
               case 0: NOT_DECODED("AND_R");
               case 1: NOT_DECODED("EOR_R");
               case 2: NOT_DECODED("LSL_R");
               case 3: NOT_DECODED("LSR_R");
               case 4: NOT_DECODED("ASR_R");
               case 5: NOT_DECODED("ADC_R");
               case 6: NOT_DECODED("SBC_R");
               case 7: NOT_DECODED("ROR_R");
               case 8: NOT_DECODED("TST_R");
               case 9: NOT_DECODED("RSB_R");
               case 10:
               { 
                  instruction->opcode = CMP_R; //T1
                  instruction->CMP_R.n = word & 7;
                  instruction->CMP_R.m = (word >> 3) & 7;
                  instruction->CMP_R.shift_t = LSL;
                  instruction->CMP_R.shift_n = 0;
                  DECODED;
               }
               case 11: NOT_DECODED("CMN_R");
               case 12: NOT_DECODED("ORR_R");
               case 13: NOT_DECODED("MUL_R");
               case 14: NOT_DECODED("BIC_R");
               case 15: NOT_DECODED("MVN_R");

            }
            ILLEGAL_OPCODE;
         }
         else if (opcode == 0b010001)
         {
            opcode = (word >> 6) & 15;
            if (opcode == 0 || opcode == 1 || ((opcode & 0b1110) == 0b0010))
            {
               // T2
               instruction->opcode = ADD_R;
               instruction->ADD_R.d = (word & 7) | ((word >> 4) & 8);
               instruction->ADD_R.n = instruction->ADD_R.d;
               instruction->ADD_R.m = (word >> 3) & 15;
               instruction->setflags = 0;
               instruction->ADD_R.shift_t = LSL;
               instruction->ADD_R.shift_n = 0;
               assert(!(instruction->ADD_R.n == 15 && instruction->ADD_R.m == 15));
               // FIXME: Check ITSTATE here if branch
               DECODED;
            }
            else if ((opcode & 0b1100) == 0b0100)
            {
               NOT_DECODED("CMP_R");
            }
            else if (opcode == 0b1000 || opcode == 0b1001 || ((opcode & 0b1110) == 0b1010))
            {
               instruction->opcode = MOV_R;
               instruction->MOV_R.d = (word & 7) | ((word >> 4) & 8);
               instruction->MOV_R.m = (word >> 3) & 15;
               instruction->setflags = 0;
               // FIXME: assert(!(instruction->MOV_R.d == 15 && state.itstate != 0 && ...LastInITBlock()) 
               DECODED;
            }
            else if ((opcode & 0b1110) == 0b1100)
            {  // T1
               instruction->opcode = BX;
               instruction->BX.m = (word >> 3) & 15;
               // FIXME: If in IT then must be last
               DECODED;
            }
            else if ((opcode & 0b1110) == 0b1110)
            {
               NOT_DECODED("BLX");
            }
            // Special data instructions and branch-and-exchange
            assert(0);            
         }
         else if ((opcode & 0b111110) == 0b010010)
         {
            NOT_DECODED("LDR_L");
         }
         else if (((opcode & 0b111100) == 0b010100) || ((opcode & 0b111000) == 0b011000) || ((opcode & 0b111000) == 0b100000))
         {
            // load/store single item
            uint8_t opA = (word >> 12) & 15;
            uint8_t opB = (word >> 9) & 7;
            if (opA == 0b0101)
            {
               if (opB == 0)
               {
                  NOT_DECODED("STR_R");
               }
               else if (opB == 1)
               {
                  NOT_DECODED("STRH_R");
               }
               else if (opB == 2)
               {
                  NOT_DECODED("STRB_R");
               }
               else if (opB == 3)
               {
                  NOT_DECODED("LDRSB_R");
               }
               else if (opB == 4)
               {
                  NOT_DECODED("LDR_R");
               }
               else if (opB == 5)
               {
                  NOT_DECODED("LDRH_R");
               }
               else if (opB == 6)
               {
                  NOT_DECODED("LDRB_R");
               }
               else if (opB == 7)
               {
                  NOT_DECODED("LDRSH_R");
               }
            }
            else if (opA == 0b0110)
            {
               if ((opB & 0b100) == 0)
               { // T1
                  instruction->opcode = STR_I;
                  instruction->STR_I.t = word & 7;
                  instruction->STR_I.n = (word >> 3) & 7;
                  instruction->STR_I.imm32 = ((word >> 6) & 31) << 2;
                  instruction->STR_I.index = 1;
                  instruction->STR_I.add = 1;
                  instruction->STR_I.wback = 0;
                  DECODED;
               }
               else
               {
                  if ((word >> 15) & 1)
                  { // T2
                     NOT_DECODED("LDR_I");
                  }
                  else
                  { // T1
                     instruction->opcode = LDR_I;
                     instruction->LDR_I.t = word & 7;
                     instruction->LDR_I.n = (word >> 3) & 7;
                     instruction->LDR_I.imm32 = (word >> 4) & 0x7c;
                     instruction->LDR_I.index = 1;
                     instruction->LDR_I.add = 1;
                     instruction->LDR_I.wback = 0;
                     DECODED;
                  }

               }                  
            }
            else if (opA == 0b0111)
            {
               if ((opB & 0b100) == 0)
               {
                  instruction->opcode = STRB_I;
                  instruction->STRB_I.t = word & 7;
                  instruction->STRB_I.n = (word >> 3) & 7;
                  instruction->STRB_I.imm32 = (word >> 6) & 31;
                  instruction->STRB_I.index = 1;
                  instruction->STRB_I.add = 1;
                  instruction->STRB_I.wback = 0;
                  DECODED;
               }
               else
               {
                  instruction->opcode = LDRB_I;
                  instruction->LDRB_I.t = word & 7;
                  instruction->LDRB_I.n = (word >> 3) & 7;
                  instruction->LDRB_I.imm32 = (word >> 6) & 31;
                  instruction->LDRB_I.index = 1;
                  instruction->LDRB_I.add = 1;
                  instruction->LDRB_I.wback = 1;
                  DECODED;
               }                  
            }           
            else if (opA == 0b1000)
            {
               if ((opB & 0b100) == 0)
               {
                  NOT_DECODED("STRH_I");
               }
               else
               {
                  NOT_DECODED("LDRH_I");
               }                  
            }           
            else if (opA == 0b1001)
            {
               if ((opB & 0b100) == 0)
               {
                  if ((word >> 15) & 1)
                  { // T2
                     instruction->opcode = STR_I;
                     instruction->STR_I.t = (word >> 8) & 7;
                     instruction->STR_I.n = 13;
                     instruction->STR_I.imm32 = (word & 255) << 2;
                     instruction->STR_I.index = 1;
                     instruction->STR_I.add = 1;
                     instruction->STR_I.wback = 0;
                     DECODED;
                  }
                  else
                  { // T1
                     NOT_DECODED("STR_I");
                  }
                  
               }
               else
               {
                  NOT_DECODED("LDR_I");
               }                  
            }           
            assert(0 && "Illegal opcode");
         }
         else if ((opcode & 0b111110) == 0b101000)
         {
            NOT_DECODED("ADR");
         }
         else if ((opcode & 0b111110) == 0b101010)
         {
            instruction->opcode = ADD_SPI;
            instruction->ADD_SPI.d = (word >> 8) & 7;
            instruction->setflags = 0;
            instruction->ADD_SPI.imm32 = (word & 255) << 2;
            DECODED;
         }
         else if ((opcode & 0b111100) == 0b101100)
         {
            // Miscellaneous 16-bit
            opcode = (word >> 5) & 127;
            if ((opcode & 0b1111100) == 0)
            {
               NOT_DECODED("ADD_SPI");
            }
            else if ((opcode & 0b1111100) == 0b0000100)
            {
               instruction->opcode = SUB_SPI;
               instruction->SUB_SPI.d = 13;
               instruction->setflags = 0;
               instruction->SUB_SPI.imm32 = (word & 127) << 2;
               DECODED;
            }
            else if ((opcode & 0b1111000) == 0b0001000)
            {
               if (((word >> 11) & 1) == 1)
               {
                  instruction->opcode = CBNZ;
                  instruction->CBNZ.imm32 = (((word >> 3) & 31) << 1) | (((word >> 9) & 1) << 6);
                  instruction->CBNZ.n = word & 7;
                  DECODED;
               }
               else
               {
                  instruction->opcode = CBZ;
                  instruction->CBZ.imm32 = (((word >> 3) & 31) << 1) | (((word >> 9) & 1) << 6);
                  instruction->CBZ.n = word & 7;
                  DECODED;
               }
            }
            else if ((opcode & 0b1111110) == 0b0010000)
            {
               NOT_DECODED("SXTH");
            }
            else if ((opcode & 0b1111110) == 0b0010010)
            {
               NOT_DECODED("SXTB");
            }
            else if ((opcode & 0b1111110) == 0b0010100)
            {
               NOT_DECODED("UXTH");
            }
            else if ((opcode & 0b1111110) == 0b0010110)
            {
               NOT_DECODED("UXTB");
            }
            else if ((opcode & 0b1111000) == 0b0011000)
            {
               NOT_DECODED("CBZ");
            }
            else if ((opcode & 0b1110000) == 0b0100000)
            {
               instruction->opcode = PUSH;
               instruction->PUSH.registers = (word & 255) | ((word & 256) << 6);
               instruction->PUSH.unaligned_allowed = 0;
               assert(instruction->PUSH.registers != 0);
               DECODED;
            }
            else if (opcode == 0b0110010)
            {
               NOT_DECODED("SETEND");
            }
            else if (opcode == 0b0110011)
            {
               NOT_DECODED("CPS");
            }
            else if ((opcode & 0b1111000) == 0b1001000)
            {
               if (((word >> 11) & 1) == 1)
               {
                  instruction->opcode = CBNZ;
                  instruction->CBNZ.n = word & 7;
                  instruction->CBNZ.imm32 = ((word >> 2) & 63) | ((word >> 3) & 64);
                  DECODED;
               }
               else
               {
                  instruction->opcode = CBZ;
                  instruction->CBZ.n = word & 7;
                  instruction->CBZ.imm32 = ((word >> 2) & 63) | ((word >> 3) & 64);
                  DECODED;
               }
            }
            else if ((opcode & 0b1111111) == 0b1010000)
            {
               NOT_DECODED("REV");
            }
            else if ((opcode & 0b1111110) == 0b1010010)
            {
               NOT_DECODED("REV16");
            }
            else if ((opcode & 0b1111110) == 0b1010110)
            {
               NOT_DECODED("REVSH");
            }
            else if ((opcode & 0b1111000) == 0b1011000)
            {
               if (((word >> 11) & 1) == 1)
               {
                  instruction->opcode = CBNZ;
                  instruction->CBNZ.n = word & 7;
                  instruction->CBNZ.imm32 = ((word >> 2) & 0x3e) | ((word >> 3) & 0x40);
                  DECODED;
               }
               else
               {
                  instruction->opcode = CBZ;
                  instruction->CBNZ.n = word & 7;
                  instruction->CBNZ.imm32 = ((word >> 2) & 0x3e) | ((word >> 3) & 0x40);
                  DECODED;
               }
            }
            else if ((opcode & 0b1110000) == 0b1100000)
            {
               instruction->opcode = POP;
               instruction->POP.registers = (word & 255) | ((word & 128) << 8);
               instruction->POP.unaligned_allowed = 0;
               DECODED;
            }
            else if ((opcode & 0b1111000) == 0b1110000)
            {
               NOT_DECODED("BKPT");
            }
            else if ((opcode & 0b1111000) == 0b1111000)
            {
               // IT and hints
               uint8_t opA = (word >> 4) & 15;
               uint8_t opB = word & 15;
               if (opB != 0)
               {
                  instruction->opcode = IT;
                  instruction->IT.firstcond = (word >> 4) & 15;
                  instruction->IT.mask = word & 15;
                  DECODED;
               }
               else if (opA == 0 && opB == 0)
               {
                  NOT_DECODED("NOP");
               }
               else if (opA == 1 && opB == 0)
               {
                  NOT_DECODED("YIELD");
               }
               else if (opA == 2 && opB == 0)
               {
                  NOT_DECODED("WFE");
               }
               else if (opA == 3 && opB == 0)
               {
                  NOT_DECODED("WFI");
               }
               else if (opA == 4 && opB == 0)
               {
                  NOT_DECODED("SEV");
               }
               ILLEGAL_OPCODE;
            }
            ILLEGAL_OPCODE;
         }
         else if ((opcode & 0b111110) == 0b110010)
         {
            // Load multiple registers
            assert(0);
         }
         else if ((opcode & 0b111100) == 0b110100)
         {
            uint8_t opcode = (word >> 8) & 15;
            if ((opcode & 0b1110) != 0b1110)
            {
               instruction->opcode = B; // T1
               instruction->condition = (word >> 8) & 15;
               instruction->B.imm32 = SignExtend(9, ((word & 255) << 1), 32);
               DECODED;
            }
            else if (opcode == 0b1110)
            {
               NOT_DECODED("UDF");
            }
            else if (opcode == 0b1111)
            {
               NOT_DECODED("SVC");
            }
            // Conditional branch and supervisor-call
            assert(0);
         }
         else if ((opcode & 0b111110) == 0b111000)
         {  // T2
            instruction->opcode = B;
            instruction->B.imm32 = SignExtend(12, ((word & 0x7ff) << 1), 32);
            DECODED;
         }
         ILLEGAL_OPCODE;
      }
   }
   ILLEGAL_OPCODE;
}

uint32_t ror32(uint32_t value, int degree)
{
   return (value >> degree) | (value << (32 - degree));
}

char* condition_name[] = {"eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc", "hi", "ls", "ge", "lt", "gt", "le", "", ""};  // Last two are AL and unconditional

int condition_passed(uint8_t condition)
{
   if (condition == 0 && state.z != 1) return 0;
   if (condition == 1 && state.z != 0) return 0;
   if (condition == 2 && state.c != 1) return 0;
   if (condition == 3 && state.c != 0) return 0;
   if (condition == 4 && state.v != 1) return 0;
   if (condition == 5 && state.v != 0) return 0;
   if (condition == 6 && state.n != 1) return 0;
   if (condition == 7 && state.n != 0) return 0;
   if (condition == 7 && state.n != 0) return 0;
   if (condition == 8 && state.c != 1 && state.z != 0) return 0;
   if (condition == 9 && !(state.c == 1 || state.z == 0)) return 0;
   if (condition == 10 && !(state.n == state.v)) return 0;
   if (condition == 11 && !(state.n != state.v)) return 0;
   if (condition == 12 && !(state.z == 0 && state.n == state.v)) return 0;
   if (condition == 13 && !(state.z == 1 || state.n != state.v)) return 0;
   return 1;
}


void print_opcode(instruction_t* instruction)
{
   printf("     %08x # %s%s%s", instruction->source_address, opcode_name[instruction->opcode], condition_name[instruction->condition], instruction->setflags?"s":"");
}
void step_machine(int steps)
{
   for (int step = 0; step < steps; step++)
   {
      instruction_t instruction;
      memset(&instruction, 0, sizeof(instruction_t));
      assert(decode_instruction(&instruction));
      if (state.t == 1 && state.itstate != 0)
      {
         // Update the condition based on itstate
         if (state.itstate != 0 && state.t == 1)
         {
            instruction.condition = (state.itstate >> 4) & 15;
         }
         if ((state.itstate & 7) == 0)
         {
            state.itstate = 0;
         }
         else
         {
            uint8_t new_low = ((state.itstate << 1) & 0b11111);
            // Clear the bottom 5 bits now we have rotated them
            state.itstate &= 0b11100000;
            // And put the new value in
            state.itstate |= new_low;
         }
      }

      printf("    %04d: ", step);
      print_opcode(&instruction);

      switch(instruction.opcode)
      {
         case LDR_I:
         {
            uint32_t offset_addr = state.r[instruction.LDR_I.n] + (instruction.LDR_I.add?(instruction.LDR_I.imm32):(-instruction.LDR_I.imm32));
            uint32_t address = instruction.LDR_I.index?(offset_addr):state.r[instruction.LDR_I.n];
            if (instruction.LDR_I.index && !instruction.LDR_I.wback) printf(" %s, [%s + {%d}]\n", reg_name[instruction.LDR_I.t], reg_name[instruction.LDR_I.n], instruction.LDR_I.imm32);
            else if (instruction.LDR_I.index && instruction.LDR_I.wback)
            {
               if (instruction.LDR_I.imm32 == 0) printf(" %s, [%s]\n", reg_name[instruction.LDR_I.t], reg_name[instruction.LDR_I.n]);
               else printf(" %s, [%s + %d]\n", reg_name[instruction.LDR_I.t], reg_name[instruction.LDR_I.n], instruction.LDR_I.imm32);
            }
            else if (!instruction.LDR_I.index && instruction.LDR_I.wback) printf(" %s, [%s] + %d\n", reg_name[instruction.LDR_I.t], reg_name[instruction.LDR_I.n], instruction.LDR_I.imm32);
            CHECK_CONDITION;
            uint32_t data = read_mem(4, address);
            if (instruction.LDR_I.wback)
               state.r[instruction.LDR_I.n] = offset_addr;
            if (instruction.LDR_I.t == 15)
               assert((address & 3) == 0);
            state.r[instruction.LDR_I.t] = data;
            break;
         }
         case LDR_L:
         {
            printf(" %s, [pc, %s#%d]\n", reg_name[instruction.LDR_L.t], (instruction.LDR_L.add?"+":"-"), instruction.LDR_L.imm32);
            CHECK_CONDITION;
            uint32_t base = state.PC & ~3;
            uint32_t address = base + (instruction.LDR_L.add?instruction.LDR_L.imm32:(-instruction.LDR_L.imm32));
            uint32_t data = read_mem(4, address);
            if (instruction.LDR_L.t == 15)
            {
               if ((address & 3) != 0)
                  UNPREDICTABLE;
               LOAD_PC(data);
            }
            else
               state.r[instruction.LDR_L.t] = data;
            printf("Loaded literal %08x from %08x\n", data, address);
            break;
         }
         case STR_I:
         {
            uint32_t offset_addr = state.r[instruction.STR_I.n] + (instruction.STR_I.add?(instruction.STR_I.imm32):(-instruction.STR_I.imm32));
            uint32_t address = instruction.STR_I.index?(offset_addr):state.r[instruction.STR_I.n];
            if (instruction.STR_I.index && !instruction.STR_I.wback) printf(" %s, [%s + {%d}]\n", reg_name[instruction.STR_I.t], reg_name[instruction.STR_I.n], instruction.STR_I.imm32);
            else if (instruction.STR_I.index && instruction.STR_I.wback) printf(" %s, [%s + %d]\n", reg_name[instruction.STR_I.t], reg_name[instruction.STR_I.n], instruction.STR_I.imm32);
            else if (!instruction.STR_I.index && instruction.STR_I.wback) printf(" %s, [%s] + %d\n", reg_name[instruction.STR_I.t], reg_name[instruction.STR_I.n], instruction.STR_I.imm32);
            CHECK_CONDITION;
            write_mem(4, address, state.r[instruction.STR_I.t]);
            if (instruction.STR_I.wback)
               state.r[instruction.STR_I.n] = offset_addr;
            break;
         }
         case STRB_I:
         {
            uint32_t offset_addr = state.r[instruction.STRB_I.n] + (instruction.STRB_I.add?(instruction.STRB_I.imm32):(-instruction.STRB_I.imm32));
            uint32_t address = instruction.STRB_I.index?(offset_addr):state.r[instruction.STRB_I.n];
            if (instruction.STRB_I.index && !instruction.STRB_I.wback) printf(" %s, [%s + {%d}]\n", reg_name[instruction.STRB_I.t], reg_name[instruction.STRB_I.n], instruction.STRB_I.imm32);
            else if (instruction.STRB_I.index && instruction.STRB_I.wback) printf(" %s, [%s + %d]\n", reg_name[instruction.STRB_I.t], reg_name[instruction.STRB_I.n], instruction.STRB_I.imm32);
            else if (!instruction.STRB_I.index && instruction.STRB_I.wback) printf(" %s, [%s] + %d\n", reg_name[instruction.STRB_I.t], reg_name[instruction.STRB_I.n], instruction.STRB_I.imm32);
            CHECK_CONDITION;
            write_mem(1, address, state.r[instruction.STRB_I.t]);
            if (instruction.STRB_I.wback)
               state.r[instruction.STRB_I.n] = offset_addr;
            break;
         }
         case ADD_I:
         {
            printf(" %s, %s, #%d\n", reg_name[instruction.ADD_I.d], reg_name[instruction.ADD_I.n], instruction.ADD_I.imm32);
            CHECK_CONDITION;
            uint32_t result;
            uint8_t carry_out;
            uint8_t overflow_out;
            AddWithCarry(state.r[instruction.ADD_I.n], instruction.ADD_I.imm32, 0, &result, &carry_out, &overflow_out);
            state.r[instruction.ADD_I.d] = result;
            if (instruction.setflags)
            {
               state.n = (result >> 31) & 1;
               state.z = (result == 0);
               state.c = carry_out;
               state.v = overflow_out;
            }
            break;
         }
         case EOR_I:
         {
            printf(" %s, %s, #%d\n", reg_name[instruction.EOR_I.d], reg_name[instruction.EOR_I.n], instruction.EOR_I.imm32);
            CHECK_CONDITION;
            uint32_t result;
            result = state.r[instruction.EOR_I.n] ^ instruction.EOR_I.imm32;         
            state.r[instruction.EOR_I.d] = result;
            if (instruction.setflags && instruction.EOR_I.d != 15)
            {
               state.n = (result >> 31) & 1;
               state.z = (result == 0);
               state.c = instruction.EOR_I.c;
            }
            break;
         }
         case TST_I:
         {
            printf(" %s, #%d\n", reg_name[instruction.TST_I.n], instruction.TST_I.imm32);
            CHECK_CONDITION;
            uint32_t result = state.r[instruction.TST_I.n] & instruction.EOR_I.imm32;
            state.n = (result >> 31) & 1;
            state.z = (result == 0);
            state.c = instruction.TST_I.c;
            break;
         }
         case ADD_R:
         {
            uint32_t shifted;
            uint32_t result;
            uint8_t carry_out;
            uint8_t overflow_out;
            if (instruction.ADD_R.shift_t == LSL && instruction.ADD_R.shift_n == 0) printf(" %s, %s, %s\n", reg_name[instruction.ADD_R.d], reg_name[instruction.ADD_R.n], reg_name[instruction.ADD_R.m]);
            else printf(" %s, %s, %s %s %d\n", reg_name[instruction.ADD_R.d], reg_name[instruction.ADD_R.n], reg_name[instruction.ADD_R.m], shift_name[instruction.ADD_R.shift_t], instruction.ADD_R.shift_n);
            CHECK_CONDITION;
            Shift(32, state.r[instruction.ADD_R.m], instruction.ADD_R.shift_t, instruction.ADD_R.shift_n, state.c, &shifted);
            AddWithCarry(state.r[instruction.ADD_R.n], shifted, 0, &result, &carry_out, &overflow_out);
            state.r[instruction.ADD_I.d] = result;
            if (instruction.setflags)
            {
               state.n = (result >> 31) & 1;
               state.z = (result == 0);
               state.c = carry_out;
               state.v = overflow_out;
            }
            break;
         }
         case BIC_I:
         {
            uint32_t result = state.r[instruction.BIC_I.n] & ~instruction.BIC_I.imm32;
            printf(" %s, %s, %d\n", reg_name[instruction.BIC_I.d], reg_name[instruction.BIC_I.n], instruction.BIC_I.imm32);
            CHECK_CONDITION;
            state.r[instruction.BIC_I.d] = result;
            if ((instruction.BIC_I.d != 15) && instruction.setflags)
            {
               state.n = (result >> 31) & 1;
               state.z = (result == 0);
               state.c = instruction.BIC_I.c;
            }
            break;
         }
         case MOV_R:
         {
            uint32_t result = state.r[instruction.MOV_R.m];
            printf(" %s, %s\n", reg_name[instruction.MOV_R.d], reg_name[instruction.MOV_R.m]);
            CHECK_CONDITION;
            state.r[instruction.MOV_R.d] = result;
            if ((instruction.MOV_R.d != 15) && instruction.setflags)
            {
               state.n = (result >> 31) & 1;
               state.z = (result == 0);
            }
            break;
         }
         case CMP_I:
         {
            uint32_t result;
            printf(" %s, %d\n", reg_name[instruction.CMP_I.n], instruction.CMP_I.imm32);
            CHECK_CONDITION;
            AddWithCarry(state.r[instruction.CMP_I.n], ~instruction.CMP_I.imm32, 1, &result, &state.c, &state.v);
            state.n = (result >> 31) & 1;
            state.z = (result == 0);
            break;
         }
         case B:
         {
            printf(" 0x%08x\n", instruction.B.imm32 + state.PC);
            CHECK_CONDITION;
            state.next_instruction = instruction.B.imm32 + state.PC;
            break;
         }
         case BL: // Both of these use the same values and do the same thing, but have different opcodes!
         case BLX:
         {
            printf(" %08x\n", instruction.BL.imm32 + ((instruction.BL.t == 0)?(state.PC&~3):state.PC));
            CHECK_CONDITION;
            if (state.t == 0)
               state.LR = state.PC - 4;
            else
               state.LR = state.PC | 1;
            if (instruction.BL.t == 0)
            {
               LOAD_PC((state.PC & ~3) + instruction.BL.imm32);
            }
            else
            {
               LOAD_PC(state.PC + instruction.BL.imm32 | 1);
            }
            state.t = instruction.BL.t;
            break;
         }
         case BX:
         {
            // FIXME: Doesnt support ThumbEE, but whatever
            printf(" %s\n", reg_name[instruction.BX.m]);
            CHECK_CONDITION;
            uint32_t address = state.r[instruction.BX.m];
            printf("Address: %08x\n", address);
            if ((address & 1) == 1)
            {
               state.t = 1;
               state.next_instruction = address & ~1;
            }
            else if ((address & 2) == 2)
            {
               state.t = 0;
               state.next_instruction = address;
            }
            else
               UNPREDICTABLE;
            break;
         }
         case PUSH:
         {
            uint8_t c = condition_passed(instruction.condition);
            printf(" { ");
            uint32_t address = state.SP - 4*BitCount(instruction.PUSH.registers);
            for (int i = 0; i <= 15; i++)
            {
               if (instruction.PUSH.registers & (1 << i))
               {
                  printf("%s ", reg_name[i]);
                  if (i == 13 && i != LowestSetBit(instruction.PUSH.registers))
                  {
                     if (c) write_mem(4, address, UNKNOWN);
                  }
                  else
                  {
                     if (c) write_mem(4, address, state.r[i]); 
                  }
                  address += 4;
               }
            }
            printf("}\n");
            if (c) state.SP -= 4*BitCount(instruction.PUSH.registers);
            break;
         }
         case POP:
         {
            uint8_t c = condition_passed(instruction.condition);
            printf(" { ");
            uint32_t address = state.SP;
            for (int i = 0; i < 15; i++)
            {
               if (instruction.POP.registers & (1 << i))
               {
                  printf("%s ", reg_name[i]);
                  if (c) state.r[i] = read_mem(4, address);
                  address += 4;
               }
            }
            if ((instruction.POP.registers >> 15) & 1)
            {
               printf("R15 ");
               if (c) LOAD_PC(read_mem(4, address));
            }
            printf("}\n");
            assert(!((instruction.POP.registers >> 13) & 1));
            if (c) state.SP += 4*BitCount(instruction.POP.registers);
            break;
         }
         case ADD_SPI:
         {
            printf(" %s, sp, #0x%x\n", reg_name[instruction.ADD_SPI.d], instruction.ADD_SPI.imm32);
            CHECK_CONDITION;
            uint32_t result;
            uint8_t carry;
            uint8_t overflow;
            AddWithCarry(state.SP, instruction.ADD_SPI.imm32, 0, &result, &carry, &overflow);
            state.r[instruction.ADD_SPI.d] = result;
            if (instruction.setflags && instruction.ADD_SPI.d != 15)
            {
               state.n = (result >> 31) & 1;
               state.z = (result == 0);
               state.c = carry;
               state.v = overflow;
            }
            break;
         }
         case SUB_SPI:
         {
            printf(" %s, #0x%x\n", reg_name[instruction.SUB_SPI.d], instruction.SUB_SPI.imm32);
            CHECK_CONDITION;
            uint32_t result;
            uint8_t carry;
            uint8_t overflow;
            AddWithCarry(state.SP, ~instruction.ADD_SPI.imm32, 1, &result, &carry, &overflow);
            if (instruction.setflags && instruction.ADD_SPI.d != 15)
            {
               state.n = (result >> 31) & 1;
               state.z = (result == 0);
               state.c = carry;
               state.v = overflow;
            }
            break;
         }
         case MOV_I:
         {
            printf(" %s, #0x%x\n", reg_name[instruction.MOV_I.d], instruction.MOV_I.imm32);
            CHECK_CONDITION;
            uint32_t result = instruction.MOV_I.imm32;
            state.r[instruction.MOV_I.d] = result;
            if (instruction.setflags && instruction.MOV_I.d != 15)
            {
               state.n = (result >> 31) & 1;
               state.z = (result == 0);
               state.c = instruction.MOV_I.d;
            }
            break;
         }
         case MOVT:
         {
            printf(" %s, #0x%x\n", reg_name[instruction.MOVT.d], instruction.MOVT.imm16);
            CHECK_CONDITION;
            state.r[instruction.MOVT.d] &= 0x0000ffff;
            state.r[instruction.MOVT.d] |= (instruction.MOVT.imm16 << 16);
            break;
         }
         case LDRB_I:
         {
            uint32_t offset_addr = state.r[instruction.LDRB_I.n] + (instruction.LDRB_I.add?(instruction.LDRB_I.imm32):(-instruction.LDRB_I.imm32));
            uint32_t address = instruction.LDRB_I.index?(offset_addr):state.r[instruction.LDRB_I.n];
            if (instruction.LDRB_I.index && !instruction.LDRB_I.wback) printf(" %s, [%s + {%d}]\n", reg_name[instruction.LDRB_I.t], reg_name[instruction.LDRB_I.n], instruction.LDRB_I.imm32);
            else if (instruction.LDRB_I.index && instruction.LDRB_I.wback) printf(" %s, [%s + %d]\n", reg_name[instruction.LDRB_I.t], reg_name[instruction.LDRB_I.n], instruction.LDRB_I.imm32);
            else if (!instruction.LDRB_I.index && instruction.LDRB_I.wback) printf(" %s, [%s] + %d\n", reg_name[instruction.LDRB_I.t], reg_name[instruction.LDRB_I.n], instruction.LDRB_I.imm32);
            CHECK_CONDITION;
            state.r[instruction.LDRB_I.t] = read_mem(1, address);
            if (instruction.LDRB_I.wback)
               state.r[instruction.LDRB_I.n] = offset_addr;
            break;
         }
         case CBNZ:
         {
            printf(" %s, 0x%x\n", reg_name[instruction.CBNZ.n], instruction.CBNZ.imm32 + state.PC);
            if (state.r[instruction.CBNZ.n] != 0)
            {
               LOAD_PC(state.PC + instruction.CBNZ.imm32);
            }
            break;
         }
         case CBZ:
         {
            printf(" %s, %08x\n", reg_name[instruction.CBNZ.n], instruction.CBNZ.imm32 + state.PC);
            if (state.r[instruction.CBNZ.n] == 0)
            {
               LOAD_PC(state.PC + instruction.CBNZ.imm32 | state.t);
            }
            break;
         }
         case CMP_R:
         {
            if (instruction.CMP_R.shift_t == LSL && instruction.CMP_R.shift_n == 0) printf(" %s, %s\n", reg_name[instruction.CMP_R.n], reg_name[instruction.CMP_R.m]);
            else printf(" %s, %s %s %d\n", reg_name[instruction.CMP_R.n], reg_name[instruction.CMP_R.m], shift_name[instruction.CMP_R.shift_t], instruction.CMP_R.shift_n);
            uint32_t shifted;
            uint32_t result;
            Shift(32, state.r[instruction.CMP_R.m], instruction.CMP_R.shift_t, instruction.CMP_R.shift_n, state.c, &shifted);
            AddWithCarry(state.r[instruction.CMP_R.n], ~shifted, 1, &result, &state.c, &state.v);
            state.z = (result == 0);
            state.n = (result >> 31) & 1;
            break;
         }
         case BKPT:
         {
            printf("\n");
            breakpoint_t* breakpoint = find_breakpoint(instruction.source_address);
            if (breakpoint->_stub != NULL)
            {
               printf("Calling stub for %s\n", breakpoint->symbol_name);
               state.r[0] = breakpoint->_stub();
               printf("Returning from stub for %s\n", breakpoint->symbol_name);
               LOAD_PC(state.LR);
               break;
            }
            else
            {
               printf("   *** Not implemented: %s\n", breakpoint->symbol_name);
               exit(0);
            }
         }
         case IT:
         {
            // This is quite complicated :(
            if (instruction.IT.mask & 0b1000) printf("");
            else if (instruction.IT.mask & 0b0100) printf("%s ", ((instruction.IT.firstcond & 1) == (instruction.IT.mask >> 3))?"t":"e");
            else if (instruction.IT.mask & 0b0010) printf("%s%s ", ((instruction.IT.firstcond & 1) == (instruction.IT.mask >> 3))?"t":"e",
                                                          ((instruction.IT.firstcond & 1) == (instruction.IT.mask >> 2))?"t":"e");
            else if (instruction.IT.mask & 0b0001) printf("%s%s%s ", ((instruction.IT.firstcond & 1) == (instruction.IT.mask >> 3))?"t":"e",
                                                          ((instruction.IT.firstcond & 1) == (instruction.IT.mask >> 2))?"t":"e",
                                                          ((instruction.IT.firstcond & 1) == (instruction.IT.mask >> 1))?"t":"e");
            printf("%s\n", condition_name[instruction.IT.firstcond]);
            state.itstate = (instruction.IT.firstcond << 4) | (instruction.IT.mask);
            break;
         }
         default:
            assert(0 && "Opcode not implemented");
      }
   }
}

int main(int argc, char** argv)
{
   if (argc != 2)
   {
      printf("Usage: %s <executable>\n", argv[0]);
      return -1;
   }
   register_stubs();
   state.next_instruction = load_executable(argv[1]);
   state.PC = 0;
   initialize_state();
   printf("Memory mapped. Starting execution at %08x\n", state.PC);
   step_machine(150);
   printf("Finished stepping\n");
   return 0;
}
