#include "src/devices/sony.h"
#include "src/debug_log.h"

#include <stdio.h>
#include <string.h>

#include "src/platform/platform.h"
#include "src/system.h"

SonyDevice* g_sony = nullptr;

SonyDevice::SonyDevice(MacSystem* system)
    : system_(system), mounted_mask_(0), inserted_mask_(0), writable_mask_(0) {
  g_sony = this;
}

SonyDevice::~SonyDevice() {
  if (g_sony == this) {
    g_sony = nullptr;
  }
}

void SonyDevice::Reset() { mounted_mask_ = 0; }

void SonyDevice::EjectAllDisks() {
  for (int i = 0; i < NumDrives; i++) {
    if (IsMounted(i)) {
      if (system_->GetPlatform()) {
        system_->GetPlatform()->SonyEject(i);
      }
    }
  }
  mounted_mask_ = 0;
}

MacErr SonyDevice::Prime(uint32_t pb_address) {
  DLOG( "SonyDevice::Prime called\n");
  fflush(stderr);
  MacErr result;
  uint32_t Sony_Count;
  uint32_t Sony_Start;
  uint32_t Sony_ActCount = 0;
  uint32_t ParamBlk = get_vm_long(pb_address + 8);
  uint32_t DeviceCtl = get_vm_long(pb_address + 12);
  uint32_t Drive_No = get_vm_word(ParamBlk + 22) - 1;  // ioVRefNum
  uint16_t IOTrap = get_vm_word(ParamBlk + 6);         // ioTrap

  bool IsWrite = (0 != (IOTrap & 0x0001));
  Sony_Start = get_vm_long(DeviceCtl + 16);  // dCtlPosition
  Sony_Count = get_vm_long(ParamBlk + 36);   // ioReqCount

  uint32_t Buffera = get_vm_long(ParamBlk + 32);  // ioBuffer

  if (system_->GetPlatform()) {
    result = system_->GetPlatform()->SonyTransfer(
        IsWrite, get_ram_address(Buffera), Drive_No, Sony_Start, Sony_Count,
        &Sony_ActCount);
  } else {
    result = mnvm_miscErr;
  }

  put_vm_long(DeviceCtl + 16, Sony_Start + Sony_ActCount);

  put_vm_word(ParamBlk + 16, result);         // ioResult
  put_vm_long(ParamBlk + 40, Sony_ActCount);  // ioActCount

  return result;
}

MacErr SonyDevice::Control(uint32_t pb_address) {
  MacErr result = mnvm_noErr;
  uint32_t ParamBlk = get_vm_long(pb_address + 8);
  uint16_t OpCode = get_vm_word(ParamBlk + 26);  // csCode

  if (OpCode == 7) {
    uint32_t Drive_No = get_vm_word(ParamBlk + 22) - 1;
    if (system_->GetPlatform()) {
      result = system_->GetPlatform()->SonyEject(Drive_No);
    } else {
      result = mnvm_miscErr;
    }
  } else {
    result = mnvm_controlErr;
  }
  return result;
}

MacErr SonyDevice::DriveStatus(uint32_t pb_address) {
  MacErr result = mnvm_noErr;
  uint32_t ParamBlk = get_vm_long(pb_address + 8);
  uint16_t OpCode = get_vm_word(ParamBlk + 26);  // csCode

  if (8 == OpCode) {
    uint32_t Drive_No = get_vm_word(ParamBlk + 22) - 1;
    if (Drive_No >= NumDrives) {
      return mnvm_nsDrvErr;
    }
    uint32_t csParam = ParamBlk + 28;
    bool inserted = (0 != (inserted_mask_ & (1 << Drive_No)));
    bool writable = (0 != (writable_mask_ & (1 << Drive_No)));

    put_vm_word(csParam, 0);
    put_ram_byte(csParam + 2, writable ? 0 : 0x80);
    put_ram_byte(csParam + 3, inserted ? 1 : 0);
    put_ram_byte(csParam + 4, 1);
    put_ram_byte(csParam + 5, 0xFF);  // -1 (double sided)

    DLOG("SonyDevice::Status: DriveStatus drive=%d inserted=%d writable=%d\n",
        (int)Drive_No, (int)inserted, (int)writable);
  } else {
    result = mnvm_statusErr;
  }
  return result;
}

MacErr SonyDevice::Close(uint32_t pb_address) {
  return mnvm_closErr; /* Can't Close Driver */
}

MacErr SonyDevice::OpenA(uint32_t pb_address) {
  DLOG( "SonyDevice::OpenA called\n");
  put_vm_long(pb_address + 8, 1024);  // Vars size
  return mnvm_noErr;
}

MacErr SonyDevice::OpenB(uint32_t pb_address) {
  DLOG( "SonyDevice::OpenB called\n");
  return mnvm_noErr;
}

MacErr SonyDevice::OpenC(uint32_t pb_address) {
  DLOG( "SonyDevice::OpenC called\n");
  return mnvm_noErr;
}

void SonyDevice::ExtnAccess(uint32_t pb_address) {
  int cmd = get_vm_word(pb_address + 4);
  DLOG( "SonyDevice::ExtnAccess called at %08x cmd=%d\n",
          (unsigned int)pb_address, cmd);
  fflush(stderr);
  MacErr result;

  switch (cmd) {
    case kCmndVersion:
      put_vm_word(pb_address + 8, 0);
      result = mnvm_noErr;
      break;
    case kCmndSonyPrime:
      result = Prime(pb_address);
      break;
    case kCmndSonyControl:
      result = Control(pb_address);
      break;
    case kCmndSonyStatus:
      result = DriveStatus(pb_address);
      break;
    case kCmndSonyClose:
      result = Close(pb_address);
      break;
    case kCmndSonyOpenA:
      result = OpenA(pb_address);
      break;
    case kCmndSonyOpenB:
      result = OpenB(pb_address);
      break;
    case kCmndSonyOpenC:
      result = OpenC(pb_address);
      break;
    default:
      result = mnvm_controlErr;
      break;
  }

  put_vm_word(pb_address + 6, result);
}

void SonyDevice::Update() {
  static uint16_t LastInsertedMask = 0;
  uint16_t InsertedMask = inserted_mask_;
  uint16_t NewInserted = InsertedMask & ~LastInsertedMask;

  if (NewInserted != 0) {
    uint16_t i;
    for (i = 0; i < NumDrives; i++) {
      if ((NewInserted & (1 << i)) != 0) {
        DLOG( "SonyDevice::Update: Mounting drive %d\n", (int)i);
      }
    }
  }
  LastInsertedMask = InsertedMask;
}

extern "C" {
void ExtnSony_Access(uint32_t pb_address) {
  if (g_sony) {
    g_sony->ExtnAccess(pb_address);
  }
}

void Sony_Update(void) {
  if (g_sony) {
    g_sony->Update();
  }
}
}
