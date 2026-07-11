#ifndef SRC_DEVICES_SONY_H_
#define SRC_DEVICES_SONY_H_

#include <cstdint>

#include "src/mac_errors.h"

class MacSystem;

#define kCmndVersion 0
#define kCmndSonyPrime 1
#define kCmndSonyControl 2
#define kCmndSonyStatus 3
#define kCmndSonyClose 4
#define kCmndSonyOpenA 5
#define kCmndSonyOpenB 6
#define kCmndSonyOpenC 7
#define kCmndSonyMount 8

#ifdef __cplusplus
extern "C" {
#endif
void ExtnSony_Access(uint32_t pb_address);
void Sony_Update(void);
#ifdef __cplusplus
}
#endif

class SonyDevice {
 public:
  SonyDevice(MacSystem* system);
  ~SonyDevice();

  void Reset();
  void Update();
  void EjectAllDisks();
  void ExtnAccess(uint32_t pb_address);

  // Accessors for state variables
  uint32_t GetInsertedMask() const { return inserted_mask_; }
  void SetInsertedMask(uint32_t mask) { inserted_mask_ = mask; }
  void AddInsertedDisk(uint32_t drive) { inserted_mask_ |= (1 << drive); }
  void RemoveInsertedDisk(uint32_t drive) { inserted_mask_ &= ~(1 << drive); }

  uint32_t GetWritableMask() const { return writable_mask_; }
  void SetWritableMask(uint32_t mask) { writable_mask_ = mask; }
  void SetWritable(uint32_t drive, bool writable) {
    if (writable)
      writable_mask_ |= (1 << drive);
    else
      writable_mask_ &= ~(1 << drive);
  }

  uint32_t GetMountedMask() const { return mounted_mask_; }
  void SetMountedMask(uint32_t mask) { mounted_mask_ = mask; }

 private:
  MacErr Prime(uint32_t pb_address);
  MacErr Control(uint32_t pb_address);
  MacErr DriveStatus(uint32_t pb_address);
  MacErr Close(uint32_t pb_address);
  MacErr OpenA(uint32_t pb_address);
  MacErr OpenB(uint32_t pb_address);
  MacErr OpenC(uint32_t pb_address);

  bool IsMounted(uint32_t drive) const {
    return (mounted_mask_ & (1 << drive)) != 0;
  }
  bool IsWritable(uint32_t drive) const {
    return (writable_mask_ & (1 << drive)) != 0;
  }

  MacSystem* system_;

  uint32_t mounted_mask_;
  uint32_t inserted_mask_;
  uint32_t writable_mask_;
};

#endif  // SRC_DEVICES_SONY_H_
