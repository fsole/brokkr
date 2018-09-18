
#include "dynamic-string.h"

bkk::string_t::string_t()
:data_(nullptr),
 size_(0u)
{
}

bkk::string_t::string_t(const bkk::string_t& s)
:bkk::string_t()
{
  bkk::string_t::operator=(s.data_);
}

bkk::string_t::string_t(const char* s)
:bkk::string_t()
{
  bkk::string_t::operator=(s);
};

bkk::string_t::~string_t()
{
  if (data_)
    delete[] data_;
}

void bkk::string_t::clear()
{
  if (data_)
    delete[] data_;

  data_ = nullptr;
  size_ = 0;
}

bool bkk::string_t::operator==(const bkk::string_t& s) const
{
  if (size_ == s.size_)
  {
    for (uint32_t i(0); i < size_; ++i)
    {
      if (data_[i] != s.data_[i])
        return false;
    }

    return true;
  }

  return false;
}

bool bkk::string_t::operator==(const char* s) const
{
  if (size_ == strlen(s))
  {
    for (uint32_t i(0); i < size_; ++i)
    {
      if (data_[i] != s[i])
        return false;
    }

    return true;
  }

  return false;
}

bool bkk::string_t::operator!=(const bkk::string_t& s) const
{
  return !(operator==(s));
}

bool bkk::string_t::operator!=(const char* s) const
{
  return !(operator==(s));
}

bool bkk::string_t::operator<(const bkk::string_t& s) const
{
  if (size_ != s.size_)
  {
    return size_ < s.size_;
  }
  else
  {
    for (uint32_t i(0); i < size_; ++i)
    {
      if (data_[i] != s.data_[i])
        return data_[i] < s.data_[i];
    }

    //strings are equal
    return false;
  }
}

bool bkk::string_t::empty() const
{
  return size_ == 0;
}

bkk::string_t& bkk::string_t::operator=(const char* s)
{
  if (data_)
    delete[] data_;

  size_ = (uint32_t)strlen(s);
  data_ = new char[size_ + 1];

  memcpy(data_, s, sizeof(char)*size_);
  data_[size_] = '\0';
  return *this;
}

bkk::string_t& bkk::string_t::operator=(const bkk::string_t& s)
{
  if (data_)
    delete[] data_;

  size_ = s.size_;
  data_ = new char[size_ + 1];

  memcpy(data_, s.data_, sizeof(char)*(size_ + 1));
  return *this;
}

bkk::string_t& bkk::string_t::operator+=(const bkk::string_t& s)
{
  uint32_t newSize = size_ + s.size_;
  char* oldContent = data_;
  data_ = new char[newSize + 1];
  if (oldContent)
  {
    memcpy(data_, oldContent, sizeof(char)*size_);
    delete[] oldContent;
  }

  memcpy(data_ + size_, s.data_, sizeof(char)*s.size_);
  size_ = newSize;
  data_[newSize] = '\0';

  return *this;
}

bkk::string_t& bkk::string_t::operator+=(const char* s)
{
  uint32_t sLen = (uint32_t)strlen(s);
  uint32_t newSize = size_ + sLen;
  char* oldContent = data_;
  data_ = new char[newSize + 1];
  if (oldContent)
  {
    memcpy(data_, oldContent, sizeof(char)*size_);
    delete[] oldContent;
  }

  memcpy(data_ + size_, s, sizeof(char)*sLen);

  size_ = newSize;
  data_[newSize] = '\0';
  return *this;
}

bkk::string_t bkk::string_t::operator+(const bkk::string_t& s) const
{
  string_t res(*this);
  return res += s;
}

bkk::string_t bkk::string_t::operator+(const char* s) const
{
  string_t res(*this);
  return res += s;
}

bkk::string_t bkk::string_t::substr(uint32_t first, uint32_t count) const
{
  bkk::string_t result;
  
  if (count > 0 && first+count <= size_)
  {
    result.data_ = new char[count + 1];
    memcpy(result.data_, &data_[first], sizeof(char) * count);
    result.data_[count] = '\0';
    result.size_ = count;
  }

  return result;
}

int32_t bkk::string_t::findLastOf(char c) const
{
  for (int i = size_-1; i >= 0; --i)
  {
    if (data_[i] == c)
      return i;
  }

  return -1;
}
