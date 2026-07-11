#include "src/emulator.h"

#include <stdio.h>

#include "src/debug_log.h"
#include "src/cpu/moira_cpu.h"
#include "src/devices/adb.h"
#include "src/devices/iwm.h"
#include "src/devices/rom.h"
#include "src/devices/rtc.h"
#include "src/devices/scc.h"
#include "src/devices/scsi.h"
#include "src/devices/sony.h"
#include "src/devices/via.h"
#include "src/devices/video.h"
#include "src/system.h"

#define CyclesScaledPerTick (130240UL * kMyClockMult * kCycleScale)
#define CyclesScaledPerSubTick (CyclesScaledPerTick / kNumSubTicks)

Emulator::Emulator(MacSystem* system)
    : system_(system),
      sub_tick_counter_(0),
      extra_sub_ticks_to_do_(0),
      cur_emulated_time_(0) {}

bool Emulator::Init() {
  DLOG("Emulator::Init started\n");

  if (system_->GetRtcDevice()->Init())
    if (::AddrSpac_Init())
      if (system_->GetRomDevice()->Init()) {
        EmulatedHardwareZap();
        DLOG("Emulator::Init successful\n");
        return true;
      }
  fprintf(stderr, "Emulator::Init failed\n");
  fflush(stderr);
  return false;
}

void Emulator::EmulatedHardwareZap() {
  ::Memory_Reset();
  ::ICT_Zap();
  system_->GetIwmDevice()->Reset();
  system_->GetSccDevice()->Reset();
  system_->GetScsiDevice()->Reset();
  system_->GetViaDevice()->Zap();
  // EmVIA2 is 0
  system_->GetSonyDevice()->Reset();
  ::Extn_Reset();
  ::m68k_reset();
}

void Emulator::DoMacReset() {
  system_->GetSonyDevice()->EjectAllDisks();
  EmulatedHardwareZap();
}

void Emulator::InterruptReset_Update() {
  ::SetInterruptButton(false);
  if (system_->get_want_mac_interrupt()) {
    ::SetInterruptButton(true);
    system_->get_want_mac_interrupt() = false;
  }
  if (system_->get_want_mac_reset()) {
    DoMacReset();
    system_->get_want_mac_reset() = false;
  }
}

void Emulator::SubTickNotify(int sub_tick) {
  // EmASC is 0
  // MySoundEnabled is false in our config
}

void Emulator::SubTickTaskDo() {
  SubTickNotify(sub_tick_counter_);
  ++sub_tick_counter_;
  if (sub_tick_counter_ < (kNumSubTicks - 1)) {
    ::ICT_add(kICT_SubTick, CyclesScaledPerSubTick);
  }
}

void Emulator::SubTickTaskStart() {
  sub_tick_counter_ = 0;
  ::ICT_add(kICT_SubTick, CyclesScaledPerSubTick);
}

void Emulator::SubTickTaskEnd() { SubTickNotify(kNumSubTicks - 1); }

void Emulator::SixtiethSecondNotify() {
  system_->GetAdbDevice()->MouseUpdate();
  InterruptReset_Update();
  // EmClassicKbrd is 0
  // EmADB is 1
  system_->GetAdbDevice()->Update();

  system_->GetViaDevice()->iCA1_PulseNtfy();
  system_->GetSonyDevice()->Update();

  // EmLocalTalk is 0
  // EmRTC is 1
  system_->GetRtcDevice()->Interrupt();
  // EmVidCard is 0

  SubTickTaskStart();
}

void Emulator::SixtiethEndNotify() {
  SubTickTaskEnd();
  system_->GetAdbDevice()->MouseEndTickNotify();
  system_->GetVideoDevice()->EndTickNotify();
}

void Emulator::ExtraTimeBeginNotify() {
  system_->GetViaDevice()->ExtraTimeBegin();
}

void Emulator::ExtraTimeEndNotify() { system_->GetViaDevice()->ExtraTimeEnd(); }

void Emulator::GoNCycles(uint32_t n) {
  uint32_t StopiCount = NextiCount + n;
  do {
    // ICT_DoCurrentTasks
    {
      int i = 0;
      uint32_t m = ICTactive;
      while (0 != m) {
        if (0 != (m & 1)) {
          if (i >= kNumICTs) {
            ICTactive &= ((1 << kNumICTs) - 1);
            m = 0;
          } else if (ICTwhen[i] == NextiCount) {
            ICTactive &= ~(1 << i);
            // ICT_DoTask
            switch (i) {
              case kICT_SubTick:
                SubTickTaskDo();
                break;
              case kICT_ADB_NewState:
                system_->GetAdbDevice()->DoNewState();
                break;
              case kICT_VIA1_Timer1Check:
                system_->GetViaDevice()->DoTimer1Check();
                break;
              case kICT_VIA1_Timer2Check:
                system_->GetViaDevice()->DoTimer2Check();
                break;
              default:
                ::ReportAbnormalID(0x1001, "unknown taskid in ICT_DoTask");
                break;
            }
          }
        }
        ++i;
        m >>= 1;
      }
    }

    // ICT_DoGetNext
    uint32_t n2;
    {
      int i = 0;
      uint32_t m = ICTactive;
      uint32_t v = n;
      while (0 != m) {
        if (0 != (m & 1)) {
          if (i >= kNumICTs) {
            m = 0;
          } else {
            uint32_t d = ICTwhen[i] - NextiCount;
            if (d < v) {
              v = d;
            }
          }
        }
        ++i;
        m >>= 1;
      }
      n2 = v;
    }

    NextiCount += n2;
    ::m68k_go_nCycles(n2);
    n = StopiCount - NextiCount;
  } while (n != 0);
}

void Emulator::DoEmulateOneTick() {
#if EnableAutoSlow
  {
    uint32_t NewQuietTime = system_->get_quiet_time() + 1;
    if (NewQuietTime > system_->get_quiet_time()) {
      system_->get_quiet_time() = NewQuietTime;
    }
  }
  {
    uint32_t NewQuietSubTicks = system_->get_quiet_sub_ticks() + kNumSubTicks;
    if (NewQuietSubTicks > system_->get_quiet_sub_ticks()) {
      system_->get_quiet_sub_ticks() = NewQuietSubTicks;
    }
  }
#endif

  SixtiethSecondNotify();

  GoNCycles(CyclesScaledPerTick);

  SixtiethEndNotify();

  if ((uint8_t)-1 == system_->get_speed_value()) {
    extra_sub_ticks_to_do_ = (uint32_t)-1;
  } else {
    uint32_t ExtraAdd =
        (kNumSubTicks << system_->get_speed_value()) - kNumSubTicks;
    uint32_t ExtraLimit = ExtraAdd << 3;

    extra_sub_ticks_to_do_ += ExtraAdd;
    if (extra_sub_ticks_to_do_ > ExtraLimit) {
      extra_sub_ticks_to_do_ = ExtraLimit;
    }
  }
}

bool Emulator::MoreSubTicksToDo() {
  bool v = false;
  if (ExtraTimeNotOver() && (extra_sub_ticks_to_do_ > 0)) {
#if EnableAutoSlow
    if ((system_->get_quiet_sub_ticks() >= 16384) &&
        (system_->get_quiet_time() >= 34) && !::WantNotAutoSlow) {
      extra_sub_ticks_to_do_ = 0;
    } else
#endif
    {
      v = true;
    }
  }
  return v;
}

void Emulator::DoEmulateExtraTime() {
  if (MoreSubTicksToDo()) {
    ExtraTimeBeginNotify();
    do {
#if EnableAutoSlow
      {
        uint32_t NewQuietSubTicks = system_->get_quiet_sub_ticks() + 1;
        if (NewQuietSubTicks > system_->get_quiet_sub_ticks()) {
          system_->get_quiet_sub_ticks() = NewQuietSubTicks;
        }
      }
#endif
      GoNCycles(CyclesScaledPerSubTick);
      --extra_sub_ticks_to_do_;
    } while (MoreSubTicksToDo());
    ExtraTimeEndNotify();
  }
}

void Emulator::RunEmulatedTicksToTrueTime() {
  int8_t n = OnTrueTime - cur_emulated_time_;
  DoEmulateOneTick();
  if (n > 1) {
    ++cur_emulated_time_;
    DoneWithDrawingForTick();

    if (n > 8) {
      n = 8;
      cur_emulated_time_ = OnTrueTime - n;
    }

    if (ExtraTimeNotOver() && (--n > 0)) {
      system_->get_em_video_disable() = true;
      do {
        DoEmulateOneTick();
        ++cur_emulated_time_;
      } while (ExtraTimeNotOver() && (--n > 0));
      system_->get_em_video_disable() = false;
    }
    system_->get_em_lag_time() = n;
  }
}

void Emulator::MainEventLoop() {
  DLOG("MainEventLoop started\n");
  for (;;) {
    WaitForNextTick();
    if (system_->get_force_mac_off()) {
      return;
    }
    RunEmulatedTicksToTrueTime();
    DoEmulateExtraTime();
  }
}

void Emulator::Run() {
  DLOG("Emulator::Run started\n");
  if (Init()) {
    MainEventLoop();
  } else {
    fprintf(stderr, "Init failed\n");
    fflush(stderr);
  }
}

extern "C" {
void ProgramMain(void) {
  if (g_system) {
    Emulator emulator(g_system);
    emulator.Run();
  }
}
void EmulationReserveAlloc(void) {
  ReserveAllocOneBlock(&RAM, kRAM_Size + RAMSafetyMarginFudge, 5, false);
}
}
