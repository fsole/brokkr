/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#pragma once

#include "vector"
#include "string"

namespace bkk
{
  namespace core
  {
    static void splitString(const std::string& s, const char* delimiters, int delimiterCount, std::vector<std::string>* v)
    {
      size_t minPos = std::string::npos;
      size_t index = 0;
      do
      {
        minPos = std::string::npos;
        for (int i = 0; i < delimiterCount; ++i)
        {
          size_t delimiterPos = s.find(delimiters[i], index);
          if (delimiterPos < minPos) minPos = delimiterPos;
        }

        if (index != minPos)
          v->push_back(s.substr(index, minPos - index));

        index = minPos + 1;

      } while (minPos != std::string::npos);
    }

    static uint64_t hashString(const char *str)
    {
      uint64_t hash = 5381ul;
      uint32_t c;

      while (c = *str++)  hash = ((hash << 5) + hash) + c;

      return hash;
    }

  }//core
}//bkk