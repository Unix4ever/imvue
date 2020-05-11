#ifndef __IMVUE_SVG__
#define __IMVUE_SVG__

#include "imvue_element.h"
#include "nanosvg.h"
#include "nanosvgrast.h"
#include "imvue_errors.h"

#include <cstdlib>
#include <iostream>
#include <ctime>
#ifdef _WIN32
#include <algorithm>
#endif

namespace ImVue {

  /*
// Written by Mark Bayazit (darkzerox)------------------------------------------------------------------------------------------------------------------
// March 23, 2009---------------------------------------------------------------------------------------------------------------------------------------
  namespace bayazit {
    inline static bool eq(const float &a, float const &b) {return fabs(a - b) <= 1e-8;}
    //inline static float min(const float &a, const float &b) {return a < b ? a : b;}
    inline static int wrap(const int &a, const int &b) {return a < 0 ? a % b + b : a % b;}
    inline static const ImVec2& at(const ImVector<ImVec2>& v, int i) {return v[wrap(i, v.size())];};
    inline static float area(const ImVec2 &a, const ImVec2 &b, const ImVec2 &c) {return (((b.x - a.x)*(c.y - a.y))-((c.x - a.x)*(b.y - a.y)));}
    inline static bool left(const ImVec2 &a, const ImVec2 &b, const ImVec2 &c) {return area(a, b, c) > 0;}
    inline static bool leftOn(const ImVec2 &a, const ImVec2 &b, const ImVec2 &c) {return area(a, b, c) >= 0;}
    inline static bool right(const ImVec2 &a, const ImVec2 &b, const ImVec2 &c) {return area(a, b, c) < 0;}
    inline static bool rightOn(const ImVec2 &a, const ImVec2 &b, const ImVec2 &c) {return area(a, b, c) <= 0;}
    inline static bool collinear(const ImVec2 &a, const ImVec2 &b, const ImVec2 &c) {return area(a, b, c) == 0;}
    inline static float sqdist(const ImVec2 &a, const ImVec2 &b) {
      float dx = b.x - a.x;
      float dy = b.y - a.y;
      return dx * dx + dy * dy;
    }
    inline static bool isReflex(const ImVector<ImVec2> &poly, const int &i) {return right(at(poly, i - 1), at(poly, i), at(poly, i + 1));}
    inline static ImVec2 intersection(const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &q1, const ImVec2 &q2) {
      ImVec2 i;
      float a1, b1, c1, a2, b2, c2, det;
      a1 = p2.y - p1.y;
      b1 = p1.x - p2.x;
      c1 = a1 * p1.x + b1 * p1.y;
      a2 = q2.y - q1.y;
      b2 = q1.x - q2.x;
      c2 = a2 * q1.x + b2 * q1.y;
      det = a1 * b2 - a2*b1;
      if (!eq(det, 0)) { // lines are not parallel
        i.x = (b2 * c1 - b1 * c2) / det;
        i.y = (a1 * c2 - a2 * c1) / det;
      }
      return i;
    }
    inline static void swap(int &a, int &b) {int c;c = a;a = b;b = c;}
    inline static void append(ImVector<ImVec2>& src,const ImVector<ImVec2>& dst,int dstStartIndex,int dstEndIndex)    {
      //e.g. lowerPoly.insert(lowerPoly.end(), poly.begin(), poly.begin() + upperIndex + 1);
      src.reserve(src.size()+dstEndIndex-dstStartIndex+1);
      for (int i=dstStartIndex;i<dstEndIndex;i++)
        src.push_back(dst[i]);
    }

    void DecomposeConcavePoly(const ImVector<ImVec2>& poly, ImVector<ImVec2>& convexPolysOut, ImVector<int>& numConvexPolyPointsOut) {
      ImVec2 upperInt, lowerInt, p, closestVert;
      float upperDist, lowerDist, d, closestDist;
      int upperIndex, lowerIndex, closestIndex;
      ImVector<ImVec2> lowerPoly, upperPoly;

      for (size_t i = 0; i < poly.size(); ++i) {
        if (isReflex(poly, i)) {
          //reflexVertices.push_back(poly[i]);
          upperDist = lowerDist = FLT_MAX;
          for (size_t j = 0; j < poly.size(); ++j) {
            if (left(at(poly, i - 1), at(poly, i), at(poly, j))
                && rightOn(at(poly, i - 1), at(poly, i), at(poly, j - 1))) { // if line intersects with an edge
              p = intersection(at(poly, i - 1), at(poly, i), at(poly, j), at(poly, j - 1)); // find the point of intersection
              if (right(at(poly, i + 1), at(poly, i), p)) { // make sure it's inside the poly
                d = sqdist(poly[i], p);
                if (d < lowerDist) { // keep only the closest intersection
                  lowerDist = d;
                  lowerInt = p;
                  lowerIndex = j;
                }
              }
            }
            if (left(at(poly, i + 1), at(poly, i), at(poly, j + 1))
                && rightOn(at(poly, i + 1), at(poly, i), at(poly, j))) {
              p = intersection(at(poly, i + 1), at(poly, i), at(poly, j), at(poly, j + 1));
              if (left(at(poly, i - 1), at(poly, i), p)) {
                d = sqdist(poly[i], p);
                if (d < upperDist) {
                  upperDist = d;
                  upperInt = p;
                  upperIndex = j;
                }
              }
            }
          }

          // if there are no vertices to connect to, choose a point in the middle
          if (lowerIndex == (upperIndex + 1) % poly.size()) {
            p.x = (lowerInt.x + upperInt.x) / 2;
            p.y = (lowerInt.y + upperInt.y) / 2;
            //steinerPoints.push_back(p);

            if (i < upperIndex) {
              //lowerPoly.insert(lowerPoly.end(), poly.begin() + i, poly.begin() + upperIndex + 1);   // converted to:
              append(lowerPoly,poly,i,upperIndex + 1);
              lowerPoly.push_back(p);
              upperPoly.push_back(p);
              if (lowerIndex != 0) {
                //upperPoly.insert(upperPoly.end(), poly.begin() + lowerIndex, poly.end());   // converted to:
                append(upperPoly,poly,lowerIndex,poly.size());
              }
              //upperPoly.insert(upperPoly.end(), poly.begin(), poly.begin() + i + 1);   // converted to:
              append(upperPoly,poly,0,i + 1);
            } else {
              if (i != 0) {
                //lowerPoly.insert(lowerPoly.end(), poly.begin() + i, poly.end());   // converted to:
                append(lowerPoly,poly,i,poly.size());
              }
              //lowerPoly.insert(lowerPoly.end(), poly.begin(), poly.begin() + upperIndex + 1);   // converted to:
              append(lowerPoly,poly,0,upperIndex + 1);
              lowerPoly.push_back(p);
              upperPoly.push_back(p);
              //upperPoly.insert(upperPoly.end(), poly.begin() + lowerIndex, poly.begin() + i + 1);   // converted to:
              append(upperPoly,poly,lowerIndex,i + 1);
            }
          } else {
            // connect to the closest point within the triangle

            if (lowerIndex > upperIndex) {
              upperIndex += poly.size();
            }
            closestDist = FLT_MAX;
            for (int j = lowerIndex; j <= upperIndex; ++j) {
              if (leftOn(at(poly, i - 1), at(poly, i), at(poly, j))
                  && rightOn(at(poly, i + 1), at(poly, i), at(poly, j))) {
                d = sqdist(at(poly, i), at(poly, j));
                if (d < closestDist) {
                  closestDist = d;
                  closestVert = at(poly, j);
                  closestIndex = j % poly.size();
                }
              }
            }

            if (i < closestIndex) {
              //lowerPoly.insert(lowerPoly.end(), poly.begin() + i, poly.begin() + closestIndex + 1);
              append(lowerPoly,poly,i,closestIndex + 1);
              if (closestIndex != 0) {
                //upperPoly.insert(upperPoly.end(), poly.begin() + closestIndex, poly.end());
                append(upperPoly,poly,closestIndex,poly.size());
              }
              //upperPoly.insert(upperPoly.end(), poly.begin(), poly.begin() + i + 1);
              append(upperPoly,poly,0,i + 1);
            } else {
              if (i != 0) {
                //lowerPoly.insert(lowerPoly.end(), poly.begin() + i, poly.end());
                append(lowerPoly,poly,i,poly.size());
              }
              //lowerPoly.insert(lowerPoly.end(), poly.begin(), poly.begin() + closestIndex + 1);
              append(lowerPoly,poly,0,closestIndex + 1);
              //upperPoly.insert(upperPoly.end(), poly.begin() + closestIndex, poly.begin() + i + 1);
              append(upperPoly,poly,closestIndex,i + 1);
            }
          }

          // solve smallest poly first
          if (lowerPoly.size() < upperPoly.size()) {
            DecomposeConcavePoly(lowerPoly, convexPolysOut, numConvexPolyPointsOut);
            DecomposeConcavePoly(upperPoly, convexPolysOut, numConvexPolyPointsOut);
          } else {
            DecomposeConcavePoly(upperPoly, convexPolysOut, numConvexPolyPointsOut);
            DecomposeConcavePoly(lowerPoly, convexPolysOut, numConvexPolyPointsOut);
          }
          return;
        }
      }
      numConvexPolyPointsOut.push_back(poly.size());
      append(convexPolysOut,poly,0,poly.size());
    }
  } // namespace bayazit

  // End code Written by Mark Bayazit (darkzerox)---------------------------------------------------------------------------------------------------------
  // March 23, 2009---------------------------------------------------------------------------------------------------------------------------------------
*/
  /**
   * Utilise nanosvg to draw vector image
   */
  class SvgImage : public Element {

    public:
      struct Shape {
        NSVGshape* nsvgShape;
        ImVector<ImVec2> path;
      };

      SvgImage()
        : src(NULL)
        , size(0,0)
        , tint_col(ImVec4(1,1,1,1))
        , border_col(ImVec4(0,0,0,0))
        , c(0)
        , mTextureID(NULL)
        , mData(NULL)
        , mImage(NULL)
        , mRasterizer(NULL)
      {
        mRasterizer = nsvgCreateRasterizer();
      }

      virtual ~SvgImage()
      {
        if(src) {
          ImGui::MemFree(src);
        }

        if(mImage) {
          nsvgDelete(mImage);
        }

        if(mRasterizer) {
          nsvgDeleteRasterizer(mRasterizer);
        }

        if(mData) {
          if(mTextureID) {
            mTextureManager->deleteTexture(mTextureID);
          }
          free(mData);
        }
      }

      bool build()
      {
        if(!mTextureManager) {
          IMVUE_EXCEPTION(ElementError, "Image requires texture manager for rendering");
          return false;
        }

        if(!Element::build()) {
          return false;
        }

        // TODO: determine dpi?
        mImage = nsvgParseFromFile(src, "px", 96.0f);
        if(!mImage) {
          IMVUE_EXCEPTION(ElementError, "Failed to read image");
          return false;
        }
        mShapes.reserve(0);

        //NSVGshape* shape = NULL;
        //NSVGpath* path = NULL;

        // read and decompose shapes to convex polygons
        /*for(shape = mImage->shapes; shape != NULL; shape = shape->next) {
          for(path = shape->paths; path != NULL; path = path->next) {
            ImVector<ImVec2> poly;
            for(int i = 0; i < path->npts-1; i += 3) {
              float* p = &path->pts[i * 2];
              if(i == 0) {
                poly.reserve(poly.size() + 4);
                poly.push_back(ImVec2(p[0], p[1]));
              } else {
                poly.reserve(poly.size() + 3);
              }
              poly.push_back(ImVec2(p[2], p[3]));
              poly.push_back(ImVec2(p[4], p[5]));
              poly.push_back(ImVec2(p[6], p[7]));
              std::cout << "New Poly size " << poly.size() << "\n";
            }
            mShapes.reserve(mShapes.size() + 1);
            mShapes.push_back(Shape{
                NULL,
                poly
            });
          }

          std::cout << mShapes.size() << "\n";
        }*/

        /*ImVector<ImVec2> convexPolys;
        ImVector<int> numConvexPoints;
        bayazit::DecomposeConcavePoly(poly, convexPolys, numConvexPoints);
        mShapes.reserve(numConvexPoints.size());

        size_t offset = 0;
        for(size_t i = 0; i < numConvexPoints.size(); i++) {
          int np = numConvexPoints[i];
          ImVector<ImVec2> path;
          path.reserve(np);
          memcpy(path.Data, &convexPolys.Data[offset], sizeof(ImVec2) * np);
          mShapes.push_back(Shape{
              NULL,
              path
          });

          offset += np;
        }*/

        return true;
      }

      void renderBody() {
        if(!mImage) {
          return;
        }

        ImVec2 imageSize(mImage->width, mImage->height);
        ImVec2& drawnSize = imageSize;

        if(size.x != 0 && size.y != 0) {
          drawnSize = size;
        }

        //ImVec2 offset = ImGui::GetCursorScreenPos();
        ImVec2 scale(drawnSize.x / mImage->width, drawnSize.y / mImage->height);
        //ImGui::ItemSize(drawnSize);
        bool changed = false;
        //bool cleanOldData = mData != NULL;
        // rasterize on change
        if(drawnSize.x != mSize.x || drawnSize.y != mSize.y) {
          if(mData) {
            free(mData);
          }

          mData = (unsigned char*)malloc((size_t)drawnSize.x * (size_t)drawnSize.y * 4);
          nsvgRasterize(mRasterizer, mImage, 0, 0, std::min(scale.x, scale.y), mData, drawnSize.x, drawnSize.y, drawnSize.x * 4);
          mSize = drawnSize;
          changed = true;
        }

        if(changed) {
          if(mTextureID) {
            mTextureManager->deleteTexture(mTextureID);
          }

          mTextureID = mTextureManager->createTexture(mData, (int)drawnSize.x, (int)drawnSize.y);
        }

        if(mTextureID) {
          ImGui::Image(mTextureID, drawnSize, ImVec2(0,0), ImVec2(1, 1), tint_col);
        }

        /*ImDrawList* drawList = ImGui::GetWindowDrawList();

        NSVGshape* shape;
        NSVGpath* path;
        float tolerance = 1.0f;
        std::srand(c++);*/

        /*for(shape = mImage->shapes; shape != NULL; shape = shape->next) {
          for(path = shape->paths; path != NULL; path = path->next) {
            drawList->PathClear();
            drawList->PathLineTo(ImVecSum(ImVec2(path->pts[0], path->pts[1]) * scale, offset));
            for(int i = 0; i < path->npts-1; i += 3) {
              float* p = &path->pts[i * 2];
              drawList->PathBezierCurveTo(
                  ImVecSum(ImVec2(p[2], p[3]) * scale, offset),
                  ImVecSum(ImVec2(p[4], p[5]) * scale, offset),
                  ImVecSum(ImVec2(p[6], p[7]) * scale, offset),
                  2
              );
            }

            if(path->closed) {
              drawList->PathLineTo(ImVecSum(ImVec2(path->pts[0], path->pts[1]) * scale, offset));
            }

            ImVec4 col = ImGui::ColorConvertU32ToFloat4(std::rand());
            col.w = 1.0f;

            if(shape->fill.type != ::NSVG_PAINT_NONE) {
              //std::cout << ((int)shape->fill.type << "\n";
              //ImVec4 col(1.0, 1.0, 1.0, shape->opacity);
              //drawList->PathFillConvex(std::rand());
              drawList->PathStroke(ImGui::ColorConvertFloat4ToU32(col), path->closed, 4);
            }
          }
        }*/

        /*for(size_t i = 0; i < mShapes.size(); ++i) {
          Shape& s = mShapes[i];
          ImVec2& p = s.path[0];
          drawList->PathLineTo(p * scale + offset);

          for(size_t j = 1; j < s.path.size(); j += 3) {
            //drawList->PathLineTo(s.path[j] * scale + offset);
            drawList->PathBezierCurveTo(
                s.path[j] * scale + offset,
                s.path[j+1] * scale + offset,
                s.path[j+2] * scale + offset,
                10
            );
          }

          ImVec4 col = ImGui::ColorConvertU32ToFloat4(std::rand());
          col.w = 1.0f;
          drawList->PathStroke(ImGui::ColorConvertFloat4ToU32(col), false, 4);
          //drawList->PathFillConvex(std::rand());
        }*/
      }

      char* src;
      ImVec2 size;
      ImVec4 tint_col;
      ImVec4 border_col;
      unsigned int c;

    private:
      ImTextureID mTextureID;
      unsigned char* mData;
      NSVGimage* mImage;
      NSVGrasterizer* mRasterizer;
      ImVector<Shape> mShapes;
      ImVec2 mSize;
  };

}

#endif
