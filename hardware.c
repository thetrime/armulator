#include "machine.h"
#include <stdlib.h>

void configure_hardware()
{
   // According to http://kppmu.go.th/images/sym/root/usr/src/sys/arm/mv/kirkwood/sheevaplug.c
   /*
    * Virtual address space layout:
    * -----------------------------
    * 0x0000_0000 - 0x7FFF_FFFF	: User Process (2 GB)
    * 0x8000_0000 - 0xBBFF_FFFF	: Unused (960 MB)
    * 0xBC00_0000 - 0xBDFF_FFFF	: Device Bus: CS1 (32 MB)
    * 0xBE00_0000 - 0xBECF_FFFF	: Unused (13 MB)
    * 0xBED0_0000 - 0xBEDF_FFFF	: Device Bus: CS2 (1 MB)
    * 0xBEE0_0000 - 0xBEEF_FFFF	: Device Bus: CS0 (1 MB)
    * 0xBEF0_0000 - 0xBEFF_FFFF	: Device Bus: BOOT (1 MB)
    * 0xBF00_0000 - 0xBFFF_FFFF	: Unused (16 MB)
    * 0xC000_0000 - virtual_avail	: Kernel Reserved (text, data, page tables,
    * 				: stack etc.)
    * virtual-avail - 0xEFFF_FFFF	: KVA (virtual_avail is typically < 0xc0a0_0000)
    * 0xF000_0000 - 0xF0FF_FFFF	: No-Cache allocation area (16 MB)
    * 0xF100_0000 - 0xF10F_FFFF	: SoC Integrated devices registers range (1 MB)
    * 0xF110_0000 - 0xF11F_FFFF	: CESA SRAM (1 MB)
    * 0xF120_0000 - 0xFFFE_FFFF	: Unused (237 MB + 960 kB)
    * 0xFFFF_0000 - 0xFFFF_0FFF	: 'High' vectors page (4 kB)
    * 0xFFFF_1000 - 0xFFFF_1FFF	: ARM_TP_ADDRESS/RAS page (4 kB)
    * 0xFFFF_2000 - 0xFFFF_FFFF	: Unused (56 kB)
    */

   unsigned char* arm_tp_data = malloc(4096);   
   map_memory(arm_tp_data, 0xffff1000, 4096);
   // In reality, the value 0xffff1020 is the CPU capabilities. Report that we have kUP and khasEvent which I assume are a uniprocessor and the WFE instruction
   write_mem(4, 0xffff1020, 0x9000);
}
