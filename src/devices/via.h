#ifndef SRC_DEVICES_VIA_H_
#define SRC_DEVICES_VIA_H_

#include <cstdint>

class MacSystem;

struct ViaState {
  uint32_t t1c_f; /* Timer 1 Counter Fixed Point */
  uint32_t t2c_f; /* Timer 2 Counter Fixed Point */
  uint8_t orb;    /* Buffer B */
  uint8_t ddr_b;  /* Data Direction Register B */
  uint8_t ddr_a;  /* Data Direction Register A */
  uint8_t t1l_l;  /* Timer 1 Latch Low */
  uint8_t t1l_h;  /* Timer 1 Latch High */
  uint8_t t2l_l;  /* Timer 2 Latch Low */
  uint8_t sr;     /* Shift Register */
  uint8_t acr;    /* Auxiliary Control Register */
  uint8_t pcr;    /* Peripheral Control Register */
  uint8_t ifr;    /* Interrupt Flag Register */
  uint8_t ier;    /* Interrupt Enable Register */
  uint8_t ora;    /* Buffer A */
};
#define VIA1_TY_DEFINED

#define kIntCA2 0 /* One_Second */
#define kIntCA1 1 /* Vertical_Blanking */
#define kIntSR 2  /* Keyboard_Data_Ready */
#define kIntCB2 3 /* Keyboard_Data */
#define kIntCB1 4 /* Keyboard_Clock */
#define kIntT2 5  /* Timer_2 */
#define kIntT1 6  /* Timer_1 */

class ViaDevice {
 public:
  ViaDevice(MacSystem* system);
  ~ViaDevice();

  void Zap();
  void Reset();

  uint32_t Access(uint32_t data, bool is_write, uint32_t address);

  void ExtraTimeBegin();
  void ExtraTimeEnd();

  void iCA1_PulseNtfy();
  void iCA2_PulseNtfy();
  void iCB1_PulseNtfy();
  void iCB2_PulseNtfy();

  void DoTimer1Check();
  void DoTimer2Check();

  uint16_t GetT1InvertTime();

  void ShiftInData(uint8_t value);
  uint8_t ShiftOutData();

  void SetInterruptFlag(uint8_t via_int);

 private:
  uint8_t Get_ORA(uint8_t selection);
  uint8_t Get_ORB(uint8_t selection);
  void Put_ORA(uint8_t selection, uint8_t data);
  void Put_ORB(uint8_t selection, uint8_t data);
  void SetDDR_A(uint8_t data);
  void SetDDR_B(uint8_t data);
  void CheckInterruptFlag();
  void Clear();
  void ClrInterruptFlag(uint8_t via_int);
  void CheckT1IntReady();

  MacSystem* system_;
  ViaState d_;
  uint8_t t1_active_;
  uint8_t t2_active_;
  bool t1_int_ready_;
  bool t1_running_;
  uint32_t t1_last_time_;
  bool t2_running_;
  bool t2c_short_time_;
  uint32_t t2_last_time_;
};

#endif  // SRC_DEVICES_VIA_H_
