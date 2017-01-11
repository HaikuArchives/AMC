#include <USBKit.h>

class UsbRoster : public BUSBRoster {
public:
  status_t DeviceAdded(BUSBDevice *dev);
  void DeviceRemoved(BUSBDevice *dev);
};

void noteOn(int instr,int,int);
void noteOff(int instr,int);
