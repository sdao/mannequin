#pragma once

#include <maya/MPoint.h>
#include <maya/MVector.h>

namespace Util {

  bool raySphereIntersection(const MPoint& rayOrigin, // = 8.5, 55.3, 105.6
    const MVector& rayDirection, // = 0, 0, -1
    const MPoint& sphereOrigin, // = 8.5, 53.4, 5
    float sphereRadius, // = 7.5
    float* outDistance) {
    MVector diff = rayOrigin - sphereOrigin; // 0, 1.9, 100.6
    MVector l = rayDirection.normal(); // 0, 0, -1

    // See Wikipedia:
    // <http://en.wikipedia.org/wiki/Line%E2%80%93sphere_intersection>
    // Note that in the OpenMaya API, x * y is dot product and x ^ y is cross.
    float a = l * l; // 1
    float b = l * diff; // -100.6
    float c = (diff * diff) - (sphereRadius * sphereRadius);
    // 10 123.97 - 56.25 = 10 067.72

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
