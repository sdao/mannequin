#pragma once

#include <string>
#include <functional>

#include <maya/MDagPath.h>

namespace std {

  template<> struct less<MDagPath> {
    bool operator()(const MDagPath& lhs, const MDagPath& rhs) const {
      std::string lhsString(lhs.fullPathName().asChar());
      std::string rhsString(rhs.fullPathName().asChar());
      return std::less<std::string>()(lhsString, rhsString);
    }
  };

}
