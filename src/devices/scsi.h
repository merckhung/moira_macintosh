#ifndef SRC_DEVICES_SCSI_H_
#define SRC_DEVICES_SCSI_H_

#include <cstdint>

constexpr int kScsiSize = 0x00010;

class MacSystem;

class ScsiDevice {
 public:
  ScsiDevice(MacSystem* system);
  ~ScsiDevice();

  void Reset();
  uint32_t Access(uint32_t data, bool is_write, uint32_t address);

 private:
  void BusReset();
  void Check();

  MacSystem* system_;
  uint8_t scsi_[kScsiSize];
};

#endif  // SRC_DEVICES_SCSI_H_
