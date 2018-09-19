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

#include "mesh.h"

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>

#include <float.h> //FLT_MAX
#include <map>
#include <string>
#include <assert.h>

using namespace bkk;
using namespace bkk::mesh;
using namespace bkk::maths;

//Helper functions
static size_t GetNextMultiple(size_t from, size_t multiple)
{
  return ((from + multiple - 1) / multiple) * multiple;
}

static void CountNodes(aiNode* node, u32& total)
{
  if (node)
  {
    total++;
    for (u32 i(0); i<node->mNumChildren; ++i)
    {
      CountNodes(node->mChildren[i], total);
    }
  }
}

static void TraverseScene(const aiNode* pNode, std::map<std::string, bkk::handle_t>& nodeNameToHandle,  const aiMesh* mesh, skeleton_t* skeleton, u32& boneIndex, bkk::handle_t parentHandle)
{
  std::string nodeName = pNode->mName.data;

  aiMatrix4x4 localTransform = pNode->mTransformation;
  localTransform.Transpose();
  bkk::maths::mat4 tx = (f32*)&localTransform.a1;
  bkk::handle_t nodeHandle = skeleton->txManager_.createTransform(tx);
  nodeNameToHandle[nodeName] = nodeHandle;


  for (uint32_t i = 0; i < mesh->mNumBones; i++)
  {
    std::string boneName(mesh->mBones[i]->mName.data);
    if (boneName == nodeName)
    {
      skeleton->bones_[boneIndex] = nodeHandle;
      skeleton->bindPose_[boneIndex] = (f32*)&mesh->mBones[i]->mOffsetMatrix.Transpose().a1;
      boneIndex++;
      break;
    }
  }

  skeleton->txManager_.setParent(nodeHandle, parentHandle);

  //Recurse on the children
  for (u32 i(0); i<pNode->mNumChildren; ++i)
  {
    TraverseScene(pNode->mChildren[i], nodeNameToHandle, mesh, skeleton, boneIndex, nodeHandle);
  }
}

static void LoadSkeleton(const aiScene* scene, const aiMesh* mesh, std::map<std::string, bkk::handle_t>& nodeNameToHandle,  skeleton_t* skeleton)
{
  u32 nodeCount(0);
  CountNodes(scene->mRootNode, nodeCount);

  skeleton->bones_ = new bkk::handle_t[mesh->mNumBones];
  skeleton->bindPose_ = new maths::mat4[mesh->mNumBones];
  
  aiMatrix4x4 globalInverse = scene->mRootNode->mTransformation;
  globalInverse.Inverse();
  skeleton->rootBoneInverseTransform_ = (f32*)&globalInverse.Transpose().a1;
  skeleton->boneCount_ = mesh->mNumBones;
  skeleton->nodeCount_ = nodeCount;

  u32 boneIndex = 0;
  TraverseScene(scene->mRootNode, nodeNameToHandle, mesh, skeleton, boneIndex, bkk::INVALID_ID);

  skeleton->txManager_.update();
}

static void LoadAnimation(const aiScene* scene, u32 animationIndex, std::map<std::string, bkk::handle_t>& nodeNameToIndex, u32 boneCount, skeletal_animation_t* animation)
{
  const aiAnimation* pAnimation = scene->mAnimations[animationIndex];

  u32 frameCount = 0;
  if (pAnimation)
  {
    for (u32 channel(0); channel < pAnimation->mNumChannels; ++channel)
    {
      frameCount = maths::maxValue(frameCount, pAnimation->mChannels[channel]->mNumPositionKeys);
    }
  }

  if (frameCount > 0 )
  {
    animation->frameCount_ = frameCount;
    animation->nodeCount_ = pAnimation->mNumChannels;
    animation->data_ = new bone_transform_t[animation->frameCount_*animation->nodeCount_];
    animation->nodes_ = new bkk::handle_t[animation->nodeCount_];
    animation->duration_ = f32( pAnimation->mDuration / pAnimation->mTicksPerSecond ) * 1000.0f;

    for (u32 channel(0); channel<pAnimation->mNumChannels; ++channel)
    {
      std::string nodeName(pAnimation->mChannels[channel]->mNodeName.data);
      std::map<std::string, bkk::handle_t>::iterator it = nodeNameToIndex.find(nodeName);
      
      animation->nodes_[channel] = it->second;
        
      //Read animation data for the bone
      vec3 position, scale;
      quat orientation;
      for (u32 frame = 0; frame<animation->frameCount_; ++frame)
      {
        size_t index = frame*animation->nodeCount_ + channel;
          
        if ( frame < pAnimation->mChannels[channel]->mNumPositionKeys )
        { 
            position = vec3(pAnimation->mChannels[channel]->mPositionKeys[frame].mValue.x,
                            pAnimation->mChannels[channel]->mPositionKeys[frame].mValue.y,
                            pAnimation->mChannels[channel]->mPositionKeys[frame].mValue.z);
        }
        animation->data_[index].position_ = position;

        if (frame < pAnimation->mChannels[channel]->mNumScalingKeys )
        {

          scale = vec3(pAnimation->mChannels[channel]->mScalingKeys[frame].mValue.x,
                        pAnimation->mChannels[channel]->mScalingKeys[frame].mValue.y,
                        pAnimation->mChannels[channel]->mScalingKeys[frame].mValue.z);
        }
        animation->data_[index].scale_ = scale;

        if (frame < pAnimation->mChannels[channel]->mNumRotationKeys )
        {
          orientation = quat(pAnimation->mChannels[channel]->mRotationKeys[frame].mValue.x,
                              pAnimation->mChannels[channel]->mRotationKeys[frame].mValue.y,
                              pAnimation->mChannels[channel]->mRotationKeys[frame].mValue.z,
                              pAnimation->mChannels[channel]->mRotationKeys[frame].mValue.w);
        }
        animation->data_[index].orientation_ = orientation;
      }
    }
  }
}

static void loadMesh(const render::context_t& context, const struct aiScene* scene, uint32_t submesh, mesh_t* mesh, export_flags_e flags, render::gpu_memory_allocator_t* allocator)
{

  const struct aiMesh* aimesh = scene->mMeshes[submesh];
  size_t vertexCount = aimesh->mNumVertices;

  bool bHasNormal(aimesh->HasNormals());
  bool bHasUV(aimesh->HasTextureCoords(0));
  u32 boneCount(aimesh->mNumBones);

  u32 vertexSize = 3;
  u32 attributeCount = 1;

  bool importNormals = (((flags & EXPORT_NORMALS) != 0) && bHasNormal);
  bool importUV = (((flags & EXPORT_UV) != 0) && bHasUV );
  bool importBoneWeights = (((flags & EXPORT_BONE_WEIGHTS) != 0) && (boneCount > 0));

  if (importNormals)
  {
    vertexSize += 3;
    ++attributeCount;
  }
  if (importUV)
  {
    vertexSize += 2;
    ++attributeCount;
  }
  if (boneCount > 0 && importBoneWeights)
  {
    vertexSize += 8;  //4 weights and 4 bone index
    attributeCount += 2;
  }

  //Attributes description
  dynamic_array_t<render::vertex_attribute_t> attributes(attributeCount);

  //First attribute is position
  attributes[0].format_ = render::vertex_attribute_t::format::VEC3;
  attributes[0].offset_ = 0;
  attributes[0].stride_ = vertexSize * sizeof(f32);
  attributes[0].instanced_ = false;

  u32 attribute = 1;
  u32 attributeOffset = 3;
  if (importNormals)
  {
    attributes[attribute].format_ = render::vertex_attribute_t::format::VEC3;
    attributes[attribute].offset_ = sizeof(f32)*attributeOffset;
    attributes[attribute].stride_ = vertexSize * sizeof(f32);
    attributes[attribute].instanced_ = false;
    ++attribute;
    attributeOffset += 3;
  }
  if (importUV)
  {
    attributes[attribute].format_ = render::vertex_attribute_t::format::VEC2;
    attributes[attribute].offset_ = sizeof(f32)*attributeOffset;
    attributes[attribute].stride_ = vertexSize * sizeof(f32);
    attributes[attribute].instanced_ = false;
    ++attribute;
    attributeOffset += 2;
  }

  u32 boneWeightOffset = attributeOffset;
  if (boneCount > 0 && importBoneWeights)
  {
    attributes[attribute].format_ = render::vertex_attribute_t::format::VEC4;
    attributes[attribute].offset_ = sizeof(f32)*attributeOffset;
    attributes[attribute].stride_ = vertexSize * sizeof(f32);
    attributes[attribute].instanced_ = false;

    ++attribute;
    attributeOffset += 4;

    attributes[attribute].format_ = render::vertex_attribute_t::format::VEC4;
    attributes[attribute].offset_ = sizeof(f32)*attributeOffset;
    attributes[attribute].stride_ = vertexSize * sizeof(f32);
    attributes[attribute].instanced_ = false;
  }

  vec3 aabbMin(FLT_MAX, FLT_MAX, FLT_MAX);
  vec3 aabbMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
  size_t vertexBufferSize(vertexCount * vertexSize * sizeof(f32));
  f32* vertexData = new f32[vertexCount * vertexSize];
  memset((u32*)vertexData, 0, vertexBufferSize);

  u32 index = 0;
  for (u32 vertex(0); vertex<vertexCount; ++vertex)
  {
    aabbMin = vec3(maths::minValue(aimesh->mVertices[vertex].x, aabbMin.x),
                   maths::minValue(aimesh->mVertices[vertex].y, aabbMin.y),
                   maths::minValue(aimesh->mVertices[vertex].z, aabbMin.z));

    aabbMax = vec3(maths::maxValue(aimesh->mVertices[vertex].x, aabbMax.x),
                   maths::maxValue(aimesh->mVertices[vertex].y, aabbMax.y),
                   maths::maxValue(aimesh->mVertices[vertex].z, aabbMax.z));

    vertexData[index++] = aimesh->mVertices[vertex].x;
    vertexData[index++] = aimesh->mVertices[vertex].y;
    vertexData[index++] = aimesh->mVertices[vertex].z;

    if (importNormals)
    {
      if (bHasNormal)
      {
        vertexData[index] = aimesh->mNormals[vertex].x;
        vertexData[index+1] = aimesh->mNormals[vertex].y;
        vertexData[index+2] = aimesh->mNormals[vertex].z;
      }

      index += 3;
    }

    if (importUV)
    {
      if (bHasUV)
      {
        vertexData[index] = aimesh->mTextureCoords[0][vertex].x;
        vertexData[index+1] = aimesh->mTextureCoords[0][vertex].y;
      }

      index += 2;
    }

    if (boneCount > 0 && importBoneWeights)
    {
      index += 8; //Bone weight and index will be filled when loading skeleton
    }
  }

  //Load skeleton
  mesh->skeleton_ = nullptr;
  mesh->animations_ = nullptr;
  if (boneCount > 0 && importBoneWeights)
  {
    std::map<std::string, bkk::handle_t> nodeNameToHandle;
    if (boneCount > 0)
    {
      mesh->skeleton_ = new skeleton_t;
      LoadSkeleton(scene, aimesh, nodeNameToHandle, mesh->skeleton_);
    }

    //Read weights and bone indices for each vertex
    for (uint32_t i = 0; i < boneCount; i++)
    {
      std::string boneName(aimesh->mBones[i]->mName.data);
      std::map<std::string, bkk::handle_t>::iterator it;
      it = nodeNameToHandle.find(boneName);
      if (it != nodeNameToHandle.end())
      {
        u32 boneIndex = 0;
        for (; boneIndex < mesh->skeleton_->boneCount_; ++boneIndex)
        {
          if (it->second.index_ == mesh->skeleton_->bones_[boneIndex].index_ )
          {
            break;
          }
        }

        u32 vertexCount = aimesh->mBones[i]->mNumWeights;
        for (u32 vertex(0); vertex < vertexCount; ++vertex)
        {
          u32 vertexId = aimesh->mBones[i]->mWeights[vertex].mVertexId;
          f32 weight = aimesh->mBones[i]->mWeights[vertex].mWeight;

          size_t vertexWeightOffset = vertexId * vertexSize + boneWeightOffset;
          size_t vertexBoneIdOffset = vertexId * vertexSize + boneWeightOffset + 4;
          while (vertexData[vertexWeightOffset] != 0.0f)
          {
            vertexWeightOffset++;
            vertexBoneIdOffset++;
          }
          vertexData[vertexWeightOffset] = weight;
          vertexData[vertexBoneIdOffset] = (f32)boneIndex;
        }
      }
    }

    //Load skeletal animations
    if (scene->HasAnimations())
    {
      mesh->animationCount_ = scene->mNumAnimations;
      mesh->animations_ = new skeletal_animation_t[mesh->animationCount_];
      for (u32 i(0); i < mesh->animationCount_; ++i)
      {
        LoadAnimation(scene, i, nodeNameToHandle, boneCount, &mesh->animations_[i]);
      }
    }
  }

  uint32_t* indices(0);
  uint32_t indexBufferSize(0);
  if (aimesh->HasFaces())
  {
    u32 indexCount = aimesh->mNumFaces * 3; //@WARNING: Assuming triangles!
    indexBufferSize = indexCount * sizeof(uint32_t);
    indices = new u32[indexCount];
    for (u32 face(0); face<aimesh->mNumFaces; ++face)
    {
      indices[face * 3] = aimesh->mFaces[face].mIndices[0];
      indices[face * 3 + 1] = aimesh->mFaces[face].mIndices[1];
      indices[face * 3 + 2] = aimesh->mFaces[face].mIndices[2];
    }
  }

  mesh->aabb_.min_ = aabbMin;
  mesh->aabb_.max_ = aabbMax;

  create(context, indices, indexBufferSize, vertexData, vertexBufferSize, &attributes[0], attributeCount, allocator, mesh);

  delete[] vertexData;
  delete[] indices;
}



/*********************
* API Implementation
**********************/


void mesh::create(const render::context_t& context,
  const uint32_t* indexData, uint32_t indexDataSize,
  const void* vertexData, size_t vertexDataSize,
  render::vertex_attribute_t* attribute, uint32_t attributeCount,
  render::gpu_memory_allocator_t* allocator,
  mesh_t* mesh)
{
  //Create vertex format
  render::vertexFormatCreate(attribute, attributeCount, &mesh->vertexFormat_);

  mesh->indexCount_ = (u32)indexDataSize / sizeof(uint32_t);
  mesh->vertexCount_ = (u32)vertexDataSize / mesh->vertexFormat_.vertexSize_;

  render::gpuBufferCreate(context, render::gpu_buffer_t::usage::INDEX_BUFFER, (void*)indexData, (size_t)indexDataSize, allocator, &mesh->indexBuffer_);
  render::gpuBufferCreate(context, render::gpu_buffer_t::usage::VERTEX_BUFFER, (void*)vertexData, (size_t)vertexDataSize, allocator, &mesh->vertexBuffer_);
}


void mesh::createFromFile(const render::context_t& context, const char* file, export_flags_e exportFlags, render::gpu_memory_allocator_t* allocator, uint32_t submesh, mesh_t* mesh)
{
  Assimp::Importer Importer;
  int flags = aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights | aiProcess_GenSmoothNormals;
  const struct aiScene* scene = Importer.ReadFile(file, flags);
  assert(scene && scene->mNumMeshes > submesh);

  loadMesh(context, scene, submesh, mesh, exportFlags, allocator);
}

uint32_t mesh::createFromFile(const render::context_t& context, const char* file, export_flags_e exportFlags, render::gpu_memory_allocator_t* allocator, mesh_t** meshes)
{
  Assimp::Importer Importer;
  int flags = aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights | aiProcess_GenSmoothNormals;
  const struct aiScene* scene = Importer.ReadFile(file, flags);
  assert(scene && scene->mNumMeshes > 0);

  uint32_t meshCount = scene->mNumMeshes;
  *meshes = new mesh_t[meshCount];
  for (uint32_t i(0); i<meshCount; ++i)
  {
    loadMesh(context, scene, i, *meshes + i, exportFlags, allocator);
  }

  return meshCount;
}

uint32_t mesh::loadMaterials(const char* file, uint32_t** materialIndices, material_t** materials)
{
  Assimp::Importer Importer;
  int flags = aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights | aiProcess_GenSmoothNormals;
  const struct aiScene* scene = Importer.ReadFile(file, flags);
  assert(scene && scene->mNumMeshes > 0);

  uint32_t meshCount = scene->mNumMeshes;
  *materialIndices = new uint32_t[meshCount];
  for (uint32_t i(0); i<meshCount; ++i)
  {
    *(*materialIndices + i) = scene->mMeshes[i]->mMaterialIndex;
  }

  uint32_t materialCount = scene->mNumMaterials;
  *materials = new material_t[materialCount];

  aiColor3D color;
  aiString path;

  material_t* material;
  for (uint32_t i(0); i < materialCount; ++i)
  {
    material = (*materials + i);

    //Diffuse color
    if (scene->mMaterials[i]->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
    {
      material->kd_ = vec3(color.r, color.g, color.b);
    }

    //Diffuse map
    material->diffuseMap_[0] = '\0';
    if (scene->mMaterials[i]->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), path) == AI_SUCCESS)
    {
      //(*(*materials + i)).diffuseMap_ = path.C_Str();
      memcpy(&material->diffuseMap_, path.C_Str(), path.length+1 );
    }
  }

  return materialCount;
}

void mesh::destroy(const render::context_t& context, mesh_t* mesh, render::gpu_memory_allocator_t* allocator)
{
  render::gpuBufferDestroy(context, allocator, &mesh->indexBuffer_);
  render::gpuBufferDestroy(context, allocator, &mesh->vertexBuffer_);

  if (mesh->skeleton_)
  {
    delete[] mesh->skeleton_->bindPose_;
    delete[] mesh->skeleton_->bones_;
    delete mesh->skeleton_;
  }

  if (mesh->animationCount_ > 0)
  {
    for (u32 i(0); i<mesh->animationCount_; ++i)
    {
      delete[] mesh->animations_[i].data_;
    }

    delete mesh->animations_;
  }

  vertexFormatDestroy(&mesh->vertexFormat_);
}

void mesh::draw(render::command_buffer_t commandBuffer, const mesh_t& mesh)
{
  vkCmdBindIndexBuffer(commandBuffer.handle_, mesh.indexBuffer_.handle_, 0, VK_INDEX_TYPE_UINT32);

  uint32_t attributeCount = mesh.vertexFormat_.attributeCount_;
  dynamic_array_t<VkBuffer> buffers(attributeCount);
  dynamic_array_t<VkDeviceSize> offsets(attributeCount);
  for (uint32_t i(0); i<attributeCount; ++i)
  {
    buffers[i] = mesh.vertexBuffer_.handle_;
    offsets[i] = 0u;
  }

  vkCmdBindVertexBuffers(commandBuffer.handle_, 0, attributeCount, &buffers[0], &offsets[0]);
  vkCmdDrawIndexed(commandBuffer.handle_, mesh.indexCount_, 1, 0, 0, 0);
}

void mesh::drawInstanced(bkk::render::command_buffer_t commandBuffer, u32 instanceCount, render::gpu_buffer_t* instanceBuffer, u32 instancedAttributesCount, const mesh_t& mesh)
{
  vkCmdBindIndexBuffer(commandBuffer.handle_, mesh.indexBuffer_.handle_, 0, VK_INDEX_TYPE_UINT32);

  uint32_t attributeCount = mesh.vertexFormat_.attributeCount_;
  dynamic_array_t<VkBuffer> buffers(attributeCount);
  dynamic_array_t<VkDeviceSize> offsets(attributeCount);
  for (uint32_t i(0); i<attributeCount; ++i)
  {
    buffers[i] = mesh.vertexBuffer_.handle_;
    offsets[i] = 0u;
  }
  vkCmdBindVertexBuffers(commandBuffer.handle_, 0, attributeCount, &buffers[0], &offsets[0]);

  if (instancedAttributesCount > 0 && instanceBuffer)
  {
    dynamic_array_t<VkBuffer> instancedBuffers(instancedAttributesCount);
    dynamic_array_t<VkDeviceSize> instancedOffsets(instancedAttributesCount);
    for (uint32_t i(0); i < instancedAttributesCount; ++i)
    {
      instancedBuffers[i] = instanceBuffer->handle_;
      instancedOffsets[i] = 0u;
    }
    vkCmdBindVertexBuffers(commandBuffer.handle_, attributeCount, instancedAttributesCount, &instancedBuffers[0], &instancedOffsets[0]);
  }

  //Draw command
  vkCmdDrawIndexed(commandBuffer.handle_, mesh.indexCount_, instanceCount, 0, 0, 0);
};


void mesh::animatorCreate(const render::context_t& context, const mesh_t& mesh, u32 animationIndex, float speed, skeletal_animator_t* animator)
{
  animator->cursor_ = 0.0f;
  animator->speed_ = speed;

  animator->skeleton_ = mesh.skeleton_;
  animator->animation_ = &mesh.animations_[animationIndex];

  animator->boneTransform_ = new maths::mat4[mesh.skeleton_->boneCount_];

  //Create an uninitialized uniform buffer
  render::gpuBufferCreate(context, render::gpu_buffer_t::usage::STORAGE_BUFFER,
    render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
    nullptr, sizeof(maths::mat4) * mesh.skeleton_->boneCount_,
    nullptr, &animator->buffer_);
}


void mesh::animatorUpdate(const render::context_t& context, f32 deltaTime, skeletal_animator_t* animator)
{
  animator->cursor_ += ( deltaTime / animator->animation_->duration_ ) * animator->speed_;

  if (animator->cursor_ > 1.0f)
  {
    animator->cursor_ -= 1.0f;
  }
 
  if (animator->cursor_ < 0.0f)
  {
    animator->cursor_ = 1.0f - animator->cursor_;
  }

  //Find out frames between which we need to interpolate
  u32 frameCount = animator->animation_->frameCount_ - 1;
  u32 frame0 = (u32)floor((frameCount)* animator->cursor_);
  u32 frame1 = maths::minValue(frame0 + 1, frameCount);

  //Local cursor between frames
  f32 t = (animator->cursor_ - ((f32)frame0 / (f32)frameCount)) / (((f32)frame1 / (f32)frameCount) - ((f32)frame0 / (f32)frameCount));

  //Pointers to animation data
  bone_transform_t* transform0 = &animator->animation_->data_[frame0 * animator->animation_->nodeCount_];
  bone_transform_t* transform1 = &animator->animation_->data_[frame1 * animator->animation_->nodeCount_];

  //Compute new local transforms
  for (u32 i(0); i<animator->animation_->nodeCount_; ++i)
  {
    //Compute new local transform of the bone
    mat4 nodeLocalTx = maths::createTransform(maths::lerp(transform0->position_, transform1->position_, t),
                                             maths::lerp(transform0->scale_, transform1->scale_, t),
                                             maths::slerp(transform0->orientation_, transform1->orientation_, t));

    animator->skeleton_->txManager_.setTransform(animator->animation_->nodes_[i], nodeLocalTx);
  
    //Increment pointers to read next bone's animation data
    transform0++;
    transform1++;
  }

  //Update global transforms
  animator->skeleton_->txManager_.update();

  //Compute final transformation for each bone
  for (u32 i = 0; i < animator->skeleton_->boneCount_; ++i)
  {
    maths::mat4* boneGlobalTx = animator->skeleton_->txManager_.getWorldMatrix(animator->skeleton_->bones_[i]);
    animator->boneTransform_[i] = animator->skeleton_->bindPose_[i] * (*boneGlobalTx) * animator->skeleton_->rootBoneInverseTransform_;
  }

  //Upload bone transforms to the uniform buffer
  render::gpuBufferUpdate(context, (void*)animator->boneTransform_, 0u, sizeof(maths::mat4)*animator->skeleton_->boneCount_, &animator->buffer_);
}

void mesh::animatorDestroy(const render::context_t& context, skeletal_animator_t* animator)
{
  delete[] animator->boneTransform_;
  render::gpuBufferDestroy(context, nullptr, &animator->buffer_);
}


mesh::mesh_t mesh::fullScreenQuad(const bkk::render::context_t& context)
{
  struct Vertex
  {
    float position[3];
    float uv[2];
  };

  //WARNING: IN Vulkan, Y is pointing down in NDC!
  static const Vertex vertices[] = { { { -1.0f,  1.0f, 0.0f },{ 0.0f, 1.0f } },
                                     { {  1.0f,  1.0f, 0.0f },{ 1.0f, 1.0f } },
                                     { {  1.0f, -1.0f, 0.0f },{ 1.0f, 0.0f } },
                                     { { -1.0f, -1.0f, 0.0f },{ 0.0f, 0.0f } }
                                   };

  static const uint32_t indices[] = { 0,1,2,0,2,3 };

  static bkk::render::vertex_attribute_t attributes[2];
  attributes[0].format_ = bkk::render::vertex_attribute_t::format::VEC3;
  attributes[0].offset_ = 0;
  attributes[0].stride_ = sizeof(Vertex);
  attributes[0].instanced_ = false;
  attributes[1].format_ = bkk::render::vertex_attribute_t::format::VEC2;
  attributes[1].offset_ = offsetof(Vertex, uv);
  attributes[1].stride_ = sizeof(Vertex);
  attributes[0].instanced_ = false;

  bkk::mesh::mesh_t mesh;
  bkk::mesh::create(context, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, nullptr, &mesh);
  return mesh;
}

mesh::mesh_t mesh::unitQuad(const bkk::render::context_t& context)
{
  struct Vertex
  {
    float position[3];
    float normal[3];
    float uv[2];
  };

  static const Vertex vertices[] = { { { -0.5f, -0.5f, 0.0f },{ 0.0f,  0.0f, 1.0f },{ 0.0f, 1.0f } },
                                     { {  0.5f, -0.5f, 0.0f },{ 0.0f,  0.0f, 1.0f },{ 1.0f, 1.0f } },
                                     { {  0.5f,  0.5f, 0.0f },{ 0.0f,  0.0f, 1.0f },{ 1.0f, 0.0f } },
                                     { { -0.5f,  0.5f, 0.0f },{ 0.0f,  0.0f, 1.0f },{ 0.0f, 0.0f } }
  };

  static const uint32_t indices[] = { 0,1,2,0,2,3 };

  static bkk::render::vertex_attribute_t attributes[3];
  attributes[0].format_ = bkk::render::vertex_attribute_t::format::VEC3;
  attributes[0].offset_ = 0;
  attributes[0].stride_ = sizeof(Vertex);
  attributes[0].instanced_ = false;

  attributes[1].format_ = bkk::render::vertex_attribute_t::format::VEC3;
  attributes[1].offset_ = offsetof(Vertex, normal);
  attributes[1].stride_ = sizeof(Vertex);
  attributes[1].instanced_ = false;

  attributes[2].format_ = bkk::render::vertex_attribute_t::format::VEC2;
  attributes[2].offset_ = offsetof(Vertex, uv);
  attributes[2].stride_ = sizeof(Vertex);
  attributes[2].instanced_ = false;

  bkk::mesh::mesh_t mesh;
  bkk::mesh::create(context, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 3, nullptr, &mesh);
  return mesh;
}

mesh_t mesh::unitCube(const bkk::render::context_t& context)
{
  struct Vertex
  {
    float position[3];
    float normal[3];
  };

  //WARNING: IN Vulkan, Y is pointing down in NDC!
  static const Vertex vertices[] = { {{ -0.5f, -0.5f, -0.5f },{ 0.0f, 0.0f, -1.0f } },
                                     {{ -0.5f, -0.5f, -0.5f },{ 0.0f, -1.0f, 0.0f } },
                                     {{ -0.5f, -0.5f, -0.5f },{ -1.0f, 0.0f, 0.0f } },


                                     { { -0.5f, -0.5f,  0.5f },{ 0.0f, 0.0f, 1.0f } },
                                     { { -0.5f, -0.5f,  0.5f },{ 0.0f, -1.0f, 0.0f } },
                                     { { -0.5f, -0.5f,  0.5f },{ -1.0f, 0.0f, 0.0f } },


                                     { { -0.5f,  0.5f, -0.5f },{ 0.0f, 0.0f, -1.0f } },
                                     { { -0.5f,  0.5f, -0.5f },{ 0.0f, 1.0f, 0.0f } },
                                     { { -0.5f,  0.5f, -0.5f },{ -1.0f, 0.0f, 0.0f } },


                                     { { -0.5f,  0.5f,  0.5f },{ 0.0f, 0.0f, 1.0f } },
                                     { { -0.5f,  0.5f,  0.5f },{ 0.0f, 1.0f, 0.0f } },
                                     { { -0.5f,  0.5f,  0.5f },{ -1.0f, 0.0f, 0.0f } },

                                     { { 0.5f, -0.5f, -0.5f },{ 0.0f, 0.0f, -1.0f } },
                                     { { 0.5f, -0.5f, -0.5f },{ 0.0f, -1.0f, 0.0f } },
                                     { { 0.5f, -0.5f, -0.5f },{ 1.0f, 0.0f, 0.0f } },

                                     { { 0.5f, -0.5f,  0.5f },{ 0.0f, 0.0f, 1.0f } },
                                     { { 0.5f, -0.5f,  0.5f },{ 0.0f, -1.0f, 0.0f } },
                                     { { 0.5f, -0.5f,  0.5f },{ 1.0f, 0.0f, 0.0f } },

                                     { { 0.5f,  0.5f, -0.5f },{ 0.0f, 0.0f, -1.0f } },
                                     { { 0.5f,  0.5f, -0.5f },{ 0.0f, 1.0f, 0.0f } },
                                     { { 0.5f,  0.5f, -0.5f },{ 1.0f, 0.0f, 0.0f } },

                                     { { 0.5f,  0.5f,  0.5f },{ 0.0f, 0.0f, 1.0f } },
                                     { { 0.5f,  0.5f,  0.5f },{ 0.0f, 1.0f, 0.0f } },
                                     { { 0.5f,  0.5f,  0.5f },{ 1.0f, 0.0f, 0.0f } }

  };

  static const uint32_t indices[] = { 3, 21, 9, 0, 18, 12, 0, 6, 18, 2, 11,8, 2, 5, 11, 7, 22, 19, 7, 10, 22, 14, 20, 23, 14, 23, 17, 1, 13, 16, 1, 16, 4, 3, 15, 21 };

  static bkk::render::vertex_attribute_t attributes[2];
  attributes[0].format_ = bkk::render::vertex_attribute_t::format::VEC3;
  attributes[0].offset_ = 0;
  attributes[0].stride_ = sizeof(Vertex);
  attributes[0].instanced_ = false;
  attributes[1].format_ = bkk::render::vertex_attribute_t::format::VEC3;;
  attributes[1].offset_ = offsetof(Vertex, normal);
  attributes[1].stride_ = sizeof(Vertex);
  attributes[1].instanced_ = false;

  bkk::mesh::mesh_t mesh;
  bkk::mesh::create(context, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, nullptr, &mesh);
  return mesh;
}