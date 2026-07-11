#include <stdio.h>
#include "src/debug_log.h"
#include <stdlib.h>
#include <string.h>

#include <cstdint>

#include "src/devices/sony.h"
#include "src/system.h"

extern "C" {

uint8_t CurIPL = 0;
uint32_t CurMacDateInSeconds = 0;
uint32_t CurMacLatitude = 0;
uint32_t CurMacLongitude = 0;
uint32_t CurMacDelta = 0;
bool WantNotAutoSlow = false;

uint32_t NextiCount = 0;

static uint16_t LastATTel = 0;

struct ATTer ATTListA[16] = {0};

// Declarations of external functions called by MMDV_Access and interrupts
void SetHeadATTel(ATTep p);
uint32_t VIA1_Access(uint32_t data, bool is_write, uint32_t address);
uint32_t SCC_Access(uint32_t data, bool is_write, uint32_t address);
uint32_t IWM_Access(uint32_t data, bool is_write, uint32_t address);
void ExtnSony_Access(uint32_t pb_address);
void ExtnDisk_Access(uint32_t pb_address);
void m68k_IPLchangeNtfy(void);

// Helper for SetUpMemBanks
static void AddToATTList(ATTep p) { ATTListA[LastATTel++] = *p; }

// Functions from GLOBGLUE.c

void SetUpMemBanks(void) {
  struct ATTer r;

  LastATTel = 0;

  // ROM
  r.cmpmask = 0x00F00000;
  r.cmpvalu = kROM_Base;
  r.usebase = ROM;
  r.usemask = kROM_Size - 1;
  r.Access = kATTA_readreadymask;
  AddToATTList(&r);
  DLOG("Bank ROM: base=%08x size=%08x host=%p\n", kROM_Base,
       kROM_Size, ROM);

  // RAM
  r.cmpmask = 0x00C00000;
  r.cmpvalu = 0;
  r.usebase = RAM;
  r.usemask = kRAM_Size - 1;
  r.Access = kATTA_readreadymask | kATTA_writereadymask;
  AddToATTList(&r);
  DLOG("Bank RAM: base=%08x size=%08x host=%p\n", 0, kRAM_Size, RAM);

  // VIA
  r.cmpmask = 0x00F80000;
  r.cmpvalu = 0x00E80000;
  r.usebase = nullptr;
  r.usemask = 0;
  r.Access = kATTA_mmdvmask;
  AddToATTList(&r);
  DLOG("Bank VIA: base=%08x\n", 0x00E80000);

  // SCC
  r.cmpmask = 0x00E00000;
  r.cmpvalu = 0x00800000;
  r.usebase = nullptr;
  r.usemask = 0;
  r.Access = kATTA_mmdvmask;
  AddToATTList(&r);

  // IWM
  r.cmpmask = 0x00E00000;
  r.cmpvalu = 0x00A00000;
  r.usebase = nullptr;
  r.usemask = 0;
  r.Access = kATTA_mmdvmask;
  AddToATTList(&r);

  // Extensions
  r.cmpmask = 0x00FF0000;
  r.cmpvalu = kExtn_Block_Base;
  r.usebase = nullptr;
  r.usemask = 0;
  r.Access = kATTA_mmdvmask;
  AddToATTList(&r);

  // Guard
  r.cmpmask = 0;
  r.cmpvalu = 0;
  r.usebase = nullptr;
  r.Access = 0;
  AddToATTList(&r);

  for (int i = 0; i < LastATTel - 1; i++) ATTListA[i].Next = &ATTListA[i + 1];
  ATTListA[LastATTel - 1].Next = nullptr;
  SetHeadATTel(&ATTListA[0]);
}

uint32_t MMDV_Access(ATTep p, uint32_t data, bool is_write, bool is_byte_size,
                     uint32_t address) {
  if ((address & 0x00F80000) == 0x00E80000)
    return VIA1_Access(data, is_write, address);
  if ((address & 0x00E00000) == 0x00800000)
    return SCC_Access(data, is_write, address);
  if ((address & 0x00E00000) == 0x00A00000)
    return IWM_Access(data, is_write, address & 0xF);
  if ((address & 0x00FF0000) == kExtn_Block_Base) {
    DLOG("Extension Access: addr=%08x Write=%d Data=%08x\n",
         (unsigned int)address, (int)is_write, (unsigned int)data);
    uint32_t id = (address >> 1) & 0x1F;
    if (id == kExtnSony) {
      ExtnSony_Access(address);
      return 0;
    }
    if (id == kExtnDisk) {
      ExtnDisk_Access(address);
      return 0;
    }
  }
  return 0xFF;
}

bool MemAccessNtfy(ATTep p) {
  if (p->cmpvalu == 0 && MemOverlay) {
    p->usebase = ROM;
    p->usemask = kROM_Size - 1;
    p->Access = kATTA_readreadymask;
    return true;
  }
  return false;
}

void ReportAbnormalID(uint16_t id, const char* msg) {
  fprintf(stderr, "Abnormal ID: %08x: %s\n", (unsigned int)id, msg);
}

void Sony_Reset(void) {}
void Extn_Reset(void) {}
void Memory_Reset(void) {}

// Functions from MoiraStubs.c

ATTep FindATTel(uint32_t addr) {
  for (unsigned int i = 0; i < (unsigned int)LastATTel; ++i) {
    if ((addr & ATTListA[i].cmpmask) == ATTListA[i].cmpvalu) {
      return &ATTListA[i];
    }
  }
  return nullptr;
}

extern int32_t g_cyclesRemaining;  // defined in main.cc
uint32_t GetCuriCount(void) { return NextiCount - g_cyclesRemaining; }

uint8_t* get_real_address0(uint32_t count, bool is_write, uint32_t address,
                           uint32_t* contig) {
  ATTep p = FindATTel(address);
  if (p &&
      (p->Access & (is_write ? kATTA_writereadymask : kATTA_readreadymask))) {
    uint32_t offset = address & p->usemask;
    uint32_t available = (p->usemask + 1) - offset;
    if (available >= count) {
      *contig = count;
    } else {
      *contig = available;
    }
    return p->usebase + offset;
  }
  *contig = 0;
  return nullptr;
}

// Empty stubs
void InitKeyCodes(void) {}
void ScreenClearChanges(void) {}
void ExtnDisk_Access(uint32_t pb_address) {}
void PbufNewNotify(uint32_t index, uint32_t count) {}
void PbufDisposeNotify(uint16_t ref_num) {}
bool PbufIsAllocated(uint32_t index) { return false; }
bool FirstFreePbuf(uint16_t* index) { return false; }
bool vSonyIsInserted(uint32_t drive_no) { return true; }
void DisconnectKeyCodes(uint32_t mask) {}
void m68k_reset_extra(void) {}
void SetHeadATTel(ATTep p) {}

void VIAorSCCinterruptChngNtfy(void) {
  uint8_t NewIPL =
      (VIA1_InterruptRequest ? 1 : 0) | (SCCInterruptRequest ? 2 : 0);
  if (NewIPL != CurIPL) {
    CurIPL = NewIPL;
    m68k_IPLchangeNtfy();
  }
}

}  // extern "C"
