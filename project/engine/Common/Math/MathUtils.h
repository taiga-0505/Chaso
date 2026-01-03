#pragma once
#include "MathTypes.h"

namespace RC {

// operator vector3 + vector3
inline Vector3 operator+(const Vector3 &a, const Vector3 &b) {
  return Vector3(a.x + b.x, a.y + b.y, a.z + b.z);
}

} // namespace RC
