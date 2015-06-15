static inline uint32_t
Bits32 (const uint32_t bits, const uint32_t msbit, const uint32_t lsbit)
{
    assert(msbit < 32 && lsbit <= msbit);
    return (bits >> lsbit) & ((1u << (msbit - lsbit + 1)) - 1);
}

static inline uint32_t
Bit32 (const uint32_t bits, const uint32_t bit)
{
    return (bits >> bit) & 1u;
}

static uint32_t ror(uint32_t val, uint32_t N, uint32_t shift)
{
    uint32_t m = shift % N;
    return (val >> m) | (val << (N - m));
}

static inline uint32_t bits(const uint32_t val, const uint32_t msbit, const uint32_t lsbit)
{
    return Bits32(val, msbit, lsbit);
}

static inline uint32_t bit(const uint32_t val, const uint32_t msbit)
{
    return bits(val, msbit, msbit);
}


// (imm32, carry_out) = ThumbExpandImm_C(imm12, carry_in)
static inline uint32_t ThumbExpandImm_C(uint32_t opcode, uint32_t carry_in, uint8_t *carry_out)
{
    uint32_t imm32; // the expaned result
    const uint32_t i = bit(opcode, 26);
    const uint32_t imm3 = bits(opcode, 14, 12);
    const uint32_t abcdefgh = bits(opcode, 7, 0);
    const uint32_t imm12 = i << 11 | imm3 << 8 | abcdefgh;

    if (bits(imm12, 11, 10) == 0)
    {
        switch (bits(imm12, 9, 8)) {
        case 0:
            imm32 = abcdefgh;
            break;

        case 1:
            imm32 = abcdefgh << 16 | abcdefgh;
            break;

        case 2:
            imm32 = abcdefgh << 24 | abcdefgh << 8;
            break;

        case 3:
            imm32 = abcdefgh  << 24 | abcdefgh << 16 | abcdefgh << 8 | abcdefgh; 
            break;
        }
        *carry_out = carry_in;
    }
    else
    {
        const uint32_t unrotated_value = 0x80 | bits(imm12, 6, 0);
        imm32 = ror(unrotated_value, 32, bits(imm12, 11, 7));
        *carry_out = Bit32(imm32, 31);
    }
    return imm32;
}

static inline uint32_t ThumbExpandImm(uint32_t opcode)
{
    // 'carry_in' argument to following function call does not affect the imm32 result.
    uint32_t carry_in = 0;
    uint8_t carry_out;
    return ThumbExpandImm_C(opcode, carry_in, &carry_out);
}

static inline int BitCount(uint32_t in)
{
   int j = 0;
   for (unsigned int i = 1; i != 0;  i<<=1)
      if (in & i)
         j++;
   return j;
}

static inline int LowestSetBit(uint32_t in)
{
   int j = 0;
   for (unsigned int i = 1; i != 0;  i<<=1, j++)
      if (in & i)
         return j;
   return 33;
}

// (imm32, carry_out) = ARMExpandImm_C(imm12, carry_in)
static inline uint32_t ARMExpandImm_C(uint32_t opcode, uint32_t carry_in, uint8_t *carry_out)
{
    uint32_t imm32;                         // the expanded result
    uint32_t imm = bits(opcode, 7, 0);      // immediate value
    uint32_t amt = 2 * bits(opcode, 11, 8); // rotate amount
    if (amt == 0)
    {
        imm32 = imm;
        *carry_out = carry_in;
    }
    else
    {
        imm32 = ror(imm, 32, amt);
        *carry_out = Bit32(imm32, 31);
    }
    return imm32;
}

static inline uint32_t ARMExpandImm(uint32_t imm12)
{
   uint32_t unrotated_value = imm12 & 0xFF;
   uint32_t ror_amount = ((imm12 >> 8) & 0xF) << 1;
   return (ror_amount == 0) ? unrotated_value :((unrotated_value >> ror_amount) | (unrotated_value << (32 - ror_amount)));
}
