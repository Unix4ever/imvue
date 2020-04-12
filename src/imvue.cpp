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
#include "imvue_generated.h"
#include "imgui.h"
#include "imvue.h"
#include "imvue_style.h"
#include "imgui_internal.h"
#include "rapidxml.hpp"
#include <iostream>

namespace ImVue {

  ComponentFactory::ComponentFactory()
    : mValid(false)
  {
  }

  ComponentFactory::ComponentFactory(Object definition)
    : mValid(false)
  {
    Object tmpl = definition["template"];
    if(!tmpl) {
      IMVUE_EXCEPTION(ElementError, "invalid component: template is required");
      return;
    }
    mTemplate = tmpl.as<ImString>();

    Object props = definition["props"];
    if(props) {
      for(Object::iterator iter = props.begin(); iter != props.end(); ++iter) {
        defineProperty(iter.key, iter.value);
      }
    }

    mData = definition;
    mValid = true;
  }

  void ComponentFactory::defineProperty(Object& key, Object& value)
  {
    ComponentProperty prop;
    if(key.type() == ObjectType::NUMBER && value.type() == ObjectType::STRING) {
      prop.setAttr(value.as<ImString>());
    } else {
      prop.setAttr(key.as<ImString>());
      if(value.type() == ObjectType::ARRAY) {
        readPropertyTypes(prop, value);
      } else if(value.type() == ObjectType::OBJECT) {
        Object type = value["type"];
        if(type) {
          readPropertyTypes(prop, type);
        }
        prop.validator = value["validator"];
        prop.def = value["default"];

        Object required = value["required"];
        if(required.type() != ObjectType::NIL) {
          prop.required = required.as<bool>();
        }
      } else {
        IMVUE_EXCEPTION(ScriptError, "bad property description type");
        return;
      }
    }

    ImU32 hash = ImHashStr(prop.id());
    mProperties[hash] = prop;
  }

  void ComponentFactory::readPropertyTypes(ComponentProperty& prop, Object value)
  {
    if(value.type() == ObjectType::INTEGER || value.type() == ObjectType::NUMBER) {
      prop.types.push_back((ObjectType)value.as<int>());
    } else if(value.type() == ObjectType::ARRAY) {
      for(Object::iterator iter = value.begin(); iter != value.end(); ++iter) {
        if(iter.value.type() != ObjectType::INTEGER && iter.value.type() != ObjectType::NUMBER) {
          IMVUE_EXCEPTION(ScriptError, "bad property expected type list value: integer expected");
          return;
        }

        prop.types.push_back((ObjectType)iter.value.as<int>());
      }
    } else {
      IMVUE_EXCEPTION(ScriptError, "bad property expected type: integer or array expected");
    }
  }

  ComponentContainer::ComponentContainer()
    : mDocument(0)
    , mRawData(NULL)
    , mMounted(false)
    , mRefs((int*)ImGui::MemAlloc(sizeof(int)))
  {
    *mRefs = 1;
  }

  ComponentContainer::~ComponentContainer()
  {
    if((*mRefs) > 1) {
      mChildren.clear();
    }
    destroy();
  }

  void ComponentContainer::destroy()
  {
    if(!mRefs || --(*mRefs) > 0) {
      return;
    }

    ImGui::MemFree(mRefs);

    fireCallback(ScriptState::BEFORE_DESTROY);
    removeChildren();

    if(mRawData) {
      ImGui::MemFree(mRawData);
    }

    if(mDocument) {
      delete mDocument;
    }

    fireCallback(ScriptState::DESTROYED);
    if(mCtx && mCtx->root == this) {
      delete mCtx;
    }
    mScriptState = 0;
  }

  bool ComponentContainer::build()
  {
    configureComponentFactories();
    return ContainerElement::build();
  }

  void ComponentContainer::configureComponentFactories()
  {
    if(mCtx && mCtx->script) {
      Object components = mCtx->script->getObject("self.components");
      if(components) {
        for(Object::iterator iter = components.begin(); iter != components.end(); ++iter) {
          Object tag;
          Object definition;
          if(components.type() == ObjectType::ARRAY) {
            tag = iter.value["tag"];
            definition = iter.value["component"];
          } else {
            tag = iter.key;
            definition = iter.value["component"];
          }

          if(!tag || !definition) {
            IMVUE_EXCEPTION(ScriptError, "malformed component %s definition", (tag ? tag.as<ImString>().c_str() : "unknown"));
            continue;
          }

          ImU32 hash = ImHashStr(tag.as<ImString>().get());
          mComponents[hash] = ComponentFactory(definition);
        }
      }
    }
  }

  Element* ComponentContainer::createElement(rapidxml::xml_node<>* node, ScriptState::Context* sctx, Element* parent)
  {
    Element* res = NULL;
    Context* ctx = mCtx;
    const char* nodeName = node->name();
    if(nodeName[0] == '\0') {
      nodeName = TEXT_NODE;
    }

    const ElementBuilder* builder = mFactory->get(nodeName);
    if(builder) {
      res = builder->create(node, ctx, sctx, parent);
    } else {
      // then try to create a component
      res = createComponent(node, ctx, sctx, parent);
    }

    if(res && sctx) {
      sctx->owner = res;
    }
    return res;
  }

  Element* ComponentContainer::createComponent(rapidxml::xml_node<>* node, Context* ctx, ScriptState::Context* sctx, Element* parent)
  {
    ImU32 nodeID = ImHashStr(node->name());
    if(mComponents.count(nodeID) == 0) {
      return NULL;
    }

    Component* component = mComponents[nodeID].create();
    try {
      component->configure(node, ctx, sctx, parent);
    } catch(...) {
      delete component;
      throw;
    }
    return component;
  }

  void ComponentContainer::parseXML(const char* data)
  {
    if(mRawData) {
      ImGui::MemFree(mRawData);
    }

    if(mDocument) {
      delete mDocument;
    }

    mMounted = false;
    removeChildren();
    // create local copy of XML data for rapidxml to control the lifespan
    mRawData = ImStrdup(data);

    mDocument = new rapidxml::xml_document<>();
    mDocument->parse<0>(mRawData);
  }

  void ComponentContainer::renderBody() {
    if(!mMounted) {
      fireCallback(ScriptState::BEFORE_MOUNT);
    }

    ContainerElement::renderBody();
    if(mCtx->script) {
      mCtx->script->update();
    }

    if(!mMounted) {
      mMounted = true;
      fireCallback(ScriptState::MOUNTED);
    }
  }

  Document::Document(Context* ctx)
  {
    mCtx = ctx;
  }

  Document::~Document()
  {
  }

  void Document::parse(const char* data)
  {
    if(mCtx == 0) {
      mCtx = createContext(createElementFactory());
    }

    mCtx->root = this;

    if(!mCtx->factory) {
      IMVUE_EXCEPTION(ElementError, "no element factory is defined in the context");
      return;
    }

    mScriptState = mCtx->script;

    std::stringstream ss;
    ss << "<document>" << data << "</document>";
    std::string raw = ss.str();
    parseXML(&raw[0]);

    rapidxml::xml_node<>* root = mDocument->first_node("document");

    rapidxml::xml_node<>* scriptNode = root->first_node("script");
    if(scriptNode && mScriptState) {
      char* data = getNodeData(mCtx, scriptNode);
      if(data) {
        try {
          mScriptState->initialize(data);
          ImGui::MemFree(data);
        } catch(...) {
          ImGui::MemFree(data);
          throw;
        }
      } else {
        mScriptState->initialize(scriptNode->value());
      }
    }

    for (rapidxml::xml_node<>* node  = root->first_node("style"); node; node = node->next_sibling("style")) {
      char* data = getNodeData(mCtx, node);
      bool scoped = node->first_attribute("scoped") != NULL;

      if(data) {
        try {
          mCtx->style->load(data, scoped);
        } catch(...) {
          ImGui::MemFree(data);
          throw;
        }
      } else {
        mCtx->style->load(node->value(), scoped);
      }
    }

    fireCallback(ScriptState::BEFORE_CREATE);

    mNode = root->first_node("template");
    if(mNode) {
      configure(mNode, mCtx);
      fireCallback(ScriptState::CREATED);
    }
  }

  Component* ComponentFactory::create()
  {
    if(!mValid) {
      return NULL;
    }

    return new Component(mTemplate, mProperties, mData);
  }

  Component::Component(ImString tmpl, ComponentProperties& props, Object data)
    : mProperties(props)
    , mData(data)
  {
    parseXML(tmpl.get());
    mFlags = mFlags | Element::COMPONENT | Element::PSEUDO_ELEMENT;
  }

  Component::~Component()
  {
  }

  bool Component::build() {
    configureComponentFactories();
    for(ComponentProperties::iterator iter = mProperties.begin(); iter != mProperties.end(); ++iter)
    {
      ComponentProperty& prop = iter->second;
      const char* name = prop.attribute.get();
      const char* attrID = prop.id();

      rapidxml::xml_attribute<>* attr = mNode->first_attribute(name);
      if(!attr) {
        attr = mNode->first_attribute(attrID);
      }

      if(!attr) {
        Object def = prop.def;
        if(prop.required && !def) {
          IMVUE_EXCEPTION(ElementError, "required property %s is not defined", attrID);
          return false;
        }

        if(def) {
          if(!prop.validate(def)) {
            IMVUE_EXCEPTION(ElementError, "validation failed on default value, prop: %s", attrID);
            return false;
          }
          ScriptState& state = *mCtx->script;
          state[attrID] = def;
        }
      }
    }

    removeChildren();
    mBuilder = mFactory->get("__element__");
    if(!Element::build()) {
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
    rapidxml::xml_node<>* tmpl = mDocument->first_node("template");
    createChildren(tmpl ? tmpl : mDocument);

    for (rapidxml::xml_node<>* node  = mDocument->first_node("style"); node; node = node->next_sibling("style")) {
      char* data = getNodeData(mCtx, node);
      bool scoped = node->first_attribute("scoped") != NULL;

      if(data) {
        try {
          mCtx->style->load(data, scoped);
        } catch(...) {
          ImGui::MemFree(data);
          throw;
        }
      } else {
        mCtx->style->load(node->value(), scoped);
      }
    }

    // clear change listeners to avoid triggering reactive change for each prop initial setup
    if(mCtx->script)
      mCtx->script->clearChanges();
    return true;
  }

  void Component::configure(rapidxml::xml_node<>* node, Context* ctx, ScriptState::Context* sctx, Element* parent)
  {
    Context* child = newChildContext(ctx);
    try {
      child->root = this;
      if(child->script) {
        child->script->initialize(mData);
        child->script->lifecycleCallback(ScriptState::BEFORE_CREATE);
      }

      mScriptState = ctx->script;
    } catch(...) {
      delete child;
      throw;
    }
    Element::configure(node, child, sctx, parent);
    if(mConfigured) {
      fireCallback(ScriptState::CREATED);
    }
  }

  bool Component::initAttribute(const char* attrID, const char* value, int flags, ScriptState::Fields* fields)
  {
    ImU32 hash = ImHashStr(attrID);

    if(mProperties.count(hash) == 0) {
      return false;
    }

    ComponentProperty& prop = mProperties[hash];
    ScriptState& state = *mCtx->script;

    if((flags & Attribute::SCRIPT) && mScriptState) {
      Object result = mScriptState->getObject(value, fields, mScriptContext);
      if(!prop.validate(result)) {
        IMVUE_EXCEPTION(ElementError, "[%s] field validation failed %s, got type: %d", getType(), attrID, result.type());
        return false;
      }
      if(result) {
        state[attrID] = result;
      }
    } else {
      state[attrID] = value;
    }

    return true;
  }

} // namespace ImVue
