#ifndef ISP_UTILS_H
#define ISP_UTILS_H

#include <stdint.h>

namespace isp_utils {

// helper to read pixels with mirror padding boundaries
template <typename T>
inline T get_pixel_mirror(const T *buf, int r, int c, uint32_t w, uint32_t h) {
   if (r < 0) {
      r = -r;
   } else if (r >= static_cast<int>(h)) {
      r = 2 * static_cast<int>(h) - 2 - r;
   }

   if (c < 0) {
      c = -c;
   } else if (c >= static_cast<int>(w)) {
      c = 2 * static_cast<int>(w) - 2 - c;
   }

   return buf[r * w + c];
}

} // namespace isp_utils

#endif // ISP_UTILS_H
