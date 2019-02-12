#pragma once

#include "vector"

namespace bkk
{
  namespace core
  {
    template <typename KEY_TYPE, typename VALUE_TYPE>
    class hash_table_t
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
        //TODO: Move last item to the gap or just resize if item to remove is the last item in the list
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
