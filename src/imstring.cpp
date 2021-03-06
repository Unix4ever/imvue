/*
Copyright (c) 2019 Artem Chernyshev

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "imstring.h"
#include "imgui.h"
#include "imgui_internal.h"

ImString::ImString()
  : mData(0)
  , mRefCount(0)
{
}

ImString::ImString(size_t len)
  : mData(0)
  , mRefCount(0)
{
  reserve(len);
}

ImString::ImString(char* string)
  : mData(string)
  , mRefCount(0)
{
  ref();
}

ImString::ImString(const char* string)
  : mData(0)
  , mRefCount(0)
{
  if(string) {
    mData = ImStrdup(string);
    ref();
  }
}

ImString::ImString(const ImString& other)
{
  mRefCount = other.mRefCount;
  mData = other.mData;
  ref();
}

ImString::~ImString()
{
  unref();
}

char& ImString::operator[](size_t pos) {
  return mData[pos];
}

ImString::operator char* () { return mData; }

bool ImString::operator==(const char* string)
{
  return strcmp(string, mData) == 0;
}

bool ImString::operator!=(const char* string)
{
  return strcmp(string, mData) != 0;
}

bool ImString::operator==(const ImString& string)
{
  return strcmp(string.c_str(), mData) == 0;
}

bool ImString::operator!=(const ImString& string)
{
  return strcmp(string.c_str(), mData) != 0;
}

ImString& ImString::operator=(const char* string)
{
  if(mData)
    unref();
  mData = ImStrdup(string);
  ref();
  return *this;
}

ImString& ImString::operator=(const ImString& other)
{
  if(mData && mData != other.mData)
    unref();
  mRefCount = other.mRefCount;
  mData = other.mData;
  ref();
  return *this;
}

void ImString::reserve(size_t len)
{
  if(mData)
    unref();
  mData = (char*)ImGui::MemAlloc(len + 1);
  mData[len] = '\0';
  ref();
}

char* ImString::get() {
  return mData;
}

const char* ImString::c_str() const {
  return mData;
}

bool ImString::empty() const {
  return mData == 0 || mData[0] == '\0';
}

int ImString::refcount() const {
  return *mRefCount;
}

void ImString::ref() {
  if(!mRefCount) {
    mRefCount = new int();
    (*mRefCount) = 0;
  }
  (*mRefCount)++;
}

void ImString::unref()
{
  if(mRefCount) {
    (*mRefCount)--;
    if(*mRefCount == 0) {
      ImGui::MemFree(mData);
      mData = 0;
      delete mRefCount;
    }
    mRefCount = 0;
  }
}
