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
    if(element->display == CSS_DISPLAY_BLOCK ||
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

    if(element->display == CSS_DISPLAY_BLOCK) {
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

    lineEnd.x = cursorStart.x;
    lineEnd.y += height + topMargin + bottomMargin;
    height = 0.0f;
    topMargin = 0.0f;
    bottomMargin = 0.0f;
  }

  void Layout::begin(ContainerElement* element)
  {
    container = element;
    contentRegion = element->size;
    height = 0.0f;
    topMargin = 0.0f;
    bottomMargin = 0.0f;
    currentElement = 0;
  }

  void Layout::end()
  {
    ImVec2 max;
    max.x = lineEnd.x;
    newLine();
    max.y = lineEnd.y;

    ImGuiWindow* window = GetCurrentWindowNoDefault();
    if(window) {
      window->DC.CursorMaxPos = ImMax(window->Pos - window->Scroll + max, window->DC.CursorMaxPos);
    }
  }
}
