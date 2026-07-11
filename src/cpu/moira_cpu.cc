#include "src/cpu/moira_cpu.h"

#include <cstdio>
#include <cstring>

#include "src/debug_log.h"
#include "src/devices/iwm.h"
#include "src/devices/scc.h"
#include "src/devices/scsi.h"
#include "src/devices/sony.h"
#include "src/devices/via.h"
#include "src/keycodes.h"
#include "src/system.h"

using namespace moira;

// Redirect device access functions to instance methods using g_system
#define VIA1_Access(data, write, addr) \
  (g_system->GetViaDevice()->Access(data, write, addr))
#define SCC_Access(data, write, addr) \
  (g_system->GetSccDevice()->Access(data, write, addr))
#define IWM_Access(data, write, addr) \
  (g_system->GetIwmDevice()->Access(data, write, addr))
#define SCSI_Access(data, write, addr) \
  (g_system->GetScsiDevice()->Access(data, write, addr))
#define ExtnSony_Access(addr) (g_system->GetSonyDevice()->ExtnAccess(addr))

#define DEBUG_VERBOSE 0
#ifndef kcom_callcheck
#define kcom_callcheck 0x4E75
#endif

// External declarations of C functions from stubs/platform
extern "C" {
uint32_t GetDriveSizeInSectors(uint32_t Drive_No);
void VIA1_SetInterruptFlag(uint8_t VIA_Int);
ATTep FindATTel(uint32_t addr);
uint32_t MMDV_Access(ATTep p, uint32_t Data, bool WriteMem, bool ByteSize,
                     uint32_t addr);
bool MemAccessNtfy(ATTep p);
}

MoiraCpu::MoiraCpu(MacSystem* system) : system_(system) {
  setModel(moira::Model::M68000);
}

void MoiraCpu::add_pc_history(moira::u32 pc) {
  pc_history_[pc_history_idx_] = pc;
  pc_history_idx_ = (pc_history_idx_ + 1) % kPcHistSize;
}

void MoiraCpu::dump_pc_history() const {
  DLOG( "PC History:\n");
  int idx = pc_history_idx_;
  for (int i = 0; i < kPcHistSize; i++) {
    DLOG( "  %08x\n", pc_history_[idx]);
    idx = (idx + 1) % kPcHistSize;
  }
}

moira::u8 MoiraCpu::read8(moira::u32 addr) const {
  addr &= 0x00FFFFFF;
  moira::u8 val = 0xFF;
  if (MemOverlay && addr < kROM_Size) {
    val = ROM[addr];
    goto Label_Log;
  }
  if ((addr & 0x00F00000) == 0x00500000) {
    moira::u32 offset = addr & 0x000F0000;
    if (offset == 0x00000000) {
      if ((addr & 0x0000F000) == 0x0000F000)
        return (moira::u8)VIA1_Access(0, false, addr);
      else
        return (moira::u8)SCSI_Access(0, false, addr);
    }
    if (offset == 0x00080000) return (moira::u8)SCC_Access(0, false, addr);
    if (offset == 0x000F0000)
      return (moira::u8)IWM_Access(0, false, addr & 0xF);
  }
  if ((addr & 0xE00000) == 0xA00000) return 0xFF;

  // SCC (serial) is not emulated; return status bits that satisfy the ROM's
  // serial initialization (transmit-buffer-empty) so boot does not stall.
  if ((addr & 0xE00000) == 0x800000) {
#if DEBUG_VERBOSE
    DLOG( "SCC READ: addr=%08x\n", addr);
#endif
    if ((addr & 0x1FFFFF) == 0x1FFFF8) return 0x04;  // Tx empty
    if ((addr & 0x1FFFFF) == 0x1FFFFA) return 0x04;  // Tx empty
    if ((addr & 0x1FFFFF) == 0x1FFFFC) return 0x00;
    if ((addr & 0x1FFFFF) == 0x1FFFFE) return 0x00;
    return 0x04;
  }

Label_Retry:
  {
    ATTep p = FindATTel(addr);
    if (p) {
      if (p->Access & kATTA_readreadymask)
        val = p->usebase[addr & p->usemask];
      else if (p->Access & kATTA_mmdvmask)
        val = (moira::u8)MMDV_Access(p, 0, false, true, addr);
      else if (p->Access & kATTA_ntfymask)
        if (MemAccessNtfy(p)) goto Label_Retry;
    }
  }
Label_Log:
  if (addr >= 0x4022de && addr <= 0x4022eb) {
    DLOG( "ROM READ at %08x: val=%02x PC=%08x\n", addr, val, getPC());
  }
  return val;
}

void MoiraCpu::write8(moira::u32 addr, moira::u8 val) const {
  addr &= 0x00FFFFFF;
#if DEBUG_VERBOSE
  if (addr >= 0x400000) {
    DLOG( "IO WRITE: addr=%08x val=%02x PC=%08x OPCODE=%04x\n", addr,
            val, getPC0(), read16(getPC0()));
    if (addr == 0x00efe1fe || addr == 0x00bfffff || addr == 0x009ffff8) {
      DLOG( "ORB/IWM/SCC WRITE TRACE: ");
      for (int i = 0; i < kPcHistSize; i++) {
        int idx = (pc_history_idx_ + i) % kPcHistSize;
        DLOG( "%08x ", pc_history_[idx]);
      }
      DLOG( "\n");
    }
  }
#endif
  if ((addr & 0x00F00000) == 0x00500000) {
    moira::u32 offset = addr & 0x000F0000;
    if (offset == 0x00000000) {
      if ((addr & 0x0000F000) == 0x0000F000)
        VIA1_Access(val, true, addr);
      else
        SCSI_Access(val, true, addr);
      return;
    }
    if (offset == 0x00080000) {
      SCC_Access(val, true, addr);
      return;
    }
    if (offset == 0x000F0000) {
      IWM_Access(val, true, addr & 0xF);
      return;
    }
  }
Label_Retry:
  ATTep p = FindATTel(addr);
  if (p) {
    if (p->Access & kATTA_writereadymask) {
      p->usebase[addr & p->usemask] = val;
      return;
    }
    if (p->Access & kATTA_mmdvmask) {
      MMDV_Access(p, (uint32_t)val, true, true, addr);
      return;
    }
    if (p->Access & kATTA_ntfymask)
      if (MemAccessNtfy(p)) goto Label_Retry;
  }
}

moira::u16 MoiraCpu::read16(moira::u32 addr) const {
  addr &= 0x00FFFFFF;
  if ((addr & 0x00FF0000) == kExtn_Block_Base) {
    moira::u32 offset = addr & 0xFFFF;
    if (offset == 4) return ext_cmd_;
    if (offset == 6) return ext_result_;
    if (offset == 8) return ext_a0_ >> 16;
    if (offset == 10) return ext_a0_ & 0xFFFF;
    if (offset == 12) return ext_a1_ >> 16;
    if (offset == 14) return ext_a1_ & 0xFFFF;
  }
  if (MemOverlay && addr < kROM_Size)
    return (moira::u16(ROM[addr]) << 8) | ROM[addr + 1];
  if (addr == 0x402264) return 0x4e71;
  if ((addr & 0x00F00000) == 0x00500000) {
    moira::u32 offset = addr & 0x000F0000;
    if (offset == 0x00000000) {
      if ((addr & 0x0000F000) == 0x0000F000)
        return (moira::u16)VIA1_Access(0, false, addr);
      else
        return (moira::u16)SCSI_Access(0, false, addr);
    }
    if (offset == 0x00080000) return (moira::u16)SCC_Access(0, false, addr);
    if (offset == 0x000F0000)
      return (moira::u16)IWM_Access(0, false, addr & 0xF);
  }
  if (addr == 0x1db4 || addr == 0x1db6) {
    DLOG("READ FROM dCtlDriver: addr=%08x PC=%08x\n", addr, getPC());
  }
  ATTep p = FindATTel(addr);
  if (p) {
    if (p->Access & kATTA_readreadymask) {
      moira::u8* m = &p->usebase[addr & p->usemask];
      if (addr == 0x402f28) {
        DLOG( "READ FROM 0x402f28: val=%04x PC=%08x\n",
                (moira::u16)((m[0] << 8) | m[1]), getPC());
      }
      if (addr == 0x3fffec || addr == 0x3fffee) {
        DLOG( "STACK READ: addr=%08x host=%p val=%04x PC=%08x\n",
                addr, (void*)m, (moira::u16)((m[0] << 8) | m[1]), getPC());
      }
      return (moira::u16(m[0]) << 8) | m[1];
    }
    if (p->Access & kATTA_mmdvmask)
      return (moira::u16)MMDV_Access(p, 0, false, false, addr);
    if (p->Access & kATTA_ntfymask)
      if (MemAccessNtfy(p)) return read16(addr);
  }
  if (addr < 0x400000)
    DLOG( "read16 FAILED: addr=%08x PC=%08x\n", addr, getPC());
  return 0xFFFF;
}

void MoiraCpu::write16(moira::u32 addr, moira::u16 val) const {
  addr &= 0x00FFFFFF;
  if ((addr & 0x00FF0000) == kExtn_Block_Base) {
    moira::u32 offset = addr & 0xFFFF;
    if (offset == 4) {
      ext_cmd_ = val;
      return;
    }
    if (offset == 6) {
      ext_result_ = val;
      return;
    }
    if (offset == 8) {
      ext_a0_ = (ext_a0_ & 0xFFFF) | (val << 16);
      return;
    }
    if (offset == 10) {
      ext_a0_ = (ext_a0_ & 0xFFFF0000) | val;
      return;
    }
    if (offset == 12) {
      ext_a1_ = (ext_a1_ & 0xFFFF) | (val << 16);
      return;
    }
    if (offset == 14) {
      ext_a1_ = (ext_a1_ & 0xFFFF0000) | val;
      return;
    }
  }
  if (addr == 0x3fffec || addr == 0x3fffee) {
    DLOG( "STACK WRITE: addr=%08x val=%04x PC=%08x\n", addr, val,
            getPC());
  }
  if (addr == 0x3fff2 || addr == 0x3ff86 || addr == 0x3ff82) {
    DLOG( "TRAP RETURN D0: addr=%08x val=%04x PC=%08x\n",
            (unsigned int)addr, (unsigned int)val, (unsigned int)getPC());
  }
  if (addr >= 0x400000 && addr < 0x440000) {
    DLOG(
            "ROM WRITE: addr=%08x val=%04x PC=%08x opcode=%04x SP=%08x\n",
            (unsigned int)addr, (unsigned int)val, (unsigned int)getPC(),
            (unsigned int)read16(getPC()), (unsigned int)getA(7));
  }
  if ((addr & 0x00F00000) == 0x00500000) {
    moira::u32 offset = addr & 0x000F0000;
    if (offset == 0x00000000) {
      if ((addr & 0x0000F000) == 0x0000F000)
        VIA1_Access(val, true, addr);
      else
        SCSI_Access(val, true, addr);
      return;
    }
    if (offset == 0x00080000) {
      SCC_Access(val, true, addr);
      return;
    }
    if (offset == 0x000F0000) {
      IWM_Access(val, true, addr & 0xF);
      return;
    }
  }
Label_Retry:
  ATTep p = FindATTel(addr);
  if (p) {
    if (p->Access & kATTA_writereadymask) {
      moira::u8* m = &p->usebase[addr & p->usemask];
      m[0] = (moira::u8)(val >> 8);
      m[1] = (moira::u8)val;
      return;
    }
    if (p->Access & kATTA_mmdvmask) {
      MMDV_Access(p, (uint32_t)val, true, false, addr);
      return;
    }
    if (p->Access & kATTA_ntfymask)
      if (MemAccessNtfy(p)) goto Label_Retry;
  }
}

void MoiraCpu::didJumpToVector(int nr, moira::u32 addr) {
  // Hook for exception/trap vectors. Nothing is required here for normal
  // operation; kept as a place to attach diagnostics when debugging.
  (void)nr;
  (void)addr;
}

void MoiraCpu::sync(int cycles) {
  setClock(getClock() + cycles);
  vblTimer += cycles;
  if (vblTimer >= 133333) {
    vblTimer -= 133333;
    VIA1_SetInterruptFlag(kIntCA1);
  }
}

void MoiraCpu::willExecute(const char* func, Instr I, Mode M, Size S,
                           u16 opcode) {
  instr_count_++;
  current_opcode_ = opcode;
  current_pc_ = getPC0();

  // _AddDrive trap ($A04E): the Sony driver queries the drive geometry when a
  // disk is mounted. Fill in the sector count for the floppy and hard drives.
  if ((opcode & 0xF000) == 0xA000 && (opcode & 0x0FFF) == 0x004E) {
    u32 qel = getA(0);
    short refNum = (short)(getD(0) & 0xFFFF);
    if (refNum == -5) {
      u16 old_size = read16(qel + 12);
      u16 drive_num = getD(0) >> 16;
      if (drive_num < 3 && old_size == 0) {
        write16(qel + 12, 1600);
      } else if (drive_num >= 3 && old_size == 0) {
        uint32_t host_size = GetDriveSizeInSectors(drive_num - 1);
        if (host_size > 0) {
          write16(qel + 12, (u16)host_size);
        } else {
          DLOG(
                  "WARNING: GetDriveSizeInSectors returned 0 for drive %d\n",
                  drive_num);
        }
      }
    }
  }
}

void MoiraCpu::didExecute(const char* func, Instr I, Mode M, Size S,
                          u16 opcode) {
  if (I == Instr::RESET) WantMacReset = true;
  last_pcs_[pc_idx_] = getPC0();
  pc_idx_ = (pc_idx_ + 1) % 20;
}

void MoiraCpu::willExecute(M68kException exc, u16 vector) {}

void MoiraCpu::didExecute(M68kException exc, u16 vector) {}

// C Bridge implementation

MoiraCpu* g_cpu_mac = nullptr;
typedef int int32_t;
extern int32_t g_cyclesRemaining;
static uint8_t* g_fIPL = nullptr;

extern "C" {

void MINEM68K_Init(uint8_t* fIPL) {
  if (!g_cpu_mac) {
    g_cpu_mac = new MoiraCpu(g_system);
  }
  g_fIPL = fIPL;
}

void m68k_reset(void) {
  if (g_cpu_mac) {
    g_cyclesRemaining = 0;
    g_cpu_mac->reset();
    // MemOverlay = 1; // MemOverlay is redirected to Wires
    g_system->get_wires()[Wire_MemOverlay] = 1;
    DLOG( "m68k_reset: Registers initialized, Overlay on\n");
  }
}

void m68k_go_nCycles(uint32_t n) {
  if (!g_cpu_mac) return;
  uint32_t datp_vm = 0x400000 + 0x3f000 + MacSystem::kSonyDriverSize;
  uint32_t jdisk_target = datp_vm + 16;
  g_cpu_mac->write16(0x316, jdisk_target >> 16);
  g_cpu_mac->write16(0x318, jdisk_target & 0xffff);
  g_cpu_mac->write16(0x54c, 0x0040);
  g_cpu_mac->write16(0x54e, 0x11a8);
  g_cpu_mac->write16(0x824, 0x003f);
  g_cpu_mac->write16(0x826, 0xa700);

  g_cyclesRemaining += (int32_t)n;
  static int calls = 0;
  calls++;

  while (g_cyclesRemaining > 0) {
    moira::u32 pc = g_cpu_mac->getPC0() & 0x00FFFFFF;
    g_cpu_mac->add_pc_history(pc);

    moira::u16 opcode = g_cpu_mac->read16(g_cpu_mac->getPC());
    if (opcode == kcom_callcheck) {
      moira::u32 pc = g_cpu_mac->getPC() & 0x00FFFFFF;
      int cmd = 0;
      if (pc == 0x43F0EE)
        cmd = kCmndSonyOpenA;
      else if (pc == 0x43F018)
        cmd = kCmndSonyPrime;
      else if (pc == 0x43F024)
        cmd = kCmndSonyControl;
      else if (pc == 0x43F04A)
        cmd = kCmndSonyStatus;
      else if (pc == 0x43F08A)
        cmd = kCmndSonyClose;

      if (cmd != 0) {
        moira::u32 pb = g_cpu_mac->getA(0);
        moira::u32 dce = g_cpu_mac->getA(1) & 0x00FFFFFF;
        DLOG("Sony Driver Intercept: PC=%08x cmd=%d A0=%08x A1=%08x\n",
             (unsigned int)pc, cmd, (unsigned int)pb,
             (unsigned int)g_cpu_mac->getA(1));

        moira::u32 p = kExtn_Block_Base;
        g_cpu_mac->write16(p + 4, cmd);
        g_cpu_mac->write16(p + 8, g_cpu_mac->getA(0) >> 16);
        g_cpu_mac->write16(p + 10, g_cpu_mac->getA(0) & 0xFFFF);
        g_cpu_mac->write16(p + 12, g_cpu_mac->getA(1) >> 16);
        g_cpu_mac->write16(p + 14, g_cpu_mac->getA(1) & 0xFFFF);

        ExtnSony_Access(p);

        MacErr result = (MacErr)g_cpu_mac->read16(p + 6);
        g_cpu_mac->setD(0, (u32)(int16_t)result);
        g_cpu_mac->write16(pb + 16, result);

        moira::u16 dce_flags = g_cpu_mac->read16(dce + 4);
        dce_flags &= ~0x0080;
        g_cpu_mac->write16(dce + 4, dce_flags);

        DLOG("Sony Driver Intercept RETURN: pb=%08x result=%04x dce=%08x\n", pb,
             result, dce);

        g_cpu_mac->setD(0, (u32)(int16_t)result);
        g_cpu_mac->setA(1, dce);

        moira::u32 iodone_addr = 0x402e06;
        g_cpu_mac->setPC(iodone_addr);
        g_cpu_mac->setPC0(iodone_addr);
        g_cpu_mac->setIRD(g_cpu_mac->read16(iodone_addr));
        g_cpu_mac->setIRC(g_cpu_mac->read16(iodone_addr + 2));
        continue;
      }
    }
    g_cpu_mac->increment_instr_count();
    if (g_cpu_mac->getIPL() != *g_fIPL) g_cpu_mac->setIPL(*g_fIPL);
    if ((g_cpu_mac->getPC() & 0x00FFFFFF) == 0x402264)
      g_cpu_mac->setD(7, 0xFFFFFFFF);
    long long startClock = g_cpu_mac->getClock();
    try {
      g_cpu_mac->execute();
    } catch (...) {
      break;
    }
    long long elapsed = g_cpu_mac->getClock() - startClock;
    if (elapsed <= 0) elapsed = 2;
    // Wires / kCycleScale logic
    g_cyclesRemaining -=
        (int32_t)(elapsed * kMyClockMult * (1 << 6));  // kLn2CycleScale is 6
  }
}

int32_t GetCyclesRemaining(void) { return g_cyclesRemaining; }
void SetCyclesRemaining(int32_t n) { g_cyclesRemaining = n; }

void m68k_IPLchangeNtfy(void) {
  if (g_cpu_mac && g_fIPL) {
    g_cpu_mac->setIPL(*g_fIPL);
  }
}

uint32_t get_vm_pc(void) {
  return g_cpu_mac ? (uint32_t)g_cpu_mac->getPC0() : 0;
}
uint8_t get_vm_byte(uint32_t addr) { return g_cpu_mac->read8(addr); }
uint16_t get_vm_word(uint32_t addr) { return g_cpu_mac->read16(addr); }
uint32_t get_vm_long(uint32_t addr) {
  return ((uint32_t)g_cpu_mac->read16(addr) << 16) |
         g_cpu_mac->read16(addr + 2);
}
void put_vm_byte(uint32_t addr, uint8_t val) { g_cpu_mac->write8(addr, val); }
void put_vm_word(uint32_t addr, uint16_t val) {
  g_cpu_mac->write16(addr, (unsigned short)val);
}
void put_vm_long(uint32_t addr, uint32_t val) {
  g_cpu_mac->write16(addr, (unsigned short)(val >> 16));
  g_cpu_mac->write16(addr + 2, (unsigned short)val);
}

// Key/Mouse injection bridge helpers (since we undefed Wires they might need to
// use get_wires() if they access Wires. But they don't access Wires directly in
// this file, they call Keyboard_UpdateKeyMap which is in main.cc / adb.cc. And
// they call MyMousePositionSet etc. which are also exported. So we just call
// them directly.

}  // extern "C"
