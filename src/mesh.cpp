
#include "mesh.h"

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>

#include <float.h> //FLT_MAX
#include <map>
#include <cassert>

using namespace bkk;
using namespace bkk::mesh;
using namespace bkk::maths;

static size_t GetNextMultiple(size_t from, size_t multiple)
{
  return ((from + multiple - 1) / multiple) * multiple;
}

void mesh::create( const render::context_t& context,
                   const uint32_t* indexData, uint32_t indexDataSize,
                   const void* vertexData, size_t vertexDataSize,
                   render::vertex_attribute_t* attribute, uint32_t attributeCount,
                   mesh_t* mesh, render::gpu_memory_allocator_t* allocator )
{
  //Create vertex format
  render::vertexFormatCreate(attribute, attributeCount, &mesh->vertexFormat_);

  mesh->indexCount_ = (u32)indexDataSize / sizeof(uint32_t);
  mesh->vertexCount_ = (u32)vertexDataSize / mesh->vertexFormat_.vertexSize_;

  render::gpuBufferCreate(context, render::gpu_buffer_usage_e::INDEX_BUFFER, (void*)indexData, (size_t)indexDataSize, allocator, &mesh->indexBuffer_);
  render::gpuBufferCreate(context, render::gpu_buffer_usage_e::VERTEX_BUFFER, (void*)vertexData, (size_t)vertexDataSize, allocator, &mesh->vertexBuffer_);

  /*
  //Create index buffer
  VkBufferCreateInfo bufferCreateInfo = {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = static_cast<uint32_t> ( indexDataSize );
  bufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  vkCreateBuffer(context.device_, &bufferCreateInfo, nullptr, &mesh->indexBuffer_);

  //Create vertex buffer
  bufferCreateInfo = {};
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = static_cast<uint32_t> (vertexDataSize);
  bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  vkCreateBuffer(context.device_, &bufferCreateInfo, nullptr, &mesh->vertexBuffer_ );

  //Get memory requirements of each buffer
  VkMemoryRequirements indexBufferMemoryRequirements = {};
  vkGetBufferMemoryRequirements(context.device_, mesh->indexBuffer_, &indexBufferMemoryRequirements);

  VkMemoryRequirements vertexBufferMemoryRequirements = {};
  vkGetBufferMemoryRequirements(context.device_, mesh->vertexBuffer_, &vertexBufferMemoryRequirements);

  //Compute total size needed for both buffers
  VkDeviceSize bufferSize = vertexBufferMemoryRequirements.size;
  VkDeviceSize indexBufferOffset = GetNextMultiple(bufferSize,indexBufferMemoryRequirements.alignment);

  bufferSize = indexBufferOffset + indexBufferMemoryRequirements.size;
  mesh->memory_=  render::gpuMemoryAllocate( context, bufferSize, vertexBufferMemoryRequirements.alignment, 0xFFFF, render::gpu_memory_type_e::HOST_VISIBLE, allocator );

  //Bind memory to the buffers
  vkBindBufferMemory(context.device_, mesh->vertexBuffer_, mesh->memory_.handle_, mesh->memory_.offset_ );
  vkBindBufferMemory(context.device_, mesh->indexBuffer_, mesh->memory_.handle_, mesh->memory_.offset_ + indexBufferOffset);

  //Fill the buffers
  uint8_t* mapping = (uint8_t*)render::gpuMemoryMap( context, mesh->memory_);
  assert(mapping);

  memcpy(mapping, vertexData, vertexDataSize);
  memcpy(mapping + indexBufferOffset,indexData, indexDataSize);
  render::gpuMemoryUnmap( context, mesh->memory_ );
  */

  
}

static void CountNodes( aiNode* node, u32& total )
{
  if( node )
  {
    total++;
    for( u32 i(0); i<node->mNumChildren; ++i )
    {
      CountNodes( node->mChildren[i], total );
    }
  }
}

static void TraverseScene( const aiNode* pNode, std::map<std::string,u32>& boneNameToIndex, const aiMesh* mesh, skeleton_t* skeleton, u32& nodeIndex, u32& boneIndex, s32 parentIndex )
{
  std::string nodeName = pNode->mName.data;
  s32 bone = -1;
  for (uint32_t i = 0; i < mesh->mNumBones; i++)
  {
    std::string boneName(mesh->mBones[i]->mName.data);
    if( boneName == nodeName )
    {
      bone = i;
      break;
    }
  }

  if( bone != -1 )
  {
    //Node is a bone
    skeleton->offset_[nodeIndex] = (f32*)&mesh->mBones[bone]->mOffsetMatrix.Transpose().a1;
    skeleton->isBone_[nodeIndex] = true;
    boneNameToIndex[nodeName] = boneIndex;
    ++boneIndex;
  }
  else
  {
    //Node is not a bone. Store its local transformation in the offset vector
    aiMatrix4x4 localTransform = pNode->mTransformation;
    localTransform.Transpose();
    skeleton->offset_[nodeIndex] = (f32*)&localTransform.a1;
    skeleton->isBone_[nodeIndex] = false;
  }

  skeleton->parent_[nodeIndex] = parentIndex;
  parentIndex = nodeIndex;
  nodeIndex++;

  //Recurse on the children
  for( u32 i(0); i<pNode->mNumChildren; ++i )
  {
    TraverseScene( pNode->mChildren[i], boneNameToIndex, mesh, skeleton, nodeIndex, boneIndex, parentIndex );
  }
}

static void LoadSkeleton( const aiScene* scene, const aiMesh* mesh, std::map<std::string,u32>& boneNameToIndex, skeleton_t* skeleton )
{
  u32 nodeCount(0);
  CountNodes( scene->mRootNode, nodeCount );

  skeleton->parent_ = new s32[nodeCount];
  skeleton->offset_ = new maths::mat4[nodeCount];
  skeleton->isBone_ = new bool[nodeCount];
  aiMatrix4x4 globalInverse = scene->mRootNode->mTransformation;
  globalInverse.Inverse();
  skeleton->globalInverseTransform_ = (f32*)&globalInverse.Transpose().a1;
  skeleton->boneCount_ = mesh->mNumBones;
  skeleton->nodeCount_ = nodeCount;

  u32 nodeIndex = 0;
  u32 boneIndex = 0;
  TraverseScene( scene->mRootNode, boneNameToIndex, mesh, skeleton, nodeIndex, boneIndex, -1 );
}

static void LoadAnimation(const aiScene* scene, u32 animationIndex, std::map<std::string,u32>& boneNameToIndex, u32 boneCount, skeletal_animation_t* animation )
{
  const aiAnimation* pAnimation = scene->mAnimations[animationIndex];
  u32 frameCount = 0;
  if( pAnimation )
  {
    for( u32 channel(0); channel<pAnimation->mNumChannels; ++channel )
    {
      std::string nodeName(pAnimation->mChannels[channel]->mNodeName.data);
      std::map<std::string,u32>::iterator it = boneNameToIndex.find(nodeName);
      if( it != boneNameToIndex.end() )
      {
        if( frameCount == 0 )
        {
          frameCount = pAnimation->mChannels[channel]->mNumPositionKeys;
          animation->data_ = new bone_transform_t[frameCount*boneCount];
          animation->frameCount_ = frameCount;
        }

        //Read animation data for the bone
        for( u32 frame = 0; frame<frameCount; ++frame)
        {
          size_t index = frame*boneCount + it->second;
          animation->data_[index].position_ = vec3( pAnimation->mChannels[channel]->mPositionKeys[frame].mValue.x,
                                                    pAnimation->mChannels[channel]->mPositionKeys[frame].mValue.y,
                                                    pAnimation->mChannels[channel]->mPositionKeys[frame].mValue.z);

          animation->data_[index].scale_ = vec3( pAnimation->mChannels[channel]->mScalingKeys[frame].mValue.x,
                                                 pAnimation->mChannels[channel]->mScalingKeys[frame].mValue.y,
                                                 pAnimation->mChannels[channel]->mScalingKeys[frame].mValue.z);

          animation->data_[index].orientation_ = quat( pAnimation->mChannels[channel]->mRotationKeys[frame].mValue.x,
                                                       pAnimation->mChannels[channel]->mRotationKeys[frame].mValue.y,
                                                       pAnimation->mChannels[channel]->mRotationKeys[frame].mValue.z,
                                                       pAnimation->mChannels[channel]->mRotationKeys[frame].mValue.w);
        }
      }
    }
  }
}

void mesh::createFromFile( const render::context_t& context, const char* file, mesh_t* mesh, render::gpu_memory_allocator_t* allocator  )
{
  Assimp::Importer Importer;
  int flags = aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights | aiProcess_GenSmoothNormals;
  const struct aiScene* scene = Importer.ReadFile(file,flags);
  assert( scene && scene->mNumMeshes > 0 );

  const struct aiMesh* aimesh = scene->mMeshes[0];
  size_t vertexCount = aimesh->mNumVertices;

  bool bHasNormal(aimesh->HasNormals() );
  bool bHasUV( aimesh->HasTextureCoords(0) );
  u32 boneCount( aimesh->mNumBones );

  u32 vertexSize = 3;
  u32 attributeCount = 1;

  if( bHasNormal )
  {
    vertexSize += 3;
    ++attributeCount;
  }
  if( bHasUV)
  {
    vertexSize += 2;
    ++attributeCount;
  }
  if( boneCount > 0 )
  {
    vertexSize += 8;  //4 weights and 4 bone index
    attributeCount += 2;
  }

  //Attributes description
  std::vector<render::vertex_attribute_t> attributes(attributeCount);

  //First attribute is position
  attributes[0].format_ = render::attribute_format_e::VEC3;
  attributes[0].offset_ = 0;
  attributes[0].stride_ = vertexSize * sizeof(f32);

  u32 attribute = 1;
  u32 attributeOffset = 3;
  if( bHasNormal )
  {
    attributes[attribute].format_ = render::attribute_format_e::VEC3;
    attributes[attribute].offset_ = sizeof(f32)*attributeOffset;
    attributes[attribute].stride_ = vertexSize * sizeof(f32);
    ++attribute;
    attributeOffset += 3;
  }
  if( bHasUV )
  {
    attributes[attribute].format_ = render::attribute_format_e::VEC2;
    attributes[attribute].offset_ = sizeof(f32)*attributeOffset;
    attributes[attribute].stride_ = vertexSize * sizeof(f32);
    ++attribute;
    attributeOffset += 2;
  }

  u32 boneWeightOffset = attributeOffset;
  if( boneCount > 0 )
  {
    attributes[attribute].format_ = render::attribute_format_e::VEC4;
    attributes[attribute].offset_ = sizeof(f32)*attributeOffset;
    attributes[attribute].stride_ = vertexSize * sizeof(f32);

    ++attribute;
    attributeOffset += 4;

    attributes[attribute].format_ = render::attribute_format_e::VEC4;
    attributes[attribute].offset_ = sizeof(f32)*attributeOffset;
    attributes[attribute].stride_ = vertexSize * sizeof(f32);
  }

  vec3 aabbMin( FLT_MAX, FLT_MAX, FLT_MAX );
  vec3 aabbMax( -FLT_MAX, -FLT_MAX, -FLT_MAX );
  size_t vertexBufferSize( vertexCount * vertexSize * sizeof(f32) );
  f32* vertexData = new f32[ vertexCount * vertexSize  ];
  u32 index(0);
  for( u32 vertex(0); vertex<vertexCount; ++vertex )
  {
	  aabbMin = vec3( maths::myMin( aimesh->mVertices[vertex].x, aabbMin.x ),
                      maths::myMin( aimesh->mVertices[vertex].y, aabbMin.y ),
                      maths::myMin( aimesh->mVertices[vertex].z, aabbMin.z ) );

	  aabbMax = vec3( maths::myMax( aimesh->mVertices[vertex].x, aabbMax.x ),
                      maths::myMax( aimesh->mVertices[vertex].y, aabbMax.y ),
                      maths::myMax( aimesh->mVertices[vertex].z, aabbMax.z ) );

    vertexData[index++] = aimesh->mVertices[vertex].x;
    vertexData[index++] = aimesh->mVertices[vertex].y;
    vertexData[index++] = aimesh->mVertices[vertex].z;

    if( bHasNormal )
    {
      vertexData[index++] = aimesh->mNormals[vertex].x;
      vertexData[index++] = aimesh->mNormals[vertex].y;
      vertexData[index++] = aimesh->mNormals[vertex].z;
    }
    if( bHasUV )
    {
      vertexData[index++] = aimesh->mTextureCoords[0][vertex].x;
      vertexData[index++] = aimesh->mTextureCoords[0][vertex].y;
    }

    if( boneCount > 0 )
    {
      //Bone weight
      vertexData[index++] = 0.0f;
      vertexData[index++] = 0.0f;
      vertexData[index++] = 0.0f;
      vertexData[index++] = 0.0f;

      //Bone index
      vertexData[index++] = 0.0f;
      vertexData[index++] = 0.0f;
      vertexData[index++] = 0.0f;
      vertexData[index++] = 0.0f;
    }
  }

  //Skeletal animations
  mesh->skeleton_ = nullptr;
  mesh->animations_ = nullptr;

  std::map<std::string, u32> boneNameToIndex;
  if( boneCount > 0 )
  {
    mesh->skeleton_ = new skeleton_t;
    LoadSkeleton( scene, aimesh, boneNameToIndex, mesh->skeleton_ );
  }

  //Read weights and bone indices for each vertex
  for (uint32_t i = 0; i < boneCount; i++)
  {
    std::string boneName(aimesh->mBones[i]->mName.data);
    std::map<std::string, u32>::iterator it;
    it = boneNameToIndex.find( boneName );
    if( it != boneNameToIndex.end() )
    {
      u32 vertexCount = aimesh->mBones[i]->mNumWeights;
      for( u32 vertex(0); vertex<vertexCount; ++vertex )
      {
        u32 vertexId = aimesh->mBones[i]->mWeights[vertex].mVertexId;
        f32 weight = aimesh->mBones[i]->mWeights[vertex].mWeight;

        size_t vertexWeightOffset = vertexId * vertexSize + boneWeightOffset;
        size_t vertexBoneIdOffset = vertexId * vertexSize + boneWeightOffset + 4;
        while( vertexData[vertexWeightOffset] != 0.0f )
        {
          vertexWeightOffset++;
          vertexBoneIdOffset++;
        }
        vertexData[vertexWeightOffset] = weight;
        vertexData[vertexBoneIdOffset] = (f32)it->second;
      }
    }
  }

  //Load animations
  if(scene->HasAnimations() )
  {
    mesh->animationCount_ = scene->mNumAnimations;
    mesh->animations_ = new skeletal_animation_t[mesh->animationCount_];
    for( u32 i(0); i<mesh->animationCount_; ++i )
    {
      LoadAnimation( scene, i, boneNameToIndex, boneCount, &mesh->animations_[i] );
    }
  }

  uint32_t* indices(0);
  uint32_t indexBufferSize(0);
  if( aimesh->HasFaces() )
  {
    u32 indexCount = aimesh->mNumFaces * 3; //@WARNING: Assuming triangles!
    indexBufferSize = indexCount * sizeof( uint32_t );
    indices = new u32[indexCount];
    for( u32 face(0); face<aimesh->mNumFaces; ++face )
    {
      indices[face*3] = aimesh->mFaces[face].mIndices[0];
      indices[face*3 + 1] = aimesh->mFaces[face].mIndices[1];
      indices[face*3 + 2] = aimesh->mFaces[face].mIndices[2];
    }
  }

  mesh->aabb_.min_ = aabbMin;
  mesh->aabb_.max_ = aabbMax;

  create( context, indices, indexBufferSize, vertexData, vertexBufferSize, &attributes[0], attributeCount, mesh, allocator);

  delete[] vertexData;
  delete[] indices;
}

void mesh::destroy( const render::context_t& context, mesh_t* mesh, render::gpu_memory_allocator_t* allocator )
{
  render::gpuBufferDestroy(context, &mesh->indexBuffer_, allocator);
  render::gpuBufferDestroy(context, &mesh->vertexBuffer_, allocator);
  //vkDestroyBuffer(context.device_, mesh->vertexBuffer_, nullptr);
  //vkDestroyBuffer(context.device_, mesh->indexBuffer_, nullptr);
  //render::gpuMemoryDeallocate(context, mesh->memory_, allocator);

  if( mesh->skeleton_ )
  {
    delete[] mesh->skeleton_->parent_;
    delete[] mesh->skeleton_->offset_;
    delete[] mesh->skeleton_->isBone_;
    delete mesh->skeleton_;
  }

  if( mesh->animationCount_ > 0 )
  {
    for( u32 i(0); i<mesh->animationCount_; ++i )
    {
      delete[] mesh->animations_[i].data_;
    }

    delete mesh->animations_;
  }

  vertexFormatDestroy( &mesh->vertexFormat_ );
}

void mesh::draw( VkCommandBuffer commandBuffer, const mesh_t& mesh )
{
  vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer_.handle_, 0, VK_INDEX_TYPE_UINT32);

  uint32_t attributeCount = mesh.vertexFormat_.attributeCount_;
  std::vector<VkBuffer> buffers(attributeCount);
  std::vector<VkDeviceSize> offsets(attributeCount);
  for( uint32_t i(0); i<attributeCount; ++i )
  {
    buffers[i] = mesh.vertexBuffer_.handle_;
    offsets[i] = 0u;
  }

  vkCmdBindVertexBuffers(commandBuffer, 0, attributeCount, &buffers[0], &offsets[0]);
  vkCmdDrawIndexed(commandBuffer, mesh.indexCount_, 1, 0, 0, 0);
}

void mesh::animatorCreate( const render::context_t& context, const mesh_t& mesh, u32 animationIndex, float duration, skeletal_animator_t* animator )
{
  // @TODO
  animator->cursor_ = 0.0f;
  animator->duration_ = duration;

  animator->skeleton_ = mesh.skeleton_;
  animator->animation_ = &mesh.animations_[animationIndex];

  animator->localPose_ = new maths::mat4[mesh.skeleton_->nodeCount_];
  animator->globalPose_ = new maths::mat4[mesh.skeleton_->nodeCount_];
  animator->boneTransform_ = new maths::mat4[mesh.skeleton_->boneCount_];

  //Create an uninitialized uniform buffer
  render::gpuBufferCreate( context, render::gpu_buffer_usage_e::UNIFORM_BUFFER,
                           render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
                           nullptr, sizeof(maths::mat4) * mesh.skeleton_->boneCount_,
                           &animator->buffer_ );
}


void mesh::animatorUpdate(const render::context_t& context, f32 time, skeletal_animator_t* animator )
{
  while( time > animator->duration_ )
  {
    time -= animator->duration_;
  }

  animator->cursor_ = time / animator->duration_;

  //Find out frames between which we need to interpolate
  u32 frameCount = animator->animation_->frameCount_ - 1 ;
  u32 frame0 = (u32)floor( (frameCount) * animator->cursor_ );
  u32 frame1 = maths::myMin( frame0 + 1, frameCount );

  //Local cursor between frames
  f32 t = ( animator->cursor_ - ( (f32)frame0 / (f32)frameCount ) ) / ( ((f32)frame1 / (f32)frameCount ) - ( (f32)frame0 / (f32)frameCount ) );

  //Pointers to animation data
  bone_transform_t* transform0 = &animator->animation_->data_[ frame0 * animator->skeleton_->boneCount_];
  bone_transform_t* transform1 = &animator->animation_->data_[ frame1 * animator->skeleton_->boneCount_];

  //Compute new bone transforms
  maths::mat4* localPose = animator->localPose_;
  maths::mat4* globalPose = animator->globalPose_;
  maths::mat4* boneTransform = animator->boneTransform_;
  for( u32 i(0); i<animator->skeleton_->nodeCount_; ++i )
  {
    bool isBone = animator->skeleton_->isBone_[i];
    if( isBone )
    {
      //Compute new local transform of the bone
      localPose[i] = maths::computeTransform( maths::lerp( transform0->position_, transform1->position_, t ),
                                              maths::lerp( transform0->scale_, transform1->scale_, t ),
                                              maths::slerp( transform0->orientation_, transform1->orientation_, t ) );

      //Increment pointers to read next bone's animation data
      transform0++;
      transform1++;
    }
    else
    {
      //If node is not a bone its local transform is in the offset_ vector of the skeleton
      localPose[i] = animator->skeleton_->offset_[i];
    }

    //Compute global transform
    s32 parent = animator->skeleton_->parent_[i];
    globalPose[i] = (parent > -1 ) ? localPose[i] * globalPose[parent] : localPose[i];

    //Compute final bone transform
    if( isBone )
    {
      *boneTransform = animator->skeleton_->offset_[i] * globalPose[i] * animator->skeleton_->globalInverseTransform_;
      boneTransform++;
    }
  }

  //Upload bone transforms to the uniform buffer
  render::gpuBufferUpdate( context, (void*)animator->boneTransform_, 0u, sizeof(maths::mat4)*animator->skeleton_->boneCount_, &animator->buffer_ );
}

void mesh::animatorDestroy( const render::context_t& context, skeletal_animator_t* animator )
{
    delete[] animator->localPose_;
    delete[] animator->globalPose_;          //Final transformation of each node
    delete[] animator->boneTransform_;

    render::gpuBufferDestroy( context, &animator->buffer_ );
}