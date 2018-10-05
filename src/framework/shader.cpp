

#include "../external/pugixml/pugixml.hpp"

#include "framework/shader.h"

using namespace bkk::framework;

shader_t::shader_t()
  :name_(nullptr),
  properties_(),
  context_(nullptr)
{
  shader_ = {};
}

shader_t::shader_t(const char* filepath)
{
  pugi::xml_document shaderFile;
  pugi::xml_parse_result result = shaderFile.load_file(filepath);
  if (!result)
    return;

  pugi::xml_attribute name(shaderFile.attribute("Name"));
  if (!name)
    return;

  const pugi::char_t* shaderName = name.value();
  size_t shaderNameLenght = strlen(shaderName);
  name_ = new char[shaderNameLenght];
  strcpy(&name_[0], shaderName);

  //Properties
}


shader_t::~shader_t()
{
  if (name_)
    delete[] name_;

  //if(shader_.handle_ != VK_NULL_HANDLE )
  //  core::render::shaderDestroy( 
}