#include "src/devices/scsi.h"

#include <string.h>

#include "src/system.h"

ScsiDevice* g_scsi = nullptr;

#define scsiRd 0x00
#define scsiWr 0x01

#define sCDR 0x00    /* current scsi data register  (r/o) */
#define sODR 0x00    /* output data register        (w/o) */
#define sICR 0x02    /* initiator command register  (r/w) */
#define sMR 0x04     /* mode register               (r/w) */
#define sTCR 0x06    /* target command register     (r/w) */
#define sCSR 0x08    /* current SCSI bus status     (r/o) */
#define sSER 0x08    /* select enable register      (w/o) */
#define sBSR 0x0A    /* bus and status register     (r/o) */
#define sDMAtx 0x0A  /* start DMA send              (w/o) */
#define sIDR 0x0C    /* input data register         (r/o) */
#define sTDMArx 0x0C /* start DMA target receive    (w/o) */
#define sRESET 0x0E  /* reset parity/interrupt      (r/o) */
#define sIDMArx 0x0E /* start DMA initiator receive (w/o) */

ScsiDevice::ScsiDevice(MacSystem* system) : system_(system) {
  g_scsi = this;
  memset(scsi_, 0, sizeof(scsi_));
}

ScsiDevice::~ScsiDevice() {
  if (g_scsi == this) {
    g_scsi = nullptr;
  }
}

void ScsiDevice::Reset() {
  for (int i = 0; i < kScsiSize; i++) {
    scsi_[i] = 0;
  }
}

void ScsiDevice::BusReset() {
  scsi_[scsiRd + sCDR] = 0;
  scsi_[scsiWr + sODR] = 0;
  scsi_[scsiRd + sICR] = 0x80;
  scsi_[scsiWr + sICR] &= 0x80;
  scsi_[scsiRd + sMR] &= 0x40;
  scsi_[scsiWr + sMR] &= 0x40;
  scsi_[scsiRd + sTCR] = 0;
  scsi_[scsiWr + sTCR] = 0;
  scsi_[scsiRd + sCSR] = 0x80;
  scsi_[scsiWr + sSER] = 0;
  scsi_[scsiRd + sBSR] = 0x10;
  scsi_[scsiWr + sDMAtx] = 0;
  scsi_[scsiRd + sIDR] = 0;
  scsi_[scsiWr + sTDMArx] = 0;
  scsi_[scsiRd + sRESET] = 0;
  scsi_[scsiWr + sIDMArx] = 0;

  // The missing piece of the puzzle.. :)
  put_ram_word(0xb22, get_ram_word(0xb22) | 0x8000);
}

void ScsiDevice::Check() {
  /*
      The arbitration select/reselect scenario
      [stub.. doesn't really work...]
  */
  if ((scsi_[scsiWr + sODR] >> 7) == 1) {
    /* Check if the Mac tries to be an initiator */
    if ((scsi_[scsiWr + sMR] & 1) == 1) {
      /* the Mac set arbitration in progress */
      /*
          stub! tell the mac that there
          is arbitration in progress...
      */
      scsi_[scsiRd + sICR] |= 0x40;
      /* ... that we didn't lose arbitration ... */
      scsi_[scsiRd + sICR] &= ~0x20;
      /*
          ... and that there isn't a higher priority ID present...
      */
      scsi_[scsiRd + sCDR] = 0x00;

      /*
          ... the arbitration and selection/reselection is
          complete. the initiator tries to connect to the SCSI
          device, fails and returns after timeout.
      */
    }
  }

  /* check the chip registers, AS SET BY THE CPU */
  if ((scsi_[scsiWr + sICR] >> 7) == 1) {
    /* Check Assert RST */
    BusReset();
  } else {
    scsi_[scsiRd + sICR] &= ~0x80;
    scsi_[scsiRd + sCSR] &= ~0x80;
  }

  if ((scsi_[scsiWr + sICR] >> 2) == 1) {
    /* Check Assert SEL */
    scsi_[scsiRd + sCSR] |= 0x02;
    scsi_[scsiRd + sBSR] = 0x10;
  } else {
    scsi_[scsiRd + sCSR] &= ~0x02;
  }
}

uint32_t ScsiDevice::Access(uint32_t data, bool is_write, uint32_t address) {
  if (address < (kScsiSize / 2)) {
    address *= 2;
    if (is_write) {
      scsi_[address + 1] = data;
      Check();
    } else {
      data = scsi_[address];
    }
  }
  return data;
}

extern "C" {
uint32_t SCSI_Access(uint32_t data, bool is_write, uint32_t address) {
  if (g_scsi) {
    return g_scsi->Access(data, is_write, address);
  }
  return 0;
}
}
