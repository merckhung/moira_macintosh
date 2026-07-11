#include "src/system.h"

#include <stdlib.h>
#include <string.h>

#include "src/devices/adb.h"
#include "src/devices/iwm.h"
#include "src/devices/rom.h"
#include "src/devices/rtc.h"
#include "src/devices/scc.h"
#include "src/devices/scsi.h"
#include "src/devices/sony.h"
#include "src/devices/via.h"
#include "src/devices/video.h"

MacSystem* g_system = nullptr;

MacSystem::MacSystem()
    : ram_(nullptr),
      rom_(nullptr),
      on_true_time_(0),
      screen_compare_buff_(nullptr),
      cur_mouse_v_(0),
      cur_mouse_h_(0),
      ict_active_(0),
      force_mac_off_(0),
      want_mac_interrupt_(0),
      want_mac_reset_(0),
      request_mac_off_(0),
      quiet_time_(0),
      quiet_sub_ticks_(0),
      speed_value_(5),
      em_video_disable_(0),
      em_lag_time_(0),
      rom_device_(nullptr),
      via_device_(nullptr),
      adb_device_(nullptr),
      iwm_device_(nullptr),
      sony_device_(nullptr),
      rtc_device_(nullptr),
      scsi_device_(nullptr),
      scc_device_(nullptr),
      video_device_(nullptr),
      platform_(nullptr) {
  memset(wires_, 0, sizeof(wires_));
  memset(ict_when_, 0, sizeof(ict_when_));
}

MacSystem::~MacSystem() {}

bool MacSystem::Init() {
  rom_device_ = std::make_unique<RomDevice>(this);
  via_device_ = std::make_unique<ViaDevice>(this);
  adb_device_ = std::make_unique<AdbDevice>(this);
  iwm_device_ = std::make_unique<IwmDevice>(this);
  sony_device_ = std::make_unique<SonyDevice>(this);
  rtc_device_ = std::make_unique<RtcDevice>(this);
  scsi_device_ = std::make_unique<ScsiDevice>(this);
  scc_device_ = std::make_unique<SccDevice>(this);
  video_device_ = std::make_unique<VideoDevice>(this);
  return true;
}
