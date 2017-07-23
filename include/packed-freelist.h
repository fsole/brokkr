#ifndef PACKED_FREELIST_H
#define PACKED_FREELIST_H

#include <vector>
#include <cassert>

namespace bkk
{
	struct handle_t
	{
		uint16_t index_;
		uint16_t generation_;
	};
	static const handle_t INVALID_ID = { 65535u,65535u };

	template <typename T>
	struct packed_freelist_t
	{
		packed_freelist_t() :headFreeList_(0u), elementCount_(0u) {}

		/**
		 * @brief Adds a new element to the list
		 * @param[in] data The data element to be added
		 * @return A valid ID to the element
		 */
		handle_t add(const T& data)
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
			headFreeList_ = freeList_[headFreeList_].index_;
			freeList_[index].index_ = elementCount_;

			handle_t id = { index, freeList_[index].generation_ };
			id_[elementCount_] = id;
			++elementCount_;
			return id;
		}

		/**
		 * @brief Get element given its ID
		 * @param[in] id The ID of the element
		 * @return A pointer to the element if the ID is valid, nullptr otherwise
		 */
		T* get(handle_t id)
		{
			uint32_t index;
			if (getIndexFromId(id, &index))
			{
				return &data_[index];
			}
			return nullptr;
		}

		/**
		 * @brief Swaps two elements
		 * @param[in] id0 The id of the first element
		 * @param[in] id1 The id of the second element
		 */
		void swap(handle_t id0, handle_t id1)
		{
			uint32_t index0;
			uint32_t index1;
			if (getIndexFromId(id0, &index0) && getIndexFromId(id1, &index1) && index0 != index1)
			{
				freeList_[id0.index_].index_ = index1;
				freeList_[id1.index_].index_ = index0;

				//Swap data
				T dataTmp = data_[index0];
				data_[index0] = data_[index1];
				data_[index1] = dataTmp;

				//Swap id
				handle_t idTmp = id_[index0];
				id_[index0] = id_[index1];
				id_[index1] = idTmp;
			}
		}

		/**
		 * @brief Removes an element given its ID
		 * @param[in] id The id of the element to remove
		 * @return True if the element has been removed, false if the element was not present
		 */
		bool remove(handle_t id)
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
				freeList_[id.index_].index_ = headFreeList_;
				freeList_[id.index_].generation_++;
				headFreeList_ = id.index_;

				--elementCount_;
				return true;
			}

			return false;
		}

		/**
		 * @brief Gets the id of an element given its index in the data vector
		 * @param[in] index The index of the element in the data vector
		 * @return The id of the element
		 */
		handle_t getIdFromIndex(uint32_t index) const
		{
			return id_[index];
		}

		/**
		 * @brief Gets the index of an element given its ID
		 * @param[in] id The id of the element
		 * @param[out] index The index in the data vector
		 * @return true if id is valid, false otherwise
		 */
		bool getIndexFromId(handle_t id, uint32_t* index) const
		{
			if (id.index_ < freeList_.size() && id.generation_ == freeList_[id.index_].generation_)
			{
				*index = freeList_[id.index_].index_;
				return true;
			}

			return false;
		}

		/**
		 * @brief Returns the number of elements in the vector
		 * @return The number of elements
		 */
		uint32_t getElementCount() const
		{
			return elementCount_;
		}

		/**
		 * @brief Get the packed data vector
		 * @return A reference to the data vector
		 */
		std::vector<T>& getData()
		{
			return data_;
		}

	private:

		std::vector<handle_t> freeList_;    ///< FreeList of IDs (vector with holes)
		uint16_t headFreeList_;         ///< Head of the freelist_

		std::vector<T> data_;           ///< Packed data
		std::vector<handle_t> id_;          ///< Id of each packed element (Needed to go from index to ID)
		uint16_t elementCount_;         ///< Number of packed elements
	};

	template <typename T>
	struct packed_freelist_iterator_t
	{
		packed_freelist_iterator_t<T>() = delete;

		packed_freelist_iterator_t<T>(packed_freelist_t<T>& list)
			: packedFreelist_(list),
			index_(0)
		{
		}

		packed_freelist_iterator_t<T>& operator++()
		{
			++index_;
			return *this;
		}

		T& get()
		{
			return packedFreelist_.getData()[index_];
		}

		bool end()
		{
			return index_ >= packedFreelist_.getElementCount();
		}

		packed_freelist_t<T>& packedFreelist_;
		uint32_t index_;
	};

}
#endif // PACKED_FREELIST_H