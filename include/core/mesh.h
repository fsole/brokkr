/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

#ifndef MESH_H
#define MESH_H

#include "core/maths.h"
#include "core/render.h"
#include "core/transform-manager.h"

namespace bkk
{
  namespace core
  {
    namespace mesh
    {
      struct skeleton_t
      {
        transform_manager_t txManager;

        bkk_handle_t* bones;
        maths::mat4* bindPose;

        maths::mat4 rootBoneInverseTransform;

        u32 boneCount;
        u32 nodeCount;
      };

      struct bone_transform_t
      {
        maths::vec3 position;
        maths::vec3 scale;
        maths::quat orientation;
      };

      struct skeletal_animation_t
      {
        u32 frameCount;
        u32 nodeCount;
        f32 duration;  //In ms

        bkk_handle_t* nodes;    //Handles of animated nodes
        bone_transform_t* data;
      };


      struct skeletal_animator_t
      {
        f32 cursor;
        float speed;

        skeleton_t* skeleton;
        const skeletal_animation_t* animation;

        maths::mat4* boneTransform;      //Final bones transforms for current time in the animation
        render::gpu_buffer_t buffer;    //Uniform buffer with the final transformation of each bone
      };

      struct mesh_t
      {
        render::gpu_buffer_t vertexBuffer;
        render::gpu_buffer_t indexBuffer;

        u32 vertexCount;
        u32 indexCount;
        maths::aabb_t aabb;

        //Only used for skinned meshes
        skeleton_t* skeleton = nullptr;
        skeletal_animation_t* animations = nullptr;
        u32 animationCount = 0u;

        render::vertex_format_t vertexFormat;
      };

      struct material_data_t
      {
        maths::vec3 kd;
        maths::vec3 ks;

        char diffuseMap[128];
      };

      enum export_flags_e
      {
        EXPORT_POSITION_ONLY = 0,
        EXPORT_NORMALS = 1,
        EXPORT_UV = 2,
        EXPORT_BONE_WEIGHTS = 4,
        EXPORT_ALL = EXPORT_NORMALS | EXPORT_UV | EXPORT_BONE_WEIGHTS

      };

      
      void create(const render::context_t& context,
        const uint32_t* indexData, uint32_t indexDataSize,
        const void* vertexData, size_t vertexDataSize,
        render::vertex_attribute_t* attribute, uint32_t attributeCount,
        render::gpu_memory_allocator_t* allocator, mesh_t* mesh);
      
      //Load all submeshes from a file
      //Warning: Allocates an array of meshes from the heap (returned by reference in 'meshes') and passes ownership of that memory to the caller
      uint32_t createFromFile(const render::context_t& context, const char* file, export_flags_e exportFlags, render::gpu_memory_allocator_t* allocator, mesh_t** meshes);

      //Load a single submesh from a file
      void createFromFile(const render::context_t& context, const char* file, export_flags_e exportFlags, render::gpu_memory_allocator_t* allocator, uint32_t subMesh, mesh_t* mesh);

      uint32_t loadMaterialData(const char* file, uint32_t** materialIndices, material_data_t** materials);

      void draw(render::command_buffer_t commandBuffer, const mesh_t& mesh);
      void drawInstanced(render::command_buffer_t commandBuffer, u32 instanceCount, render::gpu_buffer_t* instanceBuffer, u32 instancedAttributesCount, const mesh_t& mesh);
      void destroy(const render::context_t& context, mesh_t* mesh, render::gpu_memory_allocator_t* allocator = nullptr);

      //Animator
      void animatorCreate(const render::context_t& context, const mesh_t& mesh, u32 animationIndex, float speedFactor, skeletal_animator_t* animator);
      void animatorUpdate(const render::context_t& context, f32 deltaTimeInMs, skeletal_animator_t* animator);
      void animatorDestroy(const render::context_t& context, skeletal_animator_t* animator);

      mesh_t fullScreenQuad(const render::context_t& context);
      mesh_t unitQuad(const render::context_t& context);
      mesh_t unitCube(const render::context_t& context);

    } //mesh
  }//core
}//bkk
#endif  /*  MESH_H   */