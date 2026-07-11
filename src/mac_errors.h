#ifndef SRC_MAC_ERRORS_H_
#define SRC_MAC_ERRORS_H_

#include <cstdint>

typedef uint16_t MacErr;  // Replaces MacErr

#define mnvm_noErr ((MacErr)0x0000)
#define mnvm_miscErr ((MacErr)0xFFFF)
#define mnvm_controlErr ((MacErr)0xFFEF)
#define mnvm_statusErr ((MacErr)0xFFEE)
#define mnvm_closErr ((MacErr)0xFFE8)
#define mnvm_eofErr ((MacErr)0xFFD9)
#define mnvm_tmfoErr ((MacErr)0xFFD6)
#define mnvm_fnfErr ((MacErr)0xFFD5)
#define mnvm_wPrErr ((MacErr)0xFFD4)
#define mnvm_vLckdErr ((MacErr)0xFFD2)
#define mnvm_dupFNErr ((MacErr)0xFFD0)
#define mnvm_opWrErr ((MacErr)0xFFCF)
#define mnvm_paramErr ((MacErr)0xFFCE)
#define mnvm_permErr ((MacErr)0xFFCA)
#define mnvm_nsDrvErr ((MacErr)0xFFC8)
#define mnvm_wrPermErr ((MacErr)0xFFC3)
#define mnvm_offLinErr ((MacErr)0xFFBF)
#define mnvm_dirNFErr ((MacErr)0xFF88)

#endif  // SRC_MAC_ERRORS_H_
