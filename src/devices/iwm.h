#ifndef SRC_DEVICES_IWM_H_
#define SRC_DEVICES_IWM_H_

#include <cstdint>

#ifndef IWM_TY_DEFINED
struct IwmState {
  uint8_t data_in;   /* Read Data Register */
  uint8_t handshake; /* Read Handshake Register */
  uint8_t status;    /* Read Status Register */
  uint8_t mode;      /* Write Mode Register */
                     /* Drive On  : Write Data Register */
  uint8_t data_out;  /* Write Data Register */
  uint8_t lines;     /* Used to Access Disk Drive Registers */
};
#define IWM_TY_DEFINED
#endif

class MacSystem;

class IwmDevice {
 public:
  IwmDevice(MacSystem* system);
  ~IwmDevice();

  void Reset();
  uint32_t Access(uint32_t data, bool is_write, uint32_t address);

 private:
  enum Mode_Ty { On, Off };
  void Set_Lines(uint8_t line, Mode_Ty the_mode);
  uint8_t Read_Reg();
  void Write_Reg(uint8_t value);

  MacSystem* system_;
  IwmState iwm_;
};

#endif  // SRC_DEVICES_IWM_H_
