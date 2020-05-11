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

#ifndef __IMVUE_LAYOUT__
#define __IMVUE_LAYOUT__

namespace ImVue {
  class Element;
  class ContainerElement;

  struct Layout {
    Layout();
    virtual ~Layout() {}
    float topMargin;    // top margin max value
    float bottomMargin; // bottom margin max value
    float height;       // total line height

    ImVec2 cursorStart;
    ImVec2 lineEnd;
    ImVec2 contentRegion;
    ImVec2 max;

    Element* currentElement;
    ContainerElement* container;

    int index;

    /**
     * Prepares layout for element rendering
     *  * sets cursor position
     *  * wraps line if needed
     *  * saves current draw list position
     *
     * @param element Next element
     */
    virtual void beginElement(Element* element);

    /**
     * Finalizes element draw
     * May do transform for the current draw list
     * If item should be aligned or wrapped
     *
     * @param element Element that was just drawn
     */
    virtual void endElement(Element* element);

    virtual void begin(ContainerElement* container);

    virtual void newLine();

    void end();
  };

  struct FlexLayout : public Layout {
    void beginElement(Element* element);

    void endElement(Element* element);

    void begin(ContainerElement* container);

    float slices;
    float remainingLength;
    float step;
    private:
      float* getLength(const ImVec2& input);
  };

  struct StaticLayout : public Layout {
  };
}

#endif
