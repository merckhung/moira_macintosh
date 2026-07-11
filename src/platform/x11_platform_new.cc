#define IS_PLATFORM_IMPLEMENTATION
#include "src/platform/x11_platform_new.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/platform/x11_platform_creation.h"
#include "src/system.h"

X11PlatformNew::X11PlatformNew(MacSystem* system) : X11PlatformBase(system) {}

void X11PlatformNew::Scale1x(const uint8_t* src, uint32_t* dst) {
  int dst_idx = 0;
  for (int i = 0; i < kMacScreenNumBytes; ++i) {
    uint8_t b = src[i];
    for (int bit = 7; bit >= 0; --bit) {
      bool pixel = (b >> bit) & 1;
      dst[dst_idx++] = pixel ? 0xFF000000 : 0xFFFFFFFF;
    }
  }
}

void X11PlatformNew::Scale3x(const uint8_t* src, uint32_t* dst) {
  int src_width_bytes = kMacScreenWidth / 8;  // 64 bytes
  int dst_width = kMacScreenWidth * 3;        // 1536

  for (int y = 0; y < kMacScreenHeight; ++y) {
    for (int x_byte = 0; x_byte < src_width_bytes; ++x_byte) {
      uint8_t b = src[y * src_width_bytes + x_byte];
      for (int bit = 7; bit >= 0; --bit) {
        bool pixel = (b >> bit) & 1;
        uint32_t color = pixel ? 0xFF000000 : 0xFFFFFFFF;

        int pixel_x = x_byte * 8 + (7 - bit);

        // Write 3x3 block
        for (int dy = 0; dy < 3; ++dy) {
          for (int dx = 0; dx < 3; ++dx) {
            int dst_x = pixel_x * 3 + dx;
            int dst_y = y * 3 + dy;
            dst[dst_y * dst_width + dst_x] = color;
          }
        }
      }
    }
  }
}

void X11PlatformNew::ScreenOutputFrame(uint8_t* screen_current_buf) {
  if (!screen_current_buf) return;

  if (use_magnify_) {
    Scale3x(screen_current_buf, image_buffer_.data());
  } else {
    Scale1x(screen_current_buf, image_buffer_.data());
  }
  DoneWithDrawingForTick();
}

std::unique_ptr<Platform> CreateX11Platform(MacSystem* system) {
  return std::make_unique<X11PlatformNew>(system);
}
