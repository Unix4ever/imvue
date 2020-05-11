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

#ifndef __IMVUE_ELEMENT_H__
#define __IMVUE_ELEMENT_H__

#include "rapidxml.hpp"
#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "imvue_script.h"
#include "imvue_errors.h"
#include "imvue_context.h"
#include "imvue_style.h"
#include "imvue_layout.h"
#include <vector>
#include <map>
#include <iostream>
#include <cstring>
#include <sstream>
#include <unordered_map>

#ifdef _WIN32
#include <tchar.h>
#include <algorithm>
#else
#include <stdlib.h>
#include <ctype.h>
#endif

namespace ImVue {

  struct CmpChar {

    bool operator()(const char* a, const char* b) const
    {
      return std::strcmp(a, b) < 0;
    }
  };

  /**
   * ID for element text pseudo-attribute name
   */
  static const char* TEXT_ID = "@text";

  /**
   * TEXT node ID
   */
  static const char* TEXT_NODE = "__textnode__";

  /**
   * UNTYPED ELEMENT
   */
  static const char* UNTYPED = "__untyped__";

  // attribute converting utilities
  // used for parsing statically defined fields as ints/floats/arrays etc
  namespace detail {

    inline void str_to_floating(const char* value, float* dest)
    {
      *dest = std::stof(value);
    }

    inline void str_to_floating(const char* value, double* dest)
    {
      *dest = std::stod(value);
    }

    inline void str_to_floating(const char* value, long double* dest)
    {
      *dest = std::stold(value);
    }

    template<class C>
    void str_to_floating(const char* value, C* dest)
    {
      *dest = (C)std::stod(value);
    }

    template<class C>
    typename std::enable_if<std::is_floating_point<C>::value, bool>::type
    str_to_number(const char* value, C* dest) {
      try {
        str_to_floating(value, dest);
      } catch(std::exception const & e) {
        (void)e;
        return false;
      }
      return true;
    }

    inline void str_to_int(const char* value, long long* dest)
    {
      *dest = std::stoll(value);
    }

    inline void str_to_int(const char* value, unsigned long long* dest)
    {
      *dest = std::stoull(value);
    }

    template<class C>
    typename std::enable_if<std::is_signed<C>::value>::type
    str_to_int(const char* value, C* dest)
    {
      *dest = (C)std::stol(value);
    }

    template<class C>
    typename std::enable_if<!std::is_signed<C>::value>::type
    str_to_int(const char* value, C* dest)
    {
      *dest = (C)std::stoul(value);
    }

    template<class C>
    typename std::enable_if<std::is_integral<C>::value, bool>::type
    str_to_number(const char* value, C* dest) {
      try {
        str_to_int(value, dest);
      } catch(std::exception const & e) {
        (void)e;
        return false;
      }

      return true;
    }

    inline bool read(const char* value, char** dest) {
      if(*dest != NULL) {
        ImGui::MemFree(*dest);
        *dest = NULL;
      }

      if(value)
        *dest = ImStrdup(value);
      return true;
    }

    template<class C>
    bool parse_array(const char* value, ImVector<C>& values, char separator = ',');

    inline bool read(const char* value, ImGuiID* dest) {
      if(!str_to_number<ImGuiID>(value, dest)) {
        *dest = ImHashStr(value);
      }

      return true;
    }

    inline bool read(const char* value, bool* dest) {
      // flag mode
      if(value[0] == '\0') {
        *dest = true;
        return true;
      }

      if(std::strcmp(value, "true") == 0) {
        *dest = true;
        return true;
      }

      return str_to_number<bool>(value, dest);
    }

    inline bool read(const char* value, ImVec2* dest)
    {
      ImVector<float> values;
      values.reserve(2);
      if(!parse_array<float>(value, values)) {
        return false;
      }
      dest->x = values[0];
      dest->y = values[1];
      return true;
    }

    inline bool read(const char* value, ImVec4* dest)
    {
      ImVector<float> values;
      values.reserve(4);
      if(!parse_array<float>(value, values)) {
        return false;
      }
      dest->x = values[0];
      dest->y = values[1];
      dest->z = values[2];
      dest->w = values[3];
      return true;
    }

    template<class C, size_t N>
    bool read(const char* value, C (*dest)[N])
    {
      ImVector<C> values;
      values.reserve(N);
      if(!parse_array<C>(value, values) || values.size() != N) {
        return false;
      }
      memcpy(*dest, values.Data, N * sizeof(C));
      return true;
    }

    template<class C>
    typename std::enable_if<std::is_arithmetic<C>::value, bool>::type
    read(const char* value, C** dest)
    {
      if(*dest) {
        ImGui::MemFree(*dest);
        *dest = NULL;
      }
      ImVector<C> values;
      if(!parse_array<C>(value, values)) {
        return false;
      }
      *dest = (C*)ImGui::MemAlloc(values.size_in_bytes());
      memcpy(*dest, values.Data, values.size_in_bytes());
      return true;
    }

    template<class C>
    typename std::enable_if<std::is_arithmetic<C>::value, bool>::type
    read(const char* value, C* dest) {
      return str_to_number<C>(value, dest);
    }

    template<class C>
    typename std::enable_if<!std::is_arithmetic<C>::value, bool>::type
    read(const char* value, C* dest)
    {
      IM_ASSERT(false && "Detected unsupported field");
      (void)value;
      (void)dest;
      // default unsupported behaviour
      return false;
    }

    template<class C>
    bool parse_array(const char* value, ImVector<C>& values, char separator)
    {
      char endsym = 0;
      const char* start = value;
      while(isspace(start[0])) {
        start++;
      }
      switch(start[0]) {
        case '{':
          endsym = '}';
          start++;
          break;
        case '[':
          endsym = ']';
          start++;
          break;
        case '(':
          endsym = ')';
          start++;
          break;
      }

      const char* end = value + strlen(value);
      const size_t bufferSize = 512;
      char part[bufferSize] = {0};

      end = (endsym == 0 ? end : ImStrchrRange(value, end, endsym)) - 1;
      while(start < end + 1) {
        const char* next = ImStrchrRange(start, end, separator);
        if(next == 0) {
          next = end + 1;
        }

        size_t len = (size_t)(next - start);
        if(len == 0) {
          break;
        }

        if(len > bufferSize) {
          IMVUE_EXCEPTION(ElementError, "failed to parse array element. Max supported length is %d, got %d", bufferSize, len);
#if IMVUE_NO_EXCEPTIONS
          start = next + 1;
          return false;
#endif
        }

        memcpy(&part[0], start, len);
        part[len] = '\0';

        values.resize(values.size() + 1);
        values[values.size() - 1] = 0;
        if(!read(&part[0], &values[values.size() - 1])) {
          return false;
        }
        start = next + 1;
      }
      return true;
    }

    // Script state object conversion

    inline bool read(Object object, Object* dest)
    {
      *dest = object;
      return true;
    }

    template<class C, size_t N>
    inline bool read(Object& obj, C (*dest)[N])
    {
      size_t index = 0;
      for(Object::iterator iter = obj.begin(); iter != obj.end() && index < N; ++iter, ++index) {
        (*dest)[index] = iter.value.as<C>();
      }
      return true;
    }

    inline bool read(Object& obj, ImGuiID* dest) {
      switch(obj.type()) {
        case ObjectType::STRING:
          *dest = ImHashStr(obj.as<ImString>().get());
          break;
        case ObjectType::NUMBER:
          *dest = obj.as<ImGuiID>();
          break;
        default:
          return false;
      }
      return true;
    }

    inline bool read(Object& obj, char** dest) {
      if(*dest != NULL) {
        ImGui::MemFree(*dest);
        *dest = NULL;
      }

      ImString str = obj.as<ImString>();
      if(!str.get()) {
        return false;
      }

      *dest = ImStrdup(str.get());
      return true;
    }

    inline bool read(Object& obj, bool* dest) {
      *dest = obj.as<bool>();
      return true;
    }

    inline bool read(Object& value, ImVec2* dest)
    {
      ObjectKeys keys;
      value.keys(keys);
      dest->x = value[keys[0]].as<float>();
      dest->y = value[keys[1]].as<float>();
      return true;
    }

    inline bool read(Object& value, ImVec4* dest)
    {
      ObjectKeys keys;
      value.keys(keys);
      dest->x = value[keys[0]].as<float>();
      dest->y = value[keys[1]].as<float>();
      dest->z = value[keys[2]].as<float>();
      dest->w = value[keys[3]].as<float>();
      return true;
    }

    template<class C>
    typename std::enable_if<std::is_arithmetic<C>::value, bool>::type
    read(Object& value, C** dest)
    {
      if(*dest) {
        ImGui::MemFree(*dest);
        *dest = NULL;
      }
      ImVector<C> values;
      for(Object::iterator iter = value.begin(); iter != value.end(); ++iter) {
        values.push_back(iter.value.as<C>());
      }
      *dest = (C*)ImGui::MemAlloc(values.size_in_bytes());
      memcpy(*dest, values.Data, values.size_in_bytes());
      return true;
    }

    template<class C>
    typename std::enable_if<std::is_arithmetic<C>::value, bool>::type
    read(Object& value, C* dest)
    {
      try {
        *dest = value.as<C>();
      } catch(const ScriptError& e) {
        (void)e;
        return false;
      }
      return true;
    }

    template<class C>
    typename std::enable_if<!std::is_arithmetic<C>::value, bool>::type
    read(Object& value, C* dest)
    {
      IM_ASSERT(false && "Detected unsupported field");
      (void)value;
      (void)dest;
      // default unsupported behaviour
      return false;
    }
  } // namespace detail

  // -----------------------------------------------------------------------------------------
  // parser core

  class Attribute;
  class ContainerElement;
  class ElementFactory;
  class EventHandler;
  class ElementBuilder;

  /**
   * Represents basic scene element
   */
  class Element {

    public:
      typedef std::vector<Element*> Elements;

      enum InvalidationFlag {
        BUILD  = 1 << 0,
        STYLE  = 1 << 1,
        MODEL  = 1 << 2,
        LAYOUT = 1 << 3
      };

      // mutually excluding element states
      enum ElementState {
        HIDDEN   = 1 << 0,
        ACTIVE   = 1 << 1,
        HOVERED  = 1 << 2,
        DISABLED = 1 << 3,
        CHECKED  = 1 << 4,
        LINK     = 1 << 5,
        VISITED  = 1 << 6,
        FOCUSED  = 1 << 7
      };

      enum Flag {
        CONTAINER       = 1 << 0,
        PSEUDO_ELEMENT  = 1 << 1,
        COMPONENT       = 1 << 2,
        WINDOW          = 1 << 3,
        BUTTON          = 1 << 4,
        NO_LAYOUT       = 1 << 5
      };

      typedef std::unordered_map<ScriptState::FieldHash, bool> ReactiveFields;

      Element();
      virtual ~Element();

      /**
       * Element render
       */
      void render();

      /**
       * Gets element type
       */
      inline const char* getType() const { (void)UNTYPED; return mType; }

      /**
       * Get element flags
       */
      inline unsigned int getFlags() const { return mFlags; }

      /**
       * Set parent
       */
      inline void setParent(Element* element) { mParent = element; }

      /**
       * Get parent
       */
      ContainerElement* getParent(bool noPseudoElements = false);

      /**
       * Enable element
       */
      inline void enable() {
        if(!mConfigured)
          invalidateFlags(Element::BUILD);

        invalidateFlags(Element::STYLE);
        enabled = true;
      }

      /**
       * Get element ctx
       */
      inline Context* context()
      {
        return mCtx;
      }

      /**
       * Get element script context
       *
       * @param recursive traverse to parent context
       */
      inline ScriptState::Context* getContext(bool recursive = false) {
        if(mScriptContext) {
          return mScriptContext;
        }

        if(recursive && mParent) {
          return mParent->getContext(recursive);
        }

        return NULL;
      }

      /**
       * Initial element set up
       * reads xml node for the first time, adds reactivity if script state is set up
       *
       * @param node xml node
       * @param ctx Context that stores pointers to element factory, texture manager, script interface and other
       * @param sctx Additional script context (used in iterator)
       * @param parent parent element
       */
      void configure(rapidxml::xml_node<>* node, Context* ctx, ScriptState::Context* sctx = 0, Element* parent = 0);

      /**
       * Trigger evaluation of the property which is linked with field id
       *
       * @param id changed field id
       */
      void invalidate(const char* id);

      /**
       * Trigger invalidation of some specific kind
       *
       * @param flags Invalidate flags
       */
      void invalidateFlags(unsigned int flags);

      /**
       * Force element flags
       */
      inline void setFlag(Element::Flag flag)
      {
        mFlags |= flag;
      }

      /**
       * Check if Element is container
       */
      inline bool isContainer() const {
        return (mFlags & Element::CONTAINER) != 0;
      }

      /**
       * Check if Element shouldn't have layouts enabled
       */
      inline bool skipLayout() const {
        return (mFlags & Element::NO_LAYOUT) != 0;
      }

      /**
       * Check if Element is pseudo element
       */
      inline bool isPseudoElement() const {
        return (mFlags & Element::PSEUDO_ELEMENT) != 0;
      }

      /**
       * Get attribute interface by field id
       *
       * @param id field id
       */
      const Attribute* getAttribute(const char* id) const;

      /**
       * Check if underlying node has attribute
       *
       * @param id attribute name
       */
      inline bool hasAttribute(const char* name) const {
        return mNode ? mNode->first_attribute(name) != 0 : false;
      }

      /**
       * Evaluates attribute
       *
       * @param id attribute name
       * @param dest Destination to write to
       *
       * @returns true if succeed
       */
      bool evalAttribute(const char* id, char** dest);

      /**
       * Exposed mainly for tests
       */
      inline ScriptState* getState() {
        return mScriptState;
      }

      inline rapidxml::xml_node<>* node() { return mNode; }

      /**
       * Get width - left and right padding
       */
      inline float contentWidth() const
      {
        return getSize().x - padding[0] - padding[2];
      }

      /**
       * Get height - top and bottom padding
       */
      inline float contentHeight() const
      {
        return getSize().y - padding[1] - padding[3];
      }

      void setSize(const ImVec2& size);

      inline ImVec2 getSize() const {
        ImVec2 res = size;
        if(res.x <= 0) {
          res.x = computedSize.x;
        }

        if(res.y <= 0) {
          res.y = computedSize.y;
        }

        return res;
      }

      inline void setDisplay(uint16_t d)
      {
        if(display != d) {
          display = d;
          if(isContainer()) {
            invalidateFlags(Element::LAYOUT);
          }
        }
      }

      void setInlineStyle(char* style);

      void setClasses(const char* cls, int flags, ScriptState::Fields* fields);

      bool isHovered(ImGuiHoveredFlags flags = 0) const;

      inline bool hasClass(const char* cls) const
      {
        return classes.find(const_cast<char*>(cls)) != classes.end();
      }

      inline ComputedStyle* style() {
        return &mStyle;
      }

      inline bool visible() const {
        return enabled && display != CSS_DISPLAY_NONE;
      }

      inline bool hasState(ElementState s) {
        return (mState & s) != 0;
      }

      inline ImRect getMarginsRect() const {
        return ImRect(
          ImVec2(
            margins[1] == FLT_MIN ? 0 : margins[1],
            margins[0] == FLT_MIN ? 0 : margins[0]
          ),
          ImVec2(
            margins[2] == FLT_MIN ? 0 : margins[2],
            margins[3] == FLT_MIN ? 0 : margins[3]
          )
        );
      }

      // public properties

      /**
       * Element rendering is enabled
       */
      bool enabled;

      /**
       * Element id
       */
      char* id;

      /**
       * Element ref
       */
      char* ref;

      /**
       * Render condition
       */
      char* enabledAttr;

      /**
       * Element key
       */
      char* key;

      // style related variables

      ImVec2 pos;
      /**
       * Common size definition
       */
      ImVec2 size;
      ImVec2 minSize;
      ImVec2 computedSize;

      /**
       * Element display type
       */
      uint16_t display;

      /**
       * Element index
       */
      int index;
      int state;

      float padding[4];
      float margins[4];

      ImU32 bgColor;

      typedef std::map<char*, bool, CmpChar> Classes;
      Classes classes;

    protected:

      friend class ContainerElement;

      void bindListeners(ScriptState::Fields& fields, const char* attribute = 0, unsigned int flags = 0);

      void bindListener(ScriptState::FieldHash field, const char* attribute = 0, unsigned int flags = 0);

      typedef std::unordered_map<std::string, bool> DirtyProperties;

      virtual Element* createElement(rapidxml::xml_node<>* node, ScriptState::Context* sctx = 0, Element* parent = 0);

      virtual bool build();

      void readProperty(const char* name, const char* value, int flags = 0);

      virtual bool initAttribute(const char* id, const char* value, int flags = 0, ScriptState::Fields* fields = 0);

      /**
       * Renders actual element data
       * Override this method to render element
       */
      virtual void renderBody() {
        IM_ASSERT(false && "tried to render abstract element");
      }

      virtual void computeProperties();

      void fireCallback(ScriptState::LifecycleCallbackType cb, bool schedule = false);

      void addHandler(const char* name, const char* value, const ElementBuilder* builder);


      inline void setState(ElementState s)
      {
        if((mState & s) == 0) {
          mState |= s;
          invalidateFlags(Element::STYLE);

          for(size_t i = 0; i < mChildren.size(); ++i) {
            mChildren[i]->invalidateFlags(Element::STYLE);
          }
        }
      }

      inline void resetState(ElementState s) {
        if((mState & s) != 0) {
          mState ^= s;
          invalidateFlags(Element::STYLE);

          for(size_t i = 0; i < mChildren.size(); ++i) {
            mChildren[i]->invalidateFlags(Element::STYLE);
          }
        }
      }

      rapidxml::xml_node<>* mNode;

      typedef std::map<const char*, EventHandler*> Handlers;

      Handlers mHandlers;
      Elements mChildren;
      Element* mParent;
      ElementFactory* mFactory;
      ElementBuilder* mBuilder;
      TextureManager* mTextureManager;
      ScriptState* mScriptState;
      ReactiveFields mReactiveFields;
      DirtyProperties mDirtyProperties;
      Context* mCtx;
      ScriptState::Context* mScriptContext;
      ComputedStyle mStyle;

      unsigned int mInvalidFlags;
      unsigned int mFlags;
      unsigned int mState;
      int mRequiredAttrsCount;

      bool mConfigured;
      const char* mType;

  }; // class Element


  /**
   * Base class for all properties/handlers
   */
  class ElementEntity {
    public:
      ElementBuilder* owner;
  };

  /**
   * Property reader from attribute/text or from script
   */
  class Attribute : public ElementEntity {
    public:
      enum EvalFlags {
        // binds change listeners during attribute value evaluation
        BIND_LISTENERS    = 1 << 0,
        // used to evaluate text, which can contain {{variables}}
        TEMPLATED_STRING  = 1 << 1,
        // evaluation of :value="script", text will be evaluated as a script
        SCRIPT            = 1 << 2
      };

      Attribute(bool r = false)
        : required(r)
      {
      }

      virtual ~Attribute() {}

      /**
       * Read property from attribute
       *
       * @param attribute XML attribute name or @text
       * @param str XML attribute string
       * @param element target element to read
       * @param scriptState script state to use for eval
       * @param flags evaluation flags
       */
      virtual void read(
          const char* attr,
          const char* str,
          Element* element,
          ScriptState* scriptState,
          int flags = 0,
          ScriptState::Fields* fields = 0
      ) const = 0;

      /**
       * Copies value into the Object
       *
       * @param element Element to copy value from
       * @param dest script object to copy data into
       *
       * @return true if copy was successful
       */
      virtual bool copy(Element* element, Object& dest) const = 0;

      bool required;
  };

  template<typename Type>
  bool evaluateString(const char* str, Element* element, ScriptState* scriptState, int flags, ScriptState::Fields* fields, Type* value)
  {
    bool success = false;
    if(scriptState) {
      if(flags & Attribute::SCRIPT) {
        Object object = scriptState->getObject(str, fields, element->getContext());
        if(!object.valid()) {
          IMVUE_EXCEPTION(ScriptError, "failed to evaluate data %s", str);
          return false;
        }
        success = detail::read(object, value);
      } else if(flags & Attribute::TEMPLATED_STRING) {
        std::string retval;
        std::stringstream result;
        std::stringstream ss;
        bool evaluation = false;

        for(int i = 0;;i++) {
          if(str[i] == '\0') {
            break;
          }

          if(evaluation && std::strncmp(&str[i], "}}", 2) == 0) {
            Object object = scriptState->getObject(&ss.str()[0], fields, element->getContext());
            if(!object) {
              continue;
            }
            ss = std::stringstream();
            evaluation = false;
            result << object.as<ImString>().c_str();
            i++;
            continue;
          }

          if(!evaluation && std::strncmp(&str[i], "{{", 2) == 0) {
            evaluation = true;
            i++;
            continue;
          }

          if(evaluation) {
            ss << str[i];
          } else {
            result << str[i];
          }
        }

        success = detail::read(&result.str()[0], value);
      }
    }

    if(!success) {
      success = detail::read(str, value);
    }

    return success;
  }

  /**
   * MemPtr implementation of attribute reader
   */
  template<class C, class D>
  class AttributeMemPtr : public Attribute {
    public:
      AttributeMemPtr(D C::* memPtr, bool required = false)
        : Attribute(required)
        , mMemPtr(memPtr)
      {
      }

      virtual ~AttributeMemPtr() {}

      void read(const char* attribute, const char* str, Element* element, ScriptState* scriptState, int flags = 0, ScriptState::Fields* fields = 0) const
      {
        D* dest = &(static_cast<C*>(element)->*mMemPtr);
        bool success = evaluateString(str, element, scriptState, flags, fields, dest);
        if(!success && required) {
          IMVUE_EXCEPTION(ElementError, "failed to read required attribute %s", attribute);
        }
      }

      bool copy(Element* element, Object& dest) const
      {
        D* value = &(static_cast<C*>(element)->*mMemPtr);
        return dest.set(value, typeID<D>::value());
      }

    private:
      D C::* mMemPtr;
  };

  template<typename Type>
  typename std::enable_if<std::is_pointer<Type>::value, Type>::type initVariable()
  {
    return 0;
  }

  template<typename Type>
  typename std::enable_if<!std::is_pointer<Type>::value, Type>::type initVariable()
  {
    static_assert(std::is_trivially_constructible<Type>::value, "setter can only be used with trivially constructible types");
    return Type();
  }

  /**
   * Setter implementation of attribute reader
   */
  template<class C, typename Type>
  class AttributeSetter : public Attribute {
    public:

      typedef void(C::*SetterFunc)(Type);

      AttributeSetter(SetterFunc func, bool required = false)
        : Attribute(required)
        , mSetter(func)
      {
        static_assert(std::is_trivially_constructible<Type>::value, "setter can only be used with trivially constructible types");
      }

      void read(const char* attribute, const char* str, Element* element, ScriptState* scriptState, int flags = 0, ScriptState::Fields* fields = 0) const
      {
        Type value = initVariable<Type>();
        bool success = evaluateString(str, element, scriptState, flags, fields, &value);

        if(success) {
          (static_cast<C*>(element)->*mSetter)(value);
        } else if(required) {
          IMVUE_EXCEPTION(ElementError, "failed to read required attribute %s", attribute);
        }
      }

      bool copy(Element* element, Object& dest) const
      {
        (void)element;
        (void)dest;
        return false;
      }

    private:
      SetterFunc mSetter;
  };

  /**
   * Handler factory interface
   */
  class HandlerFactory : public ElementEntity {
    public:
      virtual ~HandlerFactory() {}
      virtual EventHandler* create(Element* element, const char* handlerName, const char* script) const = 0;
  };

  /**
   * Simple class factory
   */
  template<class C>
  class HandlerFactoryImpl : public HandlerFactory {
    public:
      virtual ~HandlerFactoryImpl() {}
      EventHandler* create(Element* element, const char* handlerName, const char* script) const {
        return new C(element, handlerName, script);
      }
  };

  /**
   * Abstract element builder, not to be used externally
   */
  class ElementBuilder {
    public:
      typedef ImVector<const char*> RequiredAttrs;
      RequiredAttrs mRequiredAttrs;

      virtual ~ElementBuilder()
      {
        for(Attributes::iterator iter = mAttributes.begin(); iter != mAttributes.end(); ++iter) {
          if(iter->second->owner == this) {
            delete iter->second;
          }
        }
        mAttributes.clear();
        for(Handlers::iterator iter = mHandlers.begin(); iter != mHandlers.end(); ++iter) {
          if(iter->second->owner == this)
            delete iter->second;
        }
        mHandlers.clear();
      }

      virtual Element* create(rapidxml::xml_node<>* node, Context* ctx, ScriptState::Context* sctx, Element* parent = 0) const = 0;

      inline Attribute* get(const char* name) const {
        if(mAttributes.find(name) != mAttributes.end()) {
          return mAttributes.at(name);
        }

        return nullptr;
      }

      inline EventHandler* createHandler(Element* element, const char* name, const char* fullName, const char* script) const {
        if(mHandlers.find(name) != mHandlers.end()) {
          return mHandlers.at(name)->create(element, fullName, script);
        }

        return nullptr;
      }

      const ElementBuilder::RequiredAttrs& getRequiredAttrs() const
      {
        return mRequiredAttrs;
      }

    protected:

      friend class ElementFactory;

      void merge(ElementBuilder& other) {
        if(other.mTag == mTag) {
          return;
        }

        // merge attributes
        for(Attributes::iterator iter = other.mAttributes.begin(); iter != other.mAttributes.end(); ++iter) {
          if(mAttributes.find(iter->first) == mAttributes.end()) {
            mAttributes[iter->first] = iter->second;
          }
        }

        // merge handlers
        for(Handlers::iterator iter = other.mHandlers.begin(); iter != other.mHandlers.end(); ++iter) {
          if(mHandlers.find(iter->first) == mHandlers.end()) {
            mHandlers[iter->first] = iter->second;
          }
        }

        if(other.mRequiredAttrs.size() != 0) {
          int oldSize = mRequiredAttrs.size();
          mRequiredAttrs.resize(oldSize + other.mRequiredAttrs.size());
          memcpy(&mRequiredAttrs.Data[oldSize], other.mRequiredAttrs.Data, other.mRequiredAttrs.size_in_bytes());
        }
      }

      int getLayer(ElementFactory* f);

      void readInheritance(ElementFactory* f);

      typedef std::map<const char*, Attribute*, CmpChar> Attributes;
      Attributes mAttributes;

      typedef std::map<const char*, HandlerFactory*, CmpChar> Handlers;
      Handlers mHandlers;

      typedef std::vector<ImString> Inheritance;
      Inheritance mInheritance;

      ImString mTag;
  };

  /**
   * Particular element builder, not to be used externally
   */
  template<class C>
  class ElementBuilderImpl : public ElementBuilder {
    public:
      Element* create(rapidxml::xml_node<>* node, Context* ctx, ScriptState::Context* sctx = 0, Element* parent = 0) const {
        C* res = new C();
        try {
          res->configure(node, ctx, sctx, parent);
        } catch (...) {
          delete res;
          throw;
        }
        return res;
      }

      /**
       * Register attribute
       *
       * @param name attribute name
       * @param memPtr destination to read into (&Element::variable)
       * @param required property is required
       */
      template<class Type>
      ElementBuilderImpl<C>& attribute(const char* name, Type C::* memPtr, bool required = false)
      {
        registerAttribute(name, new AttributeMemPtr<C, Type>(memPtr, required));
        return *this;
      }

      /**
       * Register attribute setter
       *
       * @param name attribute name
       * @param func setter function to use (&Element::setValue)
       * @param required property is required
       */
      template<class Type>
      ElementBuilderImpl<C>& setter(const char* name, void(C::*func)(Type), bool required = false)
      {
        registerAttribute(name, new AttributeSetter<C, Type>(func, required));
        return *this;
      }


      /**
       * Register text reader
       *
       * @param memPtr destination to read into
       * @param required property is required
       */
      template<class D>
      ElementBuilderImpl<C>& text(D C::* memPtr, bool required = false)
      {
        registerAttribute(TEXT_ID, new AttributeMemPtr<C, D>(memPtr, required));
        return *this;
      }

      /**
       * Register text setter
       *
       * @param func setter function to use (&Element::setValue)
       * @param required property is required
       */
      template<class Type>
      ElementBuilderImpl<C>& text(void(C::*func)(Type), bool required = false)
      {
        registerAttribute(TEXT_ID, new AttributeSetter<C, Type>(func, required));
        return *this;
      }

      /**
       * Defines event handler
       * @param name handler id
       */
      template<class H>
      ElementBuilderImpl<C>& handler(const char* name)
      {
        if(mHandlers.count(name) != 0) {
          delete mHandlers[name];
        }
        mHandlers[name] = new HandlerFactoryImpl<H>();
        mHandlers[name]->owner = this;
        return *this;
      }

      /**
       * Defines inheritance
       * @param name base classes to inherit from
       */
      ElementBuilderImpl<C>& inherits(const char* name)
      {
        if(mTag == name) {
          return *this;
        }
        mInheritance.push_back(ImString(name));
        return *this;
      }

      void registerAttribute(const char* id, Attribute* attr)
      {
        if(mAttributes.find(id) != mAttributes.end()) {
          delete mAttributes[id];
        }

        mAttributes[id] = attr;
        attr->owner = this;
        if(attr->required) {
          mRequiredAttrs.push_back(id);
        }
      }
  };

  /**
   * Factory that keeps all element builders.
   * Can be extended by using element<class>(...) function.
   */
  class ElementFactory {

      typedef std::map<const char*, ElementBuilder*, CmpChar> ElementBuilders;

    public:
      ElementFactory()
        : mDirty(false)
      {
      }

      ~ElementFactory() {
        if(mDirty) {
          buildInheritance();
        }

        for(Layers::reverse_iterator iter = mLayers.rbegin(); iter != mLayers.rend(); ++iter) {
          for(ElementBuilders::iterator it = iter->second.begin(); it != iter->second.end(); ++it) {
            delete it->second;
          }
        }
      }

      /**
       * Declare new element kind
       *
       * @param tagName Associated tag name
       */
      template<class C>
      ElementBuilderImpl<C>& element(const char* tagName)
      {
        if(mElementBuilders.find(tagName) == mElementBuilders.end()) {
          ElementBuilderImpl<C>* b = new ElementBuilderImpl<C>();
          b->mTag = tagName;
          if(std::is_base_of<ContainerElement, C>::value && b->mTag != "template") {
            b->inherits("template");
          } else {
            b->inherits("__element__");
          }
          mElementBuilders[tagName] = b;
          mDirty = true;
        }
        return static_cast<ElementBuilderImpl<C>&>(*mElementBuilders[tagName]);
      }

      /**
       * Get ElementBuilder using tagName
       *
       * @param tagName element tag name
       */
      inline ElementBuilder* get(const char* tagName)
      {
        if(mDirty) {
          buildInheritance();
        }

        if(mElementBuilders.find(tagName) == mElementBuilders.end()) {
          return NULL;
        }

        return mElementBuilders.at(tagName);
      }

    private:
      void buildInheritance();

      ElementBuilders mElementBuilders;

      typedef std::map<int, ElementBuilders> Layers;

      // representation of element builders sorted by inheritance
      Layers mLayers;

      bool mDirty;
  };

  // -----------------------------------------------------------------------------------------

  /**
   * Abstract event handler
   */
  class EventHandler {

    public:
      EventHandler(Element* element, const char* handlerName, const char* script);
      virtual ~EventHandler() {}

      inline const char* getScript() const { return mScript; }

      virtual bool check() = 0;

      void parseProperties();

      inline bool checkProp(const char* prop) { return mProperties.count(ImHashStr(prop)) != 0; }

    protected:
      // override this function to implement a new handler

      const char* mHandlerName;
      const char* mScript;

      typedef std::unordered_map<ImU32, bool> Properties;
      Properties mProperties;
      Element* mElement;
  };

  inline int modState();

  class InputHandler : public EventHandler {
    public:
      enum Modifiers {
        Ctrl  = 1 << 0,
        Alt   = 1 << 1,
        Shift = 1 << 2,
        Super = 1 << 3
      };

      InputHandler(Element* element, const char* handlerName, const char* script);
      inline bool checkModifiers() {
        int current = modState();
        return mExact ? (current == mModifiers) : ((current | mModifiers) || !mModifiers);
      }
    protected:
      int mModifiers;
      bool mExact;
  };

  /**
   * Modifier check functions
   */
  inline int modState() {
    int state = 0;
    ImGuiIO& io = ImGui::GetIO();
    if(io.KeyCtrl) {
      state |= InputHandler::Modifiers::Ctrl;
    }
    if(io.KeyShift) {
      state |= InputHandler::Modifiers::Shift;
    }
    if(io.KeyAlt) {
      state |= InputHandler::Modifiers::Alt;
    }
    if(io.KeySuper) {
      state |= InputHandler::Modifiers::Super;
    }
    return state;
  }

  /**
   * Mouse events handler
   */
  class MouseEventHandler : public InputHandler {

    public:

      enum Type {
        Click,
        DoubleClick,
        MouseDown,
        MouseUp,
        MouseOver,
        MouseOut
      };

      MouseEventHandler(Element* element, const char* handlerName, const char* script);
    protected:
      bool check();
    private:
      int mType;
      int mMouseButton;
      bool mHovered;
  };

  /**
   * Custom events handler
   */
  class ChangeEventHandler : public EventHandler {
    public:
      ChangeEventHandler(Element* element, const char* handlerName, const char* script) : EventHandler(element, handlerName, script) {}
    protected:
      bool check() {
        return ImGui::IsItemEdited();
      }
  };

  /**
   * Keyboard events handler
   */
  class KeyboardEventHandler : public InputHandler {
    public:
      enum Type {
        Down,
        Up,
        Press
      };

      KeyboardEventHandler(Element* element, const char* handlerName, const char* script);

      typedef std::unordered_map<ImU32, int> KeyMap;
      static KeyMap init();
    protected:
      bool check();

      void parseKeyCode(const char* data);
      int mKeyIndex;
      int mType;

      static const KeyMap keyMap;
  };

  /**
   * ContainerElement can have child elements
   */
  class ContainerElement : public Element
  {
    public:
      ContainerElement();
      virtual ~ContainerElement();

      inline const Elements& getChildren() const {
        return mChildren;
      }

      /**
       * Selector that behaves the same way as jQuery $
       */
      template<class C>
      ImVector<C*> getChildren(const char* query, bool recursive = false)
      {
        ImVector<C*> result;
        int kind = 0;
        if(query[0] == '#') {        // get by id
          kind = 1;
        } else if(query[0] == '.') { // get by class
          kind = 2;
        } else if(query[0] == '*') { // get all
          kind = 3;
        }

        for(Elements::iterator iter = mChildren.begin(); iter != mChildren.end(); ++iter) {
          bool push = false;
          Element* e = *iter;
          bool pseudoElement = (e->getFlags() & Element::PSEUDO_ELEMENT);

          if((pseudoElement || recursive) && e->isContainer()) {
            ImVector<C*> nested = static_cast<ContainerElement*>(e)->getChildren<C>(query, recursive);
            result.reserve(result.size() + nested.size());
            for(int i = 0; i < nested.size(); i++) {
              result.push_back(nested[i]);
            }
          }

          if(pseudoElement) {
            continue;
          }

          switch(kind) {
            case 0:
              push = std::strcmp(query, e->getType()) == 0;
              break;
            case 1:
              if(!e->id) {
                continue;
              }
              push = std::strcmp(&query[1], e->id) == 0;
              break;
            case 2:
              push = e->hasClass(&query[1]);
              break;
            case 3:
              push = true;
              break;
          }

          if(push) {
            result.push_back(static_cast<C*>(e));
          }
        }

        return result;
      }

      typedef void(*ElementCallback)(Element*, void*);

      /**
       * Call function for each element in the container and each for each PSEUDO_ELEMENT children stored in the container
       *
       * @param callback Callback function
       * @param userdata To be passed into callback function
       */
      void forEachChild(ElementCallback callback, void* userdata);

    protected:
      typedef void(RenderHook)(Element*);

      /**
       * Render container
       */
      virtual void renderBody();
      /**
       * Override this method to build the element
       */
      virtual bool build();

      virtual void removeChildren();

      void createChildren(rapidxml::xml_node<>* doc = 0);

      void renderChildren();

      Layout* mLayout;

      RenderHook* mPostRenderHook;
  };

  class PseudoElement : public ContainerElement {
    public:
      PseudoElement() {
        mFlags |= Element::PSEUDO_ELEMENT;
      }

      virtual ~PseudoElement()
      {
        // PseudoElement does not own the context, so it shouldn't delete it
        mScriptContext = 0;
      }
  };

  /**
   * Another type of container used to handle loops and
   */
  class ElementGroup : public PseudoElement {
    public:
      bool build();
    private:
      typedef std::unordered_map<ScriptState::FieldHash, Element*> ElementsMap;
      ElementsMap mElementsByKey;
  };

  /**
   * if/elseif/else chains
   */
  class ConditionChain : public PseudoElement {
    public:
      ConditionChain();
      void pushChild(Element* element, rapidxml::xml_node<>* node);
      bool build();
      void renderBody();
    private:
      Element* mEnabledElement;
      Element* mDefault;
      typedef ImVector<rapidxml::xml_attribute<>*> Attributes;
      Attributes mAttributes;
  };

  /**
   * Same as Vue.js <slot>
   */
  class Slot : public PseudoElement {
    public:
      void configure(rapidxml::xml_node<>* node, Context* ctx, ScriptState::Context* sctx = 0, Element* parent = 0);
  };

  /**
   * Default element for text node
   */
  class Text : public Element {

    public:

      Text() : text(0), mEstimatedSize(false) {
      }

      virtual ~Text()
      {
        if(text)
          ImGui::MemFree(text);
      }

      bool build()
      {
        mType = "span";
        if(!mBuilder) {
          mBuilder = mFactory->get(TEXT_NODE);
        }

        bool res = Element::build();
        // always display as inline block
        setDisplay(CSS_DISPLAY_INLINE);
        return res;
      }

      void renderBody()
      {
        if(!mParent || !text) {
          IMVUE_EXCEPTION(ElementError, "text node must always be attached to some parent");
          return;
        }

        if(!mEstimatedSize) {
          ImGui::TextUnformatted(text);
          mEstimatedSize = true;
        } else {
          bool popWrap = false;
          if(mParent->size.x > 0) {
            ImGui::PushTextWrapPos(mParent->pos.x + mParent->size.x);
            popWrap = true;
          }

          ImGui::TextWrapped("%s", text);

          if(popWrap)
            ImGui::PopTextWrapPos();
        }
      }

      void setText(char* value)
      {
        if(text) {
          ImGui::MemFree(text);
          text = 0;
        }

        if(!value) {
          return;
        }

        text = ImStrdup(value);
        ImGui::MemFree(value);
      }

      char* text;

    private:

      bool mEstimatedSize;
  };

  inline bool displayedInline(Element* element) {
    if(!element) {
      return false;
    }

    return element->style()->displayInline;
  }
}

#endif
