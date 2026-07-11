#define IS_PLATFORM_IMPLEMENTATION
#include "src/platform/common_platform.h"
#include "src/debug_log.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <chrono>

#include "src/devices/sony.h"
#include "src/system.h"

// Global pointer for forwarding legacy calls
Platform* g_platform = nullptr;

extern "C" {
void ReserveAllocOneBlock(uint8_t** p, uint32_t n, uint8_t align,
                          bool FillOnes) {
  if (g_platform) {
    g_platform->ReserveAllocOneBlock(p, n, align, FillOnes);
  }
}

uint32_t GetDriveSizeInSectors(uint32_t Drive_No) {
  if (g_platform) {
    return g_platform->GetDriveSizeInSectors(Drive_No);
  }
  return 0;
}

// Declaration of DiskInsertNotify in main.cc
void DiskInsertNotify(uint16_t Drive_No, bool locked);
void EmulationReserveAlloc(void);
}

// Helper to get time in microseconds
static uint64_t GetMicroseconds() {
  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(
             now.time_since_epoch())
      .count();
}

CommonPlatform::CommonPlatform(MacSystem* system) : system_(system) {
  g_platform = this;
}

CommonPlatform::~CommonPlatform() {
  if (g_platform == this) {
    g_platform = nullptr;
  }
  for (auto& disk : disks_) {
    if (disk.fd >= 0) {
      close(disk.fd);
    }
  }
}

bool CommonPlatform::Init(int argc, char** argv) {
  // ----------------------------------------------------
  // Phase 1: Allocate memory pool
  // ----------------------------------------------------
  // We need to know how much memory to allocate.
  // In legacy code, this was done by calling EmulationReserveAlloc twice.
  // First time to count, second time to allocate.
  pool_allocated_ = false;
  pool_offset_ = 0;

  // Platform blocks (size calculation)
  uint8_t* dummy_rom = nullptr;
  ReserveAllocOneBlock(&dummy_rom, kROM_Size, 5, false);
  uint8_t* dummy_screen = nullptr;
  ReserveAllocOneBlock(&dummy_screen, kMacScreenNumBytes, 5, true);

  // Emulator core blocks (RAM)
  EmulationReserveAlloc();

  pool_size_ = pool_offset_;
  DLOG("CommonPlatform: Required memory pool size: %zu bytes (%.2f MB)\n",
       pool_size_, (double)pool_size_ / (1024.0 * 1024.0));

  // Allocate pool
  pool_.resize(pool_size_);
  pool_allocated_ = true;
  pool_offset_ = 0;

  // Assign platform blocks
  ReserveAllocOneBlock(&system_->get_rom(), kROM_Size, 5, false);
  ReserveAllocOneBlock(&system_->get_screen_compare_buff(), kMacScreenNumBytes,
                       5, true);

  // Assign emulator core blocks (RAM)
  EmulationReserveAlloc();
  DLOG("CommonPlatform: Memory pool allocated and assigned.\n");

  // ----------------------------------------------------
  // Phase 2: Open disk images
  // ----------------------------------------------------
  disks_.clear();
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-') {
      // Skip options (subclass might handle them, but we skip them here)
      if (strcmp(argv[i], "--display") == 0 ||
          strcmp(argv[i], "-display") == 0) {
        if (i + 1 < argc) ++i;
      }
      continue;
    }

    DiskImage disk;
    disk.path = argv[i];
    disk.fd = open(disk.path.c_str(), O_RDWR);
    bool locked = false;
    if (disk.fd < 0) {
      locked = true;
      disk.fd = open(disk.path.c_str(), O_RDONLY);
    }
    if (disk.fd >= 0) {
      struct stat st;
      if (fstat(disk.fd, &st) == 0) {
        disk.size = st.st_size;
        disks_.push_back(disk);
        DLOG("Opened disk image: %s, size %zu bytes (%zu sectors)\n",
             disk.path.c_str(), disk.size, disk.size / 512);

        int drive_no = disks_.size() - 1;
        DiskInsertNotify(drive_no, locked ? true : false);
      } else {
        close(disk.fd);
      }
    } else {
      fprintf(stderr, "Failed to open disk image: %s\n", disk.path.c_str());
    }
  }

  // ----------------------------------------------------
  // Phase 3: Load ROM
  // ----------------------------------------------------
  std::string rom_path = RomFileName;  // Default from config.h
  DLOG("Loading ROM from: %s\n", rom_path.c_str());
  int rom_fd = open(rom_path.c_str(), O_RDONLY);
  if (rom_fd < 0) {
    fprintf(stderr, "Failed to open ROM file: %s\n", rom_path.c_str());
    return false;
  }
  ssize_t read_bytes = read(rom_fd, system_->get_rom(), kROM_Size);
  if (read_bytes != kROM_Size) {
    fprintf(stderr, "Failed to read ROM: read %zd of %d bytes\n", read_bytes,
            kROM_Size);
    close(rom_fd);
    return false;
  }
  close(rom_fd);
  DLOG("ROM loaded successfully.\n");

  // Call subclass specific initialization
  if (!OnInit()) {
    return false;
  }

  last_tick_time_us_ = GetMicroseconds();
  return true;
}

void CommonPlatform::ReserveAllocOneBlock(uint8_t** ptr, uint32_t size,
                                          uint8_t align, bool fill_ones) {
  size_t mask = (1ULL << align) - 1;
  pool_offset_ = (pool_offset_ + mask) & ~mask;

  if (pool_allocated_) {
    if (pool_offset_ + size > pool_.size()) {
      fprintf(
          stderr,
          "Pool overflow during assignment! offset=%zu, size=%zu, pool=%zu\n",
          pool_offset_, (size_t)size, pool_.size());
      exit(1);
    }
    *ptr = &pool_[pool_offset_];
    if (fill_ones) {
      memset(*ptr, 0, size);
    }
  }
  pool_offset_ += size;
}

void CommonPlatform::WaitForNextTick() {
  PollEvents();

  // Maintain 60Hz tick rate (approx 16666 us per tick)
  uint64_t current_time_us = GetMicroseconds();
  uint64_t target_time_us = last_tick_time_us_ + 16666;

  if (current_time_us < target_time_us) {
    uint64_t sleep_time_us = target_time_us - current_time_us;
    usleep(sleep_time_us);
  }
  last_tick_time_us_ = GetMicroseconds();

  system_->get_on_true_time()++;
}

bool CommonPlatform::ExtraTimeNotOver() { return false; }

MacErr CommonPlatform::SonyTransfer(bool is_write, uint8_t* buffer,
                                    uint32_t drive_no, uint32_t start_sector,
                                    uint32_t sector_count,
                                    uint32_t* actual_sector_count) {
  if (drive_no >= disks_.size()) {
    fprintf(stderr, "SonyTransfer: error: invalid drive %d\n", (int)drive_no);
    return mnvm_nsDrvErr;
  }
  auto& disk = disks_[drive_no];
  if (disk.fd < 0) {
    fprintf(stderr, "SonyTransfer: error: drive %d is offline\n",
            (int)drive_no);
    return mnvm_offLinErr;
  }

  if (lseek(disk.fd, start_sector, SEEK_SET) == (off_t)-1) {
    fprintf(stderr, "SonyTransfer: seek error: %s\n", strerror(errno));
    return mnvm_miscErr;
  }

  ssize_t bytes;
  if (is_write) {
    bytes = write(disk.fd, buffer, sector_count);
  } else {
    bytes = read(disk.fd, buffer, sector_count);
  }

  if (bytes < 0) {
    fprintf(stderr, "SonyTransfer: R/W error: %s\n", strerror(errno));
    if (actual_sector_count) *actual_sector_count = 0;
    return mnvm_miscErr;
  }

  if (actual_sector_count) {
    *actual_sector_count = bytes;
  }

  if (bytes != (ssize_t)sector_count) {
    fprintf(stderr, "SonyTransfer: short R/W: got %zd, expected %u\n", bytes,
            sector_count);
    return mnvm_eofErr;
  }

  return mnvm_noErr;
}

MacErr CommonPlatform::SonyEject(uint32_t drive_no) {
  if (drive_no >= disks_.size()) {
    return mnvm_nsDrvErr;
  }
  auto& disk = disks_[drive_no];
  if (disk.fd >= 0) {
    close(disk.fd);
    disk.fd = -1;
    if (system_ && system_->GetSonyDevice()) {
      system_->GetSonyDevice()->RemoveInsertedDisk(drive_no);
    }
    DLOG("Ejected disk drive %d\n", (int)drive_no);
  }
  return mnvm_noErr;
}

uint32_t CommonPlatform::GetDriveSizeInSectors(uint32_t drive_no) {
  if (drive_no >= disks_.size()) {
    return 0;
  }
  return disks_[drive_no].size / 512;
}

void CommonPlatform::MacMsg(const char* title, const char* msg,
                            bool should_quit) {
  fprintf(stderr, "MacMsg: %s: %s\n", title, msg);
  if (should_quit) {
    exit(1);
  }
}

void CommonPlatform::MacMsgOverride(const char* title, const char* msg) {
  fprintf(stderr, "MacMsgOverride: %s: %s\n", title, msg);
}
