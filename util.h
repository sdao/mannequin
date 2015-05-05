#pragma once

#include <maya/MPoint.h>
#include <maya/MVector.h>

namespace Util {

  bool raySphereIntersection(const MPoint& rayOrigin,
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
    float c = (diff * diff) - (sphereRadius * sphereRadius);

    float discriminant = (b * b) - (a * c);
    // 52.64

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

}
