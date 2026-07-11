#ifndef MACINTOSH_SRC_EMULATOR_H_
#define MACINTOSH_SRC_EMULATOR_H_

#include <cstdint>

class MacSystem;

class Emulator {
 public:
  explicit Emulator(MacSystem* system);
  ~Emulator() = default;

  bool Init();
  void Run();

 private:
  void EmulatedHardwareZap();
  void DoMacReset();
  void InterruptReset_Update();
  void SubTickNotify(int sub_tick);
  void SubTickTaskDo();
  void SubTickTaskStart();
  void SubTickTaskEnd();
  void SixtiethSecondNotify();
  void SixtiethEndNotify();
  void ExtraTimeBeginNotify();
  void ExtraTimeEndNotify();
  void DoEmulateOneTick();
  bool MoreSubTicksToDo();
  void DoEmulateExtraTime();
  void RunEmulatedTicksToTrueTime();
  void MainEventLoop();

  void GoNCycles(uint32_t n);

  MacSystem* system_;

  uint16_t sub_tick_counter_;
  uint32_t extra_sub_ticks_to_do_;
  uint32_t cur_emulated_time_;
};

#endif  // MACINTOSH_SRC_EMULATOR_H_
