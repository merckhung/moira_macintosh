#include "src/devices/rtc.h"

#include <stdio.h>
#include <string.h>

#include "src/devices/via.h"
#include "src/system.h"

#define HaveXPRAM (CurEmMd >= kEmMd_Plus)

#if HaveXPRAM
#define Group1Base 0x10
#define Group2Base 0x08
#else
#define Group1Base 0x00
#define Group2Base 0x10
#endif

#ifndef RTCinitPRAM
#define RTCinitPRAM 1
#endif

#ifndef TrackSpeed
#define TrackSpeed 0
#endif

#ifndef AlarmOn
#define AlarmOn 0
#endif

#ifndef DiskCacheSz
#define DiskCacheSz 4
#endif

#ifndef StartUpDisk
#define StartUpDisk 0
#endif

#ifndef DiskCacheOn
#define DiskCacheOn 0
#endif

#ifndef MouseScalingOn
#define MouseScalingOn 0
#endif

#define prb_fontHi 0
#define prb_fontLo 2
#define prb_kbdPrintHi (AutoKeyRate + (AutoKeyThresh << 4))
#define prb_kbdPrintLo 0
#define prb_volClickHi (SpeakerVol + (TrackSpeed << 3) + (AlarmOn << 7))
#define prb_volClickLo (CaretBlinkTime + (DoubleClickTime << 4))
#define prb_miscHi DiskCacheSz
#define prb_miscLo                                              \
  ((MenuBlink << 2) + (StartUpDisk << 4) + (DiskCacheOn << 5) + \
   (MouseScalingOn << 6))

RtcDevice* g_rtc = nullptr;

RtcDevice::RtcDevice(MacSystem* system) : system_(system), last_real_date_(0) {
  g_rtc = this;
  memset(&rtc_, 0, sizeof(rtc_));
}

RtcDevice::~RtcDevice() {
  if (g_rtc == this) {
    g_rtc = nullptr;
  }
}

bool RtcDevice::Init() {
  int Counter;
  uint32_t secs;

  rtc_.mode = rtc_.shift_data = rtc_.counter = 0;
  rtc_.data_out = rtc_.data_next_out = 0;
  rtc_.wr_protect = false;

  secs = CurMacDateInSeconds;
  last_real_date_ = secs;

  rtc_.seconds_1[0] = secs & 0xFF;
  rtc_.seconds_1[1] = (secs & 0xFF00) >> 8;
  rtc_.seconds_1[2] = (secs & 0xFF0000) >> 16;
  rtc_.seconds_1[3] = (secs & 0xFF000000) >> 24;

  for (Counter = 0; Counter < kParamramSize; Counter++) {
    rtc_.paramram[Counter] = 0;
  }

#if RTCinitPRAM
  rtc_.paramram[0 + Group1Base] = 168; /* valid */
  rtc_.paramram[3 + Group1Base] = 34;
  rtc_.paramram[4 + Group1Base] = 204; /* portA, high */
  rtc_.paramram[5 + Group1Base] = 10;  /* portA, low */
  rtc_.paramram[6 + Group1Base] = 204; /* portB, high */
  rtc_.paramram[7 + Group1Base] = 10;  /* portB, low */
  rtc_.paramram[13 + Group1Base] = prb_fontLo;
  rtc_.paramram[14 + Group1Base] = prb_kbdPrintHi;

#if prb_volClickHi != 0
  rtc_.paramram[0 + Group2Base] = prb_volClickHi;
#endif
  rtc_.paramram[1 + Group2Base] = prb_volClickLo;
  rtc_.paramram[2 + Group2Base] = prb_miscHi;
  rtc_.paramram[3 + Group2Base] = prb_miscLo
#if 0 != vMacScreenDepth
        | 0x80
#endif
      ;

#if HaveXPRAM /* extended parameter ram initialized */
  rtc_.paramram[12] = 0x42;
  rtc_.paramram[13] = 0x75;
  rtc_.paramram[14] = 0x67;
  rtc_.paramram[15] = 0x73;
#endif

#if (CurEmMd >= kEmMd_SE) && (CurEmMd <= kEmMd_Classic)
  rtc_.paramram[0x01] = 0x80;
  rtc_.paramram[0x02] = 0x4F;
#endif

#if (CurEmMd >= kEmMd_SE) && (CurEmMd <= kEmMd_Classic)
  /* start up disk (encoded how?) */
  rtc_.paramram[0x78] = 0x00;
  rtc_.paramram[0x79] = 0x01;
  rtc_.paramram[0x7A] = 0xFF;
  rtc_.paramram[0x7B] = 0xFE;
#endif

#if HaveXPRAM /* extended parameter ram initialized */
  do_put_mem_long(&rtc_.paramram[0xE4], CurMacLatitude);
  do_put_mem_long(&rtc_.paramram[0xE8], CurMacLongitude);
  do_put_mem_long(&rtc_.paramram[0xEC], CurMacDelta);
#endif

#endif /* RTCinitPRAM */

  return true;
}

void RtcDevice::Interrupt() {
  uint32_t Seconds = 0;
  uint32_t NewRealDate = CurMacDateInSeconds;
  uint32_t DateDelta = NewRealDate - last_real_date_;

  if (DateDelta != 0) {
    Seconds = (rtc_.seconds_1[3] << 24) + (rtc_.seconds_1[2] << 16) +
              (rtc_.seconds_1[1] << 8) + rtc_.seconds_1[0];
    Seconds += DateDelta;
    rtc_.seconds_1[0] = Seconds & 0xFF;
    rtc_.seconds_1[1] = (Seconds & 0xFF00) >> 8;
    rtc_.seconds_1[2] = (Seconds & 0xFF0000) >> 16;
    rtc_.seconds_1[3] = (Seconds & 0xFF000000) >> 24;

    last_real_date_ = NewRealDate;

    if (system_->GetViaDevice()) {
      system_->GetViaDevice()->iCA2_PulseNtfy();
    }
  }
}

uint8_t RtcDevice::Access_PRAM_Reg(uint8_t data, bool is_write,
                                   uint8_t reg_index) {
  if (is_write) {
    if (!rtc_.wr_protect) {
      rtc_.paramram[reg_index] = data;
#ifdef _RTC_Debug
      printf("Writing Address %2x, Data %2x\n", reg_index, data);
#endif
    }
  } else {
    data = rtc_.paramram[reg_index];
  }
  return data;
}

uint8_t RtcDevice::Access_Reg(uint8_t data, bool is_write, uint8_t command) {
  uint8_t t = (command & 0x7C) >> 2;
  if (t < 8) {
    if (is_write) {
      if (!rtc_.wr_protect) {
        rtc_.seconds_1[t & 0x03] = data;
      }
    } else {
      data = rtc_.seconds_1[t & 0x03];
    }
  } else if (t < 12) {
    data = Access_PRAM_Reg(data, is_write, (t & 0x03) + Group2Base);
  } else if (t < 16) {
    if (is_write) {
      switch (t) {
        case 12:
          break; /* Test Write, do nothing */
        case 13:
          rtc_.wr_protect = (data & 0x80) != 0;
          break; /* Write_Protect Register */
        default:
          ReportAbnormalID(0x0801, "Write RTC Reg unknown");
          break;
      }
    } else {
      ReportAbnormalID(0x0802, "Read RTC Reg unknown");
    }
  } else {
    data = Access_PRAM_Reg(data, is_write, (t & 0x0F) + Group1Base);
  }
  return data;
}

void RtcDevice::DoCmd() {
  switch (rtc_.mode) {
    case 0: /* This Byte is a RTC Command */
#if HaveXPRAM
      if ((rtc_.shift_data & 0x78) == 0x38) {
        rtc_.saved_cmd = rtc_.shift_data;
        rtc_.mode = 2;
#ifdef _RTC_Debug
        printf("Extended command %2x\n", rtc_.shift_data);
#endif
      } else
#endif
      {
        if ((rtc_.shift_data & 0x80) != 0x00) {
          rtc_.shift_data = Access_Reg(0, false, rtc_.shift_data);
          rtc_.data_next_out = 1;
        } else { /* Write Command */
          rtc_.saved_cmd = rtc_.shift_data;
          rtc_.mode = 1;
        }
      }
      break;
    case 1: /* This Byte is data for RTC Write */
      (void)Access_Reg(rtc_.shift_data, true, rtc_.saved_cmd);
      rtc_.mode = 0;
      break;
#if HaveXPRAM
    case 2: /* This Byte is rest of Extended RTC command address */
#ifdef _RTC_Debug
      printf("Mode 2 %2x\n", rtc_.shift_data);
#endif
      rtc_.sector =
          ((rtc_.saved_cmd & 0x07) << 5) | ((rtc_.shift_data & 0x7C) >> 2);
      if ((rtc_.saved_cmd & 0x80) != 0x00) {
        rtc_.shift_data = rtc_.paramram[rtc_.sector];
        rtc_.data_next_out = 1;
        rtc_.mode = 0;
#ifdef _RTC_Debug
        printf("Reading X Address %2x, Data  %2x\n", rtc_.sector,
               rtc_.shift_data);
#endif
      } else {
        rtc_.mode = 3;
#ifdef _RTC_Debug
        printf("Writing X Address %2x\n", rtc_.sector);
#endif
      }
      break;
    case 3: /* This Byte is data for an Extended RTC Write */
      (void)Access_PRAM_Reg(rtc_.shift_data, true, rtc_.sector);
      rtc_.mode = 0;
      break;
#endif
  }
}

void RtcDevice::UnEnabledChangeNtfy() {
  if (RTCunEnabled) {
    /* abort anything going on */
    if (rtc_.counter != 0) {
#ifdef _RTC_Debug
      printf("aborting, %2x\n", rtc_.counter);
#endif
      ReportAbnormalID(0x0803, "RTC aborting");
    }
    rtc_.mode = 0;
    rtc_.data_out = 0;
    rtc_.data_next_out = 0;
    rtc_.shift_data = 0;
    rtc_.counter = 0;
  }
}

void RtcDevice::ClockChangeNtfy() {
  if (!RTCunEnabled) {
    if (RTCclock) {
      rtc_.data_out = rtc_.data_next_out;
      rtc_.counter = (rtc_.counter - 1) & 0x07;
      if (rtc_.data_out) {
        RTCdataLine = ((rtc_.shift_data >> rtc_.counter) & 0x01);
        /*
            should notify VIA if changed, so can check
            data direction
        */
        if (rtc_.counter == 0) {
          rtc_.data_next_out = 0;
        }
      } else {
        rtc_.shift_data = (rtc_.shift_data << 1) | RTCdataLine;
        if (rtc_.counter == 0) {
          DoCmd();
        }
      }
    }
  }
}

void RtcDevice::DataLineChangeNtfy() {
#if dbglog_HAVE
  if (rtc_.data_out) {
    if (!rtc_.data_next_out) {
      /*
          ignore. The ROM doesn't read from the RTC the
          way described in the Hardware Reference.
          It reads the data after setting the clock to
          one instead of before, and then immediately
          changes the VIA direction. So the RTC
          has no way of knowing to stop driving the
          data line, which certainly can't really be
          correct.
      */
    } else {
      ReportAbnormalID(0x0804, "write RTC Data unexpected direction");
    }
  }
#endif
}

extern "C" {
void RTCunEnabled_ChangeNtfy(void) {
  if (g_rtc) g_rtc->UnEnabledChangeNtfy();
}
void RTCclock_ChangeNtfy(void) {
  if (g_rtc) g_rtc->ClockChangeNtfy();
}
void RTCdataLine_ChangeNtfy(void) {
  if (g_rtc) g_rtc->DataLineChangeNtfy();
}
}
