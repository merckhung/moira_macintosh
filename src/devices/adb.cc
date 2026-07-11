#include "src/devices/adb.h"
#include "src/debug_log.h"

#include <stdio.h>
#include <string.h>

#include "src/devices/via.h"
#include "src/keycodes.h"
#include "src/system.h"

AdbDevice* g_adb = nullptr;

#undef ADB_st0
#undef ADB_st1
#undef ADB_Int
#undef ADB_Data
#undef ADBMouseDisabled
#undef ADB_ShiftOutData
#undef ADB_ShiftInData

#define ADB_st0 (system_->get_wires()[Wire_VIA1_iB4_ADB_st0])
#define ADB_st1 (system_->get_wires()[Wire_VIA1_iB5_ADB_st1])
#define ADB_Int (system_->get_wires()[Wire_VIA1_iB3_ADB_Int])
#define ADB_Data (system_->get_wires()[Wire_VIA1_iCB2_ADB_Data])
#define ADBMouseDisabled (system_->get_wires()[Wire_ADBMouseDisabled])

#define ADB_ShiftOutData(v) (system_->GetViaDevice()->ShiftInData(v))
#define ADB_ShiftInData() (system_->GetViaDevice()->ShiftOutData())

#define ADB_MaxSzDatBuf 8

AdbDevice::AdbDevice(MacSystem* system) : system_(system) {
  g_adb = this;
  Reset();
}

AdbDevice::~AdbDevice() {
  if (g_adb == this) {
    g_adb = nullptr;
  }
}

void AdbDevice::Reset() {
  listen_dat_buf_ = false;
  index_dat_buf_ = 0;
  sz_dat_buf_ = 0;
  talk_dat_buf_ = false;
  memset(dat_buf_, 0, sizeof(dat_buf_));
  cur_cmd_ = 0;
  not_so_rand_addr_ = 1;
  mouse_adb_address_ = 3;
  saved_cur_mouse_button_ = false;
  mouse_adb_delta_h_ = 0;
  mouse_adb_delta_v_ = 0;
  keyboard_adb_address_ = 2;

  key_queue_head_ = 0;
  key_queue_tail_ = 0;
  mouse_queue_head_ = 0;
  mouse_queue_tail_ = 0;
  last_mouse_h_ = -1;
  last_mouse_v_ = -1;
}

bool AdbDevice::MouseEnabled() const { return !ADBMouseDisabled; }

void AdbDevice::DoMouseTalk() {
  switch (cur_cmd_ & 3) {
    case 0: {
      MyEvtQEl* p;
      uint16_t partH;
      uint16_t partV;
      bool overflow = false;
      bool MouseButtonChange = false;

      if (nullptr != (p = EvtQOutP())) {
        if (MyEvtQElKindMouseDelta == p->kind) {
          mouse_adb_delta_h_ += p->u.pos.h;
          mouse_adb_delta_v_ += p->u.pos.v;
          EvtQOutDone();
        }
      }
      partH = mouse_adb_delta_h_;
      partV = mouse_adb_delta_v_;

      if ((int16_t)mouse_adb_delta_h_ < 0) {
        partH = -partH;
      }
      if ((int16_t)mouse_adb_delta_v_ < 0) {
        partV = -partV;
      }
      if ((partH >> 6) > 0) {
        overflow = true;
        partH = (1 << 6) - 1;
      }
      if ((partV >> 6) > 0) {
        overflow = true;
        partV = (1 << 6) - 1;
      }
      if ((int16_t)mouse_adb_delta_h_ < 0) {
        partH = -partH;
      }
      if ((int16_t)mouse_adb_delta_v_ < 0) {
        partV = -partV;
      }
      mouse_adb_delta_h_ -= partH;
      mouse_adb_delta_v_ -= partV;
      if (!overflow) {
        if (nullptr != (p = EvtQOutP())) {
          if (MyEvtQElKindMouseButton == p->kind) {
            saved_cur_mouse_button_ = p->u.press.down;
            MouseButtonChange = true;
            EvtQOutDone();
          }
        }
      }
      if ((0 != partH) || (0 != partV) || MouseButtonChange) {
        sz_dat_buf_ = 2;
        talk_dat_buf_ = true;
        dat_buf_[0] = (saved_cur_mouse_button_ ? 0x00 : 0x80) | (partV & 127);
        dat_buf_[1] = /* 0x00 */ 0x80 | (partH & 127);
      }
    }
      ADBMouseDisabled = 0;
      break;
    case 3:
      sz_dat_buf_ = 2;
      talk_dat_buf_ = true;
      dat_buf_[0] = 0x60 | (not_so_rand_addr_ & 0x0f);
      dat_buf_[1] = 0x01;
      not_so_rand_addr_ += 1;
      break;
    default:
      ReportAbnormalID(0x0D01, "Talk to unknown mouse register");
      break;
  }
}

void AdbDevice::DoMouseListen() {
  switch (cur_cmd_ & 3) {
    case 3:
      if (dat_buf_[1] == 0xFE) {
        /* change address */
        DLOG(
                "ADB_DoMouseListen: changing MouseADBAddress from %d to %d\n",
                mouse_adb_address_, (dat_buf_[0] & 0x0F));
        mouse_adb_address_ = (dat_buf_[0] & 0x0F);
      } else {
        ReportAbnormalID(0x0D02, "unknown listen op to mouse register 3");
      }
      break;
    default:
      ReportAbnormalID(0x0D03, "listen to unknown mouse register");
      break;
  }
}

bool AdbDevice::CheckForADBkeyEvt(uint8_t* next_adb_key_event) {
  int keycode;
  bool is_down;

  if (!FindKeyEvent(&keycode, &is_down)) {
    return false;
  } else {
    switch (keycode) {
      case MKC_Control:
        keycode = 0x36;
        break;
      case MKC_Left:
        keycode = 0x3B;
        break;
      case MKC_Right:
        keycode = 0x3C;
        break;
      case MKC_Down:
        keycode = 0x3D;
        break;
      case MKC_Up:
        keycode = 0x3E;
        break;
      default:
        /* unchanged */
        break;
    }
    *next_adb_key_event = (is_down ? 0x00 : 0x80) | keycode;
    return true;
  }
}

void AdbDevice::DoKeyboardTalk() {
  switch (cur_cmd_ & 3) {
    case 0: {
      uint8_t next_adb_key_event;

      if (CheckForADBkeyEvt(&next_adb_key_event)) {
        DLOG( "ADB_DoKeyboardTalk: sending key event %02x\n",
                next_adb_key_event);
        sz_dat_buf_ = 2;
        talk_dat_buf_ = true;
        dat_buf_[0] = next_adb_key_event;
        if (!CheckForADBkeyEvt(&next_adb_key_event)) {
          dat_buf_[1] = 0xFF;
        } else {
          DLOG( "ADB_DoKeyboardTalk: sending second key event %02x\n",
                  next_adb_key_event);
          dat_buf_[1] = next_adb_key_event;
        }
      }
    } break;
    case 3:
      sz_dat_buf_ = 2;
      talk_dat_buf_ = true;
      dat_buf_[0] = 0x60 | (not_so_rand_addr_ & 0x0f);
      dat_buf_[1] = 0x01;
      not_so_rand_addr_ += 1;
      break;
    default:
      ReportAbnormalID(0x0D04, "Talk to unknown keyboard register");
      break;
  }
}

void AdbDevice::DoKeyboardListen() {
  switch (cur_cmd_ & 3) {
    case 3:
      if (dat_buf_[1] == 0xFE) {
        /* change address */
        DLOG("ADB_DoKeyboardListen: changing KeyboardADBAddress from %d to %d\n",
            keyboard_adb_address_, (dat_buf_[0] & 0x0F));
        keyboard_adb_address_ = (dat_buf_[0] & 0x0F);
      } else {
        ReportAbnormalID(0x0D05, "unknown listen op to keyboard register 3");
      }
      break;
    default:
      ReportAbnormalID(0x0D06, "listen to unknown keyboard register");
      break;
  }
}

bool AdbDevice::CheckForADBanyEvt() {
  MyEvtQEl* p = EvtQOutP();
  if (nullptr != p) {
    switch (p->kind) {
      case MyEvtQElKindMouseButton:
      case MyEvtQElKindMouseDelta:
      case MyEvtQElKindKey:
        return true;
        break;
      default:
        break;
    }
  }

  return (0 != mouse_adb_delta_h_) && (0 != mouse_adb_delta_v_);
}

void AdbDevice::DoTalk() {
  uint8_t Address = cur_cmd_ >> 4;
  if (Address == keyboard_adb_address_) {
    DLOG( "ADB_DoTalk: talking to KEYBOARD (addr=%d)\n", Address);
  } else if (Address == mouse_adb_address_) {
    DLOG( "ADB_DoTalk: talking to MOUSE (addr=%d)\n", Address);
  } else {
    DLOG( "ADB_DoTalk: talking to OTHER (addr=%d, cmd=%02x)\n",
            Address, cur_cmd_);
  }

  if (Address == mouse_adb_address_) {
    DoMouseTalk();
  } else if (Address == keyboard_adb_address_) {
    DoKeyboardTalk();
  }
}

void AdbDevice::EndListen() {
  uint8_t Address = cur_cmd_ >> 4;
  if (Address == mouse_adb_address_) {
    DoMouseListen();
  } else if (Address == keyboard_adb_address_) {
    DoKeyboardListen();
  }
}

void AdbDevice::DoReset() {
  DLOG(
          "ADB_DoReset: resetting addresses to default (kbd=2, mouse=3)\n");
  mouse_adb_address_ = 3;
  keyboard_adb_address_ = 2;
}

void AdbDevice::Flush() {
  uint8_t Address = cur_cmd_ >> 4;

  if ((Address == keyboard_adb_address_) || (Address == mouse_adb_address_)) {
    sz_dat_buf_ = 2;
    talk_dat_buf_ = true;
    dat_buf_[0] = 0x00;
    dat_buf_[1] = 0x00;
  } else {
    ReportAbnormalID(0x0D07, "Unhandled ADB Flush");
  }
}

void AdbDevice::DoNewState() {
  uint8_t state = ADB_st1 * 2 + ADB_st0;
#ifdef _VIA_Debug
  DLOG( "ADB_DoNewState: %d\n", state);
#endif
  {
    ADB_Int = 1;
    switch (state) {
      case 0: /* Start a new command */
        if (listen_dat_buf_) {
          listen_dat_buf_ = false;
          sz_dat_buf_ = index_dat_buf_;
          EndListen();
        }
        talk_dat_buf_ = false;
        index_dat_buf_ = 0;
        cur_cmd_ = ADB_ShiftInData();
        /* which sets interrupt, acknowleding command */
#ifdef _VIA_Debug
        DLOG( "in: %d\n", cur_cmd_);
#endif
        switch ((cur_cmd_ >> 2) & 3) {
          case 0: /* reserved */
            switch (cur_cmd_ & 3) {
              case 0: /* Send Reset */
                DoReset();
                break;
              case 1: /* Flush */
                Flush();
                break;
              case 2: /* reserved */
              case 3: /* reserved */
                ReportAbnormalID(0x0C01, "Reserved ADB command");
                break;
            }
            break;
          case 1: /* reserved */
            ReportAbnormalID(0x0C02, "Reserved ADB command");
            break;
          case 2: /* listen */
            listen_dat_buf_ = true;
#ifdef _VIA_Debug
            DLOG( "*** listening\n");
#endif
            break;
          case 3: /* talk */
            DoTalk();
            break;
        }
        break;
      case 1: /* Transfer date byte (even) */
      case 2: /* Transfer date byte (odd) */
        if (!listen_dat_buf_) {
          if ((!talk_dat_buf_) || (index_dat_buf_ >= sz_dat_buf_)) {
            ADB_ShiftOutData(0xFF);
            ADB_Data = 1;
            ADB_Int = 0;
          } else {
#ifdef _VIA_Debug
            DLOG( "*** talk one\n");
#endif
            ADB_ShiftOutData(dat_buf_[index_dat_buf_]);
            ADB_Data = 1;
            index_dat_buf_ += 1;
          }
        } else {
          if (index_dat_buf_ >= ADB_MaxSzDatBuf) {
            ReportAbnormalID(0x0C03, "ADB listen too much");
            (void)ADB_ShiftInData();
          } else {
#ifdef _VIA_Debug
            DLOG( "*** listen one\n");
#endif
            dat_buf_[index_dat_buf_] = ADB_ShiftInData();
            index_dat_buf_ += 1;
          }
        }
        break;
      case 3: /* idle */
        if (listen_dat_buf_) {
          ReportAbnormalID(0x0C04, "ADB idle follows listen");
        }
        if (talk_dat_buf_) {
          if (index_dat_buf_ != 0) {
            ReportAbnormalID(0x0C05, "idle when not done talking");
          }
          ADB_ShiftOutData(0xFF);
        } else if (CheckForADBanyEvt()) {
          if (((cur_cmd_ >> 2) & 3) == 3) {
            DoTalk();
          }
          ADB_ShiftOutData(0xFF);
        }
        break;
    }
  }
}

void AdbDevice::StateChangeNtfy() {
  DLOG( "ADBstate_ChangeNtfy called! st1=%d st0=%d curicount=%d\n",
          (int)ADB_st1, (int)ADB_st0, (int)GetCuriCount());
  fflush(stderr);
  ::ICT_add(kICT_ADB_NewState, 348160UL * kCycleScale / 64 * kMyClockMult);
}

void AdbDevice::DataLineChngNtfy() {
#ifdef _VIA_Debug
  DLOG( "ADB_DataLineChngNtfy: %d\n", ADB_Data);
#endif
}

void AdbDevice::Update() {
  uint8_t state = ADB_st1 * 2 + ADB_st0;

  if (state == 3) {
    if (talk_dat_buf_) {
      /* ignore, presumably being taken care of */
    } else if (CheckForADBanyEvt()) {
      if (((cur_cmd_ >> 2) & 3) == 3) {
        DoTalk();
      }
      ADB_ShiftOutData(0xFF);
    }
  }
}

void AdbDevice::MouseUpdate() {
#if HaveMasterMyEvtQLock
  if (0 != MasterMyEvtQLock) {
    --MasterMyEvtQLock;
  }
#endif

  if (MouseEnabled()) {
    MyEvtQEl* p;

    if (
#if HaveMasterMyEvtQLock
        (0 == MasterMyEvtQLock) &&
#endif
        (nullptr != (p = EvtQOutP()))) {
#if EmClassicKbrd
#if EnableMouseMotion
      if (MyEvtQElKindMouseDelta == p->kind) {
        if ((p->u.pos.h != 0) || (p->u.pos.v != 0)) {
          put_ram_word(0x0828, get_ram_word(0x0828) + p->u.pos.v);
          put_ram_word(0x082A, get_ram_word(0x082A) + p->u.pos.h);
          put_ram_byte(0x08CE, get_ram_byte(0x08CF));
        }
        EvtQOutDone();
      } else
#endif
#endif
          if (MyEvtQElKindMousePos == p->kind) {
        uint32_t NewMouse = (p->u.pos.v << 16) | p->u.pos.h;

        if (get_ram_long(0x0828) != NewMouse) {
          put_ram_long(0x0828, NewMouse);
          put_ram_long(0x082C, NewMouse);
#if EmClassicKbrd
          put_ram_byte(0x08CE, get_ram_byte(0x08CF));
#else
          put_ram_long(0x0830, NewMouse);
          put_ram_byte(0x08CE, 0xFF);
#endif
        }
        EvtQOutDone();
      }
    }
  }

#if EmClassicKbrd
  {
    MyEvtQEl* p;

    if (
#if HaveMasterMyEvtQLock
        (0 == MasterMyEvtQLock) &&
#endif
        (nullptr != (p = EvtQOutP()))) {
      if (MyEvtQElKindMouseButton == p->kind) {
        MouseBtnUp = p->u.press.down ? 0 : 1;
        EvtQOutDone();
        MasterMyEvtQLock = 4;
      }
    }
  }
#endif
}

void AdbDevice::MouseEndTickNotify() {
  if (MouseEnabled()) {
    /* tell platform specific code where the mouse went */
    system_->get_cur_mouse_v() = get_ram_word(0x082C);
    system_->get_cur_mouse_h() = get_ram_word(0x082E);
  }
}

void AdbDevice::PushKeyPress(uint8_t keycode, bool is_down) {
  DLOG( "AdbDevice::PushKeyPress: key=%02x down=%d\n", (int)keycode,
          (int)is_down);
  int next_tail = (key_queue_tail_ + 1) % kKeyQueueSize;
  if (next_tail != key_queue_head_) {
    key_queue_[key_queue_tail_].key = keycode;
    key_queue_[key_queue_tail_].down = is_down;
    key_queue_tail_ = next_tail;
  } else {
    DLOG( "Keyboard queue overflow!\n");
  }
}

void AdbDevice::PushMouseMove(int x, int y) {
  if (last_mouse_h_ == -1) {
    last_mouse_h_ = x;
    last_mouse_v_ = y;
    return;
  }
  int dh = x - last_mouse_h_;
  int dv = y - last_mouse_v_;
  if (dh != 0 || dv != 0) {
    int next_tail = (mouse_queue_tail_ + 1) % kMouseQueueSize;
    if (next_tail != mouse_queue_head_) {
      mouse_queue_[mouse_queue_tail_].kind = MyEvtQElKindMouseDelta;
      mouse_queue_[mouse_queue_tail_].u.pos.h = dh;
      mouse_queue_[mouse_queue_tail_].u.pos.v = dv;
      mouse_queue_tail_ = next_tail;
    } else {
      DLOG( "Mouse queue overflow!\n");
    }
    last_mouse_h_ = x;
    last_mouse_v_ = y;
  }
}

void AdbDevice::PushMouseButton(bool is_down) {
  int next_tail = (mouse_queue_tail_ + 1) % kMouseQueueSize;
  if (next_tail != mouse_queue_head_) {
    mouse_queue_[mouse_queue_tail_].kind = MyEvtQElKindMouseButton;
    mouse_queue_[mouse_queue_tail_].u.press.down = is_down;
    mouse_queue_[mouse_queue_tail_].u.press.key = 0;
    mouse_queue_tail_ = next_tail;
  } else {
    DLOG( "Mouse queue overflow!\n");
  }
}

MyEvtQEl* AdbDevice::EvtQOutP() {
  if (mouse_queue_head_ != mouse_queue_tail_) {
    return &mouse_queue_[mouse_queue_head_];
  }
  if (key_queue_head_ != key_queue_tail_) {
    dummy_key_evt_.kind = MyEvtQElKindKey;
    dummy_key_evt_.u.press.key = key_queue_[key_queue_head_].key;
    dummy_key_evt_.u.press.down = key_queue_[key_queue_head_].down;
    return &dummy_key_evt_;
  }
  return nullptr;
}

void AdbDevice::EvtQOutDone() {
  if (mouse_queue_head_ != mouse_queue_tail_) {
    mouse_queue_head_ = (mouse_queue_head_ + 1) % kMouseQueueSize;
  }
}

bool AdbDevice::FindKeyEvent(int* keycode, bool* is_down) {
  if (key_queue_head_ != key_queue_tail_) {
    *keycode = key_queue_[key_queue_head_].key;
    *is_down = key_queue_[key_queue_head_].down;
    DLOG( "AdbDevice::FindKeyEvent: popped key=%02x down=%d\n",
            *keycode, (int)*is_down);
    key_queue_head_ = (key_queue_head_ + 1) % kKeyQueueSize;
    return true;
  }
  return false;
}

extern "C" {
void Keyboard_UpdateKeyMap(uint8_t keycode, bool is_down) {
  if (g_adb) g_adb->PushKeyPress(keycode, is_down);
}
void MyMousePositionSet(int x, int y) {
  if (g_adb) g_adb->PushMouseMove(x, y);
}
void MyMouseButtonSet(bool is_down) {
  if (g_adb) g_adb->PushMouseButton(is_down);
}
void MyEvtQTryRecoverFromFull(void) {}

void ADB_DoNewState(void) {
  if (g_adb) g_adb->DoNewState();
}

void ADBstate_ChangeNtfy(void) {
  if (g_adb) g_adb->StateChangeNtfy();
}

void ADB_DataLineChngNtfy(void) {
  if (g_adb) g_adb->DataLineChngNtfy();
}

void ADB_Update(void) {
  if (g_adb) g_adb->Update();
}

void Mouse_Update(void) {
  if (g_adb) g_adb->MouseUpdate();
}

void Mouse_EndTickNotify(void) {
  if (g_adb) g_adb->MouseEndTickNotify();
}
}
