#ifndef MACINTOSH_SRC_PLATFORM_X11_PLATFORM_BASE_H_
#define MACINTOSH_SRC_PLATFORM_X11_PLATFORM_BASE_H_

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <vector>

#include "src/platform/common_platform.h"

class X11PlatformBase : public CommonPlatform {
 public:
  explicit X11PlatformBase(MacSystem* system);
  ~X11PlatformBase() override;

  void DoneWithDrawingForTick() override;
  void DumpDebugInfo() override;

 protected:
  void PollEvents() override;
  bool OnInit() override;

  void InitX11();
  void CloseX11();

  // X11 state
  Display* display_ = nullptr;
  Window window_ = 0;
  GC gc_ = nullptr;
  XImage* x_image_ = nullptr;
  int screen_ = 0;
  unsigned long pixel_black_ = 0;
  unsigned long pixel_white_ = 0xFFFFFF;
  bool x11_initialized_ = false;

  // Drawing buffers
  std::vector<uint32_t> image_buffer_;  // Backing store for XImage

  // Configuration
  bool use_magnify_ = true;
  int window_scale_ = 3;

 private:
  void KC2MKCAssignOne(KeySym ks, uint8_t key);
  bool KC2MKCInit();
  void CheckTheCapsLock();
  void DoKeyCode0(int i, bool down);
  void DoKeyCode(int i, bool down);
  void MousePositionNotify(int x, int y);

  KeyCode TheCapsLockCode_ = 0;
  uint8_t KC2MKC_[256];
};

#endif  // MACINTOSH_SRC_PLATFORM_X11_PLATFORM_BASE_H_
