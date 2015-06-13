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
