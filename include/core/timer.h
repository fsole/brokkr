/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef TIMER_H
#define TIMER_H

#include <chrono>

namespace bkk
{
  namespace core
  {
    namespace timer
    {
      typedef std::chrono::time_point<std::chrono::high_resolution_clock> time_point_t;

      static time_point_t getCurrent()
      {
        return std::chrono::high_resolution_clock::now();
      }

      //Difference in milliseconds
      static float getDifference(const time_point_t& tStart, const time_point_t& tEnd)
      {
        return (float)std::chrono::duration<double, std::milli>(tEnd - tStart).count();
      }

    }//timer
  }//core
}//bkk
#endif  /*  TIMER_H  */