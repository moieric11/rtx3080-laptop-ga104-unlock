#include "ecc_readout.h"

EFI_STATUS EccCollectReadout(EFI_PCI_IO_PROTOCOL *Pci,UINT64 BarSize,ECC_READOUT *Out) {
  if(!Out)return EFI_INVALID_PARAMETER;
  *Out=(ECC_READOUT){.Status=EFI_INVALID_PARAMETER};
  if(!Pci || !Pci->GetLocation || !Pci->Pci.Read || !Pci->Mem.Read)return Out->Status;
  if(BarSize<GA104_ECC_READOUT_OFFSET+4ULL || BarSize!=0x1000000ULL)return Out->Status;
  UINTN Segment,Bus,Device,Function;
  Out->Status=Pci->GetLocation(Pci,&Segment,&Bus,&Device,&Function);
  if(EFI_ERROR(Out->Status))return Out->Status;
  if(Segment || Bus!=1 || Device || Function){Out->Status=EFI_UNSUPPORTED;return Out->Status;}
  UINT32 Config[64];
  Out->Status=Pci->Pci.Read(Pci,EfiPciIoWidthUint32,0,64,Config);
  if(EFI_ERROR(Out->Status))return Out->Status;
  /* This probe is specific to the captured Lenovo board and BAR0 assignment. */
  if(Config[0]!=0x24dc10deU || Config[11]!=0x3a5817aaU || (Config[2]>>24)!=3 || Config[4]!=0xd0000000U) {
    Out->Status=EFI_UNSUPPORTED;return Out->Status;
  }
  if(!(Config[1]&2)){Out->Status=EFI_NOT_READY;return Out->Status;}
  Out->Status=Pci->Mem.Read(Pci,EfiPciIoWidthUint32,0,GA104_ECC_READOUT_OFFSET,1,&Out->Value);
  if(EFI_ERROR(Out->Status))return Out->Status;
  if(Out->Value==0xffffffffU || (Out->Value&0xffff0000U)==0xbadf0000U) {
    Out->Status=EFI_UNSUPPORTED;return Out->Status;
  }
  Out->Valid=TRUE;Out->EnabledReadout=(Out->Value&GA104_ECC_READOUT_MASK)!=0;
  return EFI_SUCCESS;
}
