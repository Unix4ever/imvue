#ifndef __IMVUE_H__
#define __IMVUE_H__

#include "imvue_element.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "rapidxml.hpp"
#include <iostream>

namespace ImVue {

  class Component;

  struct ComponentProperty {
    ImString attribute;
    // required for type validation
    ImVector<ObjectType> types;
    // default value
    Object def;
    Object validator;
    bool required;

    ComponentProperty()
      : required(false)
    {
    }

    void setAttr(ImString value) {
      attribute = ImString(value.size() + 1);
      attribute[0] = ':';
      ImStrncpy(&attribute.get()[1], value.get(), value.size() + 1);
    }

    const char* id() const {
      return &attribute.c_str()[1];
    }

    bool validate(Object value)
    {
      if(validator) {
        Object args[] = {value};
        Object rets[1];
        validator(args, rets, 1, 1);
        return rets[0].as<bool>();
      } else if(types.size() != 0) {
        ObjectType t = value.type();
        for(int i = 0; i < types.size(); ++i) {
          if(t == (ObjectType)types[i]) {
            return true;
          }
        }

        return false;
      }

      return true;
    }
  };

  typedef std::unordered_map<ImU32, ComponentProperty> ComponentProperties;

  /**
   * Loads component construction info
   * Can create new components of a type
   */
  class ComponentFactory {

    public:

      ComponentFactory();

      ComponentFactory(Object definition);

      Component* create();

    private:

      void defineProperty(Object& key, Object& value);

      void readPropertyTypes(ComponentProperty& prop, Object value);

      ComponentProperties mProperties;
      ImString mTemplate;
      Object mData;
      bool mValid;
  };

  class ComponentContainer : public ContainerElement {

    public:
      ComponentContainer();
      virtual ~ComponentContainer();
      /**
       * Create element using ElementFactory or tries to create Component
       *
       * @param node
       */
      Element* createElement(rapidxml::xml_node<>* node, ScriptState::Context* sctx = 0, Element* parent = 0);

      void renderBody();

    protected:

      virtual bool build();

      /**
       * Initialize components factories
       */
      void configureComponentFactories();
      /**
       * Creates component
       */
      Element* createComponent(rapidxml::xml_node<>* node, Context* ctx, ScriptState::Context* sctx);

      void parseXML(const char* data);

      rapidxml::xml_document<>* mDocument;

      char* mRawData;
      bool mMounted;

    private:

      typedef std::unordered_map<ImU32, ComponentFactory> ComponentFactories;
      ComponentFactories mComponents;

  };

  /**
   * Root document
   */
  class Document : public ComponentContainer {

    public:
      Document(Context* ctx = 0);
      virtual ~Document();

      /**
       * Parse document
       *
       * @param data xml file, describing the document
       */
      void parse(const char* data);
  };

  /**
   * Child document
   */
  class Component : public ComponentContainer {
    public:
      Component(ImString tmpl, ComponentProperties& props, Object data);
      virtual ~Component();

      bool build();

      void configure(rapidxml::xml_node<>* node, Context* ctx, ScriptState::Context* sctx, Element* parent);

      virtual bool initAttribute(const char* id, const char* value, int flags = 0, ScriptState::Fields* fields = 0);

    private:
      ComponentProperties& mProperties;
      Object mData;
  };

} // namespace ImVue
#endif
