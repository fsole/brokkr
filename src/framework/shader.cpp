

#include "../external/pugixml/pugixml.hpp"

#include "framework/shader.h"
#include "framework/renderer.h"
#include "core/string-utils.h"
#include <iostream>
#include <vector>
#include <algorithm>

using namespace bkk;
using namespace bkk::core;
using namespace bkk::framework;

///Helper methods
static uint32_t deserializeFieldDescription(pugi::xml_node fieldNode, uint32_t offset, buffer_desc_t::field_desc_t* field)
{
  uint32_t fieldSize = 0;
  buffer_desc_t::field_desc_t::type_e fieldType = buffer_desc_t::field_desc_t::TYPE_COUNT;
  if (strcmp(fieldNode.attribute("Type").value(), "int") == 0){
    fieldType = buffer_desc_t::field_desc_t::INT;
    fieldSize = sizeof(int);
  }
  else if (strcmp(fieldNode.attribute("Type").value(), "float") == 0) {
    fieldType = buffer_desc_t::field_desc_t::FLOAT;
    fieldSize = sizeof(float);
  }
  else if (strcmp(fieldNode.attribute("Type").value(), "vec2") == 0){
    fieldType = buffer_desc_t::field_desc_t::VEC2;
    fieldSize = 2 * sizeof(float);
  }
  else if (strcmp(fieldNode.attribute("Type").value(), "vec3") == 0){
    fieldType = buffer_desc_t::field_desc_t::VEC3;
    fieldSize = 3 * sizeof(float);
  }
  else if (strcmp(fieldNode.attribute("Type").value(), "vec4") == 0){
    fieldType = buffer_desc_t::field_desc_t::VEC4;
    fieldSize = 4 * sizeof(float);
  }
  else if (strcmp(fieldNode.attribute("Type").value(), "mat4") == 0){
    fieldType = buffer_desc_t::field_desc_t::MAT4;
    fieldSize = 16 * sizeof(float);
  }
  else if (strcmp(fieldNode.attribute("Type").value(), "compound_type") == 0) {
    fieldType = buffer_desc_t::field_desc_t::COMPOUND_TYPE;    
    for (pugi::xml_node fieldNodeChild = fieldNode.child("Field"); fieldNodeChild; fieldNodeChild = fieldNodeChild.next_sibling("Field"))
    {
      buffer_desc_t::field_desc_t childField;
      fieldSize += deserializeFieldDescription(fieldNodeChild, offset+fieldSize, &childField);
      field->fields_.push_back(childField);
    }
  }

  field->name_ = fieldNode.attribute("Name").value();
  field->type_ = fieldType;
  field->byteOffset_ = offset;
  field->size_ = fieldSize;
  field->count_ = fieldNode.attribute("Count").empty() ? 1 :
    strcmp(fieldNode.attribute("Count").value(), "") == 0 ? 0 : fieldNode.attribute("Count").as_int();

  return fieldSize;
}

static void deserializeBufferDescription(pugi::xml_node resourceNode, uint32_t binding, buffer_desc_t* bufferDesc )
{
  bufferDesc->name_ = resourceNode.attribute("Name").value();
  bufferDesc->type_ = strcmp(resourceNode.attribute("Type").value(), "uniform_buffer") == 0 ?
    buffer_desc_t::UNIFORM_BUFFER : buffer_desc_t::STORAGE_BUFFER;

  bufferDesc->binding_ = binding;
  bufferDesc->shared_ = strcmp(resourceNode.attribute("Shared").value(), "yes") == 0;

  uint32_t offset = 0u;
  for (pugi::xml_node fieldNode = resourceNode.child("Field"); fieldNode; fieldNode = fieldNode.next_sibling("Field"))
  {
    buffer_desc_t::field_desc_t field;
    offset += deserializeFieldDescription(fieldNode, offset, &field);
    bufferDesc->fields_.push_back(field);
  }

  bufferDesc->size_ = offset;
}

static void deserializeTextureDescription(pugi::xml_node resourceNode, uint32_t binding, texture_desc_t* textureDesc)
{
  textureDesc->name_ = resourceNode.attribute("Name").value();
  textureDesc->type_ = texture_desc_t::TEXTURE_2D;
  textureDesc->binding_ = binding;
}

static void fieldDescriptionToGLSL(const buffer_desc_t& bufferDesc, const buffer_desc_t::field_desc_t& fieldDesc, std::string& code )
{
  if (fieldDesc.type_ == buffer_desc_t::field_desc_t::INT){
    code += "int ";
  }
  else if (fieldDesc.type_ == buffer_desc_t::field_desc_t::FLOAT) {
    code += "float ";
  }
  else if (fieldDesc.type_ == buffer_desc_t::field_desc_t::VEC2){
    code += "vec2 ";
  }
  else if (fieldDesc.type_ == buffer_desc_t::field_desc_t::VEC3){
    code += "vec3 ";
  }
  else if (fieldDesc.type_ == buffer_desc_t::field_desc_t::VEC4){
    code += "vec4 ";
  }
  else if (fieldDesc.type_ == buffer_desc_t::field_desc_t::MAT4){
    code += "mat4 ";
  }
  else if (fieldDesc.type_ == buffer_desc_t::field_desc_t::COMPOUND_TYPE) {    
    code += bufferDesc.name_;
    code += "_";
    code += fieldDesc.name_;
    code += "_struct ";
  }

  code += fieldDesc.name_;

  if (fieldDesc.count_ != 1)
  {
    if (fieldDesc.count_ == 0){
      code += "[]";
    }
    else{
      code += "["; 
      code += fieldDesc.count_;
      code += "]";
    }
  }

  code += ";\n";
}

static void fieldDataTypesToGLSL(const buffer_desc_t& bufferDesc, const buffer_desc_t::field_desc_t& fieldDesc, std::string& result)
{
  //Inside out. First most internal structure
  for (int i = 0; i < fieldDesc.fields_.size(); i++)
    fieldDataTypesToGLSL(bufferDesc, fieldDesc.fields_[i], result);

    if (fieldDesc.type_ == buffer_desc_t::field_desc_t::COMPOUND_TYPE )
    {
      result += "struct ";
      result += bufferDesc.name_;
      result += "_";
      result += fieldDesc.name_;
      result += "_struct{\n";
      for (int j = 0; j <fieldDesc.fields_.size(); j++)
      {
        fieldDescriptionToGLSL(bufferDesc, fieldDesc.fields_[j], result);
      }
      result += "};\n";
    }
}

static std::string intToString(int n)
{
  char result[10];
  sprintf(result, "%d", n);
  return std::string(result);
}

static int stringToInt(const std::string& s)
{
  return std::stoi(s);
}

static render::vertex_format_t extractVertexFormatFromShader(std::string& code)
{
  char delimiters[5] = { ' ', '\n', '\t', '(', ')' };
  std::vector<std::string> tokens;  
  splitString(code, delimiters, 5, &tokens);

  struct attribute_desc_t
  {
    uint32_t offset;
    uint32_t size;
    render::vertex_attribute_t::format format;

    bool operator<(const attribute_desc_t& a)
    {
      return offset < a.offset;
    }
  };

  std::vector<attribute_desc_t> attributeDesc;
  uint32_t vertexSize = 0;
  for (int i = 0; i < tokens.size(); ++i)
  {
    if (tokens[i] == "in")
    {
      attribute_desc_t attribute = {};
      attribute.offset = stringToInt(tokens[i - 1]);
      
      if (tokens[i + 1] == "vec2")
      {
        attribute.format = render::vertex_attribute_t::format::VEC2;
        attribute.size = sizeof(float) * 2;
      }
      else if(tokens[i + 1] == "vec3")
      {
        attribute.format = render::vertex_attribute_t::format::VEC3;
        attribute.size = sizeof(float) * 3;
      }
      else if (tokens[i + 1] == "vec4")
      {
        attribute.format = render::vertex_attribute_t::format::VEC4;
        attribute.size = sizeof(float) * 4;
      }

      vertexSize += attribute.size;
      attributeDesc.push_back(attribute);
    }
  }
  
  std::sort(attributeDesc.begin(), attributeDesc.end());

  std::vector<render::vertex_attribute_t> vertexAttributes;
  uint32_t offset = 0;
  for (uint32_t i = 0; i < attributeDesc.size(); ++i)
  {
    render::vertex_attribute_t attribute = {};
    attribute.offset_ = offset;
    attribute.format_ = attributeDesc[i].format;
    attribute.stride_ = vertexSize;
    attribute.instanced_ = false;

    vertexAttributes.push_back(attribute);
    offset += attributeDesc[i].size;
  }

  render::vertex_format_t vertexFormat = {};
  render::vertexFormatCreate(&vertexAttributes[0], (uint32_t)vertexAttributes.size(), &vertexFormat);

  return vertexFormat;
}

static render::render_pass_t extractRenderPassFromShader(std::string& code)
{
  char delimiters[5] = { ' ', '\n', '\t', '(', ')' };
  std::vector<std::string> tokens;
  splitString(code, delimiters, 5, &tokens);


  for (int i = 0; i < tokens.size(); ++i)
  {
    if (tokens[i] == "out")
    {
      
    }
  }

  render::render_pass_t renderPass = {};
  return renderPass;
}


static std::string generateGlslCommon()
{
  char* code = R"(
    layout(set = 0, binding = 0) uniform _camera
    {
      mat4 worldToView;
      mat4 viewToWorld;
      mat4 projection;
      mat4 projectionInverse;
      vec4 imageSize;
    }camera;

    layout(set = 1, binding = 0) uniform _model
    {
      mat4 transform;
    }model; 

  )";

  return code;
}


static void generateGlslHeader(const std::vector<texture_desc_t>& textures,
                               const std::vector<buffer_desc_t>& buffers,
                               const char* version,
                               std::string& generatedCode)
{
  generatedCode = "#version ";
  generatedCode += version;
  generatedCode += "\n";

  //Data structures declarations
  for (uint32_t i = 0; i < buffers.size(); ++i)
  {
    for (int j = 0; j < buffers[i].fields_.size(); ++j)
    {
      std::string code;
      fieldDataTypesToGLSL(buffers[i], buffers[i].fields_[j], code);
      generatedCode += code;
    }
  }

  generatedCode += generateGlslCommon();

  //Textures
  for (uint32_t i = 0; i < textures.size(); ++i)
  {
    generatedCode += "layout(set=2, binding=";
    generatedCode += intToString(textures[i].binding_);
    generatedCode += ") uniform sampler2D ";
    generatedCode += textures[i].name_;
    generatedCode += ";\n";
  }

  //Buffers
  for (uint32_t i = 0; i < buffers.size(); ++i)
  {
    if (buffers[i].type_ == buffer_desc_t::UNIFORM_BUFFER){
      generatedCode += "layout(set=2, binding=";
      generatedCode += intToString(buffers[i].binding_);
      generatedCode += ") uniform _";
      generatedCode += buffers[i].name_;
      generatedCode += "{\n";
    }
    else{
      generatedCode += "layout(std140, set=2, binding=";
      generatedCode += intToString(buffers[i].binding_);
      generatedCode += ") readonly buffer _";
      generatedCode += buffers[i].name_;
      generatedCode += "{\n";
    }

    for (int j = 0; j < buffers[i].fields_.size(); ++j)
    {
      std::string code;
      fieldDescriptionToGLSL(buffers[i], buffers[i].fields_[j], code);
      generatedCode += code;
    }

    generatedCode += "}";
    generatedCode += buffers[i].name_;
    generatedCode += ";\n";
  }
}

static VkCompareOp depthTestFunctionFromString(const char* test)
{
  VkCompareOp result = VK_COMPARE_OP_LESS_OR_EQUAL;
  if (strcmp(test, "LEqual") == 0 ){
    result = VK_COMPARE_OP_LESS_OR_EQUAL;
  }
  else if (strcmp(test, "Never") == 0){
    result = VK_COMPARE_OP_NEVER;
  }
  else if (strcmp(test, "Always") == 0){
    result = VK_COMPARE_OP_ALWAYS;
  }
  else if (strcmp(test, "GEqual") == 0){
    result = VK_COMPARE_OP_GREATER_OR_EQUAL;
  }

  return result;
}

shader_t::shader_t()
:name_(),
textures_(),
buffers_()
{
}

shader_t::shader_t(const char* file, renderer_t* renderer)
:descriptorSetLayout_()
{
  initializeFromFile(file, renderer);
}

shader_t::~shader_t()
{
}

void shader_t::destroy(renderer_t* renderer)
{
  for (uint32_t i = 0; i < graphicsPipelineDescriptions_.size(); ++i)
  {
    if (vertexShaders_[i].handle_ != VK_NULL_HANDLE)
      render::shaderDestroy(renderer->getContext(), &vertexShaders_[i]);

    if (fragmentShaders_[i].handle_ != VK_NULL_HANDLE)
      render::shaderDestroy(renderer->getContext(), &fragmentShaders_[i]);

    render::vertexFormatDestroy(&vertexFormats_[i]);
    render::pipelineLayoutDestroy(renderer->getContext(), &pipelineLayouts_[i]);
  }

  for (uint32_t i(0); i < graphicsPipelines_.values_.size(); ++i)
  {
    for (uint32_t j(0); j < graphicsPipelines_.values_[i].size(); ++j)
    {
      render::graphicsPipelineDestroy(renderer->getContext(), &graphicsPipelines_.values_[i][j]);
    }
  }

  if (descriptorSetLayout_.handle_ != VK_NULL_HANDLE )
    render::descriptorSetLayoutDestroy(renderer->getContext(), &descriptorSetLayout_);

  textures_.clear();
  buffers_.clear();
  pass_.clear();
}

bool shader_t::initializeFromFile(const char* file, renderer_t* renderer)
{
  //Clean-up
  destroy(renderer);

  pugi::xml_document shaderFile;
  pugi::xml_parse_result result = shaderFile.load_file(file);
  if (!result)
  {
    //Print error
    return false;
  }

  pugi::xml_node shaderNode = shaderFile.child("Shader");
  if (shaderNode)
  {
    pugi::xml_attribute name(shaderNode.attribute("Name"));
    if (!name)
      return false;

    name_ = name.value();

    //Resources
    pugi::xml_node resourcesNode = shaderNode.child("Resources");
    uint32_t currentOffset_ = 0u;
    if (resourcesNode)
    {
      int32_t binding = 0;
      for (pugi::xml_node resourceNode = resourcesNode.child("Resource"); resourceNode; resourceNode = resourceNode.next_sibling("Resource"))
      {
        const char* resourceTypeAttr = resourceNode.attribute("Type").value();
        if (strcmp(resourceTypeAttr, "uniform_buffer") == 0 ||
            strcmp(resourceTypeAttr, "storage_buffer") == 0)
        {
          buffer_desc_t bufferDesc;
          deserializeBufferDescription(resourceNode, binding, &bufferDesc);
          buffers_.push_back(bufferDesc);
        }
        else if (strcmp(resourceTypeAttr, "texture2D") == 0)
        {
          texture_desc_t textureDesc;
          deserializeTextureDescription(resourceNode, binding, &textureDesc);
          textures_.push_back(textureDesc);
        }

        binding++;
      }
    }

    //Generate glsl code that will be appended to every shader in the file
    std::string glslHeader;
    generateGlslHeader(textures_, buffers_, shaderNode.attribute("Version").value(), glslHeader);
        
    render::context_t& context = renderer->getContext();

    //Descriptor set layout
    uint32_t descriptorCount = (uint32_t)(buffers_.size() + textures_.size());
    std::vector<render::descriptor_binding_t> bindings(descriptorCount);

    uint32_t bindingIndex = 0;
    for (uint32_t i(0); i < buffers_.size(); ++i)
    {
      render::descriptor_binding_t& binding = bindings[bindingIndex];
      switch (buffers_[i].type_)
      {
      case buffer_desc_t::UNIFORM_BUFFER:
        binding.type_ = render::descriptor_t::type::UNIFORM_BUFFER;
        break;
      case buffer_desc_t::STORAGE_BUFFER:
        binding.type_ = render::descriptor_t::type::STORAGE_BUFFER;
        break;
      }

      binding.binding_ = buffers_[i].binding_;
      binding.stageFlags_ = render::descriptor_t::stage::VERTEX | render::descriptor_t::stage::FRAGMENT;
      bindingIndex++;
    }

    for (uint32_t i(0); i < textures_.size(); ++i)
    {
      render::descriptor_binding_t& binding = bindings[bindingIndex];
      binding.type_ = render::descriptor_t::type::COMBINED_IMAGE_SAMPLER;
      binding.binding_ = textures_[i].binding_;
      binding.stageFlags_ = render::descriptor_t::stage::VERTEX | render::descriptor_t::stage::FRAGMENT;
      bindingIndex++;
    }

    descriptorSetLayout_ = {};
    render::descriptor_binding_t* bindingsPtr = bindings.empty() ? nullptr : &bindings[0];
    render::descriptorSetLayoutCreate(context, bindingsPtr, (uint32_t)bindings.size(), &descriptorSetLayout_);
    

    render::descriptor_set_layout_t descriptorSetLayouts[3] = {
      renderer->getGlobalsDescriptorSetLayout(),
      renderer->getObjectDescriptorSetLayout(),
      descriptorSetLayout_
    };

    //Render passes    
    uint32_t pass = 0;
    for (pugi::xml_node passNode = shaderNode.child("Pass"); passNode; passNode = passNode.next_sibling("Pass"))
    {
      pass_.push_back(hashString( passNode.attribute("Name").value()) );

      //Vertex shader
      render::shader_t vertexShader;
      std::string shaderCode = glslHeader;
      std::string vertexShaderCode = passNode.child("VertexShader").first_child().value();
      shaderCode += vertexShaderCode;
      render::shaderCreateFromGLSLSource(context, render::shader_t::VERTEX_SHADER, shaderCode.c_str(), &vertexShader);
      vertexShaders_.push_back(vertexShader);
      
      //Get vertex format from the code
      vertexFormats_.push_back( extractVertexFormatFromShader(vertexShaderCode) );
      

      //Fragment shader
      render::shader_t fragmentShader;
      shaderCode = glslHeader;
      shaderCode += passNode.child("FragmentShader").first_child().value();
      render::shaderCreateFromGLSLSource(context, render::shader_t::FRAGMENT_SHADER, shaderCode.c_str(), &fragmentShader);
      fragmentShaders_.push_back(fragmentShader);


      render::pipeline_layout_t pipelineLayout;
      render::pipelineLayoutCreate(context, descriptorSetLayouts, 3u, nullptr, 0u, &pipelineLayout);      
      pipelineLayouts_.push_back(pipelineLayout);

      bool depthWrite = true;
      pugi::xml_node zWrite = passNode.child("ZWrite");
      if (zWrite)
        depthWrite = strcmp(zWrite.attribute("Value").value(), "On") == 0 ? true : false;

      bool depthTest = true;
      VkCompareOp depthTestFunc = VK_COMPARE_OP_LESS_OR_EQUAL;
      pugi::xml_node zTest = passNode.child("ZTest");
      if (zTest)
      {
        if (strcmp(zTest.attribute("Value").value(), "Off") == 0)
        {
          depthTest = false;
        }
        else
        {
          depthTestFunc = depthTestFunctionFromString(zTest.attribute("Value").value());
        }
      }

      VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
      pugi::xml_node cull = passNode.child("Cull");
      if (cull)
      {
        if (strcmp(cull.attribute("Value").value(), "Front") == 0)
        {
          cullMode = VK_CULL_MODE_FRONT_BIT;
        }
        if (strcmp(cull.attribute("Value").value(), "Off") == 0)
        {
          cullMode = VK_CULL_MODE_NONE;
        }
      }

      render::graphics_pipeline_t::description_t pipelineDesc = {};      
      pipelineDesc.blendState_.resize(1);
      pipelineDesc.blendState_[0].colorWriteMask = 0xF;
      pipelineDesc.blendState_[0].blendEnable = VK_FALSE;
      pipelineDesc.cullMode_ = cullMode;
      pipelineDesc.depthTestEnabled_ = depthTest;
      pipelineDesc.depthWriteEnabled_ = depthWrite;
      pipelineDesc.depthTestFunction_ = depthTestFunc;
      pipelineDesc.vertexShader_ = vertexShader;
      pipelineDesc.fragmentShader_ = fragmentShader;
      graphicsPipelineDescriptions_.push_back(pipelineDesc);
    }

    return true;
  }
  
  return false;
}

core::render::graphics_pipeline_t shader_t::getPipeline(const char* name, frame_buffer_handle_t framebuffer, renderer_t* renderer)
{
  uint64_t passName = hashString(name);
  for (uint32_t i(0); i < pass_.size(); ++i)
  {
    if (passName == pass_[i])
      return getPipeline(i, framebuffer, renderer);
  }

  core::render::graphics_pipeline_t nullPipeline = {};
  return nullPipeline;
}

core::render::graphics_pipeline_t shader_t::getPipeline(uint32_t pass, frame_buffer_handle_t fb, renderer_t* renderer)
{
  std::vector<core::render::graphics_pipeline_t>* pipelines = graphicsPipelines_.get(fb);
  if (pipelines)
  {
    return pipelines->operator[](pass);
  }
  else
  {
    //Create all the pipelines and return the pipeline for the selected pass
    core::render::graphics_pipeline_t pipeline = {};
    uint32_t width = 0u;
    uint32_t height = 0u;
    VkRenderPass renderPass = {};
    
    frame_buffer_t* frameBuffer = renderer->getFrameBuffer(fb);
    width = frameBuffer->getWidth();
    height = frameBuffer->getHeight();
    renderPass = frameBuffer->getRenderPass().handle_;

    uint32_t count = (uint32_t)pass_.size();
    std::vector<core::render::graphics_pipeline_t> pipelines(count);
    for (uint32_t i = 0; i < count ; ++i)
    {
      graphicsPipelineDescriptions_[i].viewPort_ = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
      graphicsPipelineDescriptions_[i].scissorRect_ = { { 0,0 },{ width, height } };
      bkk::core::render::graphics_pipeline_t pipeline;
      bkk::core::render::graphicsPipelineCreate(renderer->getContext(), 
        renderPass, 0u, vertexFormats_[i], pipelineLayouts_[i], 
        graphicsPipelineDescriptions_[i], &pipelines[i]);

    }
    graphicsPipelines_.add(fb, pipelines);
    return pipelines[pass];
  }
}

core::render::descriptor_set_layout_t shader_t::getDescriptorSetLayout()
{
  return descriptorSetLayout_;
}

const std::vector<texture_desc_t>& shader_t::getTextureDescriptions() const
{
  return textures_;
}

const std::vector<buffer_desc_t>& shader_t::getBufferDescriptions() const
{
  return buffers_;
}