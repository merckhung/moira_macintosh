#ifndef MACINTOSH_SRC_PLATFORM_X11_PLATFORM_NEW_H_
#define MACINTOSH_SRC_PLATFORM_X11_PLATFORM_NEW_H_

#include "src/platform/x11_platform_base.h"

class X11PlatformNew : public X11PlatformBase {
 public:
  explicit X11PlatformNew(MacSystem* system);
  ~X11PlatformNew() override = default;

  void ScreenOutputFrame(uint8_t* screen_current_buf) override;

 private:
  // Scale 1x Mono to 32-bit Color XImage
  void Scale1x(const uint8_t* src, uint32_t* dst);
  // Scale 3x Mono to 32-bit Color XImage
  void Scale3x(const uint8_t* src, uint32_t* dst);
};

#endif  // MACINTOSH_SRC_PLATFORM_X11_PLATFORM_NEW_H_
