int cp15_accept(uint32_t);
uint32_t cp15_read(uint8_t size, uint8_t CRn, uint8_t opc1, uint8_t CRm, uint8_t opc2);
void cp15_init();
