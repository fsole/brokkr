
#ifndef TRANSFORM_MANAGER_H
#define TRANSFORM_MANAGER_H

#include "maths.h"
#include "packed-freelist.h"

namespace bkk
{
	struct transform_manager_t
	{
		
    bkk::handle_t createTransform(const maths::mat4& transform);
    bool destroyTransform(bkk::handle_t id);
    
    maths::mat4* getTransform(bkk::handle_t id);
    bool setTransform(bkk::handle_t id, const maths::mat4& transform);

    bool setParent(bkk::handle_t id, bkk::handle_t parentId);
    bkk::handle_t getParent(bkk::handle_t id);

    maths::mat4* getWorldMatrix(bkk::handle_t id);

    void update();

	private:

		//Sorts transform by hierarchy level
		void sortTransforms();

		packed_freelist_t<maths::mat4> transform_;  ///< Local transforms
		std::vector<bkk::handle_t> parent_;         ///< Parent of each transform
		std::vector<maths::mat4> world_;            ///< World transforms

		bool hierarchy_changed_;                    ///< Flag to indicates that the hierarchy has changed since the last update
	};

}//namespace bkk
#endif  //  TRANSFORM_MANAGER_H