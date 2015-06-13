#define BEGIN_STUBS void register_stubs() {
#define END_STUBS }
#define STUB(p) register_stub(#p, p)
#include "loader.h"
#define __stub uint32_t

uint32_t PTR_ARG(int i);
