/*
MIT License

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

#ifndef __IMVUE_STYLE_H__
#define __IMVUE_STYLE_H__

#if __APPLE__
#define DEFAULT_DPI 72.0f
#else
#define DEFAULT_DPI 96.0f
#endif

extern "C" {
#ifndef restrict
#define restrict
#endif
#include <libcss/libcss.h>
}

#include <iostream>

#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

struct css_computed_style;
struct css_select_ctx;

namespace ImVue {

  ImGuiWindow* GetCurrentWindowNoDefault(); // like GetCurrentWindowRead, but returns NULL if current window is fallback window

  class Element;
  class ComputedStyle;
  class Context;

  typedef bool (*styleCallback)(ComputedStyle& style);

  inline ImU32 parseColor(css_color color)
  {
    return (((color >> 16) & 0xFF) << IM_COL32_R_SHIFT) |
      (((color >> 8) & 0xFF) << IM_COL32_G_SHIFT) |
      (((color >> 0) & 0xFF) << IM_COL32_B_SHIFT) |
      (((color >> 24) & 0xFF) << IM_COL32_A_SHIFT);
  }

  enum ParseUnitsAxis { X, Y };

  float parseUnits(css_fixed value, css_unit unit, ComputedStyle& style, const ImVec2& parentSize, ParseUnitsAxis axis);

  bool setDimensions(ComputedStyle& style);

  bool setPosition(ComputedStyle& style);

  bool setColor(ComputedStyle& style);

  bool setBackgroundColor(ComputedStyle& style);

  bool setPadding(ComputedStyle& style);

  bool setMargins(ComputedStyle& style);

  bool setFont(ComputedStyle& style);

  bool setBorderRadius(ComputedStyle& style);

  bool setBorder(ComputedStyle& style);

  bool setFlexBasis(ComputedStyle& style);

  bool setOverflow(ComputedStyle& style);

  bool setMinSize(ComputedStyle& style);

  class Style;

  struct FlexSettings
  {
    // item settings
    enum Align {
      Auto,
      Stretch,
      FlexStart,
      FlexEnd,
      Center,
      Baseline,
      FirstBaseline,
      LastBaseline,
      Start,
      End,
      SelfStart,
      SelfEnd,
      Safe,
      Unsafe,
      SpaceBetween,
      SpaceAround,
      SpaceEvenly,
      Left,
      Right
    };

    float grow;
    float shrink;
    float basis;
    int order;
    Align align;

    // container settings
    enum Direction
    {
      Row     = 1,
      Column  = 1 << 1,
      Reverse = 1 << 2,
      RowReverse = Row | Reverse,
      ColumnReverse = Column | Reverse
    };

    Direction direction;

    enum FlexWrap {
      NoWrap,
      Wrap,
      WrapReverse
    };

    FlexWrap wrap;
    Align justifyContent;
    Align alignContent;
    Align alignItems;

    ParseUnitsAxis axis;
  };

  /**
   * Style state
   */
  class ComputedStyle
  {
    public:

      ComputedStyle(Element* target);
      ~ComputedStyle();
      /**
       * Compute style for an element
       *
       * @param element Target element
       */
      bool compute(Element* element);

      /**
       * Apply computed style
       */
      void begin(Element* element);

      /**
       * Roll back any changes
       */
      void end();

      ImVec2 localToGlobal(const ImVec2& pos);

      void destroy();

      void updateClassCache();

      void setInlineStyle(const char* data);

      ImVector<lwc_string*>& getClasses();

      // override copy constructor
      ComputedStyle(ComputedStyle& other)
        : libcssData(0)
        , fontName(0)
        , fontSize(0)
        , element(other.element)
        , style(0)
        , context(0)
        , nClipRect(0)
        , nCol(0)
        , nStyle(0)
        , nFonts(0)
        , fontScale(0)
        , mSelectCtx(0)
        , mInlineStyle(0)
        , mAutoSize(false)
      {
        compute(element);
      }

      void* libcssData;
      char* fontName;
      float fontSize;
      uint16_t position;

      Element* element;

      css_select_results* style;

      ImVec2 elementScreenPosition;
      ImVec2 contentRegion;
      ImVec2 parentSize;
      ImVec4 borderRadius;

      Context* context;

      struct Decoration {
        void renderBackground(ImRect rect);
        void render(ImRect rect);

        void drawJoint(
          const ImVec2& p1,
          const ImVec2& p2,
          const ImVec2& p3,
          const ImVec2& p1i,
          const ImVec2& p2i,
          const ImVec2& p3i,
          ImU32 startColor,
          ImU32 endColor,
          int numSegments
        );

        void drawJoint(
          const ImVec2& p1,
          const ImVec2& p2,
          const ImVec2& p3,
          const ImVec2& center,
          ImU32 startColor,
          ImU32 endColor,
          const ImVec2& startPoint,
          const ImVec2& endPoint,
          int numSegments
        );

        float thickness[4];
        float rounding[4];
        ImU32 col[4];
        ImU32 bgCol;
      };

      uint8_t overflowX;
      uint8_t overflowY;
      Decoration decoration;
      bool displayBlock;
      bool displayInline;
      FlexSettings flex;
      ImGuiWindow* window;

      // style stack
      int nClipRect;
      int nCol;
      int nStyle;
      int nFonts;
      float fontScale;

      uint16_t widthMode;
      uint16_t heightMode;
    private:

      void initFonts(css_media* media);

      void cleanupClassCache();

      void updateFlexSettings();

      friend class Style;
      ImVector<styleCallback> mStyleCallbacks;
      css_select_ctx* mSelectCtx;
      ImVector<lwc_string*> mClasses;
      css_stylesheet* mInlineStyle;
      bool mAutoSize;
  };

  /**
   * Wrapper for libcss
   */
  class Style {
    public:
      struct Sheet {
        css_stylesheet* sheet;
        bool scoped;
      };

      Style(Style* parent = 0);
      ~Style();

      css_select_ctx* select();

      /**
       * Parse sheet and append it to sheet list
       *
       * @param data raw sheet data
       * @param scoped adds list to local
       */
      void load(const char* data, bool scoped = false);

      void parse(const char* data, css_stylesheet** dest, bool isInline = false);

    private:

      css_stylesheet* mBase;
      ImVector<Sheet> mSheets;

      Style* mParent;

      void appendSheets(css_select_ctx* ctx, bool scoped = false);

  };
}

#endif
