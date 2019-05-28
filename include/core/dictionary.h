/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#pragma once

#include "vector"

namespace bkk
{
  namespace core
  {
    template <typename KEY_TYPE, typename VALUE_TYPE>
    class dictionary_t
    {
    public:

      void add(const KEY_TYPE& key, const VALUE_TYPE& value)
      {
        for (uint32_t i = 0; i < (uint32_t)keys_.size(); ++i)
        {
          if (key == keys_[i])
          {
            values_[i] = value;
            return;
          }
        }

        keys_.push_back(key);
        values_.push_back(value);
      }

      bool remove(const KEY_TYPE& key)
      {
        int32_t index = -1;
        for (uint32_t i = 0; i < (uint32_t)keys_.size(); ++i)
        {
          if (key == keys_[i])
          {
              uint32_t count = (uint32_t)keys_.size();
              if (i < count - 1)
              {
                keys_[i] = keys_[count - 1];
                values_[i] = values_[count - 1];
              }

              keys_.pop_back();
              values_.pop_back();
              return true;
            }
        }

        return false;
      }

      VALUE_TYPE* get(const KEY_TYPE& key)
      {
        for (uint32_t i = 0; i < (uint32_t)keys_.size(); ++i)
        {
          if (key == keys_[i])
            return &values_[i];
        }

        return nullptr;
      }
      
      std::vector<VALUE_TYPE>& data() { return values_; }

    private:
      std::vector<KEY_TYPE> keys_;
      std::vector<VALUE_TYPE> values_;
    };

  }//core
}//bkk
