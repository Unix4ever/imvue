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

#ifndef __IMVUE_SCRIPT_H__
#define __IMVUE_SCRIPT_H__

#include <string>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <memory>

#include "imgui.h"
#include "imstring.h"
#include "imgui_internal.h"
#include "imvue_errors.h"

namespace ImVue {
  class Element;
  class Document;

  class ObjectIterator;
  class Object;

  enum ObjectType {
    UNKNOWN  = -1,

    NIL      = 0,
    INTEGER  = 1,
    NUMBER   = 2,
    BOOLEAN  = 3,
    STRING   = 4,
    OBJECT   = 5,
    USERDATA = 6,
    FUNCTION = 7,
    ARRAY    = 8,

    // special cases
    VEC2     = 9
  };

#define DEFINE_TYPE_ID(cls, id) \
  template<> \
  struct typeID<cls, false> { \
    static ObjectType value() { return id; } \
  }

  template<class C, bool = std::is_integral<C>::value && !std::is_same<C, bool>::value>
  struct typeID {};

  DEFINE_TYPE_ID(bool, ObjectType::BOOLEAN);
  DEFINE_TYPE_ID(const char*, ObjectType::STRING);
  DEFINE_TYPE_ID(char*, ObjectType::STRING);
  DEFINE_TYPE_ID(Object, ObjectType::OBJECT);
  DEFINE_TYPE_ID(double, ObjectType::NUMBER);
  DEFINE_TYPE_ID(float, ObjectType::NUMBER);
  DEFINE_TYPE_ID(ImVec2, ObjectType::VEC2);

  template<class C>
  struct typeID<C, true> {
    static ObjectType value() { return ObjectType::INTEGER; }
  };

  template<class C>
  struct typeID<C, false> {
    static ObjectType value() { return ObjectType::UNKNOWN; }
  };

  typedef std::vector<Object> ObjectKeys;

  /**
   * Should be overridden by script system
   */
  class ObjectImpl {

    public:

      virtual ~ObjectImpl() {}

      /**
       * Get object type
       */
      virtual ObjectType type() const = 0;

      /**
       * Get object by key
       */
      virtual Object get(const Object& key) = 0;

      /**
       * Get object by key
       */
      virtual Object get(const char* key) = 0;

      /**
       * Get object by index
       */
      virtual Object get(int index) = 0;

      /**
       * Delete object key
       */
      virtual void erase(const Object& key) = 0;

      /**
       * Call function
       */
      virtual bool call(Object* args, Object* rets, int nargs, int nrets) = 0;

      /**
       * Get keys
       */
      virtual void keys(ObjectKeys& dest) = 0;

      /**
       * Read value as integer
       */
      virtual long readInt() = 0;

      /**
       * Read value as string
       */
      virtual ImString readString() = 0;

      /**
       * Read value as boolean
       */
      virtual bool readBool() = 0;

      /**
       * Read value as double
       */
      virtual double readDouble() = 0;

      // mandatory setters
      virtual void setObject(Object& value) = 0;
      virtual void setInteger(long value) = 0;
      virtual void setNumber(double value) = 0;
      virtual void setString(const char* value) = 0;
      virtual void setBool(bool value) = 0;

      virtual void initObject() = 0;

  };
  /**
   * Abstract object representation
   */
  class Object {

    public:

      typedef ObjectIterator iterator;

      Object();

      Object(ObjectImpl* oi);

      iterator begin();

      iterator end();

      Object operator[](const Object& key);

      Object operator[](const char* key);

      Object operator[](int index);

      void erase(const Object& key);

      bool valid() const;

      operator bool() const;

      bool operator()(Object* args = 0, Object* rets = 0, int nargs = 0, int nrets = 0);

      ObjectType type() const;

      void keys(ObjectKeys& dest);

      inline ObjectImpl* getImpl() { return mObject.get(); }

      inline const ObjectImpl* getImpl() const { return mObject.get(); }

      template<class C>
      typename std::enable_if<std::is_integral<C>::value, bool>::type set(C* value, ObjectType type)
      {
        long val = (long)(*value);
        return setValue(&val, type);
      }

      template<class C>
      typename std::enable_if<!std::is_integral<C>::value && !std::is_floating_point<C>::value, bool>::type set(C* value, ObjectType type)
      {
        return setValue(value, type);
      }

      template<class C>
      typename std::enable_if<std::is_floating_point<C>::value, bool>::type set(C* value, ObjectType type)
      {
        double val = (double)(*value);
        return setValue(&val, type);
      }

      template<typename C>
      C as() const {
        return as<C>(std::is_integral<C>());
      }

      template<typename C>
      C as(std::true_type) const
      {
        if(!mObject) {
          return 0;
        }

        return (C)mObject->readInt();
      }

      template<typename C>
      C as(std::false_type) const
      {
        IMVUE_EXCEPTION(ScriptError, "unsupported cast");
        return C{};
      }

      template<typename C>
      Object& operator=(C value) {
        set(&value, typeID<C>::value());
        return *this;
      }
    private:
      bool setValue(void* value, ObjectType type);

      std::shared_ptr<ObjectImpl> mObject;
  };

  template<>
  double Object::as<double>() const;

  template<>
  ImString Object::as<ImString>() const;

  template<>
  bool Object::as<bool>() const;

  template<>
  float Object::as<float>() const;

  class ObjectIterator {
    public:
      ObjectIterator(int index = -1)
        : mIndex(index)
      {
      }

      ObjectIterator(Object src)
        : mIndex(-1)
        , mSrc(src)
      {
        src.keys(mKeys);
        advance();
      }

      ~ObjectIterator()
      {
        mKeys.clear();
      }

      bool operator!() {
        return mIndex < 0 || mIndex > (int)mKeys.size();
      }

      bool operator==(const ObjectIterator& iter) const {
        return iter.mIndex == mIndex;
      }

      bool operator!=(const ObjectIterator& iter) const {
        return iter.mIndex != mIndex;
      }

      ObjectIterator& operator++() {
        advance();
        return *this;
      }

      Object key;
      Object value;

    private:

      void advance() {
        if(mIndex == -2) {
          return;
        }

        mIndex++;
        if(mIndex >= (int)mKeys.size()) {
          mIndex = -2;
          return;
        }
        key = mKeys[mIndex];
        value = mSrc[key];
      }

      int mIndex;
      ObjectKeys mKeys;
      Object mSrc;
  };

  /**
   * Object that can be overwritten
   */
  class MutableObject : public Object {
    public:
      MutableObject(ObjectImpl* impl) : Object(impl) {}
      MutableObject& operator=(Object& object) {
        set(&object, ObjectType::OBJECT);
        return *this;
      }
      template<typename C>
      Object& operator=(C value) {
        set(&value, typeID<C>::value());
        return *this;
      }
  };

  class ScriptState {

    public:

      typedef ImU32 RefHash;

      typedef ImU32 FieldHash;

      typedef ImVector<FieldHash> Fields;

      typedef std::unordered_map<RefHash, Element*> RefMap;

      /**
       * Single variable to be defined for the context
       */
      struct Variable {
        enum Type {
          VALUE,
          KEY
        };

        char* key;
        Object value;
        Type type;
      };

      /**
       * Context is evaluation environment
       */
      struct Context {
        Context(ScriptState::FieldHash h, Context* parent = 0)
          : hash(h)
          , owner(NULL)
          , mOffset(0)
        {
          if(parent) {
            mOffset = parent->vars.size();
            vars = parent->vars;
          }
        }

        ~Context()
        {
          while(vars.size() > mOffset)
          {
            ImGui::MemFree(vars[vars.size() - 1].key);
            vars.pop_back();
          }
        }

        void add(char* key, Object value, Variable::Type type)
        {
          vars.push_back(Variable{ImStrdup(key), value, type});
        }

        std::vector<Variable> vars;
        ScriptState::FieldHash hash;
        Element* owner;

        private:
          Context(Context& other) {
            (void)other;
            // disable copy constructor
          }

          size_t mOffset;
      };

      struct ReactListener {
        Element* element;
        ImString propertyID;
        unsigned int flags;

        bool operator==(const Element* e) {
          return element == e;
        }

        void trigger();
      };

      enum LifecycleCallbackType {
        BEFORE_CREATE = 1 << 0,
        CREATED = 1 << 1,
        BEFORE_MOUNT = 1 << 2,
        MOUNTED = 1 << 3,
        BEFORE_UPDATE = 1 << 4,
        UPDATED = 1 << 5,
        BEFORE_DESTROY = 1 << 6,
        DESTROYED = 1 << 7
      };

      ScriptState() : mLifecycleDirty(0) {}

      virtual ~ScriptState() {}

      /**
       * Get or create object from state object
       */
      virtual MutableObject operator[](const char* key) = 0;

      /**
       * Initialize script state using script string
       *
       * @param scriptData script to eval
       */
      virtual void initialize(const char* scriptData) = 0;

      /**
       * Initialize script state using object instance
       *
       * @param state script entity reference of type ObjectType::OBJECT
       */
      virtual void initialize(Object state) = 0;

      /**
       * Evaluate expression
       *
       * @param str script data
       * @param retval optionally read any return data
       * @param ctx script evaluation context
       */
      virtual void eval(const char* str, std::string* retval = 0, Fields* fields = 0, ScriptState::Context* ctx = 0) = 0;

      /**
       * Evaluate expression and get an object
       */
      virtual Object getObject(const char* str, Fields* fields = 0, ScriptState::Context* ctx = 0) = 0;

      /**
       * Parses iterator definition
       */
      virtual bool parseIterator(const char* str, ImVector<char*>& vars);

      void pushChange(const char* field);

      void pushChange(ScriptState::FieldHash h);

      bool removeListener(ScriptState::FieldHash id, Element* element);

      /**
       * Register element by reference
       * <element ref="id"/>
       */
      inline void addReference(const char* ref, Element* element) {
        RefHash hash = ImHashStr(ref);
        IM_ASSERT(mRefMap.count(hash) == 0);
        mRefMap[hash] = element;
      }

      /**
       * Remove element reference
       */
      inline void removeReference(const char* ref, Element* element) {
        RefHash hash = ImHashStr(ref);
        (void)element;
        IM_ASSERT(mRefMap[hash] == element);
        mRefMap.erase(hash);
      }

      /**
       * Lifecycle callbacks
       */
      inline void scheduleCallback(LifecycleCallbackType cb) { mLifecycleDirty |= cb; }

      inline void update() {
        if(mLifecycleDirty != 0) {
          for(int cb = LifecycleCallbackType::BEFORE_CREATE; cb != (LifecycleCallbackType::DESTROYED << 1); cb = cb << 1) {
            if(mLifecycleDirty & cb) {
              lifecycleCallback((LifecycleCallbackType)cb);
            }
          }
          mLifecycleDirty = 0;
        }

        while(mChangedStack.size() > 0) {
          changed(mChangedStack[mChangedStack.size() - 1]);
          mChangedStack.pop_back();
        }
      }

      /**
       * Clear changed stack without applying updates
       */
      inline void clearChanges()
      {
        mChangedStack.clear();
      }

      virtual void lifecycleCallback(LifecycleCallbackType cb) { (void)cb; }

      /**
       * Bind listener for property and field hash
       *
       * @param id property hash. Script state id
       * @param element element to trigger invalidation when change comes
       * @param attribute element attribute to invalidate
       * @param flags state invalidation flags
       */
      void addListener(ScriptState::FieldHash id, Element* element, const char* attribute, unsigned int flags = 0);

      /**
       * Create script state clone
       *
       * Implementation specific
       */
      virtual ScriptState* clone() const = 0;

      /**
       * Convert const char* to hash
       */
      inline FieldHash hash(const char* str) const {
        return ImHashStr(str);
      }

    protected:

      void changed(ScriptState::FieldHash hash);

      typedef std::vector<ReactListener> ListenerList;

      typedef std::unordered_map<ScriptState::FieldHash, ListenerList> Listeners;

      void handleError(const std::string& error) {
        IMVUE_LOG_ERROR(std::string("Script Error: ") + error, __FILE__, __LINE__);
      }

      Listeners mListeners;
      ImVector<ScriptState::FieldHash> mChangedStack;

      unsigned int mLifecycleDirty;
      RefMap mRefMap;

  };
} // namespace ImVue

#endif
