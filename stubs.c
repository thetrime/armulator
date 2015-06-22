#include "stub_glue.h"

__stub x_pthread_mutex_lock()
{
   // FIXME: Implement once I have threads!
   //uint32_t a0 = PTR_ARG(0);
   return 0;
}

__stub x_pthread_mutex_unlock()
{
   // FIXME: Implement once I have threads!
   return 0;
}
 
__stub x_pthread_once()
{
   return 0;
}

__stub x_pthread_self()
{
   return 0;
}

__stub x_pthread_setname_np()
{
   return 0;
}

__stub x_pthread_setschedparam()
{
   return 0;
}

__stub x_rand()
{
   return 0; // hehe
}


__stub x_CFRetain()
{
   return 0;
}

__stub x_CFRelease()
{
   return 0;
}

__stub x_MGCopyAnswer()
{
   return 0;
}


__stub x_OSSpinLockLock()
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
