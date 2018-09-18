/*
* Brokkr framework
*
* Copyright(c) 2017 by Ferran Sole
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files(the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions :
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#ifndef DYNAMIC_STRING_H
#define DYNAMIC_STRING_H

#include <string.h>
#include <stdint.h>

namespace bkk
{
  struct string_t
  {
    char* data_;
    uint32_t size_;

    string_t();
    string_t(const string_t& s);
    string_t(const char* s);

    ~string_t();
    
    inline const char* c_str() const
    {
      return data_;
    }

    void clear();
    bool empty() const;

    bool operator==(const string_t& s) const;
    bool operator==(const char* s) const;
    bool operator!=(const string_t& s) const;
    bool operator!=(const char* s) const;
    bool operator<(const string_t& s) const;

    string_t& operator=(const char* s);
    string_t& operator=(const string_t& s);

    string_t& operator+=(const string_t& s);
    string_t& operator+=(const char* s);

    string_t operator+(const string_t& s) const;
    string_t operator+(const char* s) const;

    //Utility
    string_t substr(uint32_t first, uint32_t last) const;
    int32_t findLastOf(char c) const;
  };
}

#endif 