#include "src/devices/scc.h"

#include <string.h>

#include "src/system.h"

SccDevice* g_scc = nullptr;

SccDevice::SccDevice(MacSystem* system) : system_(system) {
  g_scc = this;
  memset(&scc_, 0, sizeof(scc_));
}

SccDevice::~SccDevice() {
  if (g_scc == this) {
    g_scc = nullptr;
  }
}

void SccDevice::Reset() { scc_.pointer_bits = 0; }

uint8_t SccDevice::GetReg(int channel, uint8_t scc_reg) {
  uint8_t value = 0;
  switch (scc_reg) {
    case 0:         /* RR0 */
      value = 0x04; /* Tx buffer empty */
      break;
    default:
      value = 0;
      break;
  }
  return value;
}

void SccDevice::PutReg(uint8_t data, int channel, uint8_t scc_reg) {
  switch (scc_reg) {
    case 0:
      scc_.pointer_bits = data & 0x0F;
      break;
    default:
      break;
  }
}

uint32_t SccDevice::Access(uint32_t data, bool is_write, uint32_t address) {
  int channel = (~address) & 1;
  uint8_t scc_reg;
  if (((address >> 1) & 1) == 0) {
    /* Channel Control */
    scc_reg = scc_.pointer_bits;
    scc_.pointer_bits = 0;
  } else {
    /* Channel Data */
    scc_reg = 8;
  }
  if (is_write) {
    PutReg(data, channel, scc_reg);
  } else {
    data = GetReg(channel, scc_reg);
    if (address == 1) {
      data |= 0x04;  // Tx buffer empty (4)
    }
  }
  return data;
}

extern "C" {
uint32_t SCC_Access(uint32_t data, bool is_write, uint32_t address) {
  if (g_scc) {
    return g_scc->Access(data, is_write, address);
  }
  return 0;
}
}
