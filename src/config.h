#ifndef CONFIG_H
#define CONFIG_H

/* Automatically consolidated configuration */

// ==========================================
// 1. System Includes (from CNFGRAPI.h)
// ==========================================
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>

// ==========================================
// 2. Global compiler/host definitions & Types (from CNFGGLOB.h)
// ==========================================
/* adapt to current compiler/host processor */
#ifdef __i386__
#error "source is configured for 64 bit compiler"
#endif

#define MayInline inline __attribute__((always_inline))
#define MayNotInline __attribute__((noinline))
#define SmallGlobals 0
#define cIncludeUnused 0
#define UnusedParam(p) (void)p

/* Legacy types removed */

// ==========================================
// 3. Platform & API Configuration (from CNFGRAPI.h)
// ==========================================
#define CanGetAppPath 1
#define HaveAppPathLink 1
#define TheAppPathLink "/proc/self/exe"
#define HaveSysctlPath 0

#define RomFileName "testdata/MacSE.ROM"
#define kCheckSumRom_Size 0x00040000
#define kRomCheckSum1 0xB2E362A8
#define RomStartCheckSum 1
#define EnableDragDrop 1
#define SaveDialogEnable 1
#define EnableAltKeysMode 0
#define MKC_formac_Control MKC_CM
#define MKC_formac_Command MKC_Command
#define MKC_formac_Option MKC_Option
#define MKC_formac_Shift MKC_Shift
#define MKC_formac_CapsLock MKC_CapsLock
#define MKC_formac_Escape MKC_Escape
#define MKC_formac_BackSlash MKC_BackSlash
#define MKC_formac_Slash MKC_Slash
#define MKC_formac_Grave MKC_Grave
#define MKC_formac_Enter MKC_Enter
#define MKC_formac_PageUp MKC_PageUp
#define MKC_formac_PageDown MKC_PageDown
#define MKC_formac_Home MKC_Home
#define MKC_formac_End MKC_End
#define MKC_formac_Help MKC_Help
#define MKC_formac_ForwardDel MKC_ForwardDel
#define MKC_formac_F1 MKC_Option
#define MKC_formac_F2 MKC_Command
#define MKC_formac_F3 MKC_F3
#define MKC_formac_F4 MKC_F4
#define MKC_formac_F5 MKC_F5
#define MKC_formac_RControl MKC_CM
#define MKC_formac_RCommand MKC_Command
#define MKC_formac_ROption MKC_Option
#define MKC_formac_RShift MKC_Shift
#define MKC_UnMappedKey MKC_Control
#define VarFullScreen 1
#define WantInitFullScreen 0
#define MayFullScreen 1
#define MayNotFullScreen 1
#define WantInitMagnify 1
#define EnableMagnify 1
#define MyWindowScale 3
#define UseColorImage 1
#define WantInitRunInBackground 0
#define WantInitNotAutoSlow 0
#define WantInitSpeedValue 3
#define WantEnblCtrlInt 1
#define WantEnblCtrlRst 1
#define WantEnblCtrlKtg 1
#define NeedRequestInsertDisk 0
#define NeedDoMoreCommandsMsg 0
#define NeedDoAboutMsg 0
#define UseControlKeys 1
#define UseActvCode 0
#define EnableDemoMsg 0

/* version and other info to display to user */
#define NeedIntlChars 0
#define kStrAppName "moira_macintosh"
#define kAppVariationStr "moira_macintosh-0.1-lx64"
#define kStrCopyrightYear "2026"
#define kMaintainerName "Merck Hung"
#define kStrHomePage "https://github.com/merckhung/moira_macintosh"
#define kBldOpts "-m SE -mem 4M (Moira 68000 core)"

// ==========================================
// 4. Emulator Hardware Configuration (from EMCONFIG.h & CNFGGLOB.h)
// ==========================================
#define MySoundEnabled 1
#define MySoundRecenterSilence 0
#define kLn2SoundSampSz 3
#define dbglog_HAVE 0
#define WantAbnormalReports 0
#define NumDrives 6
#define IncludeSonyRawMode 1
#define IncludeSonyGetName 1
#define IncludeSonyNew 1
#define IncludeSonyNameNew 1

#define vMacScreenHeight 342
#define vMacScreenWidth 512
#define vMacScreenDepth 0

#define IncludePbufs 1
#define NumPbufs 4
#define EnableMouseMotion 1
#define IncludeHostTextClipExchange 1
#define EnableAutoSlow 1
#define EmLocalTalk 0
#define AutoLocation 1
#define AutoTimeZone 1

#define EmClassicKbrd 0
#define EmADB 1
#define EmRTC 1
#define EmPMU 0
#define EmVIA2 0
#define Use68020 0
#define EmFPU 0
#define EmMMU 0
#define EmASC 0

#define CurEmMd kEmMd_SE
#define kMyClockMult 100
#define WantCycByPriOp 1
#define WantCloserCyc 0

#define kRAMa_Size 0x00200000
#define kRAMb_Size 0x00200000
#define kRAM_Size (kRAMa_Size + kRAMb_Size)
#define kROM_Size 0x00040000

#define kcom_callcheck 0xA9FF
/* #define _VIA_Debug 1 */

#define IncludeVidMem 0
#define EmVidCard 0
#define MaxATTListN 16
#define IncludeExtnPbufs 1
#define IncludeExtnHostTextClipExchange 1

#define Sony_SupportDC42 1
#define Sony_SupportTags 0
#define Sony_WantChecksumsUpdated 0
#define Sony_VerifyChecksums 0
#define CaretBlinkTime 0x03
#define SpeakerVol 0x07
#define DoubleClickTime 0x05
#define MenuBlink 0x03
#define AutoKeyThresh 0x06
#define AutoKeyRate 0x03

/* the Wire variables are 1/0, not true/false */
enum {
  Wire_VIA1_iA0_SoundVolb0,
#define SoundVolb0 (Wires[Wire_VIA1_iA0_SoundVolb0])
#define VIA1_iA0 (Wires[Wire_VIA1_iA0_SoundVolb0])
  Wire_VIA1_iA1_SoundVolb1,
#define SoundVolb1 (Wires[Wire_VIA1_iA1_SoundVolb1])
#define VIA1_iA1 (Wires[Wire_VIA1_iA1_SoundVolb1])
  Wire_VIA1_iA2_SoundVolb2,
#define SoundVolb2 (Wires[Wire_VIA1_iA2_SoundVolb2])
#define VIA1_iA2 (Wires[Wire_VIA1_iA2_SoundVolb2])
  Wire_VIA1_iA4_DriveSel,
#define VIA1_iA4 (Wires[Wire_VIA1_iA4_DriveSel])
  Wire_MemOverlay,
#define MemOverlay (Wires[Wire_MemOverlay])
  Wire_VIA1_iA6_SCRNvPage2,
#define SCRNvPage2 (Wires[Wire_VIA1_iA6_SCRNvPage2])
#define VIA1_iA6 (Wires[Wire_VIA1_iA6_SCRNvPage2])
  Wire_VIA1_iA5_IWMvSel,
#define IWMvSel (Wires[Wire_VIA1_iA5_IWMvSel])
#define VIA1_iA5 (Wires[Wire_VIA1_iA5_IWMvSel])
  Wire_VIA1_iA7_SCCwaitrq,
#define SCCwaitrq (Wires[Wire_VIA1_iA7_SCCwaitrq])
#define VIA1_iA7 (Wires[Wire_VIA1_iA7_SCCwaitrq])
  Wire_VIA1_iB0_RTCdataLine,
#define RTCdataLine (Wires[Wire_VIA1_iB0_RTCdataLine])
#define VIA1_iB0 (Wires[Wire_VIA1_iB0_RTCdataLine])
#define VIA1_iB0_ChangeNtfy RTCdataLine_ChangeNtfy
  Wire_VIA1_iB1_RTCclock,
#define RTCclock (Wires[Wire_VIA1_iB1_RTCclock])
#define VIA1_iB1 (Wires[Wire_VIA1_iB1_RTCclock])
#define VIA1_iB1_ChangeNtfy RTCclock_ChangeNtfy
  Wire_VIA1_iB2_RTCunEnabled,
#define RTCunEnabled (Wires[Wire_VIA1_iB2_RTCunEnabled])
#define VIA1_iB2 (Wires[Wire_VIA1_iB2_RTCunEnabled])
#define VIA1_iB2_ChangeNtfy RTCunEnabled_ChangeNtfy
  Wire_VIA1_iA3_SCCvSync,
#define VIA1_iA3 (Wires[Wire_VIA1_iA3_SCCvSync])
  Wire_VIA1_iB3_ADB_Int,
#define ADB_Int (Wires[Wire_VIA1_iB3_ADB_Int])
#define VIA1_iB3 (Wires[Wire_VIA1_iB3_ADB_Int])
  Wire_VIA1_iB4_ADB_st0,
#define ADB_st0 (Wires[Wire_VIA1_iB4_ADB_st0])
#define VIA1_iB4 (Wires[Wire_VIA1_iB4_ADB_st0])
#define VIA1_iB4_ChangeNtfy ADBstate_ChangeNtfy
  Wire_VIA1_iB5_ADB_st1,
#define ADB_st1 (Wires[Wire_VIA1_iB5_ADB_st1])
#define VIA1_iB5 (Wires[Wire_VIA1_iB5_ADB_st1])
#define VIA1_iB5_ChangeNtfy ADBstate_ChangeNtfy
  Wire_VIA1_iCB2_ADB_Data,
#define ADB_Data (Wires[Wire_VIA1_iCB2_ADB_Data])
#define VIA1_iCB2 (Wires[Wire_VIA1_iCB2_ADB_Data])
#define VIA1_iCB2_ChangeNtfy ADB_DataLineChngNtfy
  Wire_VIA1_iB6_SCSIintenable,
#define VIA1_iB6 (Wires[Wire_VIA1_iB6_SCSIintenable])
  Wire_VIA1_iB7_SoundDisable,
#define SoundDisable (Wires[Wire_VIA1_iB7_SoundDisable])
#define VIA1_iB7 (Wires[Wire_VIA1_iB7_SoundDisable])
  Wire_VIA1_InterruptRequest,
#define VIA1_InterruptRequest (Wires[Wire_VIA1_InterruptRequest])
#define VIA1_interruptChngNtfy VIAorSCCinterruptChngNtfy
  Wire_SCCInterruptRequest,
#define SCCInterruptRequest (Wires[Wire_SCCInterruptRequest])
#define SCCinterruptChngNtfy VIAorSCCinterruptChngNtfy
  Wire_ADBMouseDisabled,
#define ADBMouseDisabled (Wires[Wire_ADBMouseDisabled])
  kNumWires
};

/* VIA configuration */
#define VIA1_ORA_FloatVal 0xFF
#define VIA1_ORB_FloatVal 0xFF
#define VIA1_ORA_CanIn 0x80
#define VIA1_ORA_CanOut 0x7F
#define VIA1_ORB_CanIn 0x09
#define VIA1_ORB_CanOut 0xF7
#define VIA1_IER_Never0 0x00
#define VIA1_IER_Never1 0x00
#define VIA1_CB2modesAllowed 0x01
#define VIA1_CA2modesAllowed 0x01

#define Mouse_Enabled() (!ADBMouseDisabled)
#define VIA1_iCA1_PulseNtfy VIA1_iCA1_Sixtieth_PulseNtfy
#define Sixtieth_PulseNtfy VIA1_iCA1_Sixtieth_PulseNtfy
#define VIA1_iCA2_PulseNtfy VIA1_iCA2_RTC_OneSecond_PulseNtfy
#define RTC_OneSecond_PulseNtfy VIA1_iCA2_RTC_OneSecond_PulseNtfy
#define GetSoundInvertTime VIA1_GetT1InvertTime
#define ADB_ShiftInData VIA1_ShiftOutData
#define ADB_ShiftOutData VIA1_ShiftInData
#define kExtn_Block_Base 0x00F40000
#define kExtn_ln2Spc 5
#define kROM_Base 0x00400000
#define kROM_ln2Spc 20
#define WantDisasm 0
#define ExtraAbnormalReports 0
#define RAMSafetyMarginFudge 0

#endif  // CONFIG_H
