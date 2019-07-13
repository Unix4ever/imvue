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
#include <vector>
#include <map>
#include <iostream>
#include <cstring>
#include <sstream>
#include <unordered_map>

#define PARSE_NUMERIC_TYPE(type) \
  inline bool read(const char* value, type* dest) { \
    *dest = parse_integral_type<type>(value); \
    return true; \
  } \
  inline bool read(Object& obj, type* dest) { \
    *dest = obj.as<type>(); \
    return true; \
  } \
  inline bool read(Object& obj, type** dest) \
  { \
    if(*dest) { \
      ImGui::MemFree(*dest); \
      *dest = NULL; \
    } \
    \
    size_t index = 0; \
    ImVector<type> values; \
    for(Object::iterator iter = obj.begin(); iter != obj.end(); ++iter, ++index) { \
      values.push_back(iter.value.as<type>()); \
    } \
    \
    *dest = (type*)ImGui::MemAlloc(values.size()); \
    memcpy(*dest, values.Data, sizeof(type) * values.size()); \
    return true; \
  } \
  inline bool read(const char* value, type** dest) \
  { \
    if(*dest) { \
      ImGui::MemFree(*dest); \
      *dest = NULL; \
    } \
    ImVector<type> values; \
    parse_integral_type_array<type>(value, values); \
    if(values.size() == 0) { \
      return false; \
    } \
    *dest = (type*)ImGui::MemAlloc(values.size() * sizeof(type)); \
    memcpy(*dest, values.Data, sizeof(type) * values.size()); \
    return true; \
  }

namespace ImVue {

  /**
   * ID for element text pseudo-attribute name
   */
  static const char* TEXT_ID = "@text";

  // attribute converting utilities
  // used for parsing statically defined fields as ints/floats/arrays etc
  namespace detail {

    template<class C>
    C parse_integral_type(const char* value) {
      C res;
      std::stringstream ss;
      ss << value;
      ss >> res;
      return res;
    }

    template<class C>
    void parse_integral_type_array(const char* value, ImVector<C>& values) {
      char buffer[64];
      int index = 0;
      for(int i = 0; i < 1024; i++) {
        char c = value[i];

        if(c == ',' || c == '\0') {
          buffer[index] = '\0';
          values.push_back(parse_integral_type<C>(&buffer[0]));
          index = 0;
        } else {
          buffer[index++] = c;
        }

        if(c == '\0')
          break;
      }
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

    PARSE_NUMERIC_TYPE(int)
    PARSE_NUMERIC_TYPE(float)
    PARSE_NUMERIC_TYPE(double)
    PARSE_NUMERIC_TYPE(size_t)

    inline bool read(const char* value, ImGuiID* dest) {
      *dest = ImHashStr(value);
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

      *dest = parse_integral_type<bool>(value);
      return true;
    }

    inline bool read(const char* value, ImVec2* dest)
    {
      ImVector<float> values;
      values.reserve(2);
      parse_integral_type_array(value, values);
      dest->x = values[0];
      dest->y = values[1];
      return true;
    }

    inline bool read(const char* value, ImVec4* dest)
    {
      ImVector<float> values;
      values.reserve(4);
      parse_integral_type_array(value, values);
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
      parse_integral_type_array<C>(value, values);
      memcpy(*dest, values.Data, N * sizeof(C));
      return true;
    }

    inline bool read(Object object, Object* dest)
    {
      *dest = object;
      return true;
    }

    template<class C>
    bool read(const char* value, C dest)
    {
      IM_ASSERT("Detected unsupported field");
      (void)value;
      (void)dest;
      // default unsupported behaviour
      return false;
    }

    // Script state object conversion

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
      *dest = ImHashStr(obj.as<ImString>().get());
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
    bool read(Object& value, C dest)
    {
      IM_ASSERT("Detected unsupported field");
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

      enum InvalidationFlag {
        BUILD = 1 << 0
      };

      enum Flag {
        CONTAINER       = 1 << 0,
        PSEUDO_ELEMENT  = 1 << 1,
        COMPONENT       = 1 << 2
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
      inline const char* getType() const { return mNode->name(); }

      /**
       * Get element flags
       */
      inline unsigned int getFlags() const { return mFlags; }

      /**
       * Set parent
       */
      inline void setParent(Element* element) { mParent = element; }

      /**
       * Enable element
       */
      inline void enable() {
        if(!mConfigured)
          invalidateFlags(Element::BUILD);
        enabled = true;
      }

      /**
       * Get context
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
       * Check if Element is container
       */
      inline bool isContainer() const {
        return (mFlags & Element::CONTAINER) != 0;
      }

      /**
       * Get attribute interface by field id
       *
       * @param id field id
       */
      const Attribute* getAttribute(const char* id) const;

      /**
       * Exposed mainly for tests
       */
      inline ScriptState* getState() {
        return mScriptState;
      }

      inline rapidxml::xml_node<>* node() { return mNode; }

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

    protected:

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

      rapidxml::xml_node<>* mNode;

      typedef std::vector<Element*> Elements;
      typedef std::map<const char*, EventHandler*> Handlers;

      Handlers mHandlers;
      Elements mChildren;
      Element* mParent;
      ElementFactory* mFactory;
      ElementBuilder* mBuilder;
      TextureManager* mTextureManager;
      char* mClickHandler;
      ScriptState* mScriptState;
      ReactiveFields mReactiveFields;
      DirtyProperties mDirtyProperties;
      Context* mCtx;
      ScriptState::Context* mScriptContext;

      unsigned int mInvalidFlags;
      unsigned int mFlags;

      bool mConfigured;

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
          const char* attribute,
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
  };

  /**
   * MemPtr implementation of attribute reader
   */
  template<class C, class D>
  class AttributeMemPtr : public Attribute {
    public:
      AttributeMemPtr(D C::* memPtr, bool required = false)
        : mMemPtr(memPtr)
        , mRequired(required)
      {
      }

      virtual ~AttributeMemPtr() {}

      void read(const char* attribute, const char* str, Element* element, ScriptState* scriptState, int flags = 0, ScriptState::Fields* fields = 0) const
      {
        D* dest = &(static_cast<C*>(element)->*mMemPtr);
        bool success = false;
        if(scriptState) {
          if(flags & SCRIPT) {
            Object object = scriptState->getObject(str, fields, element->getContext());
            if(!object.valid()) {
              IMVUE_EXCEPTION(ScriptError, "failed to evaluate data %s", str);
              return;
            }
            success = detail::read(object, dest);
          } else if(flags & TEMPLATED_STRING) {
            std::string retval;
            std::stringstream result;
            std::stringstream ss;
            bool evaluation = false;

            for(int i = 0;;i++) {
              if(str[i] == '\0') {
                break;
              }

              if(evaluation && std::strncmp(&str[i], "}}", 2) == 0) {
                scriptState->eval(&ss.str()[0], &retval, fields, element->getContext());
                ss = std::stringstream();
                evaluation = false;
                result << retval;
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

            success = detail::read(&result.str()[0], dest);
          }
        }

        if(!success) {
          success = detail::read(str, dest);
        }

        if(!success && mRequired) {
          IMVUE_EXCEPTION(ElementError, "failed to read required attribute %s", attribute);
          return;
        }
      }

      bool copy(Element* element, Object& dest) const
      {
        D* value = &(static_cast<C*>(element)->*mMemPtr);
        return dest.set(value, typeID<D>::value());
      }
    private:
      D C::* mMemPtr;
      bool mRequired;
  };

  /**
   * Handler factory interface
   */
  class HandlerFactory : public ElementEntity {
    public:
      virtual ~HandlerFactory() {}
      virtual EventHandler* create(const char* handlerName, const char* script) const = 0;
  };

  /**
   * Simple class factory
   */
  template<class C>
  class HandlerFactoryImpl : public HandlerFactory {
    public:
      virtual ~HandlerFactoryImpl() {}
      EventHandler* create(const char* handlerName, const char* script) const {
        return new C(handlerName, script);
      }
  };

  /**
   * Abstract element builder, not to be used externally
   */
  class ElementBuilder {
    public:
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

      inline EventHandler* createHandler(const char* name, const char* fullName, const char* script) const {
        if(mHandlers.find(name) != mHandlers.end()) {
          return mHandlers.at(name)->create(fullName, script);
        }

        return nullptr;
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
      }

      int getLayer(ElementFactory* f);

      void readInheritance(ElementFactory* f);

      struct CmpAttributeName {
        bool operator()(const char* a, const char* b) const
        {
          return std::strcmp(const_cast<const char*>(a), const_cast<const char*>(b)) < 0;
        }
      };

      typedef std::map<const char*, Attribute*, CmpAttributeName> Attributes;
      Attributes mAttributes;

      typedef std::map<const char*, HandlerFactory*, CmpAttributeName> Handlers;
      Handlers mHandlers;

      typedef std::vector<std::string> Inheritance;
      Inheritance mInheritance;

      std::string mTag;
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
       * Register attribute reader
       *
       * @param name attribute name
       * @param memPtr destination to read into (&Element::variable)
       * @param required property is required
       */
      template<class D>
      ElementBuilderImpl<C>& attribute(const char* name, D C::* memPtr, bool required = false)
      {
        mAttributes[name] = new AttributeMemPtr<C, D>(memPtr, required);
        mAttributes[name]->owner = this;
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
        mAttributes[TEXT_ID] = new AttributeMemPtr<C, D>(memPtr, required);
        mAttributes[TEXT_ID]->owner = this;
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
        mInheritance.push_back(name);
        return *this;
      }
  };

  /**
   * Factory that keeps all element builders.
   * Can be extended by using element<class>(...) function.
   */
  class ElementFactory {

      struct CmpElementID {
        bool operator()(const char* a, const char* b) const
        {
          return std::strcmp(a, b) < 0;
        }
      };

      typedef std::map<const char*, ElementBuilder*, CmpElementID> ElementBuilders;

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
      void buildInheritance() {
        mLayers.clear();
        mDirty = false;

        for(ElementBuilders::iterator iter = mElementBuilders.begin(); iter != mElementBuilders.end(); ++iter) {
          int layer = iter->second->getLayer(this);
          if(mLayers.count(layer) == 0) {
            mLayers[layer] = ElementBuilders();
          }
          mLayers[layer][iter->first] = iter->second;
        }

        for(Layers::iterator iter = mLayers.begin(); iter != mLayers.end(); ++iter) {
          for(ElementBuilders::iterator it = iter->second.begin(); it != iter->second.end(); ++it) {
            it->second->readInheritance(this);
          }
        }
      }

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
      EventHandler(const char* handlerName, const char* script);
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

      InputHandler(const char* handlerName, const char* script);
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
        MouseDown,
        MouseUp,
        MouseOver,
        MouseOut
      };

      MouseEventHandler(const char* handlerName, const char* script);
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
      ChangeEventHandler(const char* handlerName, const char* script) : EventHandler(handlerName, script) {}
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

      KeyboardEventHandler(const char* handlerName, const char* script);

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
          }

          if(push) {
            result.push_back(static_cast<C*>(e));
          }
        }

        return result;
      }

    protected:
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

      typedef std::vector<Element*> Elements;

      Elements mChildren;
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
}

#endif
