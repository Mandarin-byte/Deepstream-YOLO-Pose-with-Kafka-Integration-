/*
 * Utility functions for YOLO parsing
 */

#ifndef __YOLO_UTILS_H__
#define __YOLO_UTILS_H__

#include <algorithm>

template <typename T>
inline T clamp(const T& value, const T& low, const T& high) {
  return std::max(low, std::min(value, high));
}

#endif  // __YOLO_UTILS_H__
