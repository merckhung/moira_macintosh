#ifndef SRC_CPU_MOIRA_CPU_H_
#define SRC_CPU_MOIRA_CPU_H_

#include <cstdint>

#include "Moira.h"
#include "src/mac_errors.h"

class MacSystem;

class MoiraCpu : public moira::Moira {
 public:
  MoiraCpu(MacSystem* system);
  ~MoiraCpu() = default;

  long long vblTimer = 0;

  moira::u8 read8(moira::u32 addr) const override;
  void write8(moira::u32 addr, moira::u8 val) const override;
  moira::u16 read16(moira::u32 addr) const override;
  void write16(moira::u32 addr, moira::u16 val) const override;

  void didJumpToVector(int nr, moira::u32 vector) override;

  void sync(int cycles) override;
  void willExecute(const char* func, moira::Instr I, moira::Mode M,
                   moira::Size S, moira::u16 opcode) override;
  void didExecute(const char* func, moira::Instr I, moira::Mode M,
                  moira::Size S, moira::u16 opcode) override;
  void willExecute(moira::M68kException exc, moira::u16 vector) override;
  void didExecute(moira::M68kException exc, moira::u16 vector) override;

  unsigned int get_instr_count() const { return instr_count_; }
  void increment_instr_count() { instr_count_++; }

  // Accessors for the most recently executed instruction (useful for tracing).
  moira::u32 get_current_pc() const { return current_pc_; }
  moira::u16 get_current_opcode() const { return current_opcode_; }

  void add_pc_history(moira::u32 pc);
  void dump_pc_history() const;

 private:
  MacSystem* system_;

  // Members ported from MoiraBridge.c statics
  static constexpr int kPcHistSize = 256;
  mutable moira::u32 pc_history_[kPcHistSize] = {0};
  mutable int pc_history_idx_ = 0;

  mutable moira::u16 ext_cmd_ = 0;
  mutable moira::u32 ext_a0_ = 0;
  mutable moira::u32 ext_a1_ = 0;
  mutable moira::u16 ext_result_ = 0;

  mutable moira::u16 current_opcode_ = 0;
  mutable moira::u32 current_pc_ = 0;
  mutable unsigned int instr_count_ = 0;

  mutable moira::u32 last_pcs_[20] = {0};
  mutable int pc_idx_ = 0;
};

extern MoiraCpu* g_cpu_mac;

extern "C" {
void MINEM68K_Init(uint8_t* fIPL);
void m68k_reset(void);
void m68k_go_nCycles(uint32_t n);
void m68k_IPLchangeNtfy(void);
int32_t GetCyclesRemaining(void);
void SetCyclesRemaining(int32_t n);
}

#endif  // SRC_CPU_MOIRA_CPU_H_
