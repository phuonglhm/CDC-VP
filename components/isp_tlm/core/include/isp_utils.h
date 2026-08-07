#ifndef ISP_UTILS_H
#define ISP_UTILS_H

#include <stdint.h>

namespace isp_utils {

// Read a pixel with reflection padding at every boundary distance.
inline int mirror_index(int index, uint32_t size) {
   if (size <= 1) {
      return 0;
   }
   const int period = 2 * static_cast<int>(size) - 2;
   int reflected = index % period;
   if (reflected < 0) {
      reflected += period;
   }
   return reflected < static_cast<int>(size) ? reflected : period - reflected;
}

template <typename T>
inline T get_pixel_mirror(const T *buf, int r, int c, uint32_t w, uint32_t h) {
   return buf[mirror_index(r, h) * w + mirror_index(c, w)];
}

} // namespace isp_utils

#endif // ISP_UTILS_H
