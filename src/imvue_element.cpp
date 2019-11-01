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

#include "imvue_element.h"
#include "imvue.h"
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

namespace ImVue {

  EventHandler::EventHandler(const char* handlerName, const char* script)
    : mHandlerName(handlerName)
    , mScript(script)
  {
  }

  void EventHandler::parseProperties()
  {
    size_t offset = 0;
    size_t end = 0;

    while(true) {
      if(mHandlerName[end] == '.' || mHandlerName[end] == '\0') {
        ImU32 hash = ImHashStr(&mHandlerName[offset], end - offset);
        mProperties[hash] = true;
        offset = end + 1;
      }

      if(mHandlerName[end] == '\0') {
        break;
      }

      end++;
    }
  }

  InputHandler::InputHandler(const char* handlerName, const char* script)
    : EventHandler(handlerName, script)
    , mModifiers(0)
    , mExact(false)
  {
    parseProperties();
    if(checkProp("ctrl")) {
      mModifiers |= Modifiers::Ctrl;
    }

    if(checkProp("alt")) {
      mModifiers |= Modifiers::Alt;
    }

    if(checkProp("shift")) {
      mModifiers |= Modifiers::Shift;
    }

    if(checkProp("meta")) {
      mModifiers |= Modifiers::Super;
    }

    if(checkProp("exact")) {
      mExact = true;
    }
  }

  MouseEventHandler::MouseEventHandler(const char* handlerName, const char* script)
    : InputHandler(handlerName, script)
    , mType(Click)
    , mMouseButton(0)
    , mHovered(false)
  {
    if(checkProp("right")) {
      mMouseButton = 1;
    } else if(checkProp("middle")) {
      mMouseButton = 2;
    }

    if(checkProp("click")) {
      mType = Click;
    } else if(checkProp("mousedown")) {
      mType = MouseDown;
    } else if(checkProp("mouseup")) {
      mType = MouseUp;
    } else if(checkProp("mouseover")) {
      mType = MouseOver;
    } else if(checkProp("mouseout")) {
      mType = MouseOut;
    }
  }

  bool MouseEventHandler::check()
  {
    bool hovered = ImGui::IsItemHovered();
    bool changed = mHovered != hovered;
    mHovered = hovered;

    if(!mHovered && !changed) {
      return false;
    }

    bool trigger = false;
    switch(mType) {
      case Click:
        trigger = ImGui::IsItemClicked(mMouseButton);
        break;
      case MouseDown:
        trigger = ImGui::IsMouseDown(mMouseButton);
        break;
      case MouseUp:
        trigger = ImGui::IsMouseReleased(mMouseButton);
        break;
      case MouseOver:
        trigger = mHovered;
        break;
      case MouseOut:
        trigger = !mHovered;
        break;
    }

    return trigger && checkModifiers();
  }

  KeyboardEventHandler::KeyMap KeyboardEventHandler::init()
  {
    KeyMap m;
    m[ImHashStr("enter")] = ImGuiKey_Enter;
    m[ImHashStr("tab")] = ImGuiKey_Tab;
    m[ImHashStr("delete")] = ImGuiKey_Delete;
    m[ImHashStr("esc")] = ImGuiKey_Escape;
    m[ImHashStr("space")] = ImGuiKey_Space;
    m[ImHashStr("up")] = ImGuiKey_UpArrow;
    m[ImHashStr("down")] = ImGuiKey_DownArrow;
    m[ImHashStr("left")] = ImGuiKey_LeftArrow;
    m[ImHashStr("right")] = ImGuiKey_RightArrow;
    return m;
  }

  const KeyboardEventHandler::KeyMap KeyboardEventHandler::keyMap = KeyboardEventHandler::init();

  KeyboardEventHandler::KeyboardEventHandler(const char* handlerName, const char* script)
    : InputHandler(handlerName, script)
    , mKeyIndex(-1)
    , mType(Down)
  {
    parseProperties();
    if(checkProp("keyup")) {
      mType = Up;
    } else if(checkProp("keypress")) {
      mType = Press;
    }

    size_t end = strlen(handlerName);
    for(size_t i = end - 1; i > 0; --i) {
      if(handlerName[i] == '.') {
        ImU32 hash = ImHashStr(&handlerName[i + 1]);
        int keyCode = -1;
        if(keyMap.count(hash) != 0) {
          keyCode = keyMap.at(hash);
        } else {
          keyCode = atoi(&handlerName[i + 1]);
        }

        if(keyCode > 0 && keyCode < ImGuiKey_COUNT) {
          mKeyIndex = ImGui::GetKeyIndex(keyCode);
        }
        break;
      }
    }
  }

  bool KeyboardEventHandler::check()
  {
    if(mKeyIndex < 0 || (!ImGui::IsItemActive() && !ImGui::IsItemDeactivatedAfterEdit())) {
      return false;
    }

    bool trigger = false;
    switch(mType) {
      case Down:
        trigger = ImGui::IsKeyDown(mKeyIndex);
        break;
      case Up:
        trigger = ImGui::IsKeyReleased(mKeyIndex);
        break;
      case Press:
        trigger = ImGui::IsKeyPressed(mKeyIndex);
        break;
    }
    return trigger && checkModifiers();
  }

  Element::Element()
    : enabled(true)
    , id(NULL)
    , ref(NULL)
    , enabledAttr(NULL)
    , key(NULL)
    , mNode(0)
    , mParent(0)
    , mFactory(0)
    , mBuilder(0)
    , mTextureManager(0)
    , mClickHandler(NULL)
    , mScriptState(NULL)
    , mCtx(0)
    , mScriptContext(0)
    , mInvalidFlags(0)
    , mFlags(0)
    , mConfigured(false)
  {
  }

  Element::~Element()
  {
    if(mClickHandler)
      ImGui::MemFree(mClickHandler);

    if(mScriptState) {
      for(Element::ReactiveFields::const_iterator iter = mReactiveFields.begin(); iter != mReactiveFields.end(); ++iter) {
        IM_ASSERT(mScriptState->removeListener(iter->first, this) && "failed to remove listener");
      }
    }

    for(Handlers::iterator iter = mHandlers.begin(); iter != mHandlers.end(); ++iter) {
      delete iter->second;
    }
    mHandlers.clear();

    if(key) {
      ImGui::MemFree(key);
    }

    if(id) {
      ImGui::MemFree(id);
    }

    if(ref) {
      if(mScriptState) {
        mScriptState->removeReference(ref, this);
      }
      ImGui::MemFree(ref);
    }

    if(enabledAttr) {
      ImGui::MemFree(enabledAttr);
    }

    if(mScriptContext && mScriptContext->owner == this) {
      delete mScriptContext;
    }
  }

  void Element::configure(rapidxml::xml_node<>* node, Context* ctx, ScriptState::Context* sctx, Element* parent)
  {
    mCtx = ctx;
    if(!mScriptState) {
      mScriptState = ctx->script;
    }
    mParent = parent;
    mScriptContext = sctx ? sctx : (parent ? parent->getContext(true) : NULL);
    mNode = node;
    mFactory = ctx->factory;
    mTextureManager = ctx->texture;
    mConfigured = build();
  }

  void Element::invalidate(const char* attribute) {
    mDirtyProperties[attribute] = true;
  }

  const Attribute* Element::getAttribute(const char* id) const {
    return mBuilder->get(id);
  }

  void Element::invalidateFlags(unsigned int flags) {
    mInvalidFlags |= flags;
  }

  void Element::bindListeners(ScriptState::Fields& fields, const char* attribute, unsigned int flags)
  {
    for(int i = 0; i < fields.size(); ++i) {
      bindListener(fields[i], attribute, flags);
    }
  }

  void Element::bindListener(ScriptState::FieldHash field, const char* attribute, unsigned int flags)
  {
    mScriptState->addListener(field, this, attribute, flags);
    mReactiveFields[field] = true;
  }

  void Element::readProperty(const char* name, const char* value, int flags)
  {
    std::string id = name;
    if(ImStricmp(name, TEXT_ID) == 0) {
      flags |= Attribute::TEMPLATED_STRING;
    } else if(name[0] == ':') {
      flags |= Attribute::SCRIPT;
      id = id.substr(1, id.size());
    }

    // process v-if v-else v-else-if v-for
    if(ImStrnicmp(name, "v-else", 6) == 0 || id == "v-if") {
      if(enabledAttr) {
        ImGui::MemFree(enabledAttr);
      }
      enabled = (mScriptState && ImStricmp(name, "v-else") != 0) ? mScriptState->getObject(value).as<bool>() : enabledAttr != NULL;
      enabledAttr = ImStrdup(name);
    }

    ScriptState::Fields fields;
    bool initialized = initAttribute(&id[0], value, flags, &fields);

    if(flags & Attribute::BIND_LISTENERS && fields.size() > 0) {
      bindListeners(fields, name);
    }

    if(initialized) {
      return;
    }

    // check event listeners
    if(std::strncmp(&id[0], "v-on", 4) == 0) {
      addHandler(name, value, mBuilder);
    }
  }

  bool Element::initAttribute(const char* id, const char* value, int flags, ScriptState::Fields* fields)
  {
    Attribute* reader = mBuilder->get(id);
    if(reader) {
      reader->read(id, value, this, mScriptState, flags, fields);
      return true;
    }

    return false;
  }

  bool Element::build()
  {
    if(!mBuilder) {
      mBuilder = mFactory->get(getType());
    }

    if(!mBuilder) {
      return false;
    }

    int flags = mConfigured ? 0 : Attribute::BIND_LISTENERS;

    for(const rapidxml::xml_attribute<>* a = mNode->first_attribute(); a; a = a->next_attribute()) {
      readProperty(a->name(), a->value(), flags);
    }

    // reading text
    readProperty(TEXT_ID, mNode->value(), flags);
    if(ref && mScriptState) {
      mScriptState->addReference(ref, this);
    }
    return enabled;
  }

  void Element::render()
  {
    if(mInvalidFlags & Element::BUILD) {
      mConfigured = build();
      mInvalidFlags = 0;
    }

    computeProperties();
    if(!enabled) {
      return;
    }
    if(key) {
      ImGui::PushID(ImHashStr(key));
    }

    renderBody();

    if(key) {
      ImGui::PopID();
    }

    if(mScriptState) {
      for(Handlers::iterator iter = mHandlers.begin(); iter != mHandlers.end(); ++iter) {
        if(iter->second->check()) {
          mScriptState->eval(iter->second->getScript(), 0, 0, mScriptContext);
        }
      }
    }
  }

  void Element::computeProperties()
  {
    if(mDirtyProperties.size() == 0) {
      return;
    }

    bool wasEnabled = enabled;

    for(DirtyProperties::iterator iter = mDirtyProperties.begin(); iter != mDirtyProperties.end(); ++iter) {
      char* value = NULL;
      if(iter->first == TEXT_ID) {
        value = mNode->value();
      } else {
        rapidxml::xml_attribute<>* attr = mNode->first_attribute(&iter->first[0]);
        if(attr) {
          value = attr->value();
        }
      }
      if(value) {
        readProperty(&iter->first[0], value);
      }
    }
    mDirtyProperties.clear();

    if(wasEnabled != enabled && mParent) {
      mParent->invalidateFlags(Element::BUILD);
    }

    fireCallback(ScriptState::UPDATED);
  }

  void Element::addHandler(const char* name, const char* value, const ElementBuilder* builder)
  {
    if(!mScriptState) {
      return;
    }

    if(mHandlers.find(name) != mHandlers.end()) {
      delete mHandlers[name];
    }

    // max handler id length is 256 symbols
    const size_t bufferSize = 256;
    char handlerName[bufferSize] = {0};
    char fullName[bufferSize] = {0};
    int state = 0;
    size_t index = 0;

    for(size_t i = 0; i < bufferSize && name[i] != '\0'; i++) {
      switch(state) {
        case 0:
          if(name[i] == ':') {
            state++;
          }
          break;
        case 1:
          if(name[i] == '.') {
            state++;
          } else {
            handlerName[index] = name[i];
          }
        default:
          fullName[index] = name[i];
          index++;
      }
    }

    if(handlerName[0] == 0) {
      IMVUE_EXCEPTION(ElementError, "malformed handler name %s", name);
      return;
    }

    EventHandler* handler = builder->createHandler(handlerName, fullName, value);
    if(!handler) {
      IMVUE_EXCEPTION(ElementError, "failed create handler of type %s", handlerName);
      return;
    }
    mHandlers[name] = handler;
  }

  void Element::fireCallback(ScriptState::LifecycleCallbackType cb, bool schedule)
  {
    if(!mCtx->script) {
      return;
    }

    if(schedule || mCtx->root != this)
      mCtx->script->scheduleCallback(cb);
    else
      mCtx->script->lifecycleCallback(cb);
  }

  Element* Element::createElement(rapidxml::xml_node<>* node, ScriptState::Context* sctx, Element* parent) {
    return mCtx->root->createElement(node, sctx, parent);
  }

  ContainerElement::ContainerElement()
  {
    mFlags |= Element::CONTAINER;
  }

  ContainerElement::~ContainerElement()
  {
    removeChildren();
  }

  void ContainerElement::removeChildren()
  {
    while(mChildren.size() > 0) {
      delete mChildren[mChildren.size() - 1];
      mChildren.pop_back();
    }
  }

  void ContainerElement::renderChildren() {
    for(Elements::iterator el = mChildren.begin(); el != mChildren.end(); el++) {
      (*el)->render();
    }
  }

  void cleanupValues(ImVector<char*>& values)
  {
    for(int i = 0; i < values.size(); ++i) {
      if(values[i]) {
        ImGui::MemFree(values[i]);
      }
    }
    values.clear();
  }

  void ContainerElement::createChildren(rapidxml::xml_node<>* doc) {
    ScriptState::Fields fields;
    ConditionChain* chain = NULL;
    rapidxml::xml_node<>* root = doc ? doc : mNode;

    for (rapidxml::xml_node<>* node  = root->first_node(); node; node = node->next_sibling()) {

      Element* e = NULL;
      // v-for case is special: we don't need to create a single element for it
      const rapidxml::xml_attribute<>* vfor = node->first_attribute("v-for");
      if(vfor) {
        if(!mScriptState) {
          continue;
        }
        e = new ElementGroup();
        try {
          e->configure(node, mCtx, mScriptContext, this);
        } catch(...) {
          delete e;
          throw;
        }
      } else {
        e = createElement(node);
        if(!e) {
          IMVUE_EXCEPTION(ElementError, "failed to create element %s", node->name());
          continue;
        }

        try {
          if(e->enabledAttr != NULL) {
            if(ImStricmp(e->enabledAttr, "v-if") == 0) {
              chain = new ConditionChain();
              chain->configure(node, mCtx, mScriptContext, this);
              mChildren.push_back(chain);
            }

            if(!chain) {
              IMVUE_EXCEPTION(ElementError, "using v-else without v-if is not allowed");
              return;
            }

            chain->pushChild(e, node);
            continue;
          } else if(chain) {
            chain = NULL;
          }
        } catch(...) {
          delete e;
          throw;
        }
      }

      if(e)
        mChildren.push_back(e);
    }
  }

  void ContainerElement::renderBody()
  {
    renderChildren();
  }

  bool ContainerElement::build()
  {
    if(!Element::build()) {
      return false;
    }

    removeChildren();
    createChildren();
    return true;
  }

  bool ElementGroup::build() {
    const rapidxml::xml_attribute<>* vfor = mNode->first_attribute("v-for");
    ImVector<char*> values;
    if(!mScriptState->parseIterator(vfor->value(), values)) {
      IMVUE_EXCEPTION(ScriptError, "failed to parse vfor %s", vfor->value());
      return false;
    }

    char* list = values[0];
    char* valueVar = values[1];
    char* keyVar = values[2];

    if(!list || !valueVar) {
      cleanupValues(values);
      IMVUE_EXCEPTION(ScriptError, "malformed vfor definition %s", vfor->value());
      return false;
    }

    ScriptState::Fields fields;
    Object object = mScriptState->getObject(list, &fields, mScriptContext);
    if(!object) {
      cleanupValues(values);
      IMVUE_EXCEPTION(ScriptError, "object evaluation failed %s", list);
      return false;
    }

    std::map<ScriptState::FieldHash, bool> visited;

    ScriptState::FieldHash listHash = mScriptState->hash(list);

    size_t index = 0;

    for(Object::iterator iter = object.begin(); iter != object.end(); ++iter, ++index) {

      ScriptState::FieldHash hash = mScriptState->hash(iter.key.as<ImString>().get());

      if(mElementsByKey.count(hash) != 0) {
        Element* e = mElementsByKey[hash];
        mChildren[index] = e;
        mElementsByKey[hash] = e;
        visited[hash] = true;
        // update element
        ScriptState::Context* c = e->getContext();
        IM_ASSERT(c != NULL && "null context detected in the element");
        if(c->vars.size() > 0 && c->vars[0].type == ScriptState::Variable::VALUE && c->vars[0].value.type() != ObjectType::NIL) {
          c->vars[0].value = iter.value;
          mScriptState->pushChange(c->hash);
        }
      } else {
        ScriptState::FieldHash ctxHash = mScriptContext ? (mScriptContext->hash ^ listHash) : listHash;

        ScriptState::Context* c = new ScriptState::Context(ctxHash, mScriptContext);
        if(ImStricmp(valueVar, "_") != 0) {
          c->add(valueVar, iter.value, ScriptState::Variable::VALUE);
        }

        if(keyVar && ImStricmp(keyVar, "_") != 0) {
          c->add(keyVar, iter.key, ScriptState::Variable::KEY);
        }

        Element* e = createElement(mNode, c);
        if(!e) {
          IMVUE_EXCEPTION(ElementError, "failed to create element %s", mNode->name());
          return false;
        }

        if(index == mChildren.size()) {
          mChildren.push_back(e);
        } else {
          mChildren[index] = e;
        }

        mElementsByKey[hash] = e;
        visited[hash] = true;
      }
    }

    for(size_t i = mChildren.size(); i > index; --i) {
      mChildren.pop_back();
    }

    for(ElementsMap::iterator iter = mElementsByKey.begin(); iter != mElementsByKey.end();) {
      if(!visited[iter->first]) {
        delete iter->second;
        mElementsByKey.erase(iter++);
      } else {
        ++iter;
      }
    }

    cleanupValues(values);
    bindListeners(fields, NULL, Element::BUILD);
    return true;
  }

  ConditionChain::ConditionChain()
    : mEnabledElement(NULL)
    , mDefault(NULL)
  {
  }

  void ConditionChain::pushChild(Element* element, rapidxml::xml_node<>* node)
  {
    IM_ASSERT(element->enabledAttr != NULL && "element does not have enabledAttr defined, why is it in the ConditionChain?");
    if(mDefault) {
      IMVUE_EXCEPTION(ElementError, "malformed condition chain: v-else/v-else-if/v-if after the last v-else");
    }

    if(ImStricmp(element->enabledAttr, "v-else") == 0) {
      mDefault = element;
    } else {
      rapidxml::xml_attribute<>* a = node->first_attribute(element->enabledAttr);
      IM_ASSERT(a != NULL && "failed to find enabledAttr in the node");
      mAttributes.push_back(a);
      ScriptState::Fields fields;
      mScriptState->getObject(a->value(), &fields, mScriptContext);
      bindListeners(fields, NULL, Element::BUILD);
    }

    mChildren.push_back(element);
    element->setParent(this);
    invalidateFlags(Element::BUILD);
  }

  bool ConditionChain::build()
  {
    mEnabledElement = NULL;

    for(size_t i = 0; i < mChildren.size(); ++i) {
      Element* e = mChildren[i];
      e->enabled = false;
      if(e == mDefault) {
        continue;
      }
      rapidxml::xml_attribute<>* attr = mAttributes[i];
      Object object = mScriptState->getObject(attr->value(), NULL, mScriptContext);
      detail::read(object, &e->enabled);

      if(e->enabled) {
        mEnabledElement = e;
      }
    }

    if(!mEnabledElement && mDefault) {
      mEnabledElement = mDefault;
    }

    if(mEnabledElement) {
      mEnabledElement->enable();
    }
    return true;
  }

  void ConditionChain::renderBody()
  {
    if(mEnabledElement) {
      mEnabledElement->render();
    }
  }

  int ElementBuilder::getLayer(ElementFactory* f) {
    int layer = 0;
    if(mInheritance.size() != 0) {
      layer = 1;
    }

    int inheritedLayer = 0;
    for(Inheritance::iterator iter = mInheritance.begin(); iter != mInheritance.end(); ++iter) {
      ElementBuilder* b = f->get(iter->c_str());
      inheritedLayer = std::max(b->getLayer(f), inheritedLayer);
    }

    return layer + inheritedLayer;
  }

  void ElementBuilder::readInheritance(ElementFactory* f) {
    for(Inheritance::iterator iter = mInheritance.begin(); iter != mInheritance.end(); ++iter) {
      merge(*f->get(iter->c_str()));
    }
  }

  void ElementFactory::buildInheritance()
  {
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

  void Slot::configure(rapidxml::xml_node<>* node, Context* ctx, ScriptState::Context* sctx, Element* parent)
  {
    (void)parent;
    if(!ctx->root || (ctx->root->getFlags() & Element::COMPONENT) == 0) {
      IMVUE_EXCEPTION(ElementError, "slot must be a child of a component");
      return;
    }

    rapidxml::xml_node<>* n = ctx->root->node();
    if(!n->first_node() && !n->value()) {
      return;
    }

    mBuilder = ctx->factory->get("slot");
    Element::configure(node, ctx->parent ? ctx->parent : ctx, sctx, ctx->root);
  }

}
