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

#ifndef MESH_H
#define MESH_H

#include "maths.h"
#include "render.h"

namespace bkk
{
  namespace mesh
  {
    struct aabb_t
    {
      maths::vec3 min_;
      maths::vec3 max_;
    };

    struct skeleton_t
    {
      s32* parent_;           //Parents
      maths::mat4* offset_;   //Mesh to Bone space transformation for bones, local transform for regular nodes
      bool* isBone_;
      maths::mat4 globalInverseTransform_;
      u32 boneCount_;
      u32 nodeCount_;
    };

    struct bone_transform_t
    {
      maths::vec3 position_;
      maths::vec3 scale_;
      maths::quat orientation_;
    };

    struct skeletal_animation_t
    {
      bone_transform_t* data_;
      u32 frameCount_;
    };

    struct skeletal_animator_t
    {
      f32 cursor_;
      float duration_;

      const skeleton_t* skeleton_;
      const skeletal_animation_t* animation_;

      maths::mat4* localPose_;          //Local transform of each node
      maths::mat4* globalPose_;         //Final transform of each node
      maths::mat4* boneTransform_;      //Final bones transforms for current time in the animation
      render::gpu_buffer_t buffer_;    //Uniform buffer with the final transformation of each bone
    };

    struct mesh_t
    {
      render::gpu_buffer_t vertexBuffer_;
      render::gpu_buffer_t indexBuffer_;
      
      //VkBuffer vertexBuffer_;
      //VkBuffer indexBuffer_;
      //render::gpu_memory_t memory_;

      u32 vertexCount_;
      u32 indexCount_;
      aabb_t aabb_;

      //Only used for skinned meshes
      skeleton_t* skeleton_ = nullptr;
      skeletal_animation_t* animations_ = nullptr;
      u32 animationCount_ = 0u;

      render::vertex_format_t vertexFormat_;
    };

    

    void create(const render::context_t& context,
                const uint32_t* indexData, uint32_t indexDataSize,
                const void* vertexData, size_t vertexDataSize,
                render::vertex_attribute_t* attribute, uint32_t attributeCount,
                mesh_t* mesh, render::gpu_memory_allocator_t* allocator = nullptr);

    void createFromFile(const render::context_t& context, const char* file, mesh_t* mesh, render::gpu_memory_allocator_t* allocator = nullptr);
    void destroy(const render::context_t& context, mesh_t* mesh, render::gpu_memory_allocator_t* allocator = nullptr);
    void draw(VkCommandBuffer commandBuffer, const mesh_t& mesh);

    void animatorCreate(const render::context_t& context, const mesh_t& mesh, u32 animationIndex, float duration, skeletal_animator_t* animator);
    void animatorUpdate(const render::context_t& context, f32 time, skeletal_animator_t* animator);
    void animatorDestroy(const render::context_t& context, skeletal_animator_t* animator);

  } //mesh namespace
}//namespace bkk
#endif  /*  MESH_H   */