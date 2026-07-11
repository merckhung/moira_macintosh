#ifndef SRC_DEVICES_ADB_H_
#define SRC_DEVICES_ADB_H_

#include <cstdint>

class MacSystem;

#define MyEvtQElKindKey 0
#define MyEvtQElKindMouseButton 1
#define MyEvtQElKindMousePos 2
#define MyEvtQElKindMouseDelta 3

struct MyEvtQEl {
  uint8_t kind;
  uint8_t pad[3];
  union {
    struct {
      uint8_t down;
      uint8_t key;
    } press;
    struct {
      uint16_t h;
      uint16_t v;
    } pos;
  } u;
};

class AdbDevice {
 public:
  AdbDevice(MacSystem* system);
  ~AdbDevice();

  void Reset();
  void Update();
  void DoNewState();
  void StateChangeNtfy();
  void DataLineChngNtfy();

  void MouseUpdate();
  void MouseEndTickNotify();

  // Event Queue feeding (called by platform)
  void PushKeyPress(uint8_t keycode, bool is_down);
  void PushMouseMove(int x, int y);
  void PushMouseButton(bool is_down);

  // Event Queue consuming (for internal ADB use)
  MyEvtQEl* EvtQOutP();
  void EvtQOutDone();
  bool FindKeyEvent(int* keycode, bool* is_down);

 private:
  void DoMouseTalk();
  void DoMouseListen();
  void DoKeyboardTalk();
  void DoKeyboardListen();
  bool CheckForADBkeyEvt(uint8_t* next_adb_key_event);
  bool CheckForADBanyEvt();
  void DoTalk();
  void EndListen();
  void DoReset();
  void Flush();

  bool MouseEnabled() const;

  MacSystem* system_;

  // ADB State
  bool listen_dat_buf_;
  uint8_t index_dat_buf_;
  uint8_t sz_dat_buf_;
  bool talk_dat_buf_;
  uint8_t dat_buf_[8];  // ADB_MaxSzDatBuf = 8
  uint8_t cur_cmd_;
  uint8_t not_so_rand_addr_;
  uint8_t mouse_adb_address_;
  bool saved_cur_mouse_button_;
  uint16_t mouse_adb_delta_h_;
  uint16_t mouse_adb_delta_v_;
  uint8_t keyboard_adb_address_;

  // Event Queue State
  static constexpr int kKeyQueueSize = 32;
  struct MyKeyEvent {
    int key;
    bool down;
  };
  MyKeyEvent key_queue_[kKeyQueueSize];
  int key_queue_head_;
  int key_queue_tail_;

  MyEvtQEl dummy_key_evt_;

  static constexpr int kMouseQueueSize = 64;
  MyEvtQEl mouse_queue_[kMouseQueueSize];
  int mouse_queue_head_;
  int mouse_queue_tail_;

  int last_mouse_h_;
  int last_mouse_v_;
};

#endif  // SRC_DEVICES_ADB_H_
