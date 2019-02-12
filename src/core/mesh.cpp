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

#include "core/mesh.h"

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>

#include <float.h> //FLT_MAX
#include <map>
#include <string>
#include <vector>
#include <assert.h>

using namespace bkk::core;
using namespace bkk::core::mesh;
using namespace bkk::core::maths;

using bkk::core::handle_t;  //To avoid ambiguity with Windows handle_t type

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

static void TraverseScene(const aiNode* pNode, std::map<std::string, handle_t>& nodeNameToHandle,  const aiMesh* mesh, skeleton_t* skeleton, u32& boneIndex, handle_t parentHandle)
{
  std::string nodeName = pNode->mName.data;

  aiMatrix4x4 localTransform = pNode->mTransformation;
  localTransform.Transpose();
  maths::mat4 tx = (f32*)&localTransform.a1;
  handle_t nodeHandle = skeleton->txManager.createTransform(tx);
  nodeNameToHandle[nodeName] = nodeHandle;


  for (uint32_t i = 0; i < mesh->mNumBones; i++)
  {
    std::string boneName(mesh->mBones[i]->mName.data);
    if (boneName == nodeName)
    {
      skeleton->bones[boneIndex] = nodeHandle;
      skeleton->bindPose[boneIndex] = (f32*)&mesh->mBones[i]->mOffsetMatrix.Transpose().a1;
      boneIndex++;
      break;
    }
  }

  skeleton->txManager.setParent(nodeHandle, parentHandle);

  //Recurse on the children
  for (u32 i(0); i<pNode->mNumChildren; ++i)
  {
    TraverseScene(pNode->mChildren[i], nodeNameToHandle, mesh, skeleton, boneIndex, nodeHandle);
  }
}

static void LoadSkeleton(const aiScene* scene, const aiMesh* mesh, std::map<std::string, handle_t>& nodeNameToHandle,  skeleton_t* skeleton)
{
  u32 nodeCount(0);
  CountNodes(scene->mRootNode, nodeCount);

  skeleton->bones = new handle_t[mesh->mNumBones];
  skeleton->bindPose = new maths::mat4[mesh->mNumBones];
  
  aiMatrix4x4 globalInverse = scene->mRootNode->mTransformation;
  globalInverse.Inverse();
  skeleton->rootBoneInverseTransform = (f32*)&globalInverse.Transpose().a1;
  skeleton->boneCount = mesh->mNumBones;
  skeleton->nodeCount = nodeCount;

  u32 boneIndex = 0;
  TraverseScene(scene->mRootNode, nodeNameToHandle, mesh, skeleton, boneIndex, NULL_HANDLE);

  skeleton->txManager.update();
}

static void LoadAnimation(const aiScene* scene, u32 animationIndex, std::map<std::string, handle_t>& nodeNameToIndex, u32 boneCount, skeletal_animation_t* animation)
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
    animation->frameCount = frameCount;
    animation->nodeCount = pAnimation->mNumChannels;
    animation->data = new bone_transform_t[animation->frameCount*animation->nodeCount];
    animation->nodes = new handle_t[animation->nodeCount];
    animation->duration = f32( pAnimation->mDuration / pAnimation->mTicksPerSecond ) * 1000.0f;

    for (u32 channel(0); channel<pAnimation->mNumChannels; ++channel)
    {
      std::string nodeName(pAnimation->mChannels[channel]->mNodeName.data);
      std::map<std::string, handle_t>::iterator it = nodeNameToIndex.find(nodeName);
      
      animation->nodes[channel] = it->second;
        
      //Read animation data for the bone
      vec3 position, scale;
      quat orientation;
      for (u32 frame = 0; frame<animation->frameCount; ++frame)
      {
        size_t index = frame*animation->nodeCount + channel;
          
        if ( frame < pAnimation->mChannels[channel]->mNumPositionKeys )
        { 
            position = vec3(pAnimation->mChannels[channel]->mPositionKeys[frame].mValue.x,
                            pAnimation->mChannels[channel]->mPositionKeys[frame].mValue.y,
                            pAnimation->mChannels[channel]->mPositionKeys[frame].mValue.z);
        }
        animation->data[index].position = position;

        if (frame < pAnimation->mChannels[channel]->mNumScalingKeys )
        {

          scale = vec3(pAnimation->mChannels[channel]->mScalingKeys[frame].mValue.x,
                        pAnimation->mChannels[channel]->mScalingKeys[frame].mValue.y,
                        pAnimation->mChannels[channel]->mScalingKeys[frame].mValue.z);
        }
        animation->data[index].scale = scale;

        if (frame < pAnimation->mChannels[channel]->mNumRotationKeys )
        {
          orientation = quat(pAnimation->mChannels[channel]->mRotationKeys[frame].mValue.x,
                              pAnimation->mChannels[channel]->mRotationKeys[frame].mValue.y,
                              pAnimation->mChannels[channel]->mRotationKeys[frame].mValue.z,
                              pAnimation->mChannels[channel]->mRotationKeys[frame].mValue.w);
        }
        animation->data[index].orientation = orientation;
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

  bool importNormals = (((flags & EXPORT_NORMALS) != 0));
  bool importUV = (((flags & EXPORT_UV) != 0) );
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
  std::vector<render::vertex_attribute_t> attributes(attributeCount);

  //First attribute is position
  attributes[0].format = render::vertex_attribute_t::format_e::VEC3;
  attributes[0].offset = 0;
  attributes[0].stride = vertexSize * sizeof(f32);
  attributes[0].instanced = false;

  u32 attribute = 1;
  u32 attributeOffset = 3;
  if (importNormals)
  {
    attributes[attribute].format = render::vertex_attribute_t::format_e::VEC3;
    attributes[attribute].offset = sizeof(f32)*attributeOffset;
    attributes[attribute].stride = vertexSize * sizeof(f32);
    attributes[attribute].instanced = false;
    ++attribute;
    attributeOffset += 3;
  }
  if (importUV)
  {
    attributes[attribute].format = render::vertex_attribute_t::format_e::VEC2;
    attributes[attribute].offset = sizeof(f32)*attributeOffset;
    attributes[attribute].stride = vertexSize * sizeof(f32);
    attributes[attribute].instanced = false;
    ++attribute;
    attributeOffset += 2;
  }

  u32 boneWeightOffset = attributeOffset;
  if (boneCount > 0 && importBoneWeights)
  {
    attributes[attribute].format = render::vertex_attribute_t::format_e::VEC4;
    attributes[attribute].offset = sizeof(f32)*attributeOffset;
    attributes[attribute].stride = vertexSize * sizeof(f32);
    attributes[attribute].instanced = false;

    ++attribute;
    attributeOffset += 4;

    attributes[attribute].format = render::vertex_attribute_t::format_e::VEC4;
    attributes[attribute].offset = sizeof(f32)*attributeOffset;
    attributes[attribute].stride = vertexSize * sizeof(f32);
    attributes[attribute].instanced = false;
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
      else
      {
        vertexData[index] = 0.0f;
        vertexData[index + 1] = 1.0f;
        vertexData[index + 2] = 0.0f;
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
      else
      {
        vertexData[index] = 0.0f;
        vertexData[index + 1] = 0.0f;
      }

      index += 2;
    }

    if (boneCount > 0 && importBoneWeights)
    {
      index += 8; //Bone weight and index will be filled when loading skeleton
    }
  }

  //Load skeleton
  mesh->skeleton = nullptr;
  mesh->animations = nullptr;
  if (boneCount > 0 && importBoneWeights)
  {
    std::map<std::string, handle_t> nodeNameToHandle;
    if (boneCount > 0)
    {
      mesh->skeleton = new skeleton_t;
      LoadSkeleton(scene, aimesh, nodeNameToHandle, mesh->skeleton);
    }

    //Read weights and bone indices for each vertex
    for (uint32_t i = 0; i < boneCount; i++)
    {
      std::string boneName(aimesh->mBones[i]->mName.data);
      std::map<std::string, handle_t>::iterator it;
      it = nodeNameToHandle.find(boneName);
      if (it != nodeNameToHandle.end())
      {
        u32 boneIndex = 0;
        for (; boneIndex < mesh->skeleton->boneCount; ++boneIndex)
        {
          if (it->second.index_ == mesh->skeleton->bones[boneIndex].index_ )
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
      mesh->animationCount = scene->mNumAnimations;
      mesh->animations = new skeletal_animation_t[mesh->animationCount];
      for (u32 i(0); i < mesh->animationCount; ++i)
      {
        LoadAnimation(scene, i, nodeNameToHandle, boneCount, &mesh->animations[i]);
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

  mesh->aabb.min = aabbMin;
  mesh->aabb.max = aabbMax;

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
  render::vertexFormatCreate(attribute, attributeCount, &mesh->vertexFormat);

  mesh->indexCount = (u32)indexDataSize / sizeof(uint32_t);
  mesh->vertexCount = (u32)vertexDataSize / mesh->vertexFormat.vertexSize;

  render::gpuBufferCreate(context, render::gpu_buffer_t::usage_e::INDEX_BUFFER, (void*)indexData, (size_t)indexDataSize, allocator, &mesh->indexBuffer);
  render::gpuBufferCreate(context, render::gpu_buffer_t::usage_e::VERTEX_BUFFER, (void*)vertexData, (size_t)vertexDataSize, allocator, &mesh->vertexBuffer);
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
      material->kd = vec3(color.r, color.g, color.b);
    }

    //Diffuse map
    material->diffuseMap[0] = '\0';
    if (scene->mMaterials[i]->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), path) == AI_SUCCESS)
    {
      //(*(*materials + i)).diffuseMap_ = path.C_Str();
      memcpy(&material->diffuseMap, path.C_Str(), path.length+1 );
    }
  }

  return materialCount;
}

void mesh::destroy(const render::context_t& context, mesh_t* mesh, render::gpu_memory_allocator_t* allocator)
{
  render::gpuBufferDestroy(context, allocator, &mesh->indexBuffer);
  render::gpuBufferDestroy(context, allocator, &mesh->vertexBuffer);

  if (mesh->skeleton)
  {
    delete[] mesh->skeleton->bindPose;
    delete[] mesh->skeleton->bones;
    delete mesh->skeleton;
  }

  if (mesh->animationCount > 0)
  {
    for (u32 i(0); i<mesh->animationCount; ++i)
    {
      delete[] mesh->animations[i].data;
    }

    delete mesh->animations;
  }

  vertexFormatDestroy(&mesh->vertexFormat);
}

void mesh::draw(render::command_buffer_t commandBuffer, const mesh_t& mesh)
{
  vkCmdBindIndexBuffer(commandBuffer.handle, mesh.indexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);

  uint32_t attributeCount = mesh.vertexFormat.attributeCount;
  std::vector<VkBuffer> buffers(attributeCount);
  std::vector<VkDeviceSize> offsets(attributeCount);
  for (uint32_t i(0); i<attributeCount; ++i)
  {
    buffers[i] = mesh.vertexBuffer.handle;
    offsets[i] = 0u;
  }

  vkCmdBindVertexBuffers(commandBuffer.handle, 0, attributeCount, &buffers[0], &offsets[0]);
  vkCmdDrawIndexed(commandBuffer.handle, mesh.indexCount, 1, 0, 0, 0);
}

void mesh::drawInstanced(render::command_buffer_t commandBuffer, u32 instanceCount, render::gpu_buffer_t* instanceBuffer, u32 instancedAttributesCount, const mesh_t& mesh)
{
  vkCmdBindIndexBuffer(commandBuffer.handle, mesh.indexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);

  uint32_t attributeCount = mesh.vertexFormat.attributeCount;
  std::vector<VkBuffer> buffers(attributeCount);
  std::vector<VkDeviceSize> offsets(attributeCount);
  for (uint32_t i(0); i<attributeCount; ++i)
  {
    buffers[i] = mesh.vertexBuffer.handle;
    offsets[i] = 0u;
  }
  vkCmdBindVertexBuffers(commandBuffer.handle, 0, attributeCount, &buffers[0], &offsets[0]);

  if (instancedAttributesCount > 0 && instanceBuffer)
  {
    std::vector<VkBuffer> instancedBuffers(instancedAttributesCount);
    std::vector<VkDeviceSize> instancedOffsets(instancedAttributesCount);
    for (uint32_t i(0); i < instancedAttributesCount; ++i)
    {
      instancedBuffers[i] = instanceBuffer->handle;
      instancedOffsets[i] = 0u;
    }
    vkCmdBindVertexBuffers(commandBuffer.handle, attributeCount, instancedAttributesCount, &instancedBuffers[0], &instancedOffsets[0]);
  }

  //Draw command
  vkCmdDrawIndexed(commandBuffer.handle, mesh.indexCount, instanceCount, 0, 0, 0);
};


void mesh::animatorCreate(const render::context_t& context, const mesh_t& mesh, u32 animationIndex, float speed, skeletal_animator_t* animator)
{
  animator->cursor = 0.0f;
  animator->speed = speed;

  animator->skeleton = mesh.skeleton;
  animator->animation = &mesh.animations[animationIndex];

  animator->boneTransform = new maths::mat4[mesh.skeleton->boneCount];

  //Create an uninitialized uniform buffer
  render::gpuBufferCreate(context, render::gpu_buffer_t::usage_e::STORAGE_BUFFER,
    render::gpu_memory_type_e::HOST_VISIBLE_COHERENT,
    nullptr, sizeof(maths::mat4) * mesh.skeleton->boneCount,
    nullptr, &animator->buffer);
}


void mesh::animatorUpdate(const render::context_t& context, f32 deltaTime, skeletal_animator_t* animator)
{
  animator->cursor += ( deltaTime / animator->animation->duration ) * animator->speed;

  if (animator->cursor > 1.0f)
  {
    animator->cursor -= 1.0f;
  }
 
  if (animator->cursor < 0.0f)
  {
    animator->cursor = 1.0f - animator->cursor;
  }

  //Find out frames between which we need to interpolate
  u32 frameCount = animator->animation->frameCount - 1;
  u32 frame0 = (u32)floor((frameCount)* animator->cursor);
  u32 frame1 = maths::minValue(frame0 + 1, frameCount);

  //Local cursor between frames
  f32 t = (animator->cursor - ((f32)frame0 / (f32)frameCount)) / (((f32)frame1 / (f32)frameCount) - ((f32)frame0 / (f32)frameCount));

  //Pointers to animation data
  bone_transform_t* transform0 = &animator->animation->data[frame0 * animator->animation->nodeCount];
  bone_transform_t* transform1 = &animator->animation->data[frame1 * animator->animation->nodeCount];

  //Compute new local transforms
  for (u32 i(0); i<animator->animation->nodeCount; ++i)
  {
    //Compute new local transform of the bone
    mat4 nodeLocalTx = maths::createTransform(maths::lerp(transform0->position, transform1->position, t),
                                             maths::lerp(transform0->scale, transform1->scale, t),
                                             maths::slerp(transform0->orientation, transform1->orientation, t));

    animator->skeleton->txManager.setTransform(animator->animation->nodes[i], nodeLocalTx);
  
    //Increment pointers to read next bone's animation data
    transform0++;
    transform1++;
  }

  //Update global transforms
  animator->skeleton->txManager.update();

  //Compute final transformation for each bone
  for (u32 i = 0; i < animator->skeleton->boneCount; ++i)
  {
    maths::mat4* boneGlobalTx = animator->skeleton->txManager.getWorldMatrix(animator->skeleton->bones[i]);
    animator->boneTransform[i] = animator->skeleton->bindPose[i] * (*boneGlobalTx) * animator->skeleton->rootBoneInverseTransform;
  }

  //Upload bone transforms to the uniform buffer
  render::gpuBufferUpdate(context, (void*)animator->boneTransform, 0u, sizeof(maths::mat4)*animator->skeleton->boneCount, &animator->buffer);
}

void mesh::animatorDestroy(const render::context_t& context, skeletal_animator_t* animator)
{
  delete[] animator->boneTransform;
  render::gpuBufferDestroy(context, nullptr, &animator->buffer);
}


mesh::mesh_t mesh::fullScreenQuad(const render::context_t& context)
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

  static render::vertex_attribute_t attributes[2];
  attributes[0].format = render::vertex_attribute_t::format_e::VEC3;
  attributes[0].offset = 0;
  attributes[0].stride = sizeof(Vertex);
  attributes[0].instanced = false;
  attributes[1].format = render::vertex_attribute_t::format_e::VEC2;
  attributes[1].offset = offsetof(Vertex, uv);
  attributes[1].stride = sizeof(Vertex);
  attributes[0].instanced = false;

  mesh::mesh_t mesh;
  mesh::create(context, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, nullptr, &mesh);
  return mesh;
}

mesh::mesh_t mesh::unitQuad(const render::context_t& context)
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

  static render::vertex_attribute_t attributes[3];
  attributes[0].format = render::vertex_attribute_t::format_e::VEC3;
  attributes[0].offset = 0;
  attributes[0].stride = sizeof(Vertex);
  attributes[0].instanced = false;

  attributes[1].format = render::vertex_attribute_t::format_e::VEC3;
  attributes[1].offset = offsetof(Vertex, normal);
  attributes[1].stride = sizeof(Vertex);
  attributes[1].instanced = false;

  attributes[2].format = render::vertex_attribute_t::format_e::VEC2;
  attributes[2].offset = offsetof(Vertex, uv);
  attributes[2].stride = sizeof(Vertex);
  attributes[2].instanced = false;

  mesh::mesh_t mesh;
  mesh::create(context, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 3, nullptr, &mesh);
  return mesh;
}

mesh_t mesh::unitCube(const render::context_t& context)
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

  static render::vertex_attribute_t attributes[2];
  attributes[0].format = render::vertex_attribute_t::format_e::VEC3;
  attributes[0].offset = 0;
  attributes[0].stride = sizeof(Vertex);
  attributes[0].instanced = false;
  attributes[1].format = render::vertex_attribute_t::format_e::VEC3;;
  attributes[1].offset = offsetof(Vertex, normal);
  attributes[1].stride = sizeof(Vertex);
  attributes[1].instanced = false;

  mesh::mesh_t mesh;
  mesh::create(context, indices, sizeof(indices), (const void*)vertices, sizeof(vertices), attributes, 2, nullptr, &mesh);
  return mesh;
}