#ifndef MACINTOSH_SRC_PLATFORM_COMMON_PLATFORM_H_
#define MACINTOSH_SRC_PLATFORM_COMMON_PLATFORM_H_

#include <string>
#include <vector>

#include "src/devices/video.h"
#include "src/platform/platform.h"

class MacSystem;

class CommonPlatform : public Platform {
 public:
  explicit CommonPlatform(MacSystem* system);
  ~CommonPlatform() override;

  bool Init(int argc, char** argv) override;

  // Platform interface
  void WaitForNextTick() override;
  bool ExtraTimeNotOver() override;
  void ReserveAllocOneBlock(uint8_t** ptr, uint32_t size, uint8_t align,
                            bool fill_ones) override;

  MacErr SonyTransfer(bool is_write, uint8_t* buffer, uint32_t drive_no,
                      uint32_t start_sector, uint32_t sector_count,
                      uint32_t* actual_sector_count) override;
  MacErr SonyEject(uint32_t drive_no) override;
  uint32_t GetDriveSizeInSectors(uint32_t drive_no) override;

  void MacMsg(const char* title, const char* msg, bool should_quit) override;
  void MacMsgOverride(const char* title, const char* msg) override;

 protected:
  virtual void PollEvents() = 0;
  virtual bool OnInit() = 0;

  MacSystem* system_;

  // Memory pool for ReserveAllocOneBlock
  std::vector<uint8_t> pool_;
  size_t pool_size_ = 0;
  size_t pool_offset_ = 0;
  bool pool_allocated_ = false;

  // Disk images
  struct DiskImage {
    std::string path;
    int fd = -1;
    size_t size = 0;
  };
  std::vector<DiskImage> disks_;

  // Timing
  uint64_t last_tick_time_us_ = 0;
};

#endif  // MACINTOSH_SRC_PLATFORM_COMMON_PLATFORM_H_
