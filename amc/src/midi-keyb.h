#include "usb/USBKit.h"

class UsbRoster : public USBRoster {
public:
  status_t DeviceAdded(USBDevice *dev);
  void DeviceRemoved(USBDevice *dev);
};

void noteOn(int instr,int,int);
void noteOff(int instr,int);
