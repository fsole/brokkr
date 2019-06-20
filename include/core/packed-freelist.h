/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef PACKED_FREELIST_H
#define PACKED_FREELIST_H

#include "core/handle.h"

#include <assert.h>
#include <vector>

namespace bkk
{
  namespace core
  {

    template <typename T> class packed_freelist_iterator_t;

    template <typename T>
    class packed_freelist_t
    {
    public:
      packed_freelist_t() :headFreeList_(0u), elementCount_(0u) {}

      bkk_handle_t add(const T& data)
      {
        assert(elementCount_ < 65535u);

        //1. Add the new data to the data_ vector
        uint16_t size = (uint16_t)data_.size();
        if (elementCount_ == size)
        {
          //Make room for one more data element
          data_.resize(size + 1);
          id_.resize(size + 1);
        }
        data_[elementCount_] = data;

        //2. Allocate a new ID for the element
        size_t freeListSize = freeList_.size();
        if (headFreeList_ == freeListSize)
        {
          //Make room for one more id in the freelist
          freeList_.resize(size + 1);
          freeList_[size] = { uint16_t(size + 1u), 0u };
        }

        //Update the free list
        uint16_t index = headFreeList_;
        headFreeList_ = freeList_[headFreeList_].index;
        freeList_[index].index = elementCount_;

        bkk_handle_t id = { index, freeList_[index].generation };
        id_[elementCount_] = id;
        ++elementCount_;
        return id;
      }

      T* get(bkk_handle_t id)
      {
        uint32_t index;
        if (getIndexFromId(id, &index))
        {
          return &data_[index];
        }
        return nullptr;
      }

      void swap(bkk_handle_t id0, bkk_handle_t id1)
      {
        uint32_t index0;
        uint32_t index1;
        if (getIndexFromId(id0, &index0) && getIndexFromId(id1, &index1) && index0 != index1)
        {
          freeList_[id0.index].index = index1;
          freeList_[id1.index].index = index0;

          //Swap data
          T dataTmp = data_[index0];
          data_[index0] = data_[index1];
          data_[index1] = dataTmp;

          //Swap id
          bkk_handle_t idTmp = id_[index0];
          id_[index0] = id_[index1];
          id_[index1] = idTmp;
        }
      }

      bool remove(bkk_handle_t id)
      {
        uint32_t index;
        if (getIndexFromId(id, &index))
        {
          //If the item to remove is not the last item, move the last item to the gap
          uint32_t lastItem = elementCount_ - 1;
          if (index < lastItem)
          {
            swap(id_[lastItem], id_[index]);
          }

          //2. Update the free list
          freeList_[id.index].index = headFreeList_;
          freeList_[id.index].generation++;
          headFreeList_ = id.index;

          --elementCount_;
          return true;
        }

        return false;
      }

      bkk_handle_t getIdFromIndex(uint32_t index) const
      {
        return id_[index];
      }

      bool getIndexFromId(bkk_handle_t id, uint32_t* index) const
      {
        if (id.index < freeList_.size() && id.generation == freeList_[id.index].generation)
        {
          *index = freeList_[id.index].index;
          return true;
        }

        return false;
      }

      uint32_t getElementCount() const
      {
        return elementCount_;
      }

      uint32_t getData(T** data)
      {
        *data = data_.data();
        return (uint32_t)data_.size();
      }

      packed_freelist_iterator_t<T> begin()
      {
        return packed_freelist_iterator_t<T>(this, 0);
      }

      packed_freelist_iterator_t<T> end()
      {
        return packed_freelist_iterator_t<T>(this, getElementCount() );
      }

    private:

      std::vector<bkk_handle_t> freeList_;  // Free list of IDs (vector with holes)
      uint16_t headFreeList_;           // Head of the free list (fist free element in freeList_)

      std::vector<T> data_;             // Packed data
      std::vector<bkk_handle_t> id_;        // Id of each packed element (Needed to go from index to ID)
      uint16_t elementCount_;           // Number of packed elements
    };

    template <typename T>
    class packed_freelist_iterator_t
    {
    public:
      packed_freelist_iterator_t()
      :packedFreelist_(nullptr), index_(0){}

      packed_freelist_iterator_t(packed_freelist_t<T>* list, uint32_t index)
      :packedFreelist_(list), index_(index){}

      bool operator==(const packed_freelist_iterator_t<T>& it)
      {
        return (packedFreelist_ == it.packedFreelist_ &&  index_ == it.index_);
      }

      bool operator!=(const packed_freelist_iterator_t<T>& it)
      {
        return !(*this == it);
      }

      packed_freelist_iterator_t<T>& operator++()
      {
        ++index_;
        return *this;
      }

      T& get()
      {
        T* data;
        packedFreelist_->getData(&data);
        return data[index_];
      }

    private:
      packed_freelist_t<T>* packedFreelist_;
      uint32_t index_;
    };
  }//core
}//bkk
#endif // PACKED_FREELIST_H