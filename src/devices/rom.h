#ifndef SRC_DEVICES_ROM_H_
#define SRC_DEVICES_ROM_H_

class MacSystem;

class RomDevice {
 public:
  RomDevice(MacSystem* system);
  ~RomDevice();

  bool Init();

 private:
  MacSystem* system_;
  void InstallSonyPatch();
};

#endif  // SRC_DEVICES_ROM_H_
