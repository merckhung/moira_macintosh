#ifndef SRC_DEVICES_RTC_H_
#define SRC_DEVICES_RTC_H_

#include <cstdint>

// Define PARAMRAMSize based on Emulation Model.
// In our setup (Mac SE), CurEmMd is kEmMd_SE.
// We can just define it as 256 for SE (it has XPRAM).
// Let's check kEmMd_SE.
// In EMCONFIG.h: #define CurEmMd kEmMd_SE (which is > kEmMd_Plus).
// So HaveXPRAM is true, PARAMRAMSize is 256.
// To be safe, we can just use 256.
constexpr int kParamramSize = 256;

#ifndef RTC_TY_DEFINED
struct RtcState {
  /* RTC VIA Flags */
  uint8_t wr_protect;
  uint8_t data_out;
  uint8_t data_next_out;

  /* RTC Data */
  uint8_t shift_data;
  uint8_t counter;
  uint8_t mode;
  uint8_t saved_cmd;
  uint8_t sector;

  /* RTC Registers */
  uint8_t seconds_1[4];
  uint8_t paramram[kParamramSize];
};
#define RTC_TY_DEFINED
#endif

class MacSystem;

class RtcDevice {
 public:
  RtcDevice(MacSystem* system);
  ~RtcDevice();

  bool Init();
  void Interrupt();
  void UnEnabledChangeNtfy();
  void ClockChangeNtfy();
  void DataLineChangeNtfy();

 private:
  uint8_t Access_PRAM_Reg(uint8_t data, bool is_write, uint8_t reg_index);
  uint8_t Access_Reg(uint8_t data, bool is_write, uint8_t command);
  void DoCmd();

  MacSystem* system_;
  RtcState rtc_;
  uint32_t last_real_date_;
};

#endif  // SRC_DEVICES_RTC_H_
