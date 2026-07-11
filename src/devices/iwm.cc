#include "src/devices/iwm.h"
#include "src/debug_log.h"

#include <stdio.h>
#include <string.h>

#include "src/system.h"

IwmDevice* g_iwm = nullptr;

#define kph0 0x01
#define kph1 0x02
#define kph2 0x04
#define kph3 0x08
#define kmtr 0x10
#define kdrv 0x20
#define kq6 0x40
#define kq7 0x80

#define kph0L 0x00     /* CA0 off (0) */
#define kph0H 0x01     /* CA0 on (1) */
#define kph1L 0x02     /* CA1 off (0) */
#define kph1H 0x03     /* CA1 on (1) */
#define kph2L 0x04     /* CA2 off (0) */
#define kph2H 0x05     /* CA2 on (1) */
#define kph3L 0x06     /* LSTRB off (0) */
#define kph3H 0x07     /* LSTRB on (1) */
#define kmtrOff 0x08   /* motor off */
#define kmtrOn 0x09    /* motor on */
#define kintDrive 0x0A /* internal drive select */
#define kextDrive 0x0B /* external drive select */
#define kq6L 0x0C      /* Q6 off */
#define kq6H 0x0D      /* Q6 on */
#define kq7L 0x0E      /* Q7 off */
#define kq7H 0x0F      /* Q7 on */

extern "C" uint32_t get_vm_pc(void);

IwmDevice::IwmDevice(MacSystem* system) : system_(system) {
  g_iwm = this;
  Reset();
}

IwmDevice::~IwmDevice() {
  if (g_iwm == this) {
    g_iwm = nullptr;
  }
}

void IwmDevice::Reset() {
  iwm_.data_in = iwm_.handshake = iwm_.status = 0x40;  // disk in place
  iwm_.mode = iwm_.data_out = iwm_.lines = 0;
}

void IwmDevice::Set_Lines(uint8_t line, Mode_Ty the_mode) {
  if (the_mode == Off) {
    iwm_.lines &= (0xFF - line);
  } else {
    iwm_.lines |= line;
  }
}

uint8_t IwmDevice::Read_Reg() {
  uint8_t result = 0;
  switch ((iwm_.lines & (kq6 + kq7)) >> 6) {
    case 0:
      result = iwm_.data_in;
      break;
    case 1:
      result = iwm_.status;
      break;
    case 2:
      result = iwm_.handshake;
      break;
    default:
      result = 0;
      break;
  }
  static int count = 0;
  if (count < 100) {
    DLOG(
            "IWM_Read_Reg: Lines=%02x Status=%02x Result=%02x PC=%08x\n",
            (unsigned int)iwm_.lines, (unsigned int)iwm_.status,
            (unsigned int)result, (unsigned int)get_vm_pc());
    fflush(stderr);
    count++;
  }
  return result;
}

void IwmDevice::Write_Reg(uint8_t value) {
  if (((iwm_.lines & kmtr) >> 4) == 0) {
    iwm_.mode = value;
    iwm_.status = ((iwm_.status & 0xE0) + (iwm_.mode & 0x1F));
  }
}

uint32_t IwmDevice::Access(uint32_t data, bool is_write, uint32_t address) {
#ifdef _VIA_Debug
  if (get_vm_pc() != 0x4005c2) {
    DLOG(
            "IWM_Access: addr=%02x WriteMem=%d Data=%02x Lines=%02x PC=%08x\n",
            (unsigned int)address, (int)is_write, (unsigned int)data,
            (unsigned int)iwm_.lines, (unsigned int)get_vm_pc());
  }
#endif
  switch (address) {
    case kph0L:
      Set_Lines(kph0, Off);
      break;
    case kph0H:
      Set_Lines(kph0, On);
      break;
    case kph1L:
      Set_Lines(kph1, Off);
      break;
    case kph1H:
      Set_Lines(kph1, On);
      break;
    case kph2L:
      Set_Lines(kph2, Off);
      break;
    case kph2H:
      Set_Lines(kph2, On);
      break;
    case kph3L:
      Set_Lines(kph3, Off);
      break;
    case kph3H:
      Set_Lines(kph3, On);
      break;
    case kmtrOff:
      iwm_.status &= 0xDF;
      Set_Lines(kmtr, Off);
      break;
    case kmtrOn:
      iwm_.status |= 0x20;
      Set_Lines(kmtr, On);
      break;
    case kintDrive:
      Set_Lines(kdrv, Off);
      break;
    case kextDrive:
      Set_Lines(kdrv, On);
      break;
    case kq6L:
      Set_Lines(kq6, Off);
      break;
    case kq6H:
      Set_Lines(kq6, On);
      break;
    case kq7L:
      if (!is_write) {
        data = Read_Reg();
      }
      Set_Lines(kq7, Off);
      break;
    case kq7H:
      if (is_write) {
        Write_Reg(data);
      }
      Set_Lines(kq7, On);
      break;
  }

  if (!is_write) {
    data = Read_Reg();
  }
  return data;
}

extern "C" {
uint32_t IWM_Access(uint32_t data, bool is_write, uint32_t address) {
  if (g_iwm) {
    return g_iwm->Access(data, is_write, address);
  }
  return 0;
}
}
