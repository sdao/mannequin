#pragma once

#include <limits>
#include <iostream>

#include <maya/MPoint.h>
#include <maya/MVector.h>

namespace Util {

  inline bool raySphereIntersection(const MPoint& rayOrigin,
                                    const MVector& rayDirection,
                                    const MPoint& sphereOrigin,
                                    float sphereRadius,
                                    float* outDistance) {
    MVector diff = rayOrigin - sphereOrigin;
    MVector l = rayDirection.normal();

    // See Wikipedia:
    // <http://en.wikipedia.org/wiki/Line%E2%80%93sphere_intersection>
    // Note that in the OpenMaya API, x * y is dot product and x ^ y is cross.
    float a = l * l;
    float b = l * diff;
    float c = (diff * diff) - pow(sphereRadius, 2);

    float discriminant = (b * b) - (a * c);

    if (discriminant > 0.0f) {
      discriminant = sqrtf(discriminant);
      // Quadratic has at most 2 results.
      float resPos = (-b + discriminant);
      float resNeg = (-b - discriminant);

      // Neg before pos because we want to return closest isect first.
      if (resNeg > 1e-3) {
        if (outDistance) {
          *outDistance = resNeg;
        }
        return true;
      } else if (resPos > 1e-3) {
        if (outDistance) {
          *outDistance = resPos;
        }
        return true;
      }

    }

    // Either no isect was found or it was behind us.
    return false;
  }

  inline float distanceToLine(const float lx1, const float ly1,
                              const float lx2, const float ly2,
                              const float x0, const float y0,
                              float* t_out = nullptr) {
    // See <http://en.wikipedia.org/wiki/Distance_from_a_point_to_a_line>.
    float num = fabsf((ly2 - ly1) * x0 - (lx2 - lx1) * y0 +
      lx2 * ly1 - ly2 * lx1);
    float denom = sqrtf(pow(ly2 - ly1, 2) + pow(lx2 - lx1, 2));
    float result = num / denom;

    if (fabsf(denom) < 0.001f) {
      result = std::numeric_limits<float>::max();
    }

    if (t_out) {
      // Project the vector (lx1, ly1) -> (x0, y0) onto the vector
      // (lx1, ly1) -> (lx2, ly2) to estimate the parameter.
      float ax = x0 - lx1;
      float ay = y0 - ly1;
      float bmag = denom;
      float bx = (lx2 - lx1) / bmag;
      float by = (ly2 - ly1) / bmag;
      *t_out = (ax * bx + ay * by) / bmag;
    }

    return result;
  }

}
