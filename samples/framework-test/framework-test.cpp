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

#include "framework/application.h"
#include "framework/camera.h"
#include "framework/command-buffer.h"

#include "core/mesh.h"
#include "core/maths.h"

using namespace bkk::core;
using namespace bkk::framework;

class framework_test_t : public application_t
{
public:
  framework_test_t()
    :application_t("Framework test", 1200u, 800u, 3u)
  {
    renderTarget_ = renderer_.renderTargetCreate(1200u, 800u, VK_FORMAT_R32G32B32A32_SFLOAT, true);
    frameBuffer_ = renderer_.frameBufferCreate(&renderTarget_, 1u);

    //create shaders
    shader_handle_t shader = renderer_.shaderCreate("../framework-test/simple.shader");

    //create geometries
    mesh::mesh_t mesh;
    mesh::createFromFile(renderer_.getContext(), "../resources/teapot.obj", mesh::EXPORT_ALL, nullptr, 0, &mesh);
    mesh_handle_t meshHandle = renderer_.addMesh(mesh);

    //Create actors
    material_handle_t material = renderer_.materialCreate(shader);
    renderer_.getMaterial(material)->setProperty("globals.color", maths::vec3(1, 0, 0));
    renderer_.getMaterial(material)->setProperty("globals.intensity", 1.0f);
    maths::mat4 transform = maths::createTransform(maths::vec3(-5.0, -1.0, -10.0), maths::VEC3_ONE, maths::QUAT_UNIT);
    renderer_.actorCreate("teapot0", meshHandle, material, transform);

    material = renderer_.materialCreate(shader);
    renderer_.getMaterial(material)->setProperty("globals.color", maths::vec3(0, 0, 1));
    renderer_.getMaterial(material)->setProperty("globals.intensity", 1.0f);
    transform = maths::createTransform(maths::vec3(5.0, -1.0, -10.0), maths::VEC3_ONE, maths::QUAT_UNIT);
    renderer_.actorCreate("teapot1", meshHandle, material, transform);

    camera_t camera(camera_t::PERSPECTIVE_PROJECTION, 1.2f, 1200.0f / 800.0f, 1.0f, 100.0f);
    camera_ = renderer_.addCamera(camera);
  }

  void render()
  {
    beginFrame();

    renderer_.setupCamera(camera_);

    //Render opaque objects to an offscreen render target
    actor_t* visibleActors = nullptr;
    int count = renderer_.getVisibleActors(camera_, &visibleActors);
    
    command_buffer_t cmdBuffer(&renderer_, frameBuffer_);
    cmdBuffer.clearRenderTargets(maths::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    cmdBuffer.render(visibleActors, count, "OpaquePass");
    cmdBuffer.submit();
    cmdBuffer.release();

    //Copy offscreen render target to the back buffer    
    cmdBuffer = command_buffer_t(&renderer_);
    cmdBuffer.clearRenderTargets(maths::vec4(0.0f, 1.0f, 0.0f, 1.0f));
    cmdBuffer.blit(renderTarget_ /*, material*/);
    cmdBuffer.submit();
    cmdBuffer.release();

    presentFrame();
  }

  void buildGuiFrame()
  {
    ImGui::Begin("Controls");
    ImGui::End();
  }

private:
  frame_buffer_handle_t frameBuffer_;
  render_target_handle_t renderTarget_;
  camera_handle_t camera_;
};

int main()
{
  framework_test_t test;
  test.loop();

  return 0;
}