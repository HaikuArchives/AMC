#include <stdio.h>
#include "midi-keyb.h"

const bool debug=0;
void alert(const char *form,...); // amc.h

status_t UsbRoster::DeviceAdded(BUSBDevice *dev) {
  if (dev->IsHub()) return 1;  // B_OK = 0
  printf("Added USB device at '%s'\n",dev->Location());
  if (dev->SetConfiguration(dev->ConfigurationAt(0))){
    printf("Cannot configure: '%s'\n",dev->Location());
	exit(1);
  }
  const BUSBConfiguration *conf=dev->ActiveConfiguration();
  printf("  conf=%p ",conf);
  if (!conf) exit(1);

  const BUSBInterface *interf=conf->InterfaceAt(1);
  printf("interf=%p ",interf);
  if (!interf) exit(1);

  const BUSBEndpoint *ept=interf->EndpointAt(0);	
  printf("ept=%p\n",ept);
  if (!ept) exit(1);

  const int vend=dev->VendorID(),
            prod=dev->ProductID();
  printf("  vendorID=%d productID=%d\n",vend,prod);
  if (vend!=2637) alert("vendorID=%d - expected: 2637 (M-AUDIO)",vend);
  if (prod!=144) alert("productID=%d - expected: 144 (Keystation 49e)",prod);

  if (!ept->IsInput() || !ept->IsBulk()) { printf("Endpoint 0 not a bulk input\n"); exit(1); }

  int instr=1,
      ampl;
  uchar data[4];
  for(;;) {  // blijft hangen als keyboard te laat aangezet wordt
    if (!ept->BulkTransfer(data,4)) {
      if (debug) printf("0x%02x 0x%02x 0x%02x 0x%02x\n",data[0],data[1],data[2],data[3]);
      switch (data[0]) {
        case 0x09:  // note on/off
          if (debug) printf("  note %d ",data[2]);
          if (data[3]>0) { 
            if (debug) printf("on: %d\n",data[3]);
            noteOn(instr, data[2], data[3]);
          } else {
            if (debug) puts("off");
            noteOff(instr, data[2]);
          }
          break;
        case 0x0c:   // change instrument
          instr=data[2]+1;
          printf("  instr: %d\n",instr);
          break;
        case 0x0b:   // set volume or modulation
          if (data[2]==0x07) {
            ampl=data[3];
            printf("  volume: %d\n",ampl);
            //mk_nbufs.setVolume(def_ampl*ampl/0x7f);
          }
          else printf("  modulation: %d\n",data[3]);
          break;
        default:
          if (!debug) printf("0x%02x 0x%02x 0x%02x 0x%02x\n",data[0],data[1],data[2],data[3]);
      }
  	}
    else {
      printf("idle\n");
      return B_OK; // DeviceRemoved() will be called
    }
  }
  return B_OK; // never reached
}

void UsbRoster::DeviceRemoved(BUSBDevice *dev) {
  printf("Removed %s at '%s'\n", dev->IsHub() ? "hub" : "device", dev->Location());
}	
