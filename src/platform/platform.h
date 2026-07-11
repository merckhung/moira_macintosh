#ifndef MACINTOSH_SRC_PLATFORM_PLATFORM_H_
#define MACINTOSH_SRC_PLATFORM_PLATFORM_H_

#include <cstdint>

#include "src/mac_errors.h"

class Platform {
 public:
  virtual ~Platform() = default;
  virtual bool Init(int argc, char** argv) = 0;

  virtual void WaitForNextTick() = 0;
  virtual bool ExtraTimeNotOver() = 0;
  virtual void DoneWithDrawingForTick() = 0;
  virtual void ScreenOutputFrame(uint8_t* screen_current_buf) = 0;
  virtual void ReserveAllocOneBlock(uint8_t** ptr, uint32_t size, uint8_t align,
                                    bool fill_ones) = 0;

  virtual MacErr SonyTransfer(bool is_write, uint8_t* buffer, uint32_t drive_no,
                              uint32_t start_sector, uint32_t sector_count,
                              uint32_t* actual_sector_count) = 0;
  virtual MacErr SonyEject(uint32_t drive_no) = 0;
  virtual uint32_t GetDriveSizeInSectors(uint32_t drive_no) = 0;

  virtual void MacMsg(const char* title, const char* msg, bool should_quit) = 0;
  virtual void MacMsgOverride(const char* title, const char* msg) = 0;

  virtual void DumpDebugInfo() {}
};

#endif  // MACINTOSH_SRC_PLATFORM_PLATFORM_H_
