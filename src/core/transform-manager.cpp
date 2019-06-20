/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#include "core/transform-manager.h"
#include <algorithm>

using namespace bkk::core;

bkk_handle_t transform_manager_t::createTransform( const maths::mat4& transform )
{
  bkk_handle_t id = transform_.add( transform );
  if( id.index >= parent_.size() )
  {
    //Resize vectors
    uint32_t newSize = (uint32_t)parent_.size() + 1u;
    parent_.resize( newSize );
    world_.resize( newSize );
  }

  parent_[id.index] = BKK_NULL_HANDLE;
  hierarchy_changed_ = true;

  return id;
}

bool transform_manager_t::destroyTransform( bkk_handle_t id )
{
  hierarchy_changed_ = true;

  u32 index;
  u32 lastTransform( transform_.getElementCount()-1 );
  if( transform_.getIndexFromId( id, &index ) && index < lastTransform )
  {
    {
      bkk_handle_t temp = parent_[index];
      parent_[index] = parent_[lastTransform];
      parent_[lastTransform] = temp;
    }

    {
      maths::mat4 temp = world_[index];
      world_[index] = world_[lastTransform];
      world_[lastTransform] = temp;
    }
  }

  return transform_.remove( id );
}

maths::mat4* transform_manager_t::getTransform( bkk_handle_t id )
{
  return transform_.get(id);
}

bool transform_manager_t::setTransform( bkk_handle_t id, const maths::mat4& transform )
{
  maths::mat4* t = transform_.get(id);
  if( t )
  {
    *t = transform;
    return true;
  }

  return false;
}

bool transform_manager_t::setParent( bkk_handle_t id, bkk_handle_t parentId )
{
  hierarchy_changed_ = true;
  uint32_t index;
  if( transform_.getIndexFromId( id, &index ) )
  {
    parent_[index] = parentId;
    return true;
  }

  return false;
}

bkk_handle_t transform_manager_t::getParent( bkk_handle_t id )
{
  uint32_t index;
  if( transform_.getIndexFromId( id, &index ) )
  {
    return parent_[index];
  }

  return BKK_NULL_HANDLE;
}

maths::mat4* transform_manager_t::getWorldMatrix( bkk_handle_t id )
{
  uint32_t index;
  transform_.getIndexFromId( id, &index );

  if( transform_.getIndexFromId( id, &index ) )
  {
    return &world_[index];
  }

  return nullptr;
}

void transform_manager_t::sortTransforms()
{
  //1.Sort based on tree depth level to make sure we compute parent transform before its children
  struct transform_bkk_handle_t
  {
    bkk_handle_t id;
    bkk_handle_t parent;
    u32 level;
    bool operator<(const transform_bkk_handle_t& item) const{ return level < item.level; }
  };

  u32 count( transform_.getElementCount() );
  std::vector<transform_bkk_handle_t> orderedTransform(count);
  for( u32 i(0); i<count; ++i )
  {
    bkk_handle_t parentId = parent_[i];
    orderedTransform[i] = {transform_.getIdFromIndex( i ), parentId, 0};

    u32 parentIndex;
    while( transform_.getIndexFromId( parentId, &parentIndex ) )
    {
      orderedTransform[i].level++;
      parentId = parent_[parentIndex];
    }
  }

  std::sort(orderedTransform.begin(), orderedTransform.end());

  //2. Reorder transforms using the ordered helper vector
  for( u32 i(0); i<count; ++i )
  {
    transform_.swap( transform_.getIdFromIndex(i), orderedTransform[i].id );
    parent_[i] = orderedTransform[i].parent;
  }
}

void transform_manager_t::update()
{
  //Reorder transforms if hierarchy has changed since last update
  if( hierarchy_changed_ )
  {
    sortTransforms();
    hierarchy_changed_ = false;
  }

  //Update world transforms
  u32 parentIndex;

  maths::mat4* transforms;
  uint32_t transformCount = transform_.getData(&transforms);
  for( u32 i(0); i<transformCount; ++i )
  {
    world_[i] = transforms[i];
    if( transform_.getIndexFromId( parent_[i], &parentIndex ) )
    {
      world_[i] = world_[i] * world_[parentIndex];
    }
  }
}