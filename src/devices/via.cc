#include "src/devices/via.h"
#include "src/debug_log.h"

#include <stdio.h>
#include <string.h>

#include "src/system.h"

ViaDevice* g_via = nullptr;

// Undefine legacy macros that use Wires (g_system)
#undef VIA1_iA0
#undef VIA1_iA1
#undef VIA1_iA2
#undef VIA1_iA3
#undef VIA1_iA4
#undef VIA1_iA5
#undef VIA1_iA6
#undef VIA1_iA7

#undef VIA1_iB0
#undef VIA1_iB1
#undef VIA1_iB2
#undef VIA1_iB3
#undef VIA1_iB4
#undef VIA1_iB5
#undef VIA1_iB6
#undef VIA1_iB7

#undef VIA1_iCB2

#undef VIA1_InterruptRequest

// Redefine them using system_
#define VIA1_iA0 (system_->get_wires()[Wire_VIA1_iA0_SoundVolb0])
#define VIA1_iA1 (system_->get_wires()[Wire_VIA1_iA1_SoundVolb1])
#define VIA1_iA2 (system_->get_wires()[Wire_VIA1_iA2_SoundVolb2])
#define VIA1_iA3 (system_->get_wires()[Wire_VIA1_iA3_SCCvSync])
#define VIA1_iA4 (system_->get_wires()[Wire_VIA1_iA4_DriveSel])
#define VIA1_iA5 (system_->get_wires()[Wire_VIA1_iA5_IWMvSel])
#define VIA1_iA6 (system_->get_wires()[Wire_VIA1_iA6_SCRNvPage2])
#define VIA1_iA7 (system_->get_wires()[Wire_VIA1_iA7_SCCwaitrq])

#define VIA1_iB0 (system_->get_wires()[Wire_VIA1_iB0_RTCdataLine])
#define VIA1_iB1 (system_->get_wires()[Wire_VIA1_iB1_RTCclock])
#define VIA1_iB2 (system_->get_wires()[Wire_VIA1_iB2_RTCunEnabled])
#define VIA1_iB3 (system_->get_wires()[Wire_VIA1_iB3_ADB_Int])
#define VIA1_iB4 (system_->get_wires()[Wire_VIA1_iB4_ADB_st0])
#define VIA1_iB5 (system_->get_wires()[Wire_VIA1_iB5_ADB_st1])
#define VIA1_iB6 (system_->get_wires()[Wire_VIA1_iB6_SCSIintenable])
#define VIA1_iB7 (system_->get_wires()[Wire_VIA1_iB7_SoundDisable])

#define VIA1_iCB2 (system_->get_wires()[Wire_VIA1_iCB2_ADB_Data])

#define VIA1_InterruptRequest (system_->get_wires()[Wire_VIA1_InterruptRequest])

// Declare external notification functions
extern "C" {
void RTCdataLine_ChangeNtfy(void);
void RTCclock_ChangeNtfy(void);
void RTCunEnabled_ChangeNtfy(void);
void ADBstate_ChangeNtfy(void);
void ADB_DataLineChngNtfy(void);
void VIAorSCCinterruptChngNtfy(void);
void MemOverlay_ChangeNtfy(void);
}

#define VIA1_iA4_ChangeNtfy MemOverlay_ChangeNtfy

#define Ui3rPowOf2(p) (1 << (p))
#define Ui3rTestBit(i, p) (((i) & Ui3rPowOf2(p)) != 0)

#define VIA1_ORA_CanInOrOut (VIA1_ORA_CanIn | VIA1_ORA_CanOut)
#define VIA1_ORB_CanInOrOut (VIA1_ORB_CanIn | VIA1_ORB_CanOut)

#define ViaORcheckBit(p, x) \
  (Ui3rTestBit(selection, p) && ((v = (data >> p) & 1) != x))

#define CyclesPerViaTime (10 * kMyClockMult)
#define CyclesScaledPerViaTime (kCycleScale * CyclesPerViaTime)

#define kORB 0x00
#define kORA_H 0x01
#define kDDR_B 0x02
#define kDDR_A 0x03
#define kT1C_L 0x04
#define kT1C_H 0x05
#define kT1L_L 0x06
#define kT1L_H 0x07
#define kT2_L 0x08
#define kT2_H 0x09
#define kSR 0x0A
#define kACR 0x0B
#define kPCR 0x0C
#define kIFR 0x0D
#define kIER 0x0E
#define kORA 0x0F

extern "C" uint32_t get_vm_pc(void);

ViaDevice::ViaDevice(MacSystem* system) : system_(system) {
  g_via = this;
  Zap();
}

ViaDevice::~ViaDevice() {
  if (g_via == this) {
    g_via = nullptr;
  }
}

void ViaDevice::Clear() {
  d_.ora = 0;
  d_.ddr_a = 0;
  d_.orb = 0;
  d_.ddr_b = 0;
  d_.t1l_l = d_.t1l_h = 0x00;
  d_.t2l_l = 0x00;
  d_.t1c_f = 0;
  d_.t2c_f = 0;
  d_.sr = d_.acr = 0x00;
  d_.pcr = d_.ifr = d_.ier = 0x00;
  t1_active_ = t2_active_ = 0x00;
  t1_int_ready_ = false;
  t1_running_ = true;
  t1_last_time_ = 0;
  t2_running_ = true;
  t2c_short_time_ = false;
  t2_last_time_ = 0;
}

void ViaDevice::Zap() {
  Clear();
  VIA1_InterruptRequest = 0;
}

void ViaDevice::Reset() {
  SetDDR_A(0);
  SetDDR_B(0);
  Clear();
  CheckInterruptFlag();
}

uint8_t ViaDevice::Get_ORA(uint8_t selection) {
  uint8_t Value = (~VIA1_ORA_CanIn) & selection & VIA1_ORA_FloatVal;

#if Ui3rTestBit(VIA1_ORA_CanIn, 7)
  if (Ui3rTestBit(selection, 7)) {
    Value |= (VIA1_iA7 << 7);
  }
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 6)
  if (Ui3rTestBit(selection, 6)) {
    Value |= (VIA1_iA6 << 6);
  }
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 5)
  if (Ui3rTestBit(selection, 5)) {
    Value |= (VIA1_iA5 << 5);
  }
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 4)
  if (Ui3rTestBit(selection, 4)) {
    Value |= (VIA1_iA4 << 4);
  }
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 3)
  if (Ui3rTestBit(selection, 3)) {
    Value |= (VIA1_iA3 << 3);
  }
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 2)
  if (Ui3rTestBit(selection, 2)) {
    Value |= (VIA1_iA2 << 2);
  }
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 1)
  if (Ui3rTestBit(selection, 1)) {
    Value |= (VIA1_iA1 << 1);
  }
#endif
#if Ui3rTestBit(VIA1_ORA_CanIn, 0)
  if (Ui3rTestBit(selection, 0)) {
    Value |= (VIA1_iA0 << 0);
  }
#endif

  return Value;
}

uint8_t ViaDevice::Get_ORB(uint8_t selection) {
  uint8_t Value = (~VIA1_ORB_CanIn) & selection & VIA1_ORB_FloatVal;

#if Ui3rTestBit(VIA1_ORB_CanIn, 7)
  if (Ui3rTestBit(selection, 7)) {
    Value |= (VIA1_iB7 << 7);
  }
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 6)
  if (Ui3rTestBit(selection, 6)) {
    Value |= (VIA1_iB6 << 6);
  }
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 5)
  if (Ui3rTestBit(selection, 5)) {
    Value |= (VIA1_iB5 << 5);
  }
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 4)
  if (Ui3rTestBit(selection, 4)) {
    Value |= (VIA1_iB4 << 4);
  }
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 3)
  if (Ui3rTestBit(selection, 3)) {
    Value |= (VIA1_iB3 << 3);
  }
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 2)
  if (Ui3rTestBit(selection, 2)) {
    Value |= (VIA1_iB2 << 2);
  }
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 1)
  if (Ui3rTestBit(selection, 1)) {
    Value |= (VIA1_iB1 << 1);
  }
#endif
#if Ui3rTestBit(VIA1_ORB_CanIn, 0)
  if (Ui3rTestBit(selection, 0)) {
    Value |= (VIA1_iB0 << 0);
  }
#endif

  return Value;
}

void ViaDevice::Put_ORA(uint8_t selection, uint8_t data) {
#if 0 != VIA1_ORA_CanOut
    uint8_t v;
#endif

#if Ui3rTestBit(VIA1_ORA_CanOut, 7)
  if (ViaORcheckBit(7, VIA1_iA7)) {
    VIA1_iA7 = v;
#ifdef VIA1_iA7_ChangeNtfy
    VIA1_iA7_ChangeNtfy();
#endif
  }
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 6)
  if (ViaORcheckBit(6, VIA1_iA6)) {
    VIA1_iA6 = v;
#ifdef VIA1_iA6_ChangeNtfy
    VIA1_iA6_ChangeNtfy();
#endif
  }
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 5)
  if (ViaORcheckBit(5, VIA1_iA5)) {
    VIA1_iA5 = v;
#ifdef VIA1_iA5_ChangeNtfy
    VIA1_iA5_ChangeNtfy();
#endif
  }
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 4)
  if (ViaORcheckBit(4, VIA1_iA4)) {
    VIA1_iA4 = v;
#ifdef VIA1_iA4_ChangeNtfy
    VIA1_iA4_ChangeNtfy();
#endif
  }
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 3)
  if (ViaORcheckBit(3, VIA1_iA3)) {
    VIA1_iA3 = v;
#ifdef VIA1_iA3_ChangeNtfy
    VIA1_iA3_ChangeNtfy();
#endif
  }
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 2)
  if (ViaORcheckBit(2, VIA1_iA2)) {
    VIA1_iA2 = v;
#ifdef VIA1_iA2_ChangeNtfy
    VIA1_iA2_ChangeNtfy();
#endif
  }
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 1)
  if (ViaORcheckBit(1, VIA1_iA1)) {
    VIA1_iA1 = v;
#ifdef VIA1_iA1_ChangeNtfy
    VIA1_iA1_ChangeNtfy();
#endif
  }
#endif
#if Ui3rTestBit(VIA1_ORA_CanOut, 0)
  if (ViaORcheckBit(0, VIA1_iA0)) {
    VIA1_iA0 = v;
#ifdef VIA1_iA0_ChangeNtfy
    VIA1_iA0_ChangeNtfy();
#endif
  }
#endif
}

void ViaDevice::Put_ORB(uint8_t selection, uint8_t data) {
#if 0 != VIA1_ORB_CanOut
    uint8_t v;
#endif

#if Ui3rTestBit(VIA1_ORB_CanOut, 7)
  if (ViaORcheckBit(7, VIA1_iB7)) {
    VIA1_iB7 = v;
#ifdef VIA1_iB7_ChangeNtfy
    VIA1_iB7_ChangeNtfy();
#endif
  }
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 6)
  if (ViaORcheckBit(6, VIA1_iB6)) {
    VIA1_iB6 = v;
#ifdef VIA1_iB6_ChangeNtfy
    VIA1_iB6_ChangeNtfy();
#endif
  }
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 5)
  {
    uint8_t new_v = (data >> 5) & 1;
    DLOG(
            "VIA1_Put_ORB bit 5: Selection=%d DDR_B=%02x old=%d new=%d\n",
            (int)Ui3rTestBit(selection, 5), (int)d_.ddr_b, (int)VIA1_iB5,
            (int)new_v);
    fflush(stderr);
    if (Ui3rTestBit(selection, 5) && (new_v != VIA1_iB5)) {
      DLOG( "VIA1_Put_ORB bit 5 CHANGED!\n");
      fflush(stderr);
      v = new_v;
      VIA1_iB5 = v;
#ifdef VIA1_iB5_ChangeNtfy
      VIA1_iB5_ChangeNtfy();
#endif
    }
  }
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 4)
  {
    uint8_t new_v = (data >> 4) & 1;
    DLOG(
            "VIA1_Put_ORB bit 4: Selection=%d DDR_B=%02x old=%d new=%d\n",
            (int)Ui3rTestBit(selection, 4), (int)d_.ddr_b, (int)VIA1_iB4,
            (int)new_v);
    fflush(stderr);
    if (Ui3rTestBit(selection, 4) && (new_v != VIA1_iB4)) {
      DLOG( "VIA1_Put_ORB bit 4 CHANGED!\n");
      fflush(stderr);
      v = new_v;
      VIA1_iB4 = v;
#ifdef VIA1_iB4_ChangeNtfy
      VIA1_iB4_ChangeNtfy();
#endif
    }
  }
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 3)
  if (ViaORcheckBit(3, VIA1_iB3)) {
    VIA1_iB3 = v;
#ifdef VIA1_iB3_ChangeNtfy
    VIA1_iB3_ChangeNtfy();
#endif
  }
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 2)
  if (ViaORcheckBit(2, VIA1_iB2)) {
    VIA1_iB2 = v;
#ifdef VIA1_iB2_ChangeNtfy
    VIA1_iB2_ChangeNtfy();
#endif
  }
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 1)
  if (ViaORcheckBit(1, VIA1_iB1)) {
    VIA1_iB1 = v;
#ifdef VIA1_iB1_ChangeNtfy
    VIA1_iB1_ChangeNtfy();
#endif
  }
#endif
#if Ui3rTestBit(VIA1_ORB_CanOut, 0)
  if (ViaORcheckBit(0, VIA1_iB0)) {
    VIA1_iB0 = v;
#ifdef VIA1_iB0_ChangeNtfy
    VIA1_iB0_ChangeNtfy();
#endif
  }
#endif
}

void ViaDevice::SetDDR_A(uint8_t data) {
  uint8_t floatbits = d_.ddr_a & ~data;
  uint8_t unfloatbits = data & ~d_.ddr_a;

  if (floatbits != 0) {
    Put_ORA(floatbits, VIA1_ORA_FloatVal);
  }
  d_.ddr_a = data;
  if (unfloatbits != 0) {
    Put_ORA(unfloatbits, d_.ora);
  }
  if ((data & ~VIA1_ORA_CanOut) != 0) {
    ReportAbnormalID(0x0401, "Set d_.ddr_a unexpected direction");
  }
}

void ViaDevice::SetDDR_B(uint8_t data) {
  uint8_t floatbits = d_.ddr_b & ~data;
  uint8_t unfloatbits = data & ~d_.ddr_b;

  if (floatbits != 0) {
    Put_ORB(floatbits, VIA1_ORB_FloatVal);
  }
  d_.ddr_b = data;
  if (unfloatbits != 0) {
    Put_ORB(unfloatbits, d_.orb);
  }
  if ((data & ~VIA1_ORB_CanOut) != 0) {
    ReportAbnormalID(0x0402, "Set d_.ddr_b unexpected direction");
  }
}

void ViaDevice::CheckInterruptFlag() {
  uint8_t NewInterruptRequest = ((d_.ifr & d_.ier) != 0) ? 1 : 0;
#ifdef _VIA_Debug
  DLOG(
          "VIA1_CheckInterruptFlag: IFR=%02x IER=%02x NewIRQ=%d OldIRQ=%d\n",
          (int)d_.ifr, (int)d_.ier, (int)NewInterruptRequest,
          (int)VIA1_InterruptRequest);
#endif

  if (NewInterruptRequest != VIA1_InterruptRequest) {
    VIA1_InterruptRequest = NewInterruptRequest;
#ifdef VIA1_interruptChngNtfy
    VIA1_interruptChngNtfy();
#endif
  }
}

void ViaDevice::ClrInterruptFlag(uint8_t VIA_Int) {
  d_.ifr &= ~((uint8_t)1 << VIA_Int);
  CheckInterruptFlag();
}

void ViaDevice::SetInterruptFlag(uint8_t VIA_Int) {
#ifdef _VIA_Debug
  DLOG( "VIA1_SetInterruptFlag: %d\n", (int)VIA_Int);
#endif
  d_.ifr |= ((uint8_t)1 << VIA_Int);
  CheckInterruptFlag();
}

void ViaDevice::CheckT1IntReady() {
  if (t1_running_) {
    bool NewT1IntReady = false;

    if ((d_.ifr & (1 << kIntT1)) == 0) {
      if (((d_.acr & 0x40) != 0) || (t1_active_ == 1)) {
        NewT1IntReady = true;
      }
    }

    if (t1_int_ready_ != NewT1IntReady) {
      t1_int_ready_ = NewT1IntReady;
      if (NewT1IntReady) {
        DoTimer1Check();
      }
    }
  }
}

void ViaDevice::DoTimer1Check() {
  if (t1_running_) {
    uint32_t NewTime = GetCuriCount();
    uint32_t deltaTime = (NewTime - t1_last_time_);
    if (deltaTime != 0) {
      uint32_t Temp = d_.t1c_f; /* Get Timer 1 Counter */
      uint32_t deltaTemp = (deltaTime / CyclesPerViaTime)
                           << (16 - kLn2CycleScale);
      uint32_t NewTemp = Temp - deltaTemp;
      if (deltaTime > (0x00010000UL * CyclesScaledPerViaTime) ||
          ((Temp <= deltaTemp) && (Temp != 0))) {
        if ((d_.acr & 0x40) != 0) {
          /* Reload Counter from Latches */
          uint16_t v = (d_.t1l_h << 8) + d_.t1l_l;
          uint16_t ntrans =
              1 + ((v == 0) ? 0 : (((deltaTemp - Temp) / v) >> 16));
          NewTemp += (((uint32_t)v * ntrans) << 16);
#if Ui3rTestBit(VIA1_ORB_CanOut, 7)
          if ((d_.acr & 0x80) != 0) {
            if ((ntrans & 1) != 0) {
              VIA1_iB7 ^= 1;
#ifdef VIA1_iB7_ChangeNtfy
              VIA1_iB7_ChangeNtfy();
#endif
            }
          }
#endif
          SetInterruptFlag(kIntT1);
        } else {
          if (t1_active_ == 1) {
            t1_active_ = 0;
            SetInterruptFlag(kIntT1);
          }
        }
      }

      d_.t1c_f = NewTemp;
      t1_last_time_ = NewTime;
    }

    t1_int_ready_ = false;
    if ((d_.ifr & (1 << kIntT1)) == 0) {
      if (((d_.acr & 0x40) != 0) || (t1_active_ == 1)) {
        uint32_t NewTemp = d_.t1c_f; /* Get Timer 1 Counter */
        uint32_t NewTimer;
#ifdef _VIA_Debug
        DLOG( "posting Timer1Check, %d, %d\n", NewTemp,
                GetCuriCount());
#endif
        if (NewTemp == 0) {
          NewTimer = (0x00010000UL * CyclesScaledPerViaTime);
        } else {
          NewTimer =
              (1 + (NewTemp >> (16 - kLn2CycleScale))) * CyclesPerViaTime;
        }
        ::ICT_add(kICT_VIA1_Timer1Check, NewTimer);
        t1_int_ready_ = true;
      }
    }
  }
}

void ViaDevice::DoTimer2Check() {
  if (t2_running_) {
    uint32_t NewTime = GetCuriCount();
    uint32_t Temp = d_.t2c_f; /* Get Timer 2 Counter */
    uint32_t deltaTime = (NewTime - t2_last_time_);
    uint32_t deltaTemp = (deltaTime / CyclesPerViaTime)
                         << (16 - kLn2CycleScale); /* may overflow */
    uint32_t NewTemp = Temp - deltaTemp;
    if (t2_active_ == 1) {
      if (deltaTime > (0x00010000UL * CyclesScaledPerViaTime) ||
          ((Temp <= deltaTemp) && (Temp != 0))) {
        t2c_short_time_ = false;
        t2_active_ = 0;
        SetInterruptFlag(kIntT2);
      } else {
        uint32_t NewTimer;
#ifdef _VIA_Debug
        DLOG( "posting Timer2Check, %d, %d\n", NewTemp,
                GetCuriCount());
#endif
        if (NewTemp == 0) {
          NewTimer = (0x00010000UL * CyclesScaledPerViaTime);
        } else {
          NewTimer =
              (1 + (NewTemp >> (16 - kLn2CycleScale))) * CyclesPerViaTime;
        }
        ::ICT_add(kICT_VIA1_Timer2Check, NewTimer);
      }
    }
    d_.t2c_f = NewTemp;
    t2_last_time_ = NewTime;
  }
}

uint16_t ViaDevice::GetT1InvertTime() {
  uint16_t v;

  if ((d_.acr & 0xC0) == 0xC0) {
    v = (d_.t1l_h << 8) + d_.t1l_l;
  } else {
    v = 0;
  }
  return v;
}

void ViaDevice::ShiftInData(uint8_t v) {
  uint8_t ShiftMode = (d_.acr & 0x1C) >> 2;

  if (ShiftMode != 3) {
#if ExtraAbnormalReports
    if (ShiftMode == 0) {
      /* happens on reset */
    } else {
      ReportAbnormalID(0x0403, "VIA Not ready to shift in");
    }
#endif
  } else {
    d_.sr = v;
    SetInterruptFlag(kIntSR);
    SetInterruptFlag(kIntCB1);
  }
}

uint8_t ViaDevice::ShiftOutData() {
  if (((d_.acr & 0x1C) >> 2) != 7) {
    ReportAbnormalID(0x0404, "VIA Not ready to shift out");
    return 0;
  } else {
    SetInterruptFlag(kIntSR);
    SetInterruptFlag(kIntCB1);
    VIA1_iCB2 = (d_.sr & 1);
    return d_.sr;
  }
}

uint32_t ViaDevice::Access(uint32_t data, bool is_write, uint32_t address) {
  int Reg_No = (address >> 9) & 0x000F;
#ifdef _VIA_Debug
  if (!(Reg_No == 13 && !is_write)) {
    DLOG(
            "VIA1_Access: addr=%08x Reg=%d Write=%d Data=%02x PC=%08x\n",
            (unsigned int)address, Reg_No, (int)is_write, (unsigned int)data,
            (unsigned int)get_vm_pc());
  }
#endif
  switch (Reg_No) {
    case kORB:
#if VIA1_CB2modesAllowed != 0x01
      if ((d_.pcr & 0xE0) == 0)
#endif
      {
        ClrInterruptFlag(kIntCB2);
      }
      ClrInterruptFlag(kIntCB1);
      if (is_write) {
        d_.orb = data;
        Put_ORB(d_.ddr_b, d_.orb);
      } else {
        data = (d_.orb & d_.ddr_b) | Get_ORB(~d_.ddr_b);
      }
      break;
    case kDDR_B:
      if (is_write) {
        SetDDR_B(data);
      } else {
        data = d_.ddr_b;
      }
      break;
    case kDDR_A:
      if (is_write) {
        SetDDR_A(data);
      } else {
        data = d_.ddr_a;
      }
      break;
    case kT1C_L:
      if (is_write) {
        d_.t1l_l = data;
      } else {
        ClrInterruptFlag(kIntT1);
        DoTimer1Check();
        data = (d_.t1c_f & 0x00FF0000) >> 16;
      }
      break;
    case kT1C_H:
      if (is_write) {
        d_.t1l_h = data;
        ClrInterruptFlag(kIntT1);
        d_.t1c_f = (data << 24) + (d_.t1l_l << 16);
        if ((d_.acr & 0x40) == 0) {
          t1_active_ = 1;
        }
        t1_last_time_ = GetCuriCount();
        DoTimer1Check();
      } else {
        DoTimer1Check();
        data = (d_.t1c_f & 0xFF000000) >> 24;
      }
      break;
    case kT1L_L:
      if (is_write) {
        d_.t1l_l = data;
      } else {
        data = d_.t1l_l;
      }
      break;
    case kT1L_H:
      if (is_write) {
        d_.t1l_h = data;
      } else {
        data = d_.t1l_h;
      }
      break;
    case kT2_L:
      if (is_write) {
        d_.t2l_l = data;
      } else {
        ClrInterruptFlag(kIntT2);
        DoTimer2Check();
        data = (d_.t2c_f & 0x00FF0000) >> 16;
      }
      break;
    case kT2_H:
      if (is_write) {
        d_.t2c_f = (data << 24) + (d_.t2l_l << 16);
        ClrInterruptFlag(kIntT2);
        t2_active_ = 1;

        if (d_.t2c_f < (128UL << 16) && (d_.t2c_f != 0)) {
          t2c_short_time_ = true;
          t2_running_ = true;
        }
        t2_last_time_ = GetCuriCount();
        DoTimer2Check();
      } else {
        DoTimer2Check();
        data = (d_.t2c_f & 0xFF000000) >> 24;
      }
      break;
    case kSR:
#ifdef _VIA_Debug
      DLOG( "d_.sr: %d, %d, %d\n", is_write, ((d_.acr & 0x1C) >> 2),
              data);
#endif
      if (is_write) {
        d_.sr = data;
      }
      ClrInterruptFlag(kIntSR);
      switch ((d_.acr & 0x1C) >> 2) {
        case 3: /* Shifting In */
          break;
        case 6: /* shift out under o2 clock */
          if ((!is_write) || (d_.sr != 0)) {
            ReportAbnormalID(0x0405, "VIA shift mode 6, non zero");
          } else {
            if (VIA1_iCB2 != 0) {
              VIA1_iCB2 = 0;
#ifdef VIA1_iCB2_ChangeNtfy
              VIA1_iCB2_ChangeNtfy();
#endif
            }
          }
          break;
        case 7: /* Shifting Out */
          break;
      }
      if (!is_write) {
        data = d_.sr;
      }
      break;
    case kACR:
      if (is_write) {
        if ((d_.acr & 0x10) != ((uint8_t)data & 0x10)) {
          /* shift direction has changed */
          if ((data & 0x10) == 0) {
            /*
                no longer an output,
                set data to float value
            */
            if (VIA1_iCB2 == 0) {
              VIA1_iCB2 = 1;
#ifdef VIA1_iCB2_ChangeNtfy
              VIA1_iCB2_ChangeNtfy();
#endif
            }
          }
        }
        d_.acr = data;
        if ((d_.acr & 0x20) != 0) {
          ReportAbnormalID(0x0406, "Set d_.acr T2 Timer pulse counting");
        }
        switch ((d_.acr & 0xC0) >> 6) {
          case 2:
            ReportAbnormalID(0x0407, "Set d_.acr T1 Timer mode 2");
            break;
        }
        CheckT1IntReady();
        switch ((d_.acr & 0x1C) >> 2) {
          case 0:
            ClrInterruptFlag(kIntSR);
            break;
          case 1:
          case 2:
          case 4:
          case 5:
            ReportAbnormalID(0x0408, "Set d_.acr shift mode 1,2,4,5");
            break;
          default:
            break;
        }
        if ((d_.acr & 0x03) != 0) {
          ReportAbnormalID(0x0409, "Set d_.acr T2 Timer latching enabled");
        }
      } else {
        data = d_.acr;
      }
      break;
    case kPCR:
      if (is_write) {
        d_.pcr = data;
#define Ui3rSetContains(s, i) (((s) & (1 << (i))) != 0)
        if (!Ui3rSetContains(VIA1_CB2modesAllowed, (d_.pcr >> 5) & 0x07)) {
          ReportAbnormalID(0x040A, "Set d_.pcr CB2 Control mode?");
        }
        if ((d_.pcr & 0x10) != 0) {
          ReportAbnormalID(0x040B, "Set d_.pcr CB1 INTERRUPT CONTROL?");
        }
        if (!Ui3rSetContains(VIA1_CA2modesAllowed, (d_.pcr >> 1) & 0x07)) {
          ReportAbnormalID(0x040C, "Set d_.pcr CA2 INTERRUPT CONTROL?");
        }
        if ((d_.pcr & 0x01) != 0) {
          ReportAbnormalID(0x040D, "Set d_.pcr CA1 INTERRUPT CONTROL?");
        }
      } else {
        data = d_.pcr;
      }
      break;
    case kIFR:
      if (is_write) {
        d_.ifr = d_.ifr & ((~data) & 0x7F);
        CheckInterruptFlag();
        CheckT1IntReady();
      } else {
        data = d_.ifr;
        if ((d_.ifr & d_.ier) != 0) {
          data |= 0x80;
        }
      }
      break;
    case kIER:
      if (is_write) {
        if ((data & 0x80) == 0) {
          d_.ier = d_.ier & ((~data) & 0x7F);
        } else {
          d_.ier = d_.ier | (data & 0x7F);
        }
        CheckInterruptFlag();
      } else {
        data = d_.ier | 0x80;
      }
      break;
    case kORA:
    case kORA_H:
      if ((d_.pcr & 0xE) == 0) {
        ClrInterruptFlag(kIntCA2);
      }
      ClrInterruptFlag(kIntCA1);
      if (is_write) {
        d_.ora = data;
        Put_ORA(d_.ddr_a, d_.ora);
      } else {
        data = (d_.ora & d_.ddr_a) | Get_ORA(~d_.ddr_a);
      }
      break;
  }
  return data;
}

void ViaDevice::ExtraTimeBegin() {
  if (t1_running_) {
    DoTimer1Check();
    t1_running_ = false;
  }
  if (t2_running_ && (!t2c_short_time_)) {
    DoTimer2Check();
    t2_running_ = false;
  }
}

void ViaDevice::ExtraTimeEnd() {
  if (!t1_running_) {
    t1_running_ = true;
    t1_last_time_ = GetCuriCount();
    DoTimer1Check();
  }
  if (!t2_running_) {
    t2_running_ = true;
    t2_last_time_ = GetCuriCount();
    DoTimer2Check();
  }
}

void ViaDevice::iCA1_PulseNtfy() {
  static int count = 0;
  if (count < 10) {
    DLOG( "ViaDevice::iCA1_PulseNtfy: 60Hz tick\n");
    count++;
  }
  SetInterruptFlag(kIntCA1);
}

void ViaDevice::iCA2_PulseNtfy() { SetInterruptFlag(kIntCA2); }

void ViaDevice::iCB1_PulseNtfy() { SetInterruptFlag(kIntCB1); }

void ViaDevice::iCB2_PulseNtfy() { SetInterruptFlag(kIntCB2); }

// Global C wrappers for Moira/stubs (which are still in C/C++ bridge)

extern "C" {
uint32_t VIA1_Access(uint32_t data, bool is_write, uint32_t address) {
  if (g_via) {
    return g_via->Access(data, is_write, address);
  }
  return 0;
}

void VIA1_Zap(void) {
  if (g_via) {
    g_via->Zap();
  }
}

void VIA1_Reset(void) {
  if (g_via) {
    g_via->Reset();
  }
}

void VIA1_ExtraTimeBegin(void) {
  if (g_via) {
    g_via->ExtraTimeBegin();
  }
}

void VIA1_ExtraTimeEnd(void) {
  if (g_via) {
    g_via->ExtraTimeEnd();
  }
}

void VIA1_ShiftInData(uint8_t v) {
  if (g_via) {
    g_via->ShiftInData(v);
  }
}

uint8_t VIA1_ShiftOutData(void) {
  if (g_via) {
    return g_via->ShiftOutData();
  }
  return 0;
}

void VIA1_DoTimer1Check(void) {
  if (g_via) {
    g_via->DoTimer1Check();
  }
}

void VIA1_DoTimer2Check(void) {
  if (g_via) {
    g_via->DoTimer2Check();
  }
}

uint16_t VIA1_GetT1InvertTime(void) {
  if (g_via) {
    return g_via->GetT1InvertTime();
  }
  return 0;
}
}
