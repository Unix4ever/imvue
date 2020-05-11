/*
Copyright (c) 2019-2020 Artem Chernyshev

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

#ifndef __IMVUE_HTML_H__
#define __IMVUE_HTML_H__

#include "imgui.h"
#include "imvue_element.h"


namespace ImVue {

  /**
   * Top level html container
   */
  class Html : public ContainerElement {

    public:

      void renderBody()
      {
        if(!GetCurrentWindowNoDefault()) {
          ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
          ImGui::SetNextWindowPos(ImVec2(0, 0));
          ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize));
          if(ImGui::Begin("###htmlWindow", NULL, ImGuiWindowFlags_NoDecoration)) {
            ImGui::PopStyleVar();
            ContainerElement::renderBody();
          } else {
            ImGui::PopStyleVar();
          }

          ImGui::End();
        } else {
          ContainerElement::renderBody();
        }
      }
  };

  inline void addMaxPadding(Element* element)
  {
    ImVec2 max;
    if(element->style()->widthMode != CSS_WIDTH_SET) {
      max.x += element->padding[2];
    }

    if(element->style()->heightMode != CSS_HEIGHT_SET) {
      max.y += element->padding[3];
    }

    ImVec2 size = ImGui::GetItemRectMax() - ImGui::GetItemRectMin() + max;
    if(size.x < element->minSize.x) {
      max.x += element->minSize.x - size.x;
    }
    if(size.y < element->minSize.y) {
      max.y += element->minSize.y - size.y;
    }

    ImGui::SetCursorScreenPos(ImGui::GetItemRectMax() + max);
  }

  /**
   * Base class for any html node
   */
  class HtmlContainer : public ContainerElement {

    public:

      HtmlContainer()
        : mID(0)
        , mWidth(0)
      {
      }

      ~HtmlContainer()
      {
      }

      bool build()
      {
        bool built = ContainerElement::build();
        if(built) {
          Element* parent = mParent;

          while(parent) {
            ImU32 id = ImHashStr(parent->getType());
            if(!mID) {
              mID = id;
            } else {
              mID = mID ^ id;
            }
            parent = parent->getParent();
          }

        }

        return built;
      }

      void renderBody()
      {
        bool useChild = mStyle.overflowX == CSS_OVERFLOW_SCROLL || mStyle.overflowY == CSS_OVERFLOW_SCROLL;
        bool drawContent = true;
        ImVec2 pos = ImGui::GetCursorScreenPos();

        ImRect pad(ImVec2(0, 0), ImVec2(0, 0));
        if(display != CSS_DISPLAY_INLINE) {
          pad.Min = pad.Max = ImGui::GetStyle().FramePadding;
          if(padding[0] >= 0) {
            pad.Min.x = padding[0];
          }

          if(padding[1] >= 0) {
            pad.Min.y = padding[1];
          }

          if(padding[2] >= 0) {
            pad.Max.x = padding[2];
          }

          if(padding[3] >= 0) {
            pad.Max.y = padding[3];
          }
        }
        int flags = ImGuiWindowFlags_NavFlattened | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize;
        if(mStyle.overflowX == CSS_OVERFLOW_SCROLL) {
          flags |= ImGuiWindowFlags_HorizontalScrollbar;
        }

        if(mStyle.overflowY != CSS_OVERFLOW_SCROLL) {
          flags |= ImGuiWindowFlags_NoScrollbar;
        }

        if(useChild) {
          ImVec2 childSize(size.x > 0 ? size.x : mWidth, size.y);
          childSize -= ImVec2(
              ImMax(mStyle.decoration.thickness[0], mStyle.decoration.thickness[2]),
              ImMax(mStyle.decoration.thickness[1], mStyle.decoration.thickness[3])
          );
          ImGui::PushStyleColor(ImGuiCol_ChildBg, 0);
          drawContent = ImGui::BeginChild(mID + index, childSize, false, flags);
          ImGui::PopStyleColor();
          ImGui::SetCursorPos(pad.Min);
        } else {
          ImGui::BeginGroup();
          ImGui::SetCursorScreenPos(pos + pad.Min);
        }

        if(drawContent) {
          if(mChildren.size() == 0 && mStyle.heightMode != CSS_HEIGHT_SET) {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImVec2 s = size;
            if(s.y == 0) {
              s.y = ImGui::GetTextLineHeight();
            }
            ImRect bb(p, p + s);
            ImGui::ItemSize(s);
            ImGui::ItemAdd(bb, 0);
          } else {
            ContainerElement::renderBody();
          }
        }

        addMaxPadding(this);
        if(useChild) {
          mWidth = ImGui::GetItemRectSize().x + pad.Min.x;
          if(mStyle.overflowX != CSS_OVERFLOW_SCROLL) {
            ImGui::GetCurrentWindowRead()->DC.CursorMaxPos.x = 0;
          }

          if(mStyle.overflowY != CSS_OVERFLOW_SCROLL) {
            ImGui::GetCurrentWindowRead()->DC.CursorMaxPos.y = 0;
          }
          ImGui::EndChild();
        } else {
          ImGui::EndGroup();
        }
      }

    private:
      ImU32 mID;
      float mWidth;
  };

  static int InputTextCallback(ImGuiInputTextCallbackData* data)
  {
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
      // Resize string callback
      ImVector<char>* str = (ImVector<char>*)data->UserData;
      IM_ASSERT(data->Buf == str->begin());
      str->resize(data->BufSize);
      data->Buf = str->begin();
    }

    return 0;
  }

  /**
   * Text input
   */
  class Input : public Element {

    public:

      enum InputType {
        BUTTON,
        CHECKBOX,
        COLOR,
        DATE,
        DATETIME_LOCAL,
        EMAIL,
        TFILE,
        HIDDEN,
        IMAGE,
        MONTH,
        NUMBER,
        PASSWORD,
        RADIO,
        RANGE,
        RESET,
        SEARCH,
        SUBMIT,
        TEL,
        TEXT,
        TIME,
        URL,
        WEEK
      };

      Input()
        : placeholder(0)
        , format(0)
        , min(0.0f)
        , max(100.0f)
        , step(1.0f)
        , name(0)
        , speed(1.0f)
        , mValue(0)
        , mModel(0)
        , mValueUpdated(false)
        , mActivate(false)
        , mID(0)
        , mType(TEXT)
      {
        mBuffer.resize(48);
        memset(mBuffer.Data, 0, sizeof(char) * 48);
      }

      ~Input()
      {
        if(placeholder) {
          ImGui::MemFree(placeholder);
        }

        if(mValue) {
          ImGui::MemFree(mValue);
        }

        if(mModel) {
          ImGui::MemFree(mModel);
        }

        if(format) {
          ImGui::MemFree(format);
        }

        if(name) {
          ImGui::MemFree(name);
        }
      }

      bool build()
      {
        bool built = Element::build();
        if(built) {
          Element* parent = mParent;

          while(parent) {
            ImU32 id = ImHashStr(parent->getType());
            if(!mID) {
              mID = id;
            } else {
              mID = mID ^ id;
            }
            parent = parent->getParent();
          }

        }

        return built;
      }

      void renderBody()
      {
        if(mInvalidFlags & Element::MODEL) {
          mInvalidFlags ^= Element::MODEL;
          syncModel();
        }

        ImGui::PushID(mID + index);
        ImGui::BeginGroup();
        ImVec2 pos = ImGui::GetCursorScreenPos();

        ImRect pad(ImVec2(0, 0), ImVec2(0, 0));
        if(display != CSS_DISPLAY_INLINE) {
          pad.Min = pad.Max = ImGui::GetStyle().FramePadding;
          if(padding[0] >= 0) {
            pad.Min.x = padding[0];
          }

          if(padding[1] >= 0) {
            pad.Min.y = padding[1];
          }

          if(padding[2] >= 0) {
            pad.Max.x = padding[2];
          }

          if(padding[3] >= 0) {
            pad.Max.y = padding[3];
          }
        }

        ImGui::SetCursorScreenPos(pos + pad.Min);
        int popColor = 0;
        if(mStyle.decoration.bgCol != 0) {
          ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
          ++popColor;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_CallbackResize;
        if(size.x != 0) {
          ImGui::PushItemWidth(contentWidth());
        }
        int col[4] = {0};
        bool changed = false;
        bool checked = hasState(CHECKED);

        switch(mType) {
          case PASSWORD:
            flags |= ImGuiInputTextFlags_Password;
          case TEXT:
            if(mValueUpdated) {
              strcpy(&mBuffer.Data[0], mValue);
              changed = true;
            }

            if(placeholder) {
              changed = ImGui::InputTextWithHint("##label", placeholder, mBuffer.Data, mBuffer.size() + 1, flags, InputTextCallback, &mBuffer);
            } else {
              changed = ImGui::InputText("##label", mBuffer.Data, mBuffer.size() + 1, flags, InputTextCallback, &mBuffer);
            }

            break;
          case BUTTON:
            if(mValue)
              ImGui::TextUnformatted(mValue);
            break;
          case COLOR:
            if(mValueUpdated) {
              char* p = mValue;
              while (*p == '#' || ImCharIsBlankA(*p))
                  p++;

              if (strlen(p) == 8) {
                  sscanf(p, "%02X%02X%02X%02X", (unsigned int*)&col[0], (unsigned int*)&col[1], (unsigned int*)&col[2], (unsigned int*)&col[3]);
              } else {
                  sscanf(p, "%02X%02X%02X", (unsigned int*)&col[0], (unsigned int*)&col[1], (unsigned int*)&col[2]);
                  col[3] = 0xFF;
              }

              mColor.x = (float)col[0]/255.0f;
              mColor.y = (float)col[1]/255.0f;
              mColor.z = (float)col[2]/255.0f;
              mColor.w = (float)col[3]/255.0f;
              changed = true;
            }

            ImGui::ColorButton(placeholder ? placeholder : "color", mColor);
            if (ImGui::BeginPopup("color-picker")) {
              changed = ImGui::ColorPicker4("##picker", (float*)&mColor, ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview);
              ImGui::EndPopup();
            }

            break;
          case CHECKBOX:
            if(ImGui::Checkbox(placeholder ? placeholder : "##checkbox", &checked)) {
              if(checked) {
                setState(CHECKED);
              } else {
                resetState(CHECKED);
              }
              changed = true;
            }
            break;
          case RADIO:
            if(ImGui::RadioButton(placeholder ? placeholder : "##radio", hasState(CHECKED))) {
              setState(CHECKED);
              changed = true;
            }
            break;
          case RANGE:
            changed = ImGui::SliderFloat(placeholder ? placeholder : "##slider", &mSliderValue, min, max, format ? format : "%.3f", step);
            break;
          case NUMBER:
            changed = ImGui::DragFloat(placeholder ? placeholder : "##dragfloat", &mSliderValue, speed, min, max, format ? format : "%.3f", step);
            break;
          default:
            // nothing
            break;
        }

        if(size.x != 0) {
          ImGui::PopItemWidth();
        }
        ImGui::PopStyleColor(popColor);
        ImGui::PopStyleVar(2);
        addMaxPadding(this);
        ImGui::EndGroup();

        if(ImGui::IsItemClicked(0)) {
          if((mFlags & Element::BUTTON) == 0) {
            mActivate = true;
          }

          if(mType == COLOR) {
            ImGui::OpenPopup("color-picker");
          }
        }
        ImGui::PopID();

        if(!ImGui::IsMouseDown(0) && mActivate) {
          mActivate = false;
          ImGui::SetKeyboardFocusHere(-1);
        }
        mValueUpdated = false;

        if(changed) {
          syncModel(true);
        }
      }

      void setType(char* type) {
        if(ImStricmp(type, "text") == 0) {
          mType = TEXT;
        } else if(ImStricmp(type, "password") == 0) {
          mType = PASSWORD;
        } else if(ImStricmp(type, "button") == 0) {
          mType = BUTTON;
        } else if(ImStricmp(type, "color") == 0) {
          mType = COLOR;
        } else if(ImStricmp(type, "checkbox") == 0) {
          mType = CHECKBOX;
        } else if(ImStricmp(type, "radio") == 0) {
          mType = RADIO;
        } else if(ImStricmp(type, "range") == 0) {
          mType = RANGE;
        } else if(ImStricmp(type, "number") == 0) {
          mType = NUMBER;
        }

        if(mType == TEXT || mType == PASSWORD) {
          mFlags ^= Element::BUTTON;
        }

        if(type)
          ImGui::MemFree(type);
      }

      void setValue(char* value)
      {
        if(mValue == value || (mValue && strcmp(value, mValue) == 0)) {
          return;
        }

        if(mValue) {
          ImGui::MemFree(mValue);
          mValue = 0;
        }

        if(!value) {
          return;
        }

        mValue = ImStrdup(value);
        ImGui::MemFree(value);
        mValueUpdated = true;
      }

      void setModel(char* key)
      {
        if(mModel) {
          ImGui::MemFree(mModel);
          ImU32 id = mScriptState->hash(mModel);
          bool removed = mScriptState->removeListener(id, this);
          (void)removed;
          IM_ASSERT(removed && "failed to remove listener");
          mModel = 0;
        }

        if(key[0] != '\0') {
          bindListener(mScriptState->hash(key), NULL, Element::MODEL);
          invalidateFlags(Element::MODEL);
          mModel = ImStrdup(key);
        }

        if(key)
          ImGui::MemFree(key);
      }

      void setChecked(bool value)
      {
        value ? setState(CHECKED) : resetState(CHECKED);
      }

      char* placeholder;

      // range only
      char* format;
      float min;
      float max;
      float step;

      // radio only
      char* name;

      // number only
      float speed;
    private:

      void syncModel(bool write = false)
      {
        if(!mModel || (mFlags & BUTTON))
          return;

        ScriptState& scriptState = *mScriptState;
        Object object = scriptState[mModel];

        if(write) {
          switch(mType) {
            case PASSWORD:
            case TEXT:
              scriptState[mModel] = mBuffer.Data;
              break;
            case COLOR:
              scriptState[mModel] = ImGui::ColorConvertFloat4ToU32(mColor);
              break;
            case CHECKBOX:
              if(object.type() == ObjectType::USERDATA) {
                updateArray(object);
              } else {
                scriptState[mModel] = hasState(CHECKED);
              }
              break;
            case RADIO:
              scriptState[mModel] = mValue ? mValue : placeholder;
              break;
            case NUMBER:
            case RANGE:
              scriptState[mModel] = mSliderValue;
            default:
              break;
          }
        } else {
          switch(mType) {
            case PASSWORD:
            case TEXT:
              setValue(object.as<ImString>().get());
              break;
            case COLOR:
              mColor = ImGui::ColorConvertU32ToFloat4(object.as<ImU32>());
              break;
            case CHECKBOX:
              if(object.type() == ObjectType::USERDATA) {
                resetState(CHECKED);
                for(Object::iterator iter = object.begin(); iter != object.end(); ++iter) {
                  if(strcmp(iter.value.as<ImString>().get(), mValue ? mValue : placeholder) == 0) {
                    setState(CHECKED);
                    break;
                  }
                }
              } else {
                if(object.as<bool>())
                  setState(CHECKED);
                else
                  resetState(CHECKED);
              }

              break;
            case RADIO:
              if(strcmp(object.as<ImString>().get(), (mValue ? mValue : placeholder)) == 0)
                setState(CHECKED);
              else
                resetState(CHECKED);

              break;
            case NUMBER:
            case RANGE:
              mSliderValue = object.as<float>();
            default:
              break;
          }
        }
      }

      void updateArray(Object& object)
      {
        bool found = false;
        int index = 0;
        const char* value = mValue ? mValue : placeholder;

        for(Object::iterator iter = object.begin(); iter != object.end(); ++iter) {
          index++;
          if(strcmp(iter.value.as<ImString>().get(), value) == 0) {
            if(!hasState(CHECKED)) {
              object.erase(iter.key);
            }
            found = true;
            break;
          }
        }

        if(hasState(CHECKED) && !found) {
          object[index + 1] = value;
        }
      }

      char* mValue;
      char* mModel;
      bool mValueUpdated;
      bool mActivate;
      float mSliderValue;
      ImVec4 mColor;
      ImU32 mID;
      InputType mType;
      ImVector<char> mBuffer;

  };

  inline void registerHtmlExtras(ElementFactory& factory)
  {
    factory.element<Html>("html");
    factory.element<HtmlContainer>("body");
    factory.element<HtmlContainer>("div");
    factory.element<HtmlContainer>("span");
    factory.element<HtmlContainer>("label");
    factory.element<HtmlContainer>("p");
    factory.element<HtmlContainer>("h1");
    factory.element<HtmlContainer>("h2");
    factory.element<HtmlContainer>("h3");
    factory.element<HtmlContainer>("h4");
    factory.element<HtmlContainer>("h5");
    factory.element<HtmlContainer>("h6");
    factory.element<Input>("input")
      .setter("type", &Input::setType)
      .setter("v-model", &Input::setModel)
      .setter("value", &Input::setValue)
      .setter("checked", &Input::setChecked)
      .attribute("name", &Input::name)
      .attribute("min", &Input::min)
      .attribute("max", &Input::max)
      .attribute("step", &Input::step)
      .attribute("format", &Input::format)
      .attribute("speed", &Input::speed)
      .attribute("placeholder", &Input::placeholder);
  }

}

#endif
