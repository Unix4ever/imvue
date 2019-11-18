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


#include "imgui.h"

#ifndef __IM_STRING__
#define __IM_STRING__

class ImString {

  public:
    ImString();

    ImString(size_t len);

    ImString(char* string);

    explicit ImString(const char* string);

    ImString(const ImString& other);

    ~ImString();

    char& operator[](size_t pos);

    operator char* ();

    bool operator==(const char* string);

    bool operator!=(const char* string);

    bool operator==(const ImString& string);

    bool operator!=(const ImString& string);

    ImString& operator=(const char* string);

    ImString& operator=(const ImString& other);

    inline size_t size() const { return mData ? strlen(mData) + 1 : 0; }

    void reserve(size_t len);

    char* get();

    const char* c_str() const;

    bool empty() const;

    int refcount() const;

    void ref();

    void unref();

  private:

    char* mData;
    int* mRefCount;
};

#endif
