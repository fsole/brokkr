/*
* Copyright(c) Ferran Sole (2017-2019)
*
* This file is part of brokkr framework
* (see https://github.com/fsole/brokkr).
* The use of this software is governed by the LICENSE file.
*/

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
      field->fields.push_back(childField);
    }
  }

  field->name = fieldNode.attribute("Name").value();
  field->type = fieldType;
  field->byteOffset = offset;
  
  field->count = fieldNode.attribute("Count").empty() ? 1 :
    strcmp(fieldNode.attribute("Count").value(), "") == 0 ? 0 : fieldNode.attribute("Count").as_int();

  if (field->count > 1)
    fieldSize *= field->count;

  field->size = fieldSize;
  return fieldSize;
}

static void deserializeBufferDescription(pugi::xml_node resourceNode, uint32_t binding, buffer_desc_t* bufferDesc )
{
  bufferDesc->name = resourceNode.attribute("Name").value();
  bufferDesc->type = strcmp(resourceNode.attribute("Type").value(), "uniform_buffer") == 0 ?
    buffer_desc_t::UNIFORM_BUFFER : buffer_desc_t::STORAGE_BUFFER;

  bufferDesc->binding = binding;
  bufferDesc->shared = strcmp(resourceNode.attribute("Shared").value(), "yes") == 0;

  uint32_t offset = 0u;
  for (pugi::xml_node fieldNode = resourceNode.child("Field"); fieldNode; fieldNode = fieldNode.next_sibling("Field"))
  {
    buffer_desc_t::field_desc_t field;
    offset += deserializeFieldDescription(fieldNode, offset, &field);
    bufferDesc->fields.push_back(field);
  }

  bufferDesc->size = offset;
}

static void deserializeTextureDescription(pugi::xml_node resourceNode, uint32_t binding, texture_desc_t* textureDesc)
{
  textureDesc->name = resourceNode.attribute("Name").value();
  textureDesc->binding = binding;

  if (strcmp(resourceNode.attribute("Type").value(), "texture2D") == 0)
  {
    textureDesc->type = texture_desc_t::TEXTURE_2D;
  }
  else if (strcmp(resourceNode.attribute("Type").value(), "textureCube") == 0)
  {
    textureDesc->type = texture_desc_t::TEXTURE_CUBE;
  }
  else if (strcmp(resourceNode.attribute("Type").value(), "storageImage") == 0)
  {
    textureDesc->type = texture_desc_t::TEXTURE_STORAGE_IMAGE;
  }

  //Format
  if (strcmp(resourceNode.attribute("Format").value(), "RGBA32F") == 0)
  {
    textureDesc->format = texture_desc_t::FORMAT_RGBA32F;
  }
  else if (strcmp(resourceNode.attribute("Format").value(), "RGBA32I") == 0)
  {
    textureDesc->format = texture_desc_t::FORMAT_RGBA32I;
  }
  else if (strcmp(resourceNode.attribute("Format").value(), "RGBA32UI") == 0)
  {
    textureDesc->format = texture_desc_t::FORMAT_RGBA32UI;
  }
  else if (strcmp(resourceNode.attribute("Format").value(), "RGBA8I") == 0)
  {
    textureDesc->format = texture_desc_t::FORMAT_RGBA8I;
  }
  else if (strcmp(resourceNode.attribute("Format").value(), "RGBA8UI") == 0)
  {
    textureDesc->format = texture_desc_t::FORMAT_RGBA8UI;
  }
  
}

static void fieldDescriptionToGLSL(const buffer_desc_t& bufferDesc, const buffer_desc_t::field_desc_t& fieldDesc, std::string& code )
{
  switch (fieldDesc.type)
  {
    case buffer_desc_t::field_desc_t::INT:
    {
      code += "int ";
      break;
    }
    case buffer_desc_t::field_desc_t::FLOAT:
    {
      code += "float ";
      break;
    }
    case buffer_desc_t::field_desc_t::VEC2:
    {
      code += "vec2 ";
      break;
    }
    case buffer_desc_t::field_desc_t::VEC3:
    {
      code += "vec3 ";
      break;
    }
    case buffer_desc_t::field_desc_t::VEC4:
    {
      code += "vec4 ";
      break;
    }
    case buffer_desc_t::field_desc_t::MAT4:
    {
      code += "mat4 ";
      break;
    }
    case buffer_desc_t::field_desc_t::COMPOUND_TYPE:
    {
      code += bufferDesc.name;
      code += "_";
      code += fieldDesc.name;
      code += "_struct ";
      break;
    }
  }

  code += fieldDesc.name;

  if (fieldDesc.count != 1)
  {
    if (fieldDesc.count == 0){
      code += "[]";
    }
    else{
      code += "["; 
      code += intToString( fieldDesc.count );
      code += "]";
    }
  }

  code += ";\n";
}

static void fieldDataTypesToGLSL(const buffer_desc_t& bufferDesc, const buffer_desc_t::field_desc_t& fieldDesc, std::string& result)
{
  //Inside out. First most internal structure
  for (int i = 0; i < fieldDesc.fields.size(); i++)
    fieldDataTypesToGLSL(bufferDesc, fieldDesc.fields[i], result);

    if (fieldDesc.type == buffer_desc_t::field_desc_t::COMPOUND_TYPE )
    {
      result += "struct ";
      result += bufferDesc.name;
      result += "_";
      result += fieldDesc.name;
      result += "_struct{\n";
      for (int j = 0; j <fieldDesc.fields.size(); j++)
      {
        fieldDescriptionToGLSL(bufferDesc, fieldDesc.fields[j], result);
      }
      result += "};\n";
    }
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
    render::vertex_attribute_t::format_e format;

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
        attribute.format = render::vertex_attribute_t::format_e::VEC2;
        attribute.size = sizeof(float) * 2;
      }
      else if(tokens[i + 1] == "vec3")
      {
        attribute.format = render::vertex_attribute_t::format_e::VEC3;
        attribute.size = sizeof(float) * 3;
      }
      else if (tokens[i + 1] == "vec4")
      {
        attribute.format = render::vertex_attribute_t::format_e::VEC4;
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
    attribute.offset = offset;
    attribute.format = attributeDesc[i].format;
    attribute.stride = vertexSize;
    attribute.instanced = false;

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
    for (int j = 0; j < buffers[i].fields.size(); ++j)
    {
      std::string code;
      fieldDataTypesToGLSL(buffers[i], buffers[i].fields[j], code);
      generatedCode += code;
    }
  }

  generatedCode += generateGlslCommon();

  //Textures
  for (uint32_t i = 0; i < textures.size(); ++i)
  {
    generatedCode += "layout(set=2, binding=";
    generatedCode += intToString(textures[i].binding);

    if (textures[i].type == texture_desc_t::TEXTURE_2D)
    {
      generatedCode += ") uniform sampler2D ";
    }
    else if (textures[i].type == texture_desc_t::TEXTURE_CUBE)
    {
      generatedCode += ") uniform samplerCube ";
    }

    generatedCode += textures[i].name;
    generatedCode += ";\n";
  }

  //Buffers
  for (uint32_t i = 0; i < buffers.size(); ++i)
  {
    if (buffers[i].type == buffer_desc_t::UNIFORM_BUFFER){
      generatedCode += "layout(set=2, binding=";
      generatedCode += intToString(buffers[i].binding);
      generatedCode += ") uniform _";
      generatedCode += buffers[i].name;
      generatedCode += "{\n";
    }
    else{
      generatedCode += "layout(std140, set=2, binding=";
      generatedCode += intToString(buffers[i].binding);
      generatedCode += ") readonly buffer _";
      generatedCode += buffers[i].name;
      generatedCode += "{\n";
    }

    for (int j = 0; j < buffers[i].fields.size(); ++j)
    {
      std::string code;
      fieldDescriptionToGLSL(buffers[i], buffers[i].fields[j], code);
      generatedCode += code;
    }

    generatedCode += "}";
    generatedCode += buffers[i].name;
    generatedCode += ";\n";
  }
}

static void generateGlslHeaderCompute(const std::vector<texture_desc_t>& textures,
  const std::vector<buffer_desc_t>& buffers,
  const char* version, uint32_t localSizeX, uint32_t localSizeY, uint32_t localSizeZ,
  std::string& generatedCode)
{
  generatedCode = "#version ";
  generatedCode += version;
  generatedCode += "\n";
  generatedCode += "#extension GL_ARB_separate_shader_objects : enable\n";
  generatedCode += "#extension GL_ARB_shading_language_420pack : enable\n";
  generatedCode += "layout(local_size_x = " + intToString(localSizeX);
  generatedCode += ", local_size_y = " + intToString(localSizeY);
  generatedCode += ", local_size_z = " + intToString(localSizeZ);
  generatedCode += " ) in;\n";

  //Data structures declarations
  for (uint32_t i = 0; i < buffers.size(); ++i)
  {
    for (int j = 0; j < buffers[i].fields.size(); ++j)
    {
      std::string code;
      fieldDataTypesToGLSL(buffers[i], buffers[i].fields[j], code);
      generatedCode += code;
    }
  }

  //Textures
  for (uint32_t i = 0; i < textures.size(); ++i)
  {
    generatedCode += "layout(set=0, binding=";
    generatedCode += intToString(textures[i].binding);

    switch (textures[i].format)
    {
    case texture_desc_t::FORMAT_RGBA8I:
      generatedCode += ", rgba8i)";
      break;
    case texture_desc_t::FORMAT_RGBA8UI:
      generatedCode += ", rgba8ui)";
      break;
    case texture_desc_t::FORMAT_RGBA32I:
      generatedCode += ", rgba32i)";
      break;
    case texture_desc_t::FORMAT_RGBA32UI:
      generatedCode += " rgba32ui)";
      break;
    case texture_desc_t::FORMAT_RGBA32F:
      generatedCode += ", rgba32f)";
      break;
    }

    if( (textures[i].type == texture_desc_t::TEXTURE_2D) || (textures[i].type == texture_desc_t::TEXTURE_STORAGE_IMAGE) )
    {
      generatedCode += " uniform image2D ";
    }

    generatedCode += textures[i].name;
    generatedCode += ";\n";
  }

  //Buffers
  for (uint32_t i = 0; i < buffers.size(); ++i)
  {
    if (buffers[i].type == buffer_desc_t::UNIFORM_BUFFER) {
      generatedCode += "layout(set=0, binding=";
      generatedCode += intToString(buffers[i].binding);
      generatedCode += ") uniform _";
      generatedCode += buffers[i].name;
      generatedCode += "{\n";
    }
    else {
      generatedCode += "layout(std140, set=0, binding=";
      generatedCode += intToString(buffers[i].binding);
      generatedCode += ") buffer _";
      generatedCode += buffers[i].name;
      generatedCode += "{\n";
    }

    for (int j = 0; j < buffers[i].fields.size(); ++j)
    {
      std::string code;
      fieldDescriptionToGLSL(buffers[i], buffers[i].fields[j], code);
      generatedCode += code;
    }

    generatedCode += "}";
    generatedCode += buffers[i].name;
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

static void parseResources( core::render::context_t& context,
                            const pugi::xml_node& resourcesNode,
                            std::vector<texture_desc_t>* textures,
                            std::vector<buffer_desc_t>* buffers,
                            core::render::descriptor_set_layout_t* descriptorSetLayout)
{
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
        (*buffers).push_back(bufferDesc);
      }
      else if ( (strcmp(resourceTypeAttr, "texture2D") == 0) ||
                (strcmp(resourceTypeAttr, "textureCube") == 0) ||
                (strcmp(resourceTypeAttr, "storageImage") == 0))
      {
        texture_desc_t textureDesc;
        deserializeTextureDescription(resourceNode, binding, &textureDesc);
        (*textures).push_back(textureDesc);
      }

      binding++;
    }
  }

  //Descriptor set layout
  uint32_t descriptorCount = (uint32_t)( (*buffers).size() + (*textures).size());
  std::vector<render::descriptor_binding_t> bindings(descriptorCount);

  uint32_t bindingIndex = 0;
  for (uint32_t i(0); i < (*buffers).size(); ++i)
  {
    render::descriptor_binding_t& binding = bindings[bindingIndex];
    switch ( (*buffers)[i].type)
    {
    case buffer_desc_t::UNIFORM_BUFFER:
      binding.type = render::descriptor_t::type_e::UNIFORM_BUFFER;
      break;
    case buffer_desc_t::STORAGE_BUFFER:
      binding.type = render::descriptor_t::type_e::STORAGE_BUFFER;
      break;
    }

    binding.binding = (*buffers)[i].binding;
    binding.stageFlags = render::descriptor_t::stage_e::VERTEX | render::descriptor_t::stage_e::FRAGMENT | render::descriptor_t::stage_e::COMPUTE;
    bindingIndex++;
  }

  for (uint32_t i(0); i < (*textures).size(); ++i)
  {
    render::descriptor_binding_t& binding = bindings[bindingIndex];

    binding.type = render::descriptor_t::type_e::COMBINED_IMAGE_SAMPLER;
    if( (*textures)[i].type == texture_desc_t::TEXTURE_STORAGE_IMAGE )
      binding.type = render::descriptor_t::type_e::STORAGE_IMAGE;

    binding.binding = (*textures)[i].binding;

    //TODO: Get stage flags from file
    binding.stageFlags = render::descriptor_t::stage_e::VERTEX | render::descriptor_t::stage_e::FRAGMENT | render::descriptor_t::stage_e::COMPUTE;
    bindingIndex++;
  }

  *descriptorSetLayout = {};
  render::descriptor_binding_t* bindingsPtr = bindings.empty() ? nullptr : &bindings[0];
  render::descriptorSetLayoutCreate(context, bindingsPtr, (uint32_t)bindings.size(), descriptorSetLayout);
}

render::graphics_pipeline_t::description_t parsePipelineDescription(const pugi::xml_node& passNode )
{
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

  //Blend state
  VkPipelineColorBlendAttachmentState defaultBlend = {
    VK_FALSE,
    VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
    VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0xF };

  std::vector<VkPipelineColorBlendAttachmentState> blendStates(1);
  blendStates[0] = defaultBlend;

  for (pugi::xml_node blendNode = passNode.child("Blend"); blendNode; blendNode = blendNode.next_sibling("Blend"))
  {
    uint32_t target = stringToInt(blendNode.attribute("Target").value());
    if (target >= (uint32_t)blendStates.size())
    {
      uint32_t oldSize = (uint32_t)blendStates.size();
      uint32_t newSize = target + 1u;

      blendStates.resize(newSize);
      for (uint32_t i(oldSize); i < newSize; ++i)
        blendStates[i] = defaultBlend;
    }
  }

  render::graphics_pipeline_t::description_t pipelineDesc = {};
  pipelineDesc.blendState = blendStates;
  pipelineDesc.cullMode = cullMode;
  pipelineDesc.depthTestEnabled = depthTest;
  pipelineDesc.depthWriteEnabled = depthWrite;
  pipelineDesc.depthTestFunction = depthTestFunc;

  return pipelineDesc;
}


/**************************
* shader_t Implementation *
***************************/
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
    if (vertexShaders_[i].handle != VK_NULL_HANDLE)
      render::shaderDestroy(renderer->getContext(), &vertexShaders_[i]);

    if (fragmentShaders_[i].handle != VK_NULL_HANDLE)
      render::shaderDestroy(renderer->getContext(), &fragmentShaders_[i]);

    render::vertexFormatDestroy(&vertexFormats_[i]);
    render::pipelineLayoutDestroy(renderer->getContext(), &pipelineLayouts_[i]);
  }

  std::vector< std::vector<core::render::graphics_pipeline_t> >& pipelines = graphicsPipelines_.data();
  for (uint32_t i(0); i < pipelines.size(); ++i)
  {
    for (uint32_t j(0); j < pipelines[i].size(); ++j)
      render::graphicsPipelineDestroy(renderer->getContext(), &pipelines[i][j]);
  }

  if (descriptorSetLayout_.handle != VK_NULL_HANDLE)
  {
    render::descriptorSetLayoutDestroy(renderer->getContext(), &descriptorSetLayout_);
  }

  for( uint32_t i(0); i<computePipelines_.size(); ++i )
  {
    render::shaderDestroy(renderer->getContext(), &computeShaders_[i]);
    render::pipelineLayoutDestroy(renderer->getContext(), &pipelineLayouts_[i]);
    render::computePipelineDestroy(renderer->getContext(), &computePipelines_[i]);
  }

  descriptorSetLayout_ = {};
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
    if (name)
      name_ = name.value();

    render::context_t& context = renderer->getContext();

    //Resources
    pugi::xml_node resourcesNode = shaderNode.child("Resources");
    parseResources(context, resourcesNode, &textures_, &buffers_, &descriptorSetLayout_);

    //Compute shader
    if (shaderNode.child("ComputeShader") )
    {
      for (pugi::xml_node computeShaderNode = shaderNode.child("ComputeShader"); computeShaderNode; computeShaderNode = computeShaderNode.next_sibling("ComputeShader"))
      {
        pass_.push_back(hashString(computeShaderNode.attribute("Name").value()));

        uint32_t localSizeX = 1;
        if (computeShaderNode.attribute("LocalSizeX"))
          localSizeX = stringToInt(computeShaderNode.attribute("LocalSizeX").value());

        uint32_t localSizeY = 1;
        if (computeShaderNode.attribute("LocalSizeY"))
          localSizeY = stringToInt(computeShaderNode.attribute("LocalSizeY").value());

        uint32_t localSizeZ = 1;
        if (computeShaderNode.attribute("LocalSizeZ"))
          localSizeZ = stringToInt(computeShaderNode.attribute("LocalSizeZ").value());

        std::string computeShaderCode;
        generateGlslHeaderCompute(textures_, buffers_, shaderNode.attribute("Version").value(),
          localSizeX, localSizeY, localSizeZ, computeShaderCode);

        computeShaderCode += computeShaderNode.first_child().value();

        //Create shader and pipeline
        render::shader_t computeShader = {};
        render::shaderCreateFromGLSLSource(context, render::shader_t::COMPUTE_SHADER, computeShaderCode.c_str(), &computeShader);
        render::pipeline_layout_t pipelineLayout = {};
        render::pipelineLayoutCreate(context, &descriptorSetLayout_, 1u, nullptr, 0u, &pipelineLayout);
        render::compute_pipeline_t pipeline = {};
        render::computePipelineCreate(context, pipelineLayout, computeShader, &pipeline);

        computeShaders_.push_back(computeShader);
        pipelineLayouts_.push_back(pipelineLayout);
        computePipelines_.push_back(pipeline);
      }
    }
    else
    {
      //Generate glsl code that will be appended to every shader in the file
      std::string glslHeader;
      generateGlslHeader(textures_, buffers_, shaderNode.attribute("Version").value(), glslHeader);

      uint32_t pass = 0;
      for (pugi::xml_node passNode = shaderNode.child("Pass"); passNode; passNode = passNode.next_sibling("Pass"))
      {
        pass_.push_back(hashString(passNode.attribute("Name").value()));

        //Vertex shader
        render::shader_t vertexShader;
        std::string shaderCode = glslHeader;
        std::string vertexShaderCode = passNode.child("VertexShader").first_child().value();
        shaderCode += vertexShaderCode;
        render::shaderCreateFromGLSLSource(context, render::shader_t::VERTEX_SHADER, shaderCode.c_str(), &vertexShader);
        vertexShaders_.push_back(vertexShader);

        //Vertex format
        vertexFormats_.push_back(extractVertexFormatFromShader(vertexShaderCode));
        
        //Fragment shader
        render::shader_t fragmentShader;
        shaderCode = glslHeader;
        shaderCode += passNode.child("FragmentShader").first_child().value();
        render::shaderCreateFromGLSLSource(context, render::shader_t::FRAGMENT_SHADER, shaderCode.c_str(), &fragmentShader);
        fragmentShaders_.push_back(fragmentShader);

        //Pipeline layout
        render::pipeline_layout_t pipelineLayout;
        render::descriptor_set_layout_t descriptorSetLayouts[3] = {
          renderer->getGlobalsDescriptorSetLayout(),
          renderer->getObjectDescriptorSetLayout(),
          descriptorSetLayout_
        };

        render::pipelineLayoutCreate(context, descriptorSetLayouts, 3u, nullptr, 0u, &pipelineLayout);
        pipelineLayouts_.push_back(pipelineLayout);

        //Pipeline description
        render::graphics_pipeline_t::description_t pipelineDesc = parsePipelineDescription(passNode);
        pipelineDesc.vertexShader = vertexShader;
        pipelineDesc.fragmentShader = fragmentShader;
        graphicsPipelineDescriptions_.push_back(pipelineDesc);
      }
    }
    return true;
  }  
  return false;
}

core::render::graphics_pipeline_t shader_t::getPipeline(const char* name, frame_buffer_bkk_handle_t framebuffer, renderer_t* renderer)
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

core::render::graphics_pipeline_t shader_t::getPipeline(uint32_t pass, frame_buffer_bkk_handle_t fb, renderer_t* renderer)
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
    renderPass = frameBuffer->getRenderPass().handle;

    uint32_t count = (uint32_t)pass_.size();
    std::vector<core::render::graphics_pipeline_t> pipelines(count);
    for (uint32_t i = 0; i < count ; ++i)
    {
      graphicsPipelineDescriptions_[i].viewPort = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
      graphicsPipelineDescriptions_[i].scissorRect = { { 0,0 },{ width, height } };
      if (graphicsPipelineDescriptions_[i].blendState.size() < frameBuffer->getTargetCount())
      {
        uint32_t oldSize = (uint32_t)graphicsPipelineDescriptions_[i].blendState.size();
        uint32_t newSize = frameBuffer->getTargetCount();
        graphicsPipelineDescriptions_[i].blendState.resize(newSize);
        for (uint32_t j(0);  j< newSize; ++j)
        {
          graphicsPipelineDescriptions_[i].blendState[j] = {
            VK_FALSE,
            VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
            VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0xF };
        }
      }

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

uint32_t shader_t::getPassIndexFromName(const char* pass) const
{
  if (pass != nullptr)
  {
    uint64_t passName = hashString(pass);
    for (uint32_t i(0); i < pass_.size(); ++i)
    {
      if (passName == pass_[i])
        return i;
    }
  }

  return 0;
}


core::render::compute_pipeline_t shader_t::getComputePipeline(const char* name)
{
  uint64_t hash = hashString(name);
  for (uint32_t i(0); i < pass_.size(); ++i)
  {
    if (hash == pass_[i])
      return getComputePipeline(i);
  }

  return core::render::compute_pipeline_t{};
}

core::render::compute_pipeline_t shader_t::getComputePipeline(uint32_t pass)
{
  if (pass >= computePipelines_.size()) 
    return core::render::compute_pipeline_t{};

  return computePipelines_[pass];
}

render::pipeline_layout_t shader_t::getPipelineLayout(const char* name)
{
  uint64_t hash = hashString(name);
  for (uint32_t i(0); i < pass_.size(); ++i)
  {
    if (hash == pass_[i])
      return getPipelineLayout(i);
  }

  return render::pipeline_layout_t{};
}

core::render::pipeline_layout_t shader_t::getPipelineLayout(uint32_t pass)
{
  if (pass >= pipelineLayouts_.size())
    return core::render::pipeline_layout_t{};

  return pipelineLayouts_[pass];
}
