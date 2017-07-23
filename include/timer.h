
#ifndef TIMER_H
#define TIMER_H

#include <chrono>

namespace bkk
{
  namespace time
  {
    static std::chrono::time_point<std::chrono::high_resolution_clock> getCurrent()
    {
      return std::chrono::high_resolution_clock::now();
    }

    //Difference in milliseconds
    static float getDifference(const std::chrono::time_point<std::chrono::high_resolution_clock>& tStart,
      const std::chrono::time_point<std::chrono::high_resolution_clock>& tEnd)
    {
      return (float)std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    }
  }

}//namespace bkk
#endif  /*  TIMER_H  */