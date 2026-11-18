#pragma once

#include <cmath>
#include <stdexcept>

namespace fireteam {

inline void requireValidDt(float deltaSeconds) {
  if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0f) {
    throw std::invalid_argument("deltaSeconds must be finite and non-negative");
  }
}

}  // namespace fireteam
