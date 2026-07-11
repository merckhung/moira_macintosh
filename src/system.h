#ifndef SRC_SYSTEM_H_
#define SRC_SYSTEM_H_

#include <cstdint>
#include <memory>

#include "src/mac_errors.h"
extern "C" {
#include "src/config.h"

void ReportAbnormalID(uint16_t id, const char* message);
void VIAorSCCinterruptChngNtfy(void);
void ICT_add(int task_id, uint32_t when);

void ADB_Update(void);
void ADB_DoNewState(void);
void Keyboard_UpdateKeyMap(uint8_t keycode, bool is_down);
void MyMousePositionSet(int x, int y);
void MyMouseButtonSet(bool is_down);
void MyEvtQTryRecoverFromFull(void);

MacErr vSonyTransfer(bool is_write, uint8_t* buffer, uint32_t drive_no,
                     uint32_t start_sector, uint32_t sector_count,
                     uint32_t* actual_sector_count);
MacErr vSonyEject(uint32_t drive_no);

uint8_t* get_ram_address(unsigned int address);
uint8_t get_ram_byte(unsigned int address);
void put_ram_byte(unsigned int address, uint8_t value);
uint16_t get_ram_word(unsigned int address);
void put_ram_word(unsigned int address, uint16_t value);
uint32_t get_ram_long(unsigned int address);
void put_ram_long(unsigned int address, uint32_t value);
uint8_t* get_real_address0(uint32_t count, bool is_write, uint32_t address,
                           uint32_t* contig);

void do_put_mem_word(uint8_t* ptr, uint16_t value);
void do_put_mem_long(uint8_t* ptr, uint32_t value);
uint16_t do_get_mem_word(uint8_t* ptr);
uint32_t do_get_mem_long(uint8_t* ptr);

// Emulation loop helpers (defined in main.cc or included C files)
bool AddrSpac_Init(void);
void Memory_Reset(void);
void Extn_Reset(void);
void ICT_Zap(void);
bool ExtraTimeNotOver(void);
void SetInterruptButton(bool value);

bool FirstFreeDisk(void* refnum, uint16_t* drive_no);
void DiskInsertNotify(uint16_t drive_no, bool is_locked);
void DiskEjectedNotify(uint16_t drive_no);
bool AnyDiskInserted(void);

void PbufNewNotify(uint32_t index, uint32_t count);
void PbufDisposeNotify(uint16_t ref_num);
bool PbufIsAllocated(uint32_t index);
bool FirstFreePbuf(uint16_t* index);

void InitKeyCodes(void);
void ScreenClearChanges(void);
bool vSonyIsInserted(uint32_t drive_no);
void DisconnectKeyCodes(uint32_t mask);
void Keyboard_UpdateKeyMap(uint8_t keycode, bool is_down);

constexpr int kLn2CycleScale = 6;
constexpr int kCycleScale = 1 << kLn2CycleScale;

constexpr int kNumSubTicks = 8;

enum {
  kICT_SubTick,
  kICT_ADB_NewState,
  kICT_VIA1_Timer1Check,
  kICT_VIA1_Timer2Check,
  kNumICTs
};

// Address Translation Table definitions (from GLOBGLUE.h)
typedef struct ATTer* ATTep;
struct ATTer {
  ATTep Next;
  uint32_t cmpmask;
  uint32_t cmpvalu;
  uint32_t usemask;
  uint8_t* usebase;
  uint32_t Access;
  int MMDV;
  int Ntfy;
};

#define kATTA_readreadymask (1 << 0)
#define kATTA_writereadymask (1 << 1)
#define kATTA_mmdvmask (1 << 2)
#define kATTA_ntfymask (1 << 3)
#define kATTA_readwritereadymask (kATTA_readreadymask | kATTA_writereadymask)

enum { kMMDV_VIA1 = 1, kMMDV_SCC, kMMDV_Extn, kMMDV_SCSI, kMMDV_IWM };

constexpr int kExtnSony = 1;
constexpr int kExtnDisk = 2;

// Globals
extern uint8_t CurIPL;
extern uint32_t NextiCount;
extern struct ATTer ATTListA[16];
extern uint32_t CurMacDateInSeconds;
extern uint32_t CurMacLatitude;
extern uint32_t CurMacLongitude;
extern uint32_t CurMacDelta;
extern bool WantNotAutoSlow;

// Functions
uint32_t GetCuriCount(void);
void SetHeadATTel(ATTep p);
uint32_t get_vm_pc(void);
uint8_t get_vm_byte(uint32_t addr);
uint16_t get_vm_word(uint32_t addr);
uint32_t get_vm_long(uint32_t addr);
void put_vm_byte(uint32_t addr, uint8_t val);
void put_vm_word(uint32_t addr, uint16_t val);
void put_vm_long(uint32_t addr, uint32_t val);
}

class RomDevice;
class ViaDevice;
class AdbDevice;
class IwmDevice;
class SonyDevice;
class RtcDevice;
class ScsiDevice;
class SccDevice;
class VideoDevice;
#include "src/platform/platform.h"

class MacSystem {
 public:
  MacSystem();
  ~MacSystem();

  bool Init();

  uint8_t*& get_ram() { return ram_; }
  uint8_t*& get_rom() { return rom_; }

  void set_ram(uint8_t* ram) { ram_ = ram; }
  void set_rom(uint8_t* rom) { rom_ = rom; }

  uint8_t* get_wires() { return wires_; }
  uint32_t& get_on_true_time() { return on_true_time_; }
  uint8_t*& get_screen_compare_buff() { return screen_compare_buff_; }
  uint16_t& get_cur_mouse_v() { return cur_mouse_v_; }
  uint16_t& get_cur_mouse_h() { return cur_mouse_h_; }
  uint32_t& get_ict_active() { return ict_active_; }
  uint32_t* get_ict_when() { return ict_when_; }

  bool& get_force_mac_off() { return force_mac_off_; }
  bool& get_want_mac_interrupt() { return want_mac_interrupt_; }
  bool& get_want_mac_reset() { return want_mac_reset_; }
  bool& get_request_mac_off() { return request_mac_off_; }
  uint32_t& get_quiet_time() { return quiet_time_; }
  uint32_t& get_quiet_sub_ticks() { return quiet_sub_ticks_; }
  uint8_t& get_speed_value() { return speed_value_; }
  bool& get_em_video_disable() { return em_video_disable_; }
  int8_t& get_em_lag_time() { return em_lag_time_; }

  RomDevice* GetRomDevice() { return rom_device_.get(); }
  ViaDevice* GetViaDevice() { return via_device_.get(); }
  AdbDevice* GetAdbDevice() { return adb_device_.get(); }
  IwmDevice* GetIwmDevice() { return iwm_device_.get(); }
  SonyDevice* GetSonyDevice() { return sony_device_.get(); }
  RtcDevice* GetRtcDevice() { return rtc_device_.get(); }
  ScsiDevice* GetScsiDevice() { return scsi_device_.get(); }
  SccDevice* GetSccDevice() { return scc_device_.get(); }
  VideoDevice* GetVideoDevice() { return video_device_.get(); }
  Platform* GetPlatform() { return platform_.get(); }
  void SetPlatform(std::unique_ptr<Platform> platform) {
    platform_ = std::move(platform);
  }

  static constexpr int kSonyDriverSize = 430;

 private:
  uint8_t* ram_;
  uint8_t* rom_;
  uint8_t wires_[kNumWires];
  uint32_t on_true_time_;
  uint8_t* screen_compare_buff_;
  uint16_t cur_mouse_v_;
  uint16_t cur_mouse_h_;
  uint32_t ict_active_;
  uint32_t ict_when_[kNumICTs];

  bool force_mac_off_;
  bool want_mac_interrupt_;
  bool want_mac_reset_;
  bool request_mac_off_;
  uint32_t quiet_time_;
  uint32_t quiet_sub_ticks_;
  uint8_t speed_value_;
  bool em_video_disable_;
  int8_t em_lag_time_;

  std::unique_ptr<RomDevice> rom_device_;
  std::unique_ptr<ViaDevice> via_device_;
  std::unique_ptr<AdbDevice> adb_device_;
  std::unique_ptr<IwmDevice> iwm_device_;
  std::unique_ptr<SonyDevice> sony_device_;
  std::unique_ptr<RtcDevice> rtc_device_;
  std::unique_ptr<ScsiDevice> scsi_device_;
  std::unique_ptr<SccDevice> scc_device_;
  std::unique_ptr<VideoDevice> video_device_;
  std::unique_ptr<Platform> platform_;
};

extern MacSystem* g_system;

#define ROM (g_system->get_rom())
#define ROM_ALREADY_DEFINED
#define RAM (g_system->get_ram())
#define RAM_ALREADY_DEFINED
#define Wires (g_system->get_wires())
#define WIRES_ALREADY_DEFINED
#define OnTrueTime (g_system->get_on_true_time())
#define ONTRUETIME_ALREADY_DEFINED
#define screencomparebuff (g_system->get_screen_compare_buff())
#define SCREENCOMPAREBUFF_ALREADY_DEFINED
#define CurMouseV (g_system->get_cur_mouse_v())
#define CurMouseH (g_system->get_cur_mouse_h())
#define ICTactive (g_system->get_ict_active())
#define ICTACTIVE_ALREADY_DEFINED
#define ICTwhen (g_system->get_ict_when())
#define ICTWHEN_ALREADY_DEFINED

#define ForceMacOff (g_system->get_force_mac_off())
#define FORCEMACOFF_ALREADY_DEFINED
#define WantMacInterrupt (g_system->get_want_mac_interrupt())
#define WANTMACINTERRUPT_ALREADY_DEFINED
#define WantMacReset (g_system->get_want_mac_reset())
#define WANTMACRESET_ALREADY_DEFINED
#define RequestMacOff (g_system->get_request_mac_off())
#define REQUESTMACOFF_ALREADY_DEFINED
#define QuietTime (g_system->get_quiet_time())
#define QUIETTIME_ALREADY_DEFINED
#define QuietSubTicks (g_system->get_quiet_sub_ticks())
#define QUIETSUBTICKS_ALREADY_DEFINED
#define SpeedValue (g_system->get_speed_value())
#define SPEEDVALUE_ALREADY_DEFINED
#define EmVideoDisable (g_system->get_em_video_disable())
#define EMVIDEODISABLE_ALREADY_DEFINED
#define EmLagTime (g_system->get_em_lag_time())
#define EMLAGTIME_ALREADY_DEFINED

#ifndef IS_PLATFORM_IMPLEMENTATION
#define WaitForNextTick (g_system->GetPlatform()->WaitForNextTick)
#define ExtraTimeNotOver (g_system->GetPlatform()->ExtraTimeNotOver)
#define DoneWithDrawingForTick (g_system->GetPlatform()->DoneWithDrawingForTick)
#define Screen_OutputFrame (g_system->GetPlatform()->ScreenOutputFrame)
#define ReserveAllocOneBlock (g_system->GetPlatform()->ReserveAllocOneBlock)
#define vSonyTransfer (g_system->GetPlatform()->SonyTransfer)
#define vSonyEject (g_system->GetPlatform()->SonyEject)
#define MacMsg (g_system->GetPlatform()->MacMsg)
#define MacMsgOverride (g_system->GetPlatform()->MacMsgOverride)
#endif

#endif  // SRC_SYSTEM_H_
