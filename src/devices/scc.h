#ifndef SRC_DEVICES_SCC_H_
#define SRC_DEVICES_SCC_H_

#include <cstdint>

struct SccState {
  uint8_t pointer_bits;
};

class MacSystem;

class SccDevice {
 public:
  SccDevice(MacSystem* system);
  ~SccDevice();

  void Reset();
  uint32_t Access(uint32_t data, bool is_write, uint32_t address);

 private:
  uint8_t GetReg(int channel, uint8_t scc_reg);
  void PutReg(uint8_t data, int channel, uint8_t scc_reg);

  MacSystem* system_;
  SccState scc_;
};

#endif  // SRC_DEVICES_SCC_H_
