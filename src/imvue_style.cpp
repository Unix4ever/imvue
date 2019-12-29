/*
MIT License

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

#include "imvue_style.h"
#include "imvue_element.h"
#include "imvue.h"
#include "imvue_errors.h"
#include <algorithm>
#include "css/select.h"

namespace ImVue {

#define IM_NORMALIZE2F_OVER_ZERO(VX,VY)     { float d2 = VX*VX + VY*VY; if (d2 > 0.0f) { float inv_len = 1.0f / ImSqrt(d2); VX *= inv_len; VY *= inv_len; } }
#define IM_FIXNORMAL2F(VX,VY)               { float d2 = VX*VX + VY*VY; if (d2 < 0.5f) d2 = 0.5f; float inv_lensq = 1.0f / d2; VX *= inv_lensq; VY *= inv_lensq; }
#define AA_SIZE 1.0f
#define IM_SEGMENTS(rounding)    (IM_PI * rounding)

  inline ImVec2 quadBezierCalc(ImVec2 p1, ImVec2 p2, ImVec2 p3, float t)
  {
    return ImVec2(
        (1 - t) * (1 - t) * p1.x + 2 * (1 - t) * t * p2.x + t * t * p3.x,
        (1 - t) * (1 - t) * p1.y + 2 * (1 - t) * t * p2.y + t * t * p3.y
    );
  }

  inline ImVec2 getNormal(ImVec2 p1, ImVec2 p2)
  {
    float dx = p2.x - p1.x;
    float dy = p2.y - p1.y;
    IM_NORMALIZE2F_OVER_ZERO(dx, dy);
    return ImVec2(dy, -dx);
  }

  void ComputedStyle::Decoration::drawJoint(
      const ImVec2& p1,
      const ImVec2& p2,
      const ImVec2& p3,
      const ImVec2& p1i,
      const ImVec2& p2i,
      const ImVec2& p3i,
      ImU32 startColor,
      ImU32 endColor,
      int numSegments
  )
  {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 uv = drawList->_Data->TexUvWhitePixel;
    const int pointsCount = numSegments + 1;
    const int idxCount = numSegments * 18;
    const int vtxCount = pointsCount * 4;
    drawList->PrimReserve(idxCount, vtxCount);
    ImVec2 normal;
    ImVec2 outer = quadBezierCalc(p1, p2, p3, 0);
    ImVec2 inner = quadBezierCalc(p1i, p2i, p3i, 0);
    unsigned int idx1 = drawList->_VtxCurrentIdx;

    for(int i = 0; i < pointsCount; ++i) {
      float t = (float)i/(float)numSegments;

      ImVec2 nextOuter = quadBezierCalc(p1, p2, p3, t);
      ImVec2 nextInner = quadBezierCalc(p1i, p2i, p3i, t);
      ImVec2 nextNormal = getNormal(outer, nextOuter);
      float dmx = (nextNormal.x + normal.x) * 0.5f;
      float dmy = (nextNormal.y + normal.y) * 0.5f;
      IM_FIXNORMAL2F(dmx, dmy)

      dmx *= AA_SIZE;
      dmy *= AA_SIZE;

      ImU32 col;
      if(t < 0.5) {
        col = startColor > 0 ? startColor : endColor;
      } else {
        col = endColor > 0 ? endColor : startColor;
      }
      const ImU32 col_trans = col & ~IM_COL32_A_MASK;

      unsigned int idx2 = drawList->_VtxCurrentIdx;

      // Add vertices
      drawList->_VtxWritePtr[0].pos = nextOuter + ImVec2(dmx, dmy) / 2; drawList->_VtxWritePtr[0].uv = uv; drawList->_VtxWritePtr[0].col = col_trans;
      drawList->_VtxWritePtr[1].pos = nextOuter - ImVec2(dmx, dmy) / 2; drawList->_VtxWritePtr[1].uv = uv; drawList->_VtxWritePtr[1].col = col;
      drawList->_VtxWritePtr[2].pos = nextInner + ImVec2(dmx, dmy) / 2; drawList->_VtxWritePtr[2].uv = uv; drawList->_VtxWritePtr[2].col = col;
      drawList->_VtxWritePtr[3].pos = nextInner - ImVec2(dmx, dmy) / 2; drawList->_VtxWritePtr[3].uv = uv; drawList->_VtxWritePtr[3].col = col_trans;
      drawList->_VtxWritePtr += 4;
      drawList->_VtxCurrentIdx += 4;

      if(i == 0) {
        continue;
      }

      // Add indices
      drawList->_IdxWritePtr[0]  = (ImDrawIdx)(idx2+1); drawList->_IdxWritePtr[1]  = (ImDrawIdx)(idx1+1); drawList->_IdxWritePtr[2]  = (ImDrawIdx)(idx1+2);
      drawList->_IdxWritePtr[3]  = (ImDrawIdx)(idx1+2); drawList->_IdxWritePtr[4]  = (ImDrawIdx)(idx2+2); drawList->_IdxWritePtr[5]  = (ImDrawIdx)(idx2+1);
      drawList->_IdxWritePtr[6]  = (ImDrawIdx)(idx2+1); drawList->_IdxWritePtr[7]  = (ImDrawIdx)(idx1+1); drawList->_IdxWritePtr[8]  = (ImDrawIdx)(idx1+0);
      drawList->_IdxWritePtr[9]  = (ImDrawIdx)(idx1+0); drawList->_IdxWritePtr[10] = (ImDrawIdx)(idx2+0); drawList->_IdxWritePtr[11] = (ImDrawIdx)(idx2+1);
      drawList->_IdxWritePtr[12] = (ImDrawIdx)(idx2+2); drawList->_IdxWritePtr[13] = (ImDrawIdx)(idx1+2); drawList->_IdxWritePtr[14] = (ImDrawIdx)(idx1+3);
      drawList->_IdxWritePtr[15] = (ImDrawIdx)(idx1+3); drawList->_IdxWritePtr[16] = (ImDrawIdx)(idx2+3); drawList->_IdxWritePtr[17] = (ImDrawIdx)(idx2+2);
      drawList->_IdxWritePtr += 18;
      normal = nextNormal;
      outer = nextOuter;
      inner = nextInner;

      idx1 = idx2;
    }
  }

  void ComputedStyle::Decoration::drawJoint(
    const ImVec2& p1,
    const ImVec2& p2,
    const ImVec2& p3,
    const ImVec2& center,
    ImU32 startColor,
    ImU32 endColor,
    const ImVec2& startPoint,
    const ImVec2& endPoint,
    int numSegments
  )
  {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    bool half = false;
    drawList->PathLineTo(startPoint);
    for(int i = 0; i < numSegments; ++i) {
      float t1 = (float)i/(float)numSegments;
      float t2 = (float)(i+1)/(float)numSegments;

      ImVec2 tr1 = quadBezierCalc(p1, p2, p3, t1);
      ImVec2 tr2 = quadBezierCalc(p1, p2, p3, t2);
      drawList->PathLineTo(tr1);
      drawList->PathLineTo(tr2);
      if(t1 > 0.5 && !half) {
        drawList->PathLineTo(center);
        drawList->PathFillConvex(startColor == 0 ? endColor : startColor);
        drawList->PathLineTo(quadBezierCalc(p1, p2, p3, 0.5));
        half = true;
      }
    }
    drawList->PathLineTo(endPoint);
    drawList->PathLineTo(center);
    drawList->PathFillConvex(endColor == 0 ? startColor : endColor);
  }

  void ComputedStyle::Decoration::renderBackground(ImRect rect)
  {
    if(bgCol == 0) {
      return;
    }

    rect.Min -= ImVec2(thickness[0], thickness[1]) * 0.1f;
    rect.Max += ImVec2(thickness[2], thickness[3]) * 0.1f;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float tlrx, tlry, trrx, trry, tbrx, tbry, tblx, tbly;
    tlrx = ImMax(0.0f, rounding[0] - thickness[0]);
    tlry = ImMax(0.0f, rounding[0] - thickness[1]);

    trrx = ImMax(0.0f, rounding[1] - thickness[2]);
    trry = ImMax(0.0f, rounding[1] - thickness[1]);

    tbrx = ImMax(0.0f, rounding[2] - thickness[2]);
    tbry = ImMax(0.0f, rounding[2] - thickness[3]);

    tblx = ImMax(0.0f, rounding[3] - thickness[0]);
    tbly = ImMax(0.0f, rounding[3] - thickness[3]);

    drawList->PathLineTo(rect.Min + ImVec2(0, tlry));
    if(tlrx > 0 || tlry > 0) {
      drawList->PathBezierCurveTo(rect.Min + ImVec2(0, tlry),
          rect.Min,
          rect.Min + ImVec2(tlrx, 0),
          IM_SEGMENTS(rounding[0]));
    }
    drawList->PathLineTo(ImVec2(rect.Max.x - trrx, rect.Min.y));
    if(trrx > 0 || trry > 0) {
      drawList->PathBezierCurveTo(ImVec2(rect.Max.x - trrx, rect.Min.y),
          ImVec2(rect.Max.x, rect.Min.y),
          ImVec2(rect.Max.x, rect.Min.y + trry),
          IM_SEGMENTS(rounding[1]));
    }
    drawList->PathLineTo(ImVec2(rect.Max.x, rect.Max.y - tbry));

    if(tbrx > 0 || tbry > 0) {
      drawList->PathBezierCurveTo(ImVec2(rect.Max.x, rect.Max.y - tbry),
          rect.Max,
          ImVec2(rect.Max.x - tbrx, rect.Max.y),
          IM_SEGMENTS(rounding[2]));
    }
    drawList->PathLineTo(ImVec2(rect.Min.x + tblx, rect.Max.y));

    if(tblx > 0 || tbly > 0) {
      drawList->PathBezierCurveTo(ImVec2(rect.Min.x + tblx, rect.Max.y),
          ImVec2(rect.Min.x, rect.Max.y),
          ImVec2(rect.Min.x, rect.Max.y - tbly),
          IM_SEGMENTS(rounding[3]));
    }
    drawList->PathFillConvex(bgCol);
  }

  void ComputedStyle::Decoration::render(ImRect rect)
  {
    ImVec2 s = rect.GetSize();
    if(s.x <= 0 && s.y <= 0) {
      return;
    }
    // TODO: this function definitely has room for improvement
    // 1. try to implement continuous flow for line when drawing using same color
    // 2. loops ?
    renderBackground(rect);
    rect.Min.x -= thickness[0];
    rect.Min.y -= thickness[1];
    rect.Max.x += thickness[2];
    rect.Max.y += thickness[3];

    ImVec2 corners[4] = {
      rect.Min,
      ImVec2(rect.Max.x, rect.Min.y),
      rect.Max,
      ImVec2(rect.Min.x, rect.Max.y)
    };

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 p1, p2, p3, p1i, p2i, p3i;
    int numSegments;

    if(rounding[0] > 0 && (thickness[0] > 0 || thickness[1] > 0)) {
      p1 = corners[0] + ImVec2(0, rounding[0]);
      p2 = corners[0];
      p3 = corners[0] + ImVec2(rounding[0], 0);

      numSegments = IM_SEGMENTS(rounding[0]);

      if(rounding[0] > thickness[0] && rounding[0] > thickness[1]) {
        p1i = corners[0] + ImVec2(thickness[0], rounding[0]);
        p2i = corners[0] + ImVec2(thickness[0], thickness[1]);
        p3i = corners[0] + ImVec2(rounding[0], thickness[1]);
        drawJoint(p1, p2, p3, p1i, p2i, p3i, col[0], col[1], numSegments);
      } else {
        ImVec2 center = corners[0] + ImVec2(ImMax(rounding[0], thickness[0]), ImMax(rounding[0], thickness[1]));
        ImVec2 startPoint = ImVec2(corners[0].x, center.y);
        ImVec2 endPoint = ImVec2(center.x, corners[0].y);

        drawJoint(p1, p2, p3, center, col[0], col[1], startPoint, endPoint, numSegments);
      }
    }

    if(rounding[1] > 0 && (thickness[1] > 0 || thickness[2] > 0)) {
      p1 = corners[1] - ImVec2(rounding[1], 0);
      p2 = corners[1];
      p3 = corners[1] + ImVec2(0, rounding[1]);

      numSegments = IM_SEGMENTS(rounding[1]);

      if(rounding[1] > thickness[1] && rounding[1] > thickness[2]) {
        p1i = corners[1] + ImVec2(-rounding[1], thickness[1]);
        p2i = corners[1] + ImVec2(-thickness[2], thickness[1]);
        p3i = corners[1] + ImVec2(-thickness[2], rounding[1]);
        drawJoint(p1, p2, p3, p1i, p2i, p3i, col[1], col[2], numSegments);
      } else {
        ImVec2 center = corners[1] + ImVec2(-ImMax(rounding[1], thickness[2]), ImMax(rounding[1], thickness[1]));
        ImVec2 startPoint = ImVec2(center.x, corners[1].y);
        ImVec2 endPoint = ImVec2(corners[1].x, center.y);

        drawJoint(p1, p2, p3, center, col[1], col[2], startPoint, endPoint, numSegments);
      }
    }

    if(rounding[2] > 0 && (thickness[2] > 0 || thickness[3] > 0)) {
      p1 = corners[2] - ImVec2(0, rounding[2]);
      p2 = corners[2];
      p3 = corners[2] - ImVec2(rounding[2], 0);

      numSegments = IM_SEGMENTS(rounding[2]);

      if(rounding[2] > thickness[2] && rounding[2] > thickness[3]) {
        p1i = corners[2] - ImVec2(thickness[2], rounding[2]);
        p2i = corners[2] - ImVec2(thickness[2], thickness[3]);
        p3i = corners[2] - ImVec2(rounding[2], thickness[3]);
        drawJoint(p1, p2, p3, p1i, p2i, p3i, col[2], col[3], numSegments);
      } else {
        ImVec2 center = corners[2] - ImVec2(ImMax(rounding[2], thickness[2]), ImMax(rounding[2], thickness[3]));
        ImVec2 startPoint = ImVec2(corners[2].x, center.y);
        ImVec2 endPoint = ImVec2(center.x, corners[2].y);

        drawJoint(p1, p2, p3, center, col[2], col[3], startPoint, endPoint, numSegments);
      }
    }

    if(rounding[3] > 0 && (thickness[3] > 0 || thickness[0] > 0)) {
      p1 = corners[3] + ImVec2(rounding[3], 0);
      p2 = corners[3];
      p3 = corners[3] - ImVec2(0, rounding[3]);

      numSegments = IM_SEGMENTS(rounding[3]);

      if(rounding[3] > thickness[3] && rounding[3] > thickness[0]) {
        p1i = corners[3] + ImVec2(rounding[3], -thickness[3]);
        p2i = corners[3] + ImVec2(thickness[0], -thickness[3]);
        p3i = corners[3] + ImVec2(thickness[0], -rounding[3]);
        drawJoint(p1, p2, p3, p1i, p2i, p3i, col[3], col[0], numSegments);
      } else {
        ImVec2 center = corners[3] + ImVec2(ImMax(rounding[3], thickness[0]), -ImMax(rounding[3], thickness[3]));
        ImVec2 startPoint = ImVec2(center.x, corners[3].y);
        ImVec2 endPoint = ImVec2(corners[3].x, center.y);

        drawJoint(p1, p2, p3, center, col[3], col[0], startPoint, endPoint, numSegments);
      }
    }

    if(thickness[0] > 0) {
      // left
      drawList->PathLineTo(corners[3] + ImVec2(0, -rounding[3] + 0.5f));
      drawList->PathLineTo(corners[0] + ImVec2(0, rounding[0] - 0.5f));
      drawList->PathLineTo(corners[0] + ImVec2(thickness[0], ImMax(rounding[0] - 0.5f, thickness[1] - 0.5f)));
      drawList->PathLineTo(corners[3] + ImVec2(thickness[0], -ImMax(rounding[3] - 0.5f, thickness[3] - 0.5f)));
      drawList->PathFillConvex(col[0]);
    }

    if(thickness[1] > 0) {
      // top
      drawList->PathLineTo(corners[0] + ImVec2(rounding[0] - 0.5f, 0));
      drawList->PathLineTo(corners[1] + ImVec2(-rounding[1] + 0.5f, 0));
      drawList->PathLineTo(corners[1] + ImVec2(-ImMax(rounding[1] - .5f, thickness[2] - 0.5f), thickness[1]));
      drawList->PathLineTo(corners[0] + ImVec2(ImMax(rounding[0] -.5f, thickness[0] - 0.5f), thickness[1]));
      drawList->PathFillConvex(col[1]);
    }

    if(thickness[2] > 0) {
      // right
      drawList->PathLineTo(corners[1] + ImVec2(0, rounding[1] - .5f));
      drawList->PathLineTo(corners[2] + ImVec2(0, -rounding[2] + .5f));
      drawList->PathLineTo(corners[2] + ImVec2(-thickness[2], -ImMax(rounding[2] - .5f, thickness[3] - 0.5f)));
      drawList->PathLineTo(corners[1] + ImVec2(-thickness[2], ImMax(rounding[1] - .5f, thickness[1])));
      drawList->PathFillConvex(col[2]);
    }

    if(thickness[3] > 0) {
      // bottom
      drawList->PathLineTo(corners[2] + ImVec2(-rounding[2] + .5f, 0));
      drawList->PathLineTo(corners[3] + ImVec2(rounding[3] - .5f, 0));
      drawList->PathLineTo(corners[3] + ImVec2(ImMax(rounding[3] - .5f, thickness[0]), -thickness[3]));
      drawList->PathLineTo(corners[2] + ImVec2(-ImMax(rounding[2] - .5f, thickness[2]), -thickness[3]));
      drawList->PathFillConvex(col[3]);
    }
  }

  ImGuiWindow* GetCurrentWindowNoDefault()
  {
    if(!ImGui::GetCurrentContext()) {
      return 0;
    }

    ImGuiWindow* window = ImGui::GetCurrentWindowRead();
    if(window && strcmp(window->Name, "Debug##Default") == 0) {
      return 0;
    }

    return window;
  }

  /* Table of function pointers for the LibCSS Select API. */
  static css_select_handler selectHandler = {
    CSS_SELECT_HANDLER_VERSION_1,

    node_name,
    node_classes,
    node_id,
    named_ancestor_node,
    named_parent_node,
    named_sibling_node,
    named_generic_sibling_node,
    parent_node,
    sibling_node,
    node_has_name,
    node_has_class,
    node_has_id,
    node_has_attribute,
    node_has_attribute_equal,
    node_has_attribute_dashmatch,
    node_has_attribute_includes,
    node_has_attribute_prefix,
    node_has_attribute_suffix,
    node_has_attribute_substring,
    node_is_root,
    node_count_siblings,
    node_is_empty,
    node_is_link,
    node_is_visited,
    node_is_hover,
    node_is_active,
    node_is_focus,
    node_is_enabled,
    node_is_disabled,
    node_is_checked,
    node_is_target,
    node_is_lang,
    node_presentational_hint,
    ua_default_for_property,
    compute_font_size,
    set_libcss_node_data,
    get_libcss_node_data
  };

  float parseUnits(css_fixed value, css_unit unit, ComputedStyle& style, ParseUnitsAxis axis)
  {
    return parseUnits(value, unit, style, style.contentRegion, axis);
  }

  //typedef float (*parseUnitsImpl)(css_fixed value, css_unit unit, ComputedStyle& style, const ImVec2& parentSize, ParseUnitsAxis axis);
  //static parseUnitsImpl unitParsers[CSS_UNIT_COUNT

  float parseUnits(css_fixed value, css_unit unit, ComputedStyle& style, const ImVec2& parentSize, ParseUnitsAxis axis) {

    float res = FIXTOFLT(value);

    ImVec2& scales = style.context->scale;

    float scale = axis == ParseUnitsAxis::Y ? scales.y : scales.x;
    float parentLength = axis == ParseUnitsAxis::Y ? parentSize.y : parentSize.x;
    Element* root = style.context->root;
    while(root->getParent() != 0 && root) {
      root = root->getParent()->context()->root;
    }

    switch(unit) {
      case CSS_UNIT_PCT:
        if(parentLength == 0) {
          return res;
        }

        res = parentLength * res / 100;
        break;

      case CSS_UNIT_EM:
        res = style.fontSize * res;
        break;

      case CSS_UNIT_REM:
        res = root->style()->fontSize * res;
        break;

      case CSS_UNIT_IN:
        res = DEFAULT_DPI * res * scale;
        break;

      case CSS_UNIT_CM:
        res = DEFAULT_DPI / 2.54 * res * scale;
        break;

      default:
        res *= scale;
        break;

    }

    return res;
  }

  bool setDimensions(ComputedStyle& cs)
  {
    bool changed = false;
    int width = 0;
    int height = 0;

    ImVec2 res(cs.element->size.x, cs.element->size.y);
    css_unit unit = CSS_UNIT_PX;

    uint8_t type = css_computed_width(
        cs.style->styles[CSS_PSEUDO_ELEMENT_NONE],
        &width, &unit
    );

    if(type == CSS_WIDTH_SET) {
      res.x = parseUnits(width, unit, cs, ParseUnitsAxis::X);
      changed = true;
    }

    type = css_computed_height(
        cs.style->styles[CSS_PSEUDO_ELEMENT_NONE],
        &height, &unit
    );

    if(type == CSS_HEIGHT_SET) {
      res.y = parseUnits(height, unit, cs, ParseUnitsAxis::Y);
      changed = true;
    }

    cs.element->setSize(res);
    return changed;
  }

  bool setPosition(ComputedStyle& cs)
  {
    bool changed = false;
    ImVec2 pos;
    ImVec2 offset(cs.element->padding[0], cs.element->padding[1]);
    ImVec2 parentSize = cs.contentRegion;
    uint16_t position = css_computed_position(cs.style->styles[CSS_PSEUDO_ELEMENT_NONE]);
    cs.position = position;
    ImGuiWindow* window = 0;
    Element* parent = cs.element->getParent();

    switch(position) {
      case CSS_POSITION_ABSOLUTE:
        window = GetCurrentWindowNoDefault();
        if(window) {
          if(parent && parent->style()->position == CSS_POSITION_RELATIVE) {
            pos = window->Pos + parent->pos;
          } else {
            pos = window->Pos;
          }
          break;
        }
      case CSS_POSITION_FIXED:
        parentSize = ImGui::GetIO().DisplaySize;
        break;
      case CSS_POSITION_RELATIVE:
        pos = cs.elementScreenPosition;
        break;
    }

    ImVec2 elementSize = cs.element->getSize();
    css_fixed x = 0;
    css_fixed y = 0;
    css_unit unit = CSS_UNIT_PX;
    uint16_t leftType = css_computed_left(
        cs.style->styles[CSS_PSEUDO_ELEMENT_NONE],
        &x, &unit);

    if(leftType == CSS_LEFT_SET) {
      offset.x = parseUnits(x, unit, cs, parentSize, ParseUnitsAxis::X);
      changed = true;
    }

    uint16_t topType = css_computed_top(
        cs.style->styles[CSS_PSEUDO_ELEMENT_NONE],
        &y, &unit);

    if(topType == CSS_TOP_SET) {
      offset.y = parseUnits(y, unit, cs, parentSize, ParseUnitsAxis::Y);
      changed = true;
    }

    css_fixed bottom = 0;
    css_fixed right = 0;
    ImVec2 size(cs.element->size.x, cs.element->size.y);
    if(css_computed_right(
        cs.style->styles[CSS_PSEUDO_ELEMENT_NONE],
        &right, &unit) == CSS_RIGHT_SET) {
      if(position == CSS_POSITION_RELATIVE) {
        offset.x -= parseUnits(x, unit, cs, parentSize, ParseUnitsAxis::X);
      } else {
        if(leftType == CSS_LEFT_SET) {
          size.x = parentSize.x - offset.x - parseUnits(right, unit, cs, parentSize, ParseUnitsAxis::X);
        } else {
          offset.x = parentSize.x - elementSize.x - parseUnits(right, unit, cs, parentSize, ParseUnitsAxis::X);
        }
      }
      changed = true;
    }

    if(css_computed_bottom(
        cs.style->styles[CSS_PSEUDO_ELEMENT_NONE],
        &bottom, &unit) == CSS_BOTTOM_SET) {

      if(position == CSS_POSITION_RELATIVE) {
        offset.y = -parseUnits(y, unit, cs, parentSize, ParseUnitsAxis::Y);
      } else {
        if(topType == CSS_TOP_SET) {
          size.y = parentSize.y - offset.y - parseUnits(bottom, unit, cs, parentSize, ParseUnitsAxis::Y);
        } else {
          offset.y = parentSize.y - elementSize.y - parseUnits(bottom, unit, cs, parentSize, ParseUnitsAxis::Y);
        }
      }

      changed = true;
    }

    if(cs.element->getFlags() & Element::WINDOW) {
      ImGui::SetNextWindowPos(pos + offset);
    } else {
      ImGui::SetCursorScreenPos(pos + offset);
    }
    cs.element->setSize(size);
    return changed;
  }

  bool setColor(ComputedStyle& cs)
  {
    css_color color;
    if(css_computed_color(
        cs.style->styles[CSS_PSEUDO_ELEMENT_NONE],
        &color) != CSS_COLOR_COLOR) {
      return false;
    }

    if(color == 0) {
      return false;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, parseColor(color));
    cs.nCol++;
    return true;
  }

  bool setBackgroundColor(ComputedStyle& cs)
  {
    css_color color;
    uint16_t type = css_computed_background_color(
        cs.style->styles[CSS_PSEUDO_ELEMENT_NONE],
        &color);

    if(color == 0 || type != CSS_BACKGROUND_COLOR_COLOR) {
      return false;
    }

    ImGuiCol col;
    if(cs.element->getFlags() & Element::WINDOW) {
      col = ImGuiCol_WindowBg;
    } else {
      col = ImGuiCol_ChildBg;
    }
    cs.element->bgColor = cs.decoration.bgCol = parseColor(color);
    ImGui::PushStyleColor(col, parseColor(color));
    cs.nCol++;
    return true;
  }

  struct SpacingFunc {
    uint8_t (*func)(const css_computed_style*, css_fixed*, css_unit*);
    ParseUnitsAxis axis;
  };

  bool setPadding(ComputedStyle& cs)
  {
    css_fixed value = 0;
    css_unit unit = CSS_UNIT_PX;

    SpacingFunc funcs[4] = {
      SpacingFunc{css_computed_padding_left, ParseUnitsAxis::X},
      SpacingFunc{css_computed_padding_top, ParseUnitsAxis::Y},
      SpacingFunc{css_computed_padding_right, ParseUnitsAxis::X},
      SpacingFunc{css_computed_padding_bottom, ParseUnitsAxis::Y}
    };

    bool defined = false;

    for(size_t i = 0; i < 4; i++) {
      if(funcs[i].func(cs.style->styles[CSS_PSEUDO_ELEMENT_NONE], &value, &unit) == CSS_PADDING_SET)
      {
        float res = parseUnits(value, unit, cs, funcs[i].axis);
        cs.element->padding[i] = res;
        defined = true;
      }
    }

    if(defined) {
      ImGuiStyle& style = ImGui::GetStyle();
      ImVec2 windowPadding = style.WindowPadding;
      ImVec2 framePadding = style.FramePadding;

      if(cs.element->padding[0] >= 0.0f) {
        windowPadding.x = cs.element->padding[0];
        framePadding.x = cs.element->padding[0];
      }

      if(cs.element->padding[1] >= 0.0f) {
        windowPadding.y = cs.element->padding[1];
        framePadding.y = cs.element->padding[1];
      }

      if(cs.element->getFlags() & Element::WINDOW) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, windowPadding);
      } else {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, framePadding);
      }
      ++cs.nStyle;
    }
    return defined;
  }

  bool setMargins(ComputedStyle& cs)
  {
    css_fixed value = 0;
    css_unit unit = CSS_UNIT_PX;

    SpacingFunc funcs[4] = {
      SpacingFunc{css_computed_margin_top, ParseUnitsAxis::Y},
      SpacingFunc{css_computed_margin_left, ParseUnitsAxis::X},
      SpacingFunc{css_computed_margin_right, ParseUnitsAxis::X},
      SpacingFunc{css_computed_margin_bottom, ParseUnitsAxis::Y}
    };

    bool defined = false;

    for(size_t i = 0; i < 4; i++) {
      if(funcs[i].func(cs.style->styles[CSS_PSEUDO_ELEMENT_NONE], &value, &unit) == CSS_MARGIN_SET)
      {
        float res = parseUnits(value, unit, cs, funcs[i].axis);
        cs.element->margins[i] = res;
        defined = true;
      } else {
        cs.element->margins[i] = FLT_MIN;
      }
    }

    return defined;
  }

  bool setFont(ComputedStyle& cs)
  {
    const char* fontName = cs.fontName;

    cs.fontScale = ImGui::GetIO().FontGlobalScale;
    int fonts = 0;

    css_fixed fs;
    css_unit unit;
    uint8_t code = css_computed_font_size(cs.style->styles[CSS_PSEUDO_ELEMENT_NONE], &fs, &unit);

    float size, defaultFontSize;
    size = defaultFontSize = ImGui::GetFont()->FontSize;

    if(fontName) {
      defaultFontSize = cs.element->context()->fontManager->getFont(fontName).size;
    }

    Element* parent = cs.element->getParent();
    if(parent) {
      cs.fontSize = parent->style()->fontSize;
    } else {
      cs.fontSize = defaultFontSize;
    }

    if(code == CSS_FONT_SIZE_INHERIT) {
      float inherited = parent ? parent->style()->fontSize : 0;
      if(inherited > 0) {
        size = inherited;
      }
    } else {
      size = parseUnits(fs, unit, cs, ParseUnitsAxis::Y);
    }

    if(size == 0) {
      return false;
    }

    float expectedScale = size / defaultFontSize;
    bool changed = expectedScale != ImGui::GetIO().FontGlobalScale;
    if(changed) {
      ImGui::GetIO().FontGlobalScale = expectedScale;
    }

    if(fontName) {
      if(cs.element->context()->fontManager->pushFont(fontName)) {
        fonts = 1;
      }
    }

    if(fonts == 0 && changed) {
      ImGui::PushFont(ImGui::GetFont());
      fonts = 1;
    }

    cs.fontSize = size;
    cs.nFonts += fonts;
    return true;
  }

  bool setBorderRadius(ComputedStyle& cs)
  {
    css_fixed value;
    css_unit unit;
    bool changed = false;
    uint8_t (*funcs[4])(const css_computed_style*, css_fixed*, css_unit*) = {
      css_computed_border_radius_top_left,
      css_computed_border_radius_top_right,
      css_computed_border_radius_bottom_right,
      css_computed_border_radius_bottom_left
    };

    for(size_t i = 0; i < 4; i++) {
      if(funcs[i](cs.style->styles[CSS_PSEUDO_ELEMENT_NONE], &value, &unit) == CSS_BORDER_RADIUS_SET)
      {
        cs.decoration.rounding[i] = parseUnits(value, unit, cs, ParseUnitsAxis::X);
        changed = true;
      } else {
        cs.decoration.rounding[i] = 0.0f;
      }
    }

    cs.borderRadius.x = cs.decoration.rounding[0];
    cs.borderRadius.y = cs.decoration.rounding[1];
    cs.borderRadius.z = cs.decoration.rounding[2];
    cs.borderRadius.w = cs.decoration.rounding[3];
    return changed;
  }

  bool setBorder(ComputedStyle& cs)
  {
    bool changed = false;
    uint8_t (*styles[4])(const css_computed_style*) = {
      css_computed_border_left_style,
      css_computed_border_top_style,
      css_computed_border_right_style,
      css_computed_border_bottom_style
    };

    uint8_t (*widths[4])(const css_computed_style*, css_fixed*, css_unit*) = {
      css_computed_border_left_width,
      css_computed_border_top_width,
      css_computed_border_right_width,
      css_computed_border_bottom_width
    };

    uint8_t (*colors[4])(const css_computed_style*, css_color*) = {
      css_computed_border_left_color,
      css_computed_border_top_color,
      css_computed_border_right_color,
      css_computed_border_bottom_color
    };

    for(size_t i = 0; i < 4; i++) {
      uint8_t type = styles[i](cs.style->styles[CSS_PSEUDO_ELEMENT_NONE]);
      if(type == CSS_BORDER_STYLE_NONE || type == CSS_BORDER_STYLE_INHERIT) {
        continue;
      }

      css_fixed value;
      css_unit unit;
      css_color col;
      widths[i](cs.style->styles[CSS_PSEUDO_ELEMENT_NONE], &value, &unit);
      colors[i](cs.style->styles[CSS_PSEUDO_ELEMENT_NONE], &col);

      cs.decoration.thickness[i] = ImMax(1.0f, parseUnits(value, unit, cs, ParseUnitsAxis::X));
      cs.decoration.col[i] = parseColor(col);
      changed = true;
      int marginIndex = i < 2 ? (i + 1) % 2 : i;
      float m = cs.element->margins[marginIndex];
      cs.element->margins[marginIndex] = m != FLT_MIN ? m + cs.decoration.thickness[i] : cs.decoration.thickness[i];
    }

    return changed;
  }

  ComputedStyle::ComputedStyle(Element* target)
    : libcssData(0)
    , fontName(0)
    , fontSize(0)
    , element(target)
    , style(0)
    , context(0)
    , nCol(0)
    , nStyle(0)
    , nFonts(0)
    , fontScale(0)
    , mSelectCtx(0)
    , mWidthMode(CSS_WIDTH_AUTO)
    , mHeightMode(CSS_HEIGHT_AUTO)
    , mInlineStyle(0)
    , mAutoSize(false)
  {
    memset(&decoration, 0, sizeof(Decoration));
  }

  ComputedStyle::~ComputedStyle()
  {
    if(fontName) {
      ImGui::MemFree(fontName);
    }
    if(mInlineStyle) {
      css_stylesheet_destroy(mInlineStyle);
    }
    cleanupClassCache();
    destroy();
  }

  bool ComputedStyle::compute(Element* e)
  {
    element = e;
    context = e->context();
    css_error code;

    css_media media;
    memset(&media, 0, sizeof(css_media));
    media.type = CSS_MEDIA_SCREEN;
    if(mSelectCtx) {
      css_select_ctx_destroy(mSelectCtx);
    }

    mSelectCtx = element->context()->style->select();
    if(!mSelectCtx) {
      return false;
    }

    css_select_results* newStyle = 0;
		code = css_select_style(mSelectCtx, element,
				&media, mInlineStyle,
				&selectHandler,
        &selectHandler,
				&newStyle
    );

    if(style && memcmp(newStyle, style, sizeof(css_select_results)) == 0) {
      if(css_select_results_destroy(newStyle) != CSS_OK) {
        IMVUE_EXCEPTION(StyleError, "failed to destroy tmp style %s", css_error_to_string(code));
      }
      return false;
    }

    if(style) {
      if(css_select_results_destroy(style) != CSS_OK) {
        IMVUE_EXCEPTION(StyleError, "failed to destroy style %s", css_error_to_string(code));
      }
    }

    style = newStyle;

    if (code != CSS_OK) {
      IMVUE_EXCEPTION(StyleError, "failed to select style %s", css_error_to_string(code));
      return false;
    }

    mStyleCallbacks.clear();

    position = CSS_POSITION_INHERIT;
    uint16_t display = css_computed_display(style->styles[CSS_PSEUDO_ELEMENT_NONE], false);

    // fonts should go first as line height might affect units parser
    initFonts(&media);

    for(size_t i = 0; i < 4; i++) {
      element->padding[i] = -1.0f;
      element->margins[i] = FLT_MIN;
    }

    // dimensions
    {
      css_fixed width = 0;
      css_fixed height = 0;
      css_unit unit = CSS_UNIT_PX;
      mWidthMode = css_computed_width(
          style->styles[CSS_PSEUDO_ELEMENT_NONE],
          &width, &unit
      );

      mHeightMode = css_computed_height(
          style->styles[CSS_PSEUDO_ELEMENT_NONE],
          &height, &unit
      );

      bool shouldSetDimensions = mWidthMode == CSS_WIDTH_SET || mHeightMode == CSS_HEIGHT_SET;

      if(shouldSetDimensions && display != CSS_DISPLAY_INLINE)
        mStyleCallbacks.push_back(setDimensions);

      if(shouldSetDimensions) {
        element->size = ImVec2(0.f, 0.f);
        element->invalidate("size");
      }
    }

    // positioning
    {
      position = css_computed_position(style->styles[CSS_PSEUDO_ELEMENT_NONE]);

      switch(position) {
        case CSS_POSITION_FIXED:
        case CSS_POSITION_ABSOLUTE:
          display = CSS_DISPLAY_BLOCK;
        case CSS_POSITION_RELATIVE:
          mStyleCallbacks.push_back(setPosition);
          mAutoSize = false;
          break;
        default:
          mAutoSize = true;
      }
    }

    // color
    if(setColor(*this)) {
      mStyleCallbacks.push_back(setColor);
    }

    // background color
    if(setBackgroundColor(*this)) {
      mStyleCallbacks.push_back(setBackgroundColor);
    }

    // padding
    if(display != CSS_DISPLAY_INLINE) {
      if(setPadding(*this)) {
        mStyleCallbacks.push_back(setPadding);
      }
    }

    // margins
    {
      if(setMargins(*this)) {
        mStyleCallbacks.push_back(setMargins);
      }
    }

    // border radius
    if(setBorderRadius(*this)) {
      mStyleCallbacks.push_back(setBorderRadius);
    }

    // border styles
    if(setBorder(*this)) {
      mStyleCallbacks.push_back(setBorder);
    }

    element->display = css_computed_display(style->styles[CSS_PSEUDO_ELEMENT_NONE], false);
    end();

    return true;
  }

  void ComputedStyle::begin(Element* e)
  {
    element = e;
    if(!style) {
      return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindow* window = GetCurrentWindowNoDefault();
    if(window){
      Element* parent = e->getParent();
      while(parent && parent->display == CSS_DISPLAY_INLINE) {
        parent = parent->getParent();
      }

      if(parent && (parent->getFlags() & Element::WINDOW) == 0){
        contentRegion = parent->getSize() - ImVec2(parent->padding[0] + parent->padding[2], parent->padding[1] + parent->padding[3]);
      } else {
        contentRegion = ImGui::GetWindowSize() - ImGui::GetCursorStartPos() - window->Scroll - ImGui::GetStyle().WindowPadding;
      }
    } else {
      contentRegion = io.DisplaySize;
    }

    elementScreenPosition = ImGui::GetCursorScreenPos();

    for(int i = 0; i < mStyleCallbacks.size(); i++)
    {
      mStyleCallbacks[i](*this);
    }

    if(element->display == CSS_DISPLAY_BLOCK && mAutoSize) {
      if(mWidthMode == CSS_WIDTH_AUTO) {
        ImRect margins = element->getMarginsRect();
        element->size.x = contentRegion.x - margins.Max.x - margins.Min.x;
      }
    }
  }

  void ComputedStyle::end()
  {
    ImGui::PopStyleColor(nCol);
    ImGui::PopStyleVar(nStyle);
    if(fontScale) {
      ImGui::GetIO().FontGlobalScale = fontScale;
    }
    while(nFonts-- > 0) {
      ImGui::PopFont();
    }
    nCol = 0;
    nStyle = 0;
    nFonts = 0;
    fontScale = 0;
  }

  void ComputedStyle::destroy()
  {
    end();
    if (libcssData != 0) {
      css_libcss_node_data_handler(&selectHandler, CSS_NODE_DELETED,
          NULL, element, NULL, libcssData);
      libcssData = 0;
    }

    if(!style) {
      return;
    }

    if(mSelectCtx) {
      css_select_ctx_destroy(mSelectCtx);
    }

    css_error code = css_select_results_destroy(style);
    style = 0;
    if (code != CSS_OK) {
      IMVUE_EXCEPTION(StyleError, "failed to destroy style %s", css_error_to_string(code));
    }
  }

  void ComputedStyle::updateClassCache()
  {
    cleanupClassCache();
    for(Element::Classes::iterator iter = element->classes.begin(); iter != element->classes.end(); ++iter) {
      lwc_string* str = 0;
      if(lwc_intern_string(iter->first, strlen(iter->first), &str) != lwc_error_ok) {
        continue;
      }

      mClasses.push_back(str);
    }
  }

  void ComputedStyle::setInlineStyle(const char* style)
  {
    if(mInlineStyle) {
      css_stylesheet_destroy(mInlineStyle);
      mInlineStyle = 0;
    }

    if(style[0] == '\0') {
      return;
    }

    element->context()->style->parse(style, &mInlineStyle, true);
  }

  ImVector<lwc_string*>& ComputedStyle::getClasses()
  {
    return mClasses;
  }

  void ComputedStyle::initFonts(css_media* media)
  {
    lwc_string** fontNames = NULL;
    css_computed_font_family(style->styles[CSS_PSEUDO_ELEMENT_NONE], &fontNames);
    css_error code;

    if(fontNames) {
      for(int i = 0;;i++) {
        if(!fontNames[i]) {
          break;
        }

        const char* data = lwc_string_data(fontNames[i]);
        if(fontName && strcmp(fontName, data) == 0)
          break;

        css_select_font_faces_results* fontFaces = NULL;
        css_select_font_faces(mSelectCtx,
            media, fontNames[i],
            &fontFaces);

        if(fontFaces && fontFaces->n_font_faces != 0) {
          for(uint32_t j = 0; j < fontFaces->n_font_faces; j++) {
            css_font_face* fontFace = fontFaces->font_faces[j];
            uint32_t count = 0;
            code = css_font_face_count_srcs(fontFace, &count);
            if(code != CSS_OK) {
              continue;
            }

            uint32_t rangeCount = 0;
            code = css_font_face_count_ranges(fontFace, &rangeCount);

            if(code != CSS_OK) {
              continue;
            }

            for(uint32_t k = 0; k < count; k++) {
              const css_font_face_src* src = NULL;
              code = css_font_face_get_src(fontFace, k, &src);
              if(code != CSS_OK) {
                IMVUE_EXCEPTION(StyleError, "failed to get font src %s", css_error_to_string(code));
                continue;
              }

              css_font_face_location_type location = css_font_face_src_location_type(src);
              css_font_face_format fmt = css_font_face_src_format(src);

              // currently we support only ttf and otf
              // unspecified falls back to otf/ttf
              if(fmt != CSS_FONT_FACE_FORMAT_OPENTYPE && fmt != CSS_FONT_FACE_FORMAT_UNSPECIFIED) {
                IMVUE_EXCEPTION(StyleError, "unsupported font format %d", fmt);
                continue;
              }

              // only local location is supported
              if(location == CSS_FONT_FACE_LOCATION_TYPE_URI) {
                IMVUE_EXCEPTION(StyleError, "uri location is not supported");
                continue;
              }

              lwc_string* path = NULL;
              code = css_font_face_src_get_location(src, &path);
              if(code != CSS_OK) {
                IMVUE_EXCEPTION(StyleError, "failed to get font src location %s", css_error_to_string(code));
                continue;
              }

              if(fontName != 0) {
                ImGui::MemFree(fontName);
                fontName = 0;
              }

              // read ranges
              const css_unicode_range* range = NULL;
              ImVector<ImWchar> glyphRanges;
              int index = 0;
              if(rangeCount > 0) {
                glyphRanges.resize(rangeCount * 2 + 1);
                for(uint8_t l = 0; l < rangeCount; ++l) {
                  code = css_font_face_get_range(fontFace, l, &range);
                  glyphRanges[index++] = (ImWchar)(*range)[0];
                  glyphRanges[index++] = (ImWchar)(*range)[1];
                }
              }

              // finally load font using specified path
              if(element->context()->fontManager->loadFontFromFile(data, lwc_string_data(path), glyphRanges)) {
                fontName = ImStrdup(data);
              } else {
                IMVUE_EXCEPTION(StyleError, "failed to load font %s", data);
              }
            }
          }

          css_select_font_faces_results_destroy(fontFaces);
        }
      }
    }

    css_fixed fs;
    css_unit unit;
    uint8_t fstype = css_computed_font_size(style->styles[CSS_PSEUDO_ELEMENT_NONE], &fs, &unit);

    if(fontName || fstype != CSS_FONT_SIZE_INHERIT) {
      fontSize = parseUnits(fs, unit, *this, ParseUnitsAxis::Y);
      mStyleCallbacks.push_back(setFont);
    }

    if(fstype == CSS_FONT_SIZE_INHERIT) {
      Element* parent = element->getParent();
      float defaultFontSize = ImGui::GetFont() ? ImGui::GetFont()->FontSize : 0.0f;
      fontSize = parent ? parent->style()->fontSize : defaultFontSize;
    }
  }

  void ComputedStyle::cleanupClassCache()
  {
    while(mClasses.size() > 0) {
      lwc_string_unref(mClasses[mClasses.size() - 1]);
      mClasses.pop_back();
    }
  }

  const char* cssBase = "* {"
    "display: block;"
  "}\n"
  "html {"
    "width: 100%;"
    "height: 100%;"
  "}"
  "body {"
    "width: 100%;"
    "height: 100%;"
  "}\n"
  "h1, h2, h3, h4, h5, h6 {"
    "font-weight: bold;"
  "}\n"
  "h1 { font-size: 2em; margin: 0.67em 0 }\n"
  "h2 { font-size: 1.5em; margin: 0.75em 0 }\n"
  "h3 { font-size: 1.17em; margin: 0.83em 0 }\n"
  "h4 { font-size: 1em; margin: 1.33em 0 }\n"
  "h5 { font-size: 0.83em; margin: 1.67em 0 }\n"
  "h6 { font-size: 0.67em; margin: 2.33em 0 }\n"
  // define inline elements
  "a, abbr, acronym, b, bdi, bdo, big, br, button, cite, code, "
  "data, datalist, del, dfn, em, embed, i, iframe, img, input, "
  "ins, kbd, label, map, mark, meter, noscript, object, output, "
  "picture, progress, q, ruby, s, samp, script, select, "
  "select, small, span, strong, sub, sup, svg, textarea, time, u, tt, var, wbr"
  // imgui items that should be displayed inline
  ", menu, same-line, tab-item"
  "{"
    "display: inline;"
  "}"
  // imgui items that are actually positioned absolute
  "tooltip, popup-modal, indent, unindent, next-column {"
    "position: fixed;"
  "}";


  Style::Style(Style* parent)
    : mBase(0)
    , mParent(parent)
  {
    parse(cssBase, &mBase);
  }

  Style::~Style()
  {
    while(mSheets.size() > 0) {
      css_stylesheet_destroy(mSheets[mSheets.size() - 1].sheet);
      mSheets.pop_back();
    }

    if(mBase) {
      css_stylesheet_destroy(mBase);
    }
  }

  css_select_ctx* Style::select()
  {
    css_select_ctx* ctx = 0;
    css_error code = css_select_ctx_create(&ctx);
    if (code != CSS_OK) {
      IMVUE_EXCEPTION(StyleError, "failed to create select context %s", css_error_to_string(code));
      return 0;
    }

    code = css_select_ctx_append_sheet(ctx, mBase, CSS_ORIGIN_AUTHOR,
        NULL);
    if (code != CSS_OK)
      IMVUE_EXCEPTION(StyleError, "failed to append base stylesheet: %s", css_error_to_string(code));

    if(mParent)
      mParent->appendSheets(ctx, false);
    appendSheets(ctx, true);
    return ctx;
  }

  void Style::load(const char* data, bool scoped)
  {
    css_stylesheet* sheet = NULL;
    parse(data, &sheet);
    if(sheet) {
      mSheets.push_back(
        Sheet{
          sheet,
          scoped
      });
    }
  }

  void Style::appendSheets(css_select_ctx* ctx, bool scoped)
  {
    for(int i = 0; i < mSheets.size(); ++i) {
      if(mSheets[i].scoped && !scoped) {
        continue;
      }

      css_error code = css_select_ctx_append_sheet(ctx, mSheets[i].sheet, CSS_ORIGIN_AUTHOR, NULL);
      if(code != CSS_OK)
      {
        IMVUE_EXCEPTION(StyleError, "failed to append stylesheet: %s", css_error_to_string(code));
      }
    }
  }

  void Style::parse(const char* data, css_stylesheet** dest, bool isInline)
  {
    css_stylesheet* sheet = *dest;
    css_error code;
    if(sheet) {
      code = css_stylesheet_destroy(sheet);
      if(code != CSS_OK) {
        IMVUE_EXCEPTION(StyleError, "failed to destroy stylesheet: %s", css_error_to_string(code));
      }
    }

    css_stylesheet_params params;
    params.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
    params.level = CSS_LEVEL_21;
    params.charset = "UTF-8";
    params.url = "-";
    params.title = "-";
    params.allow_quirks = false;
    params.inline_style = isInline;
    params.resolve = resolve_url;
    params.resolve_pw = NULL;
    params.import = NULL;
    params.import_pw = NULL;
    params.color = NULL;
    params.color_pw = NULL;
    params.font = resolve_font;
    params.font_pw = NULL;
    code = css_stylesheet_create(&params, &sheet);
    if (code != CSS_OK)
      IMVUE_EXCEPTION(StyleError, "failed to create stylesheet: %s", css_error_to_string(code));
    code = css_stylesheet_append_data(sheet, (const uint8_t *) data,
        strlen(data));
    if (code != CSS_OK && code != CSS_NEEDDATA)
      IMVUE_EXCEPTION(StyleError, "css_stylesheet_append_data failed: %s", css_error_to_string(code));
    code = css_stylesheet_data_done(sheet);
    if (code != CSS_OK)
      IMVUE_EXCEPTION(StyleError, "css_stylesheet_data_done failed: %s", css_error_to_string(code));

    *dest = sheet;
  }
}
