/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef HANDLE_H
#define HANDLE_H

#include <stdint.h>

namespace bkk
{
  namespace core
  {
    template <size_t N1, size_t N2>
    struct generic_handle_t
    {
      uint32_t index : N1;
      uint32_t generation : N2;
    };

    template <size_t N1, size_t N2>
    bool operator==(const generic_handle_t<N1,N2>& h0, const generic_handle_t<N1, N2>& h1)
    {
      return (h0.index == h1.index) && (h0.generation == h1.generation);
    }

    template <size_t N1, size_t N2>
    bool operator!=(const generic_handle_t<N1, N2>& h0, const generic_handle_t<N1, N2>& h1)
    {
      return (h0.index != h1.index) || (h0.generation != h1.generation);
    }

    typedef generic_handle_t<16u, 16u> bkk_handle_t;
    static const bkk_handle_t BKK_NULL_HANDLE = { 65535u,65535u };
  }
}

#endif HANDLE_H