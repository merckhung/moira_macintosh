#include "src/devices/video.h"

#include <stdio.h>

#include "src/system.h"

#define kMain_Offset 0x5900
#define kAlternate_Offset 0xD900
#define kMain_Buffer (kRAM_Size - kMain_Offset)
#define kAlternate_Buffer (kRAM_Size - kAlternate_Offset)

VideoDevice::VideoDevice(MacSystem* system) : system_(system) {}

void VideoDevice::EndTickNotify() {
  uint8_t* screencurrentbuff;
  // We assume IncludeVidMem is 0 as per EMCONFIG.h
  if (system_->get_wires()[Wire_VIA1_iA6_SCRNvPage2] == 1) {
    screencurrentbuff = ::get_ram_address(kMain_Buffer);
  } else {
    screencurrentbuff = ::get_ram_address(kAlternate_Buffer);
  }

  if (system_->GetPlatform()) {
    system_->GetPlatform()->ScreenOutputFrame(screencurrentbuff);
  }
}

// C wrapper for legacy code
extern "C" {
void Screen_EndTickNotify(void) {
  if (g_system && g_system->GetVideoDevice()) {
    g_system->GetVideoDevice()->EndTickNotify();
  }
}
}
