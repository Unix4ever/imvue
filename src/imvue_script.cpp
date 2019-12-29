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


#include <algorithm>

#include "imvue_script.h"
#include "imvue_element.h"

namespace ImVue {

  Object::Object()
    : mObject(nullptr)
  {
  }

  Object::Object(ObjectImpl* oi)
    : mObject(oi)
  {
  }

  Object::iterator Object::begin()
  {
    return Object::iterator(*this);
  }

  Object::iterator Object::end()
  {
    return Object::iterator(-2);
  }

  Object Object::operator[](const Object& key)
  {
    return mObject->get(key);
  }

  Object Object::operator[](const char* key)
  {
    return mObject->get(key);
  }

  Object Object::operator[](int index)
  {
    return mObject->get(index);
  }

  void Object::erase(const Object& key)
  {
    mObject->erase(key);
  }

  bool Object::valid() const {
    return mObject != 0;
  }

  Object::operator bool() const
  {
    return type() != ObjectType::NIL;
  }

  bool Object::operator()(Object* args, Object* rets, int nargs, int nrets)
  {
    if(type() != ObjectType::FUNCTION) {
      IMVUE_EXCEPTION(ScriptError, "object type %d is not callable", type());
      return Object();
    }

    return mObject->call(args, rets, nargs, nrets);
  }

  ObjectType Object::type() const {
    return !mObject ? ObjectType::NIL : mObject->type();
  }

  void Object::keys(ObjectKeys& dest) {
    return mObject->keys(dest);
  }

  bool Object::setValue(void* value, ObjectType type)
  {
    switch(type) {
      case ObjectType::OBJECT:
        if(mObject) {
          mObject->setObject(*reinterpret_cast<Object*>(value));
        } else {
          mObject = reinterpret_cast<Object*>(value)->mObject;
        }
        break;
      case ObjectType::INTEGER:
        mObject->setInteger(*reinterpret_cast<long*>(value));
        break;
      case ObjectType::NUMBER:
        mObject->setNumber(*reinterpret_cast<double*>(value));
        break;
      case ObjectType::STRING:
        mObject->setString(*reinterpret_cast<const char**>(value));
        break;
      case ObjectType::BOOLEAN:
        mObject->setBool(*reinterpret_cast<bool*>(value));
        break;
      case ObjectType::VEC2:
        mObject->initObject();
        (*this)["x"] = (*reinterpret_cast<ImVec2*>(value)).x;
        (*this)["y"] = (*reinterpret_cast<ImVec2*>(value)).y;
        break;
      default:
        return false;
    }
    return true;
  }

  template<>
  double Object::as<double>() const
  {
    if(!mObject) {
      return 0.0;
    }

    return mObject->readDouble();
  }

  template<>
  ImString Object::as<ImString>() const
  {
    ImString str;
    if(!mObject) {
      return str;
    }

    str = mObject->readString();
    return str;
  }

  template<>
  bool Object::as<bool>() const
  {
    if(!mObject) {
      return false;
    }

    return mObject->readBool();
  }

  template<>
  float Object::as<float>() const
  {
    return (float)as<double>();
  }

  void ScriptState::ReactListener::trigger()
  {
    if(propertyID.get()) {
      element->invalidate(propertyID.get());
    }

    if(flags) {
      element->invalidateFlags(flags);
    }
  }

  bool ScriptState::parseIterator(const char* str, ImVector<char*>& vars)
  {
    bool readingVars = true;
    bool stopReadingVars = false;
    int openBrackets = 0;
    int closedBrackets = 0;
    size_t start = 0;
    size_t end = 0;
    size_t len = 0;

    bool stop = false;

    vars.resize(4);
    memset(vars.Data, 0, sizeof(char*) * vars.size());

    int index = 1;

    // parse iterator
    for(size_t i = 0; !stop; ++i) {
      switch(str[i]) {
        case '(':
          start++;
          openBrackets++;
          break;
        case '\0':
          stop = true;
        case ')':
          if(!stop)
            closedBrackets++;
        case ' ':
          if(ImStrnicmp(&str[i], " in ", 4) == 0) {
            i += 3;
            stopReadingVars = true;
          }
        case ',':
          len = end - start;
          if(len > 0) {
            char* var = (char*)ImGui::MemAlloc(len + 1);
            memset(var, 0, len);
            var[len] = '\0';
            if(readingVars) {
              memcpy(var, &str[start], len);
              if(index >= vars.size()) {
                IMVUE_EXCEPTION(ElementError, "failed to parse v-for '%s': failed to parse k,v,index", str);
                return false;
              }
              vars[index++] = var;
            } else {
              vars[0] = var;
              memcpy(vars[0], &str[start], len);
            }
          }
          start = i + 1;
          end = start;
          if(stopReadingVars) {
            readingVars = false;
          }
          break;
        default:
          end = i + 1;
          break;
      }
    }

    if(vars[0] == NULL) {
      IMVUE_EXCEPTION(ElementError, "failed to parse v-for: failed to read iteration target");
      return false;
    }

    if(closedBrackets != openBrackets) {
      IMVUE_EXCEPTION(ElementError, "failed to parse v-for: unbalansed brackets");
      return false;
    }

    return true;
  }

  void ScriptState::addListener(ScriptState::FieldHash id, Element* element, const char* attribute, unsigned int flags)
  {
    if(mListeners.count(id) == 0) {
      mListeners[id] = ListenerList();
    }

    ReactListener listener{
      element,
      ImString(attribute),
      flags
    };
    mListeners[id].push_back(listener);
  }

  bool ScriptState::removeListener(ScriptState::FieldHash field, Element* element)
  {
    if(mListeners.count(field) == 0) {
      return false;
    }

    ListenerList& listeners = mListeners[field];
    if(listeners.size() == 1) {
      mListeners.erase(field);
      return true;
    }

    listeners.erase(std::remove(listeners.begin(), listeners.end(), element), listeners.end());
    return true;
  }

  void ScriptState::pushChange(const char* field) {
    pushChange(hash(field));
  }

  void ScriptState::pushChange(ScriptState::FieldHash h) {
    mChangedStack.push_back(h);
  }

  void ScriptState::changed(ScriptState::FieldHash id)
  {
    if(mListeners.count(id) > 0) {
      lifecycleCallback(LifecycleCallbackType::BEFORE_UPDATE);
      ListenerList listeners = mListeners[id];
      for(ListenerList::iterator iter = listeners.begin(); iter != listeners.end(); ++iter) {
        iter->trigger();
      }
    }
  }

} // namespace ImVue
