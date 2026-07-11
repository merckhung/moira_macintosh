#ifndef MACINTOSH_SRC_DEVICES_VIDEO_H_
#define MACINTOSH_SRC_DEVICES_VIDEO_H_

#include "src/config.h"

constexpr int kMacScreenDepth = vMacScreenDepth;

#if (0 == kMacScreenDepth)
constexpr int kClutSize = 2;
#else
constexpr int kClutSize = 1 << (1 << kMacScreenDepth);
#endif

constexpr int kMacScreenWidth = vMacScreenWidth;
constexpr int kMacScreenHeight = vMacScreenHeight;

constexpr long kMacScreenNumPixels =
    (long)kMacScreenHeight * (long)kMacScreenWidth;
constexpr long kMacScreenNumBits = kMacScreenNumPixels << kMacScreenDepth;
constexpr long kMacScreenNumBytes = kMacScreenNumBits / 8;
constexpr long kMacScreenBitWidth = (long)kMacScreenWidth << kMacScreenDepth;
constexpr long kMacScreenByteWidth = kMacScreenBitWidth / 8;

constexpr long kMacScreenMonoNumBytes = kMacScreenNumPixels / 8;
constexpr long kMacScreenMonoByteWidth = (long)kMacScreenWidth / 8;

class MacSystem;

class VideoDevice {
 public:
  explicit VideoDevice(MacSystem* system);
  ~VideoDevice() = default;

  void EndTickNotify();

 private:
  MacSystem* system_;
};

#endif  // MACINTOSH_SRC_DEVICES_VIDEO_H_
