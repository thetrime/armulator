#include "stub_glue.h"

__stub _pthread_mutex_lock()
{
   // FIXME: Implement once I have threads!
   //uint32_t a0 = PTR_ARG(0);
   return 0;
}

__stub _pthread_mutex_unlock()
{
   // FIXME: Implement once I have threads!
   return 0;
}
 
__stub _pthread_once()
{
   return 0;
}

__stub _pthread_self()
{
   return 0;
}

__stub _pthread_setname_np()
{
   return 0;
}

__stub _pthread_setschedparam()
{
   return 0;
}

__stub _rand()
{
   return 0; // hehe
}


__stub _CFRetain()
{
   return 0;
}

__stub _CFRelease()
{
   return 0;
}

__stub _MGCopyAnswer()
{
   return 0;
}


__stub _OSSpinLockLock()
{
   /*
   if ((*0xffff1020 & 0x9000) == 0x1000)
      return = *0x3f01213c;
   else if (*0xffff1020 == 0x8000)
      return *0x3f012134;
   else
      return *0x3f012138;
   */
}
