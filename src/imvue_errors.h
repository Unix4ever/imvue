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

#ifndef __IMVUE_ERRORS_H__
#define __IMVUE_ERRORS_H__

#include <stdexcept>

namespace ImVue {

  static std::string format(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char smallBuffer[1024];
    size_t size = vsnprintf(smallBuffer, sizeof smallBuffer, format, args);
    va_end(args);

    if (size < 1024) 
      return std::string(smallBuffer);

    char* buffer = (char*)ImGui::MemAlloc(size  + 1);

    va_start(args, format);
    vsnprintf(buffer, size + 1, format, args);
    va_end(args);
    return std::string(buffer);
  }

#ifndef IMVUE_LOG_ERROR
#define IMVUE_LOG_ERROR(desc, file, line) std::cout << desc << " " << file << ":" << line << "\n";
#endif

#if defined(IMVUE_NO_EXCEPTIONS)
#if defined(IMVUE_LOG_ERROR)
#define IMVUE_EXCEPTION(cls, ...) IMVUE_LOG_ERROR(format(__VA_ARGS__), __FILE__, __LINE__)
#else
#define IMVUE_EXCEPTION(cls, ...)
#endif
#else
#define IMVUE_EXCEPTION(cls, ...) throw cls(format(__VA_ARGS__))
#endif

  /**
   * Raised when script execution fails
   */
  class ScriptError : public std::runtime_error {
    public:
      ScriptError(const std::string& detailedMessage)
        : runtime_error("Script execution error: " + detailedMessage)
      {
      }
  };

  /**
   * Element creation failure (incorrect arguments, missing required arguments)
   */
  class ElementError : public std::runtime_error {
    public:
      ElementError(const std::string& detailedMessage)
        : runtime_error("Element build error: " + detailedMessage)
      {
      }
  };

} // namespace ImVue

#endif
