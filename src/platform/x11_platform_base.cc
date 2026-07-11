#define IS_PLATFORM_IMPLEMENTATION
#include "src/platform/x11_platform_base.h"
#include "src/debug_log.h"

#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/devices/adb.h"
#include "src/keycodes.h"
#include "src/system.h"

X11PlatformBase::X11PlatformBase(MacSystem* system) : CommonPlatform(system) {}

X11PlatformBase::~X11PlatformBase() { CloseX11(); }

bool X11PlatformBase::OnInit() {
  InitX11();
  if (!KC2MKCInit()) {
    fprintf(stderr, "Failed to initialize keycodes map\n");
    return false;
  }
  return true;
}

void X11PlatformBase::InitX11() {
  display_ = XOpenDisplay(nullptr);
  if (!display_) {
    fprintf(stderr, "Failed to open X display\n");
    exit(1);
  }
  screen_ = DefaultScreen(display_);
  pixel_black_ = BlackPixel(display_, screen_);
  pixel_white_ = WhitePixel(display_, screen_);

  int depth = DefaultDepth(display_, screen_);
  Visual* visual = DefaultVisual(display_, screen_);
  DLOG("Default screen depth: %d, Visual class: %d\n", depth, visual->c_class);

  int width = kMacScreenWidth;
  int height = kMacScreenHeight;
  if (use_magnify_) {
    width *= window_scale_;
    height *= window_scale_;
  }

  window_ = XCreateSimpleWindow(display_, RootWindow(display_, screen_), 10, 10,
                                width, height, 1, pixel_black_, pixel_white_);

  XSelectInput(display_, window_,
               ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                   ButtonReleaseMask | PointerMotionMask | EnterWindowMask |
                   LeaveWindowMask);

  XMapWindow(display_, window_);
  gc_ = XCreateGC(display_, window_, 0, nullptr);

  // Create XImage
  image_buffer_.resize(width * height, 0xFFFFFFFF);  // Initialized to white

  x_image_ = XCreateImage(display_, visual, depth, ZPixmap, 0,
                          reinterpret_cast<char*>(image_buffer_.data()), width,
                          height, 32, width * 4);

  x11_initialized_ = true;
  DLOG("X11 initialized successfully. Window size: %dx%d\n", width, height);
}

void X11PlatformBase::CloseX11() {
  if (x11_initialized_) {
    XFreeGC(display_, gc_);
    XDestroyWindow(display_, window_);
    // Prevent XDestroyImage from freeing image_buffer_
    x_image_->data = nullptr;
    XDestroyImage(x_image_);
    XCloseDisplay(display_);
    x11_initialized_ = false;
  }
}

void X11PlatformBase::DoneWithDrawingForTick() {
  if (x11_initialized_) {
    int width = kMacScreenWidth;
    int height = kMacScreenHeight;
    if (use_magnify_) {
      width *= window_scale_;
      height *= window_scale_;
    }
    XPutImage(display_, window_, gc_, x_image_, 0, 0, 0, 0, width, height);
    XFlush(display_);
  }
}

void X11PlatformBase::DumpDebugInfo() {
  CommonPlatform::DumpDebugInfo();
  DLOG("X11 Platform: %s\n",
       x11_initialized_ ? "Initialized" : "Not Initialized");
}

void X11PlatformBase::MousePositionNotify(int x, int y) {
  int mac_x = x;
  int mac_y = y;
  int width = kMacScreenWidth;
  int height = kMacScreenHeight;
  if (use_magnify_) {
    width *= window_scale_;
    height *= window_scale_;
  }

  if (width > 0 && height > 0) {
    mac_x = mac_x * kMacScreenWidth / width;
    mac_y = mac_y * kMacScreenHeight / height;
  }

  if (mac_x < 0)
    mac_x = 0;
  else if (mac_x >= kMacScreenWidth)
    mac_x = kMacScreenWidth - 1;

  if (mac_y < 0)
    mac_y = 0;
  else if (mac_y >= kMacScreenHeight)
    mac_y = kMacScreenHeight - 1;

  system_->GetAdbDevice()->PushMouseMove(mac_x, mac_y);
}

void X11PlatformBase::KC2MKCAssignOne(KeySym ks, uint8_t key) {
  KeyCode code = XKeysymToKeycode(display_, ks);
  if (code != NoSymbol) {
    KC2MKC_[code] = key;
  }
}

bool X11PlatformBase::KC2MKCInit() {
  for (int i = 0; i < 256; ++i) {
    KC2MKC_[i] = MKC_None;
  }

  // Redundant mappings
#ifdef XK_KP_Insert
  KC2MKCAssignOne(XK_KP_Insert, MKC_KP0);
#endif
#ifdef XK_KP_End
  KC2MKCAssignOne(XK_KP_End, MKC_KP1);
#endif
#ifdef XK_KP_Down
  KC2MKCAssignOne(XK_KP_Down, MKC_KP2);
#endif
#ifdef XK_KP_Next
  KC2MKCAssignOne(XK_KP_Next, MKC_KP3);
#endif
#ifdef XK_KP_Left
  KC2MKCAssignOne(XK_KP_Left, MKC_KP4);
#endif
#ifdef XK_KP_Begin
  KC2MKCAssignOne(XK_KP_Begin, MKC_KP5);
#endif
#ifdef XK_KP_Right
  KC2MKCAssignOne(XK_KP_Right, MKC_KP6);
#endif
#ifdef XK_KP_Home
  KC2MKCAssignOne(XK_KP_Home, MKC_KP7);
#endif
#ifdef XK_KP_Up
  KC2MKCAssignOne(XK_KP_Up, MKC_KP8);
#endif
#ifdef XK_KP_Prior
  KC2MKCAssignOne(XK_KP_Prior, MKC_KP9);
#endif
#ifdef XK_KP_Delete
  KC2MKCAssignOne(XK_KP_Delete, MKC_Decimal);
#endif

  KC2MKCAssignOne(XK_asciitilde, MKC_formac_Grave);
  KC2MKCAssignOne(XK_underscore, MKC_Minus);
  KC2MKCAssignOne(XK_plus, MKC_Equal);
  KC2MKCAssignOne(XK_braceleft, MKC_LeftBracket);
  KC2MKCAssignOne(XK_braceright, MKC_RightBracket);
  KC2MKCAssignOne(XK_bar, MKC_formac_BackSlash);
  KC2MKCAssignOne(XK_colon, MKC_SemiColon);
  KC2MKCAssignOne(XK_quotedbl, MKC_SingleQuote);
  KC2MKCAssignOne(XK_less, MKC_Comma);
  KC2MKCAssignOne(XK_greater, MKC_Period);
  KC2MKCAssignOne(XK_question, MKC_formac_Slash);

  KC2MKCAssignOne(XK_a, MKC_A);
  KC2MKCAssignOne(XK_b, MKC_B);
  KC2MKCAssignOne(XK_c, MKC_C);
  KC2MKCAssignOne(XK_d, MKC_D);
  KC2MKCAssignOne(XK_e, MKC_E);
  KC2MKCAssignOne(XK_f, MKC_F);
  KC2MKCAssignOne(XK_g, MKC_G);
  KC2MKCAssignOne(XK_h, MKC_H);
  KC2MKCAssignOne(XK_i, MKC_I);
  KC2MKCAssignOne(XK_j, MKC_J);
  KC2MKCAssignOne(XK_k, MKC_K);
  KC2MKCAssignOne(XK_l, MKC_L);
  KC2MKCAssignOne(XK_m, MKC_M);
  KC2MKCAssignOne(XK_n, MKC_N);
  KC2MKCAssignOne(XK_o, MKC_O);
  KC2MKCAssignOne(XK_p, MKC_P);
  KC2MKCAssignOne(XK_q, MKC_Q);
  KC2MKCAssignOne(XK_r, MKC_R);
  KC2MKCAssignOne(XK_s, MKC_S);
  KC2MKCAssignOne(XK_t, MKC_T);
  KC2MKCAssignOne(XK_u, MKC_U);
  KC2MKCAssignOne(XK_v, MKC_V);
  KC2MKCAssignOne(XK_w, MKC_W);
  KC2MKCAssignOne(XK_x, MKC_X);
  KC2MKCAssignOne(XK_y, MKC_Y);
  KC2MKCAssignOne(XK_z, MKC_Z);

  // Main mappings
  KC2MKCAssignOne(XK_F1, MKC_formac_F1);
  KC2MKCAssignOne(XK_F2, MKC_formac_F2);
  KC2MKCAssignOne(XK_F3, MKC_formac_F3);
  KC2MKCAssignOne(XK_F4, MKC_formac_F4);
  KC2MKCAssignOne(XK_F5, MKC_formac_F5);
  KC2MKCAssignOne(XK_F6, MKC_F6);
  KC2MKCAssignOne(XK_F7, MKC_F7);
  KC2MKCAssignOne(XK_F8, MKC_F8);
  KC2MKCAssignOne(XK_F9, MKC_F9);
  KC2MKCAssignOne(XK_F10, MKC_F10);
  KC2MKCAssignOne(XK_F11, MKC_F11);
  KC2MKCAssignOne(XK_F12, MKC_F12);

#ifdef XK_Delete
  KC2MKCAssignOne(XK_Delete, MKC_formac_ForwardDel);
#endif
#ifdef XK_Insert
  KC2MKCAssignOne(XK_Insert, MKC_formac_Help);
#endif
#ifdef XK_Help
  KC2MKCAssignOne(XK_Help, MKC_formac_Help);
#endif
#ifdef XK_Home
  KC2MKCAssignOne(XK_Home, MKC_formac_Home);
#endif
#ifdef XK_End
  KC2MKCAssignOne(XK_End, MKC_formac_End);
#endif

#ifdef XK_Page_Up
  KC2MKCAssignOne(XK_Page_Up, MKC_formac_PageUp);
#else
#ifdef XK_Prior
  KC2MKCAssignOne(XK_Prior, MKC_formac_PageUp);
#endif
#endif

#ifdef XK_Page_Down
  KC2MKCAssignOne(XK_Page_Down, MKC_formac_PageDown);
#else
#ifdef XK_Next
  KC2MKCAssignOne(XK_Next, MKC_formac_PageDown);
#endif
#endif

#ifdef XK_Print
  KC2MKCAssignOne(XK_Print, MKC_Print);
#endif
#ifdef XK_Scroll_Lock
  KC2MKCAssignOne(XK_Scroll_Lock, MKC_ScrollLock);
#endif
#ifdef XK_Pause
  KC2MKCAssignOne(XK_Pause, MKC_Pause);
#endif

  KC2MKCAssignOne(XK_KP_Add, MKC_KPAdd);
  KC2MKCAssignOne(XK_KP_Subtract, MKC_KPSubtract);
  KC2MKCAssignOne(XK_KP_Multiply, MKC_KPMultiply);
  KC2MKCAssignOne(XK_KP_Divide, MKC_KPDevide);
  KC2MKCAssignOne(XK_KP_Enter, MKC_formac_Enter);
  KC2MKCAssignOne(XK_KP_Equal, MKC_KPEqual);

  KC2MKCAssignOne(XK_KP_0, MKC_KP0);
  KC2MKCAssignOne(XK_KP_1, MKC_KP1);
  KC2MKCAssignOne(XK_KP_2, MKC_KP2);
  KC2MKCAssignOne(XK_KP_3, MKC_KP3);
  KC2MKCAssignOne(XK_KP_4, MKC_KP4);
  KC2MKCAssignOne(XK_KP_5, MKC_KP5);
  KC2MKCAssignOne(XK_KP_6, MKC_KP6);
  KC2MKCAssignOne(XK_KP_7, MKC_KP7);
  KC2MKCAssignOne(XK_KP_8, MKC_KP8);
  KC2MKCAssignOne(XK_KP_9, MKC_KP9);
  KC2MKCAssignOne(XK_KP_Decimal, MKC_Decimal);

  KC2MKCAssignOne(XK_Left, MKC_Left);
  KC2MKCAssignOne(XK_Right, MKC_Right);
  KC2MKCAssignOne(XK_Up, MKC_Up);
  KC2MKCAssignOne(XK_Down, MKC_Down);

  KC2MKCAssignOne(XK_grave, MKC_formac_Grave);
  KC2MKCAssignOne(XK_minus, MKC_Minus);
  KC2MKCAssignOne(XK_equal, MKC_Equal);
  KC2MKCAssignOne(XK_bracketleft, MKC_LeftBracket);
  KC2MKCAssignOne(XK_bracketright, MKC_RightBracket);
  KC2MKCAssignOne(XK_backslash, MKC_formac_BackSlash);
  KC2MKCAssignOne(XK_semicolon, MKC_SemiColon);
  KC2MKCAssignOne(XK_apostrophe, MKC_SingleQuote);
  KC2MKCAssignOne(XK_comma, MKC_Comma);
  KC2MKCAssignOne(XK_period, MKC_Period);
  KC2MKCAssignOne(XK_slash, MKC_formac_Slash);

  KC2MKCAssignOne(XK_Escape, MKC_formac_Escape);

  KC2MKCAssignOne(XK_Tab, MKC_Tab);
  KC2MKCAssignOne(XK_Return, MKC_Return);
  KC2MKCAssignOne(XK_space, MKC_Space);
  KC2MKCAssignOne(XK_BackSpace, MKC_BackSpace);

  KC2MKCAssignOne(XK_Caps_Lock, MKC_formac_CapsLock);
  KC2MKCAssignOne(XK_Num_Lock, MKC_Clear);

  KC2MKCAssignOne(XK_Meta_L, MKC_formac_Command);
  KC2MKCAssignOne(XK_Meta_R, MKC_formac_RCommand);

  KC2MKCAssignOne(XK_Mode_switch, MKC_formac_Option);
  KC2MKCAssignOne(XK_Menu, MKC_formac_Option);
  KC2MKCAssignOne(XK_Super_L, MKC_formac_Option);
  KC2MKCAssignOne(XK_Super_R, MKC_formac_ROption);
  KC2MKCAssignOne(XK_Hyper_L, MKC_formac_Option);
  KC2MKCAssignOne(XK_Hyper_R, MKC_formac_ROption);

  KC2MKCAssignOne(XK_F13, MKC_formac_Option);

  KC2MKCAssignOne(XK_Shift_L, MKC_formac_Shift);
  KC2MKCAssignOne(XK_Shift_R, MKC_formac_RShift);

  KC2MKCAssignOne(XK_Alt_L, MKC_formac_Command);
  KC2MKCAssignOne(XK_Alt_R, MKC_formac_RCommand);

  KC2MKCAssignOne(XK_Control_L, MKC_formac_Control);
  KC2MKCAssignOne(XK_Control_R, MKC_formac_RControl);

  KC2MKCAssignOne(XK_1, MKC_1);
  KC2MKCAssignOne(XK_2, MKC_2);
  KC2MKCAssignOne(XK_3, MKC_3);
  KC2MKCAssignOne(XK_4, MKC_4);
  KC2MKCAssignOne(XK_5, MKC_5);
  KC2MKCAssignOne(XK_6, MKC_6);
  KC2MKCAssignOne(XK_7, MKC_7);
  KC2MKCAssignOne(XK_8, MKC_8);
  KC2MKCAssignOne(XK_9, MKC_9);
  KC2MKCAssignOne(XK_0, MKC_0);

  KC2MKCAssignOne(XK_A, MKC_A);
  KC2MKCAssignOne(XK_B, MKC_B);
  KC2MKCAssignOne(XK_C, MKC_C);
  KC2MKCAssignOne(XK_D, MKC_D);
  KC2MKCAssignOne(XK_E, MKC_E);
  KC2MKCAssignOne(XK_F, MKC_F);
  KC2MKCAssignOne(XK_G, MKC_G);
  KC2MKCAssignOne(XK_H, MKC_H);
  KC2MKCAssignOne(XK_I, MKC_I);
  KC2MKCAssignOne(XK_J, MKC_J);
  KC2MKCAssignOne(XK_K, MKC_K);
  KC2MKCAssignOne(XK_L, MKC_L);
  KC2MKCAssignOne(XK_M, MKC_M);
  KC2MKCAssignOne(XK_N, MKC_N);
  KC2MKCAssignOne(XK_O, MKC_O);
  KC2MKCAssignOne(XK_P, MKC_P);
  KC2MKCAssignOne(XK_Q, MKC_Q);
  KC2MKCAssignOne(XK_r, MKC_R);
  KC2MKCAssignOne(XK_s, MKC_S);
  KC2MKCAssignOne(XK_t, MKC_T);
  KC2MKCAssignOne(XK_u, MKC_U);
  KC2MKCAssignOne(XK_v, MKC_V);
  KC2MKCAssignOne(XK_w, MKC_W);
  KC2MKCAssignOne(XK_x, MKC_X);
  KC2MKCAssignOne(XK_y, MKC_Y);
  KC2MKCAssignOne(XK_z, MKC_Z);

  TheCapsLockCode_ = XKeysymToKeycode(display_, XK_Caps_Lock);

  return true;
}

void X11PlatformBase::CheckTheCapsLock() {
  int NewMousePosh;
  int NewMousePosv;
  Window root_return;
  Window child_return;
  int root_x_return;
  int root_y_return;
  unsigned int mask_return;

  XQueryPointer(display_, window_, &root_return, &child_return, &root_x_return,
                &root_y_return, &NewMousePosh, &NewMousePosv, &mask_return);

  system_->GetAdbDevice()->PushKeyPress(MKC_formac_CapsLock,
                                        (mask_return & LockMask) != 0);
}

void X11PlatformBase::DoKeyCode0(int i, bool down) {
  uint8_t key = KC2MKC_[i];
  if (MKC_None != key) {
    system_->GetAdbDevice()->PushKeyPress(key, down);
  }
}

void X11PlatformBase::DoKeyCode(int i, bool down) {
  if (i == TheCapsLockCode_) {
    CheckTheCapsLock();
  } else if (i >= 0 && i < 256) {
    DoKeyCode0(i, down);
  }
}

void X11PlatformBase::PollEvents() {
  if (!x11_initialized_) return;

  XEvent event;
  while (XPending(display_)) {
    XNextEvent(display_, &event);
    switch (event.type) {
      case Expose:
        DoneWithDrawingForTick();
        break;
      case KeyPress:
        if (event.xkey.window == window_) {
          KeySym ks = XLookupKeysym(&event.xkey, 0);
          // Quit on Ctrl-Q
          if (ks == XK_q && (event.xkey.state & ControlMask)) {
            fprintf(stderr, "Control-Q detected, exiting...\n");
            exit(0);
          }
          MousePositionNotify(event.xkey.x, event.xkey.y);
          DoKeyCode(event.xkey.keycode, true);
        }
        break;
      case KeyRelease:
        if (event.xkey.window == window_) {
          MousePositionNotify(event.xkey.x, event.xkey.y);
          DoKeyCode(event.xkey.keycode, false);
        }
        break;
      case ButtonPress:
        if (event.xbutton.window == window_) {
          MousePositionNotify(event.xbutton.x, event.xbutton.y);
          system_->GetAdbDevice()->PushMouseButton(true);
        }
        break;
      case ButtonRelease:
        if (event.xbutton.window == window_) {
          MousePositionNotify(event.xbutton.x, event.xbutton.y);
          system_->GetAdbDevice()->PushMouseButton(false);
        }
        break;
      case MotionNotify:
        if (event.xmotion.window == window_) {
          MousePositionNotify(event.xmotion.x, event.xmotion.y);
        }
        break;
      case EnterNotify:
        if (event.xcrossing.window == window_) {
          MousePositionNotify(event.xcrossing.x, event.xcrossing.y);
        }
        break;
      case LeaveNotify:
        if (event.xcrossing.window == window_) {
          MousePositionNotify(event.xcrossing.x, event.xcrossing.y);
        }
        break;
    }
  }
}
