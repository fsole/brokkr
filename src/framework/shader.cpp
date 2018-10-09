

#include "../external/pugixml/pugixml.hpp"

#include "framework/shader.h"
#include <iostream>

using namespace bkk;
using namespace bkk::framework;

///Helper methods
static uint32_t deserializeFieldDescription(pugi::xml_node fieldNode, uint32_t offset, buffer_desc_t::field_desc_t* field)
{
  uint32_t fieldSize = 0;
  buffer_desc_t::field_desc_t::type_e fieldType = buffer_desc_t::field_desc_t::TYPE_COUNT;
  if (strcmp(fieldNode.attribute("Type").value(), "float") == 0){
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
}

static void deserializeTextureDescription(pugi::xml_node resourceNode, uint32_t binding, texture_desc_t* textureDesc)
{
  textureDesc->name_ = resourceNode.attribute("Name").value();
  textureDesc->type_ = texture_desc_t::TEXTURE_2D;
  textureDesc->binding_ = binding;
}

static void fieldDescriptionToGLSL(const buffer_desc_t& bufferDesc, const buffer_desc_t::field_desc_t& fieldDesc, std::string& code )
{
  if (fieldDesc.type_ == buffer_desc_t::field_desc_t::FLOAT){
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

  //Textures
  for (uint32_t i = 0; i < textures.size(); ++i)
  {
    generatedCode += "layout(set=0, binding=";
    generatedCode += intToString(textures[i].binding_);
    generatedCode += ") uniform sampler2D ";
    generatedCode += textures[i].name_;
    generatedCode += ";\n";
  }

  //Buffers
  for (uint32_t i = 0; i < buffers.size(); ++i)
  {
    generatedCode += "layout(set=0, binding=";
    generatedCode += intToString(buffers[i].binding_);

    if (buffers[i].type_ == buffer_desc_t::UNIFORM_BUFFER){
      generatedCode += ") uniform _";
      generatedCode += buffers[i].name_;
      generatedCode += "{\n";
    }
    else{
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

shader_t::shader_t()
:name_(),
textures_(),
buffers_(),
context_(nullptr)
{
}

shader_t::shader_t(const char* file, core::render::context_t* context)
:name_(),
 context_(context)
{
  initializeFromFile(file, context);
}

void shader_t::clear()
{
  textures_.clear();
  buffers_.clear();
  passNames_.clear();

  for (uint32_t i = 0; i < vertexShaders_.size(); ++i)
  {
    if (vertexShaders_[i].handle_ != VK_NULL_HANDLE)
      core::render::shaderDestroy(*context_, &vertexShaders_[i]);
  }

  for (uint32_t i = 0; i < fragmentShaders_.size(); ++i)
  {
    if (fragmentShaders_[i].handle_ != VK_NULL_HANDLE)
      core::render::shaderDestroy(*context_, &fragmentShaders_[i]);
  }
}

shader_t::~shader_t()
{
  clear();
}

bool shader_t::initializeFromFile(const char* file, core::render::context_t* context)
{
  //Clean-up
  clear();

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
    generateGlslHeader(textures_, buffers_, shaderNode.attribute("Version").value(), glslHeader_);

    //Render passes
    uint32_t pass = 0;    
    for (pugi::xml_node passNode = shaderNode.child("Pass"); passNode; passNode = passNode.next_sibling("Pass"))
    {
      passNames_.push_back(passNode.attribute("Name").value() );

      std::string shaderCode = glslHeader_;
      shaderCode += passNode.child("VertexShader").first_child().value();
      std::cout << shaderCode << std::endl;

      core::render::shader_t vertexShader;
      bkk::core::render::shaderCreateFromGLSLSource(*context_, bkk::core::render::shader_t::VERTEX_SHADER, shaderCode.c_str(), &vertexShader);
      vertexShaders_.push_back(vertexShader);

      shaderCode = glslHeader_;
      shaderCode += passNode.child("FragmentShader").first_child().value();
      std::cout << shaderCode << std::endl;

      core::render::shader_t fragmentShader;
      bkk::core::render::shaderCreateFromGLSLSource(*context_, bkk::core::render::shader_t::FRAGMENT_SHADER, shaderCode.c_str(), &fragmentShader);
      fragmentShaders_.push_back(fragmentShader);
    }

    return true;
  }
  else
  {
    return false;
  }
}