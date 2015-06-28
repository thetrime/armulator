#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "machine.h"

#define A0 state.r[0]
#define A1 state.r[1]
#define A2 state.r[2]
#define A3 state.r[3]
// CHECKME: Is A5-A9 correct?
#define A4 (uint32_t)read_mem(4, state.SP-4)
#define A5 (uint32_t)read_mem(4, state.SP-8)
#define A6 (uint32_t)read_mem(4, state.SP-12)
#define A7 (uint32_t)read_mem(4, state.SP-16)
#define A8 (uint32_t)read_mem(4, state.SP-20)
#define A9 (uint32_t)read_mem(4, state.SP-24)


#define abort(...) {printf(__VA_ARGS__); assert(0);}

/* Mach .......... */
uint32_t mach_task_self()
{
   printf(" .... Hello from mach_task_self()!\n");
   return 0;
}

uint32_t mach_reply_port()
{
   printf(" .... Hello from mach_reply_port()!\n");
   return 0;
}

uint32_t mach_msg_trap()
{
   printf(" .... Hello from mach_message_trap(%08x,%08x,%08x,%08x,%08x,%08x,%08x)!\n", A0, A1, A2, A3, A4, A5, A6);
   return 0;
}

/* Posix ............... */

uint32_t posix_sigprocmask()
{
   printf(" .... Hello from sigprocmask(%08x, %08x, %08x)\n", A0, A1, A2);
   return 0;
}

uint32_t posix_getpid()
{
   printf(" .... Hello from getpid()\n");
   return 0xdeadbeef;
}

uint32_t posix_kill()
{
   printf(" .... Hello from kill(%08x, %08x)\n", A0, A1);
   return 0;   
}


uint32_t (*mach_call[256])(void) = {[0x1a] = mach_reply_port,
                                    [0x1c] = mach_task_self,
                                    [0x1f] = mach_msg_trap};
uint32_t (*posix_call[256])(void) = {[0x14] = posix_getpid,
                                     [0x25] = posix_kill,
                                     [0x30] = posix_sigprocmask};
   
uint32_t syscall(int32_t number)
{
   if (number < 0)
   {
      if (mach_call[-number] == NULL)
         abort("mach trap 0x%x is not implemented", -number);
      return mach_call[-number]();
   }
   else
   {
      if (posix_call[number] == NULL)
         abort("posix syscall 0x%x is not implemented", number);     
      return posix_call[number]();
   }
}
