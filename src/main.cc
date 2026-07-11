#undef _VIA_Debug
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <cstdint>
#include <memory>

#include "src/cpu/moira_cpu.h"
#include "src/debug_log.h"
#include "src/devices/rom.h"
#include "src/devices/sony.h"
#include "src/devices/via.h"
#include "src/platform/x11_platform_creation.h"
#include "src/system.h"

#define AllFiles 1
#define CheckRomCheckSum 0

extern "C" {
#include "src/config.h"

#ifndef kcom_callcheck
#define kcom_callcheck 0x4E75
#endif

int32_t g_cyclesRemaining = 0;

// Prototypes for functions that will be defined later or in included C files
void ProgramMain(void);

void SetUpMemBanks(void);
uint32_t MMDV_Access(ATTep p, uint32_t data, bool is_write, bool is_byte_size,
                     uint32_t address);
bool MemAccessNtfy(ATTep p);

ATTep FindATTel(uint32_t address);
uint32_t SCSI_Access(uint32_t data, bool is_write, uint32_t address);
uint32_t SCC_Access(uint32_t data, bool is_write, uint32_t address);
uint32_t IWM_Access(uint32_t data, bool is_write, uint32_t address);
uint32_t VIA1_Access(uint32_t data, bool is_write, uint32_t address);
void ExtnSony_Access(uint32_t pb_address);
void ExtnDisk_Access(uint32_t pb_address);
void put_vm_word(uint32_t address, uint16_t value);
void put_vm_long(uint32_t address, uint32_t value);
uint16_t get_vm_word(uint32_t address);
void ICT_add(int task_id, uint32_t when);
void ICT_Zap(void);
void SetInterruptButton(bool value);
bool AddrSpac_Init(void);
void MemOverlay_ChangeNtfy(void);
void VIA1_SetInterruptFlag(uint8_t via_int) {
  g_system->GetViaDevice()->SetInterruptFlag(via_int);
}

void do_put_mem_word(uint8_t* ptr, uint16_t value) {
  ptr[0] = (value >> 8);
  ptr[1] = (uint8_t)value;
}
void do_put_mem_long(uint8_t* ptr, uint32_t value) {
  ptr[0] = (value >> 24);
  ptr[1] = (value >> 16);
  ptr[2] = (value >> 8);
  ptr[3] = (uint8_t)value;
}
uint16_t do_get_mem_word(uint8_t* ptr) {
  return ((uint16_t)ptr[0] << 8) | ptr[1];
}
uint32_t do_get_mem_long(uint8_t* ptr) {
  return ((uint32_t)ptr[0] << 24) | ((uint32_t)ptr[1] << 16) |
         ((uint32_t)ptr[2] << 8) | ptr[3];
}

void DiskInsertNotify(uint16_t drive_no, bool is_locked) {
  DLOG("DiskInsertNotify: drive_no=%d is_locked=%d\n", (int)drive_no,
       (int)is_locked);
  if (g_system && g_system->GetSonyDevice()) {
    g_system->GetSonyDevice()->AddInsertedDisk(drive_no);
    g_system->GetSonyDevice()->SetWritable(drive_no, !is_locked);
  }
}

void DiskEjectedNotify(uint16_t drive_no) {
  if (g_system && g_system->GetSonyDevice()) {
    g_system->GetSonyDevice()->RemoveInsertedDisk(drive_no);
  }
}

bool AnyDiskInserted(void) {
  if (g_system && g_system->GetSonyDevice()) {
    return g_system->GetSonyDevice()->GetInsertedMask() != 0;
  }
  return false;
}

uint8_t* get_ram_address(unsigned int address) {
  return RAM + (address & 0x00FFFFFF);
}
uint8_t get_ram_byte(unsigned int address) { return RAM[address & 0x00FFFFFF]; }
void put_ram_byte(unsigned int address, uint8_t value) {
  RAM[address & 0x00FFFFFF] = value;
}
uint16_t get_ram_word(unsigned int address) {
  return do_get_mem_word(RAM + (address & 0x00FFFFFF));
}
void put_ram_word(unsigned int address, uint16_t value) {
  do_put_mem_word(RAM + (address & 0x00FFFFFF), value);
}
uint32_t get_ram_long(unsigned int address) {
  return do_get_mem_long(RAM + (address & 0x00FFFFFF));
}
void put_ram_long(unsigned int address, uint32_t value) {
  do_put_mem_long(RAM + (address & 0x00FFFFFF), value);
}

// ROMEMDEV.c migrated to C++ RomDevice

bool AddrSpac_Init(void) {
  DLOG("AddrSpac_Init called\n");
  VIA1_iA4 = 1;
  MemOverlay = 1;
  VIA1_iB3 = 1;
  VIA1_iB4 = 1;
  VIA1_iB5 = 1;
  VIA1_iCB2 = 1;

  SetUpMemBanks();

  // ROM patches from MoiraBridge.cpp
  if (ROM) {
    DLOG("Applying ROM patches...\n");
    ROM[0x26da] = 0x4e;
    ROM[0x26db] = 0x71;
    ROM[0x26dc] = 0x4e;
    ROM[0x26dd] = 0x71;
    ROM[0x26de] = 0x4e;
    ROM[0x26df] = 0x71;
    ROM[0x26e0] = 0x4e;
    ROM[0x26e1] = 0x71;

    // Bypass IWM initialization in Sony driver Open
    ROM[0x3530c] = 0x4e;
    ROM[0x3530d] = 0x75;  // RTS
    ROM[0x35316] = 0x4e;
    ROM[0x35317] = 0x75;  // RTS
    ROM[0x34ff6] = 0x4e;
    ROM[0x34ff7] = 0x75;  // RTS

    // Bypass floppy drive detection loop (Subroutine 41a426) -> return success
    // (moveq #0, D0; rts)
    ROM[0x1a426] = 0x70;
    ROM[0x1a427] = 0x00;
    ROM[0x1a428] = 0x4e;
    ROM[0x1a429] = 0x75;

    // Bypass floppy drive status poll loop (Subroutine 41a8bc) -> return
    // success (moveq #0, D0; rts)
    ROM[0x1a8bc] = 0x70;
    ROM[0x1a8bd] = 0x00;
    ROM[0x1a8be] = 0x4e;
    ROM[0x1a8bf] = 0x75;

    // Bypass floppy drive command loop (Subroutine 41a8e0) -> return success
    // (moveq #0, D0; rts)
    ROM[0x1a8e0] = 0x70;
    ROM[0x1a8e1] = 0x00;
    ROM[0x1a8e2] = 0x4e;
    ROM[0x1a8e3] = 0x75;

    // Bypass floppy drive status check (Subroutine 41a794) -> return success
    // (moveq #0, D0; rts)
    ROM[0x1a794] = 0x70;
    ROM[0x1a795] = 0x00;
    ROM[0x1a796] = 0x4e;
    ROM[0x1a797] = 0x75;

    // Bypass floppy drive status check 2 (Subroutine 41a7bc) -> return success
    // (moveq #0, D0; rts)
    ROM[0x1a7bc] = 0x70;
    ROM[0x1a7bd] = 0x00;
    ROM[0x1a7be] = 0x4e;
    ROM[0x1a7bf] = 0x75;

    // Force drive connection check (Subroutine 434fc4) to return success (moveq
    // #0, D0; rts)
    ROM[0x34fc4] = 0x70;
    ROM[0x34fc5] = 0x00;
    ROM[0x34fc6] = 0x4e;
    ROM[0x34fc7] = 0x75;
  }

  MINEM68K_Init(&CurIPL);
  return true;
}

void ICT_add(int task_id, uint32_t when) {
  if (task_id < kNumICTs) {
    ICTwhen[task_id] = NextiCount + when;
    ICTactive |= (1 << task_id);
  }
}
void ICT_Zap(void) { ICTactive = 0; }
void SetInterruptButton(bool value) {}

void MINEM68K_Init(uint8_t* fIPL);
void m68k_reset(void);
void m68k_go_nCycles(uint32_t n);
void m68k_IPLchangeNtfy(void);

}  // extern "C"

#include <signal.h>

static void sigterm_handler(int signum) { exit(0); }

void MemOverlay_ChangeNtfy(void) {
  // The overlay maps the ROM over low memory at reset. Once the boot code has
  // switched it off from ROM, ignore any attempt to re-enable it from RAM.
  if (g_cpu_mac && g_cpu_mac->getPC() < 0x400000 && VIA1_iA4 != 0) {
    DLOG("PREVENTED MemOverlay change to %d from RAM PC=%08x\n", VIA1_iA4,
         g_cpu_mac->getPC());
    return;
  }
  MemOverlay = VIA1_iA4;
  DLOG("MemOverlay changed to %d\n", (int)MemOverlay);
}

int main(int argc, char** argv) {
  signal(SIGTERM, sigterm_handler);
  auto system_owner = std::make_unique<MacSystem>();
  g_system = system_owner.get();
  g_system->SetPlatform(CreateX11Platform(g_system));
  g_system->Init();
  if (g_system->GetPlatform()->Init(argc, argv)) {
    ProgramMain();
  }
  return 0;
}
