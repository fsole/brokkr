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

#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

#include <stdint.h>

namespace bkk
{  
  namespace core
  {
    template <typename T>
    struct dynamic_array_t
    {
      uint32_t size_;
      uint32_t capacity_;
      T* data_;

      dynamic_array_t()
        :size_(0u),
        capacity_(0u),
        data_(nullptr)
      {
      }

      dynamic_array_t(uint32_t size)
        :dynamic_array_t()
      {
        growArray(size);
        size_ = size;
      }

      dynamic_array_t(const dynamic_array_t& v)
        :dynamic_array_t()
      {
        operator=(v);
      }

      ~dynamic_array_t()
      {
        if (data_)
          delete[] data_;
      }

      dynamic_array_t& operator=(const dynamic_array_t<T>& v)
      {
        clear();
        resize(v.size_);
        memcpy(data_, v.data_, capacity_ * sizeof(T));
        return *this;
      }

      void clear()
      {
        size_ = capacity_ = 0;
        if (data_)
        {
          delete[] data_;
          data_ = nullptr;
        }
      }

      uint32_t size() const
      {
        return size_;
      }

      bool empty()
      {
        return size_ == 0;
      }

      T& operator[](uint32_t index)
      {
        return data_[index];
      }

      const T& operator[](uint32_t index) const
      {
        return data_[index];
      }

      void resize(uint32_t newSize)
      {
        growArray(newSize);
        size_ = newSize;
      }

      T* data()
      {
        return data_;
      }

      const T* data() const
      {
        return data_;
      }

      T& front()
      {
        return data_[0];
      }

      void push_back(const T& v)
      {
        if (size_ == capacity_)
        {
          growArray(size_ + 1);
        }

        data_[size_++] = v;
      }

      void sort()
      {
        mergeSort(data_, 0, size_ - 1);
      }

      void swap(uint32_t a, uint32_t b)
      {
        T temp = data_[a];
        data_[a] = data_[b];
        data_[b] = temp;
      }

    private:

      void growArray(uint32_t newSize)
      {
        if (newSize > capacity_)
        {
          T* oldData = data_;

          uint32_t growSize = newSize + newSize / 2;
          data_ = new T[growSize];

          if (oldData)
          {
            memcpy(data_, oldData, sizeof(T)*size_);
            delete[] oldData;
          }

          //Initialize new memory to zero
          memset(data_ + capacity_, 0, sizeof(T)*(growSize - capacity_));
          
          capacity_ = growSize;
        }
      }

      void merge(T *a, uint32_t low, uint32_t high, uint32_t mid)
      {
        dynamic_array_t<T> temp(high - low + 1);
        uint32_t i = low;
        uint32_t j = mid + 1;
        uint32_t k = 0;
        while (i <= mid && j <= high)
        {
          if (a[i] < a[j])
          {
            temp[k] = a[i];
            k++;
            i++;
          }
          else
          {
            temp[k] = a[j];
            k++;
            j++;
          }
        }

        while (i <= mid)
        {
          temp[k] = a[i];
          k++;
          i++;
        }

        while (j <= high)
        {
          temp[k] = a[j];
          k++;
          j++;
        }

        for (i = low; i <= high; i++)
        {
          a[i] = temp[i - low];
        }
      }

      void mergeSort(T *a, uint32_t low, uint32_t high)
      {
        uint32_t mid;
        if (low < high)
        {
          mid = (low + high) / 2;
          mergeSort(a, low, mid);
          mergeSort(a, mid + 1, high);
          merge(a, low, high, mid);
        }
      }

    };
  }
}

#endif
