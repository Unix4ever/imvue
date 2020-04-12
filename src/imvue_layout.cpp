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

#include "imvue_element.h"
#include "imvue_layout.h"

namespace ImVue {

  bool skipLayout(Element* element)
  {
    return element->getFlags() & Element::WINDOW || !GetCurrentWindowNoDefault() ||
      element->style()->position == CSS_POSITION_FIXED ||
      element->style()->position == CSS_POSITION_ABSOLUTE;
  }

  Layout::Layout()
    : topMargin(0)
    , bottomMargin(0)
    , height(0)
    , currentElement(0)
    , container(0)
    , index(0)
  {
  }

  void Layout::beginElement(Element* element)
  {
    if(skipLayout(element)) {
      return;
    }

    if(!currentElement) {
      lineEnd = ImGui::GetCursorPos();
      cursorStart = lineEnd;
    }

    // move to the new line if current element is block
    // or if it's inline but should be wrapped to the next line
    if(element->style()->displayBlock ||
        (contentRegion.x > 0 && lineEnd.x + element->getSize().x > contentRegion.x)) {
      newLine();
    }

    currentElement = element;
    // initial position setup
    ImVec2 pos = lineEnd;
    // apply element margins

    ImGuiStyle& style = ImGui::GetStyle();
    // top
    if(element->margins[0] == FLT_MIN) {
      topMargin = style.ItemSpacing.y;
    } else {
      // always greater than 0
      topMargin = ImMax(element->margins[0], topMargin);
    }

    if(ImGui::GetColumnOffset() != 0) {
      pos.x = ImGui::GetColumnOffset() + GetCurrentWindowNoDefault()->Scroll.x;
    }

    // left
    if(element->margins[1] == FLT_MIN) {
      pos.x += style.ItemSpacing.y;
    } else {
      pos.x += element->margins[1];
    }

    pos.y += topMargin;
    ImGui::SetCursorPos(pos);
    lineEnd.x = pos.x;
  }

  void Layout::endElement(Element* element)
  {
    if(skipLayout(element)) {
      if(GetCurrentWindowNoDefault())
        ImGui::SetCursorPos(lineEnd);
      return;
    }

    IM_ASSERT(currentElement == element);

    // right
    if(element->margins[2] != FLT_MIN) {
      lineEnd.x += element->margins[2];
    }

    // bottom
    if(element->margins[3] != FLT_MIN) {
      bottomMargin = ImMax(element->margins[3], bottomMargin);
    }

    ImVec2 size = element->getSize();
    lineEnd.x += size.x;
    height = ImMax(size.y, height);

    if(element->style()->displayBlock) {
      newLine();
    } else {
      ImGui::SetCursorPos(ImVec2(lineEnd.x, cursorStart.y));
    }

    ++index;
  }

  void Layout::newLine()
  {
    index = 0;
    if(!currentElement) {
      return;
    }

    max = ImMax(lineEnd, max);
    lineEnd.x = cursorStart.x;
    lineEnd.y += height + topMargin + bottomMargin;
    height = 0.0f;
    topMargin = 0.0f;
    bottomMargin = 0.0f;
    max = ImMax(lineEnd, max);
  }

  void Layout::begin(ContainerElement* element)
  {
    container = element;
    contentRegion = element->size;
    height = 0.0f;
    topMargin = 0.0f;
    bottomMargin = 0.0f;
    currentElement = 0;

    max = ImVec2(0.0f, 0.0f);
  }

  void Layout::end()
  {
    newLine();

    ImGuiWindow* window = GetCurrentWindowNoDefault();
    if(window) {
      window->DC.CursorMaxPos = ImMax(window->Pos - window->Scroll + max, window->DC.CursorMaxPos);
    }
  }

  void processElement(Element* element, void* userdata)
  {
    FlexLayout* layout = (FlexLayout*)userdata;
    FlexSettings& flex = element->style()->flex;
    IM_ASSERT(element->style()->displayBlock == false);
    if(flex.grow == 0) {
      ImRect rect = element->getMarginsRect();

      if(layout->container->style()->flex.direction & FlexSettings::Row) {
        layout->remainingLength -= element->getSize().x + rect.Min.x + rect.Max.x;
      } else {
        layout->remainingLength -= element->getSize().y + rect.Min.y + rect.Max.y;
      }
    } else {
      layout->slices += flex.grow;
    }
  }

  void FlexLayout::beginElement(Element* element)
  {
    if(skipLayout(element)) {
      return;
    }

    if(!currentElement) {
      lineEnd = ImGui::GetCursorPos();
      cursorStart = lineEnd;
    }

    currentElement = element;
    // initial position setup
    ImVec2 pos = lineEnd;
    // apply element margins

    ImGuiStyle& style = ImGui::GetStyle();
    // top
    if(element->margins[0] == FLT_MIN) {
      topMargin = style.ItemSpacing.y;
    } else {
      // always greater than 0
      topMargin = ImMax(element->margins[0], topMargin);
    }

    // left
    if(element->margins[1] == FLT_MIN) {
      pos.x += style.ItemSpacing.y;
    } else {
      pos.x += element->margins[1];
    }

    FlexSettings& flex = element->style()->flex;
    if(flex.grow > 0 && step > 0.0f) {
      ImRect rect = element->getMarginsRect();

      if(container->style()->flex.direction & FlexSettings::Row) {
        element->setSize(ImVec2(flex.grow * step - rect.Min.x - rect.Max.x, element->size.y));
      } else {
        element->setSize(ImVec2(element->size.x, flex.grow * step - rect.Min.y - rect.Max.y));
      }
    }

    pos.y += topMargin;
    ImGui::SetCursorPos(pos);
    lineEnd.x = pos.x;
  }

  void FlexLayout::endElement(Element* element)
  {
    if(skipLayout(element)) {
      if(GetCurrentWindowNoDefault())
        ImGui::SetCursorPos(lineEnd);
      return;
    }

    IM_ASSERT(currentElement == element);

    // right
    if(element->margins[2] != FLT_MIN) {
      lineEnd.x += element->margins[2];
    }

    // bottom
    if(element->margins[3] != FLT_MIN) {
      bottomMargin = ImMax(element->margins[3], bottomMargin);
    }

    ImVec2 size = element->getSize();
    lineEnd.x += size.x;
    height = ImMax(size.y, height);

    FlexSettings& flex = container->style()->flex;
    if(flex.direction & FlexSettings::Column) {
      newLine();
    } else {
      ImGui::SetCursorPos(ImVec2(lineEnd.x, cursorStart.y));
    }
  }

  void FlexLayout::begin(ContainerElement* container)
  {
    Layout::begin(container);
    slices = 0.0f;
    FlexSettings& flex = container->style()->flex;
    if(flex.direction & FlexSettings::Row) {
      remainingLength = contentRegion.x;
    } else if(container->style()->heightMode == CSS_HEIGHT_SET) {
      remainingLength = contentRegion.y;
    }

    // TODO: recalculate only if layout is invalid
    container->forEachChild(processElement, this);
    if(slices > 0.0f)
      step = remainingLength / slices;
    else
      step = 0.0f;
  }
}
