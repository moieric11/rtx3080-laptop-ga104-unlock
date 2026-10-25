#include <stdio.h>
#include <string.h>
#undef NULL
#include "ecc_readout.h"
static EFI_PCI_IO_PROTOCOL Pci;
static UINT32 Config[64],Value;
static EFI_STATUS ConfigStatus,MemoryStatus;
static UINTN Bus;
static unsigned Reads,Writes,Failures;
#define CHECK(x) do{if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);Failures++;}}while(0)
static EFI_STATUS EFIAPI Location(EFI_PCI_IO_PROTOCOL *p,UINTN *s,UINTN *b,UINTN *d,UINTN *f){CHECK(p==&Pci);*s=*d=*f=0;*b=Bus;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI ReadConfig(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 o,UINTN n,VOID *out){
  CHECK(p==&Pci && w==EfiPciIoWidthUint32 && !o && n==64);if(!EFI_ERROR(ConfigStatus))memcpy(out,Config,sizeof(Config));return ConfigStatus;
}
static EFI_STATUS EFIAPI ReadMemory(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT8 bar,UINT64 o,UINTN n,VOID *out){
  CHECK(p==&Pci && w==EfiPciIoWidthUint32 && !bar && o==0x823814 && n==1);Reads++;
  if(!EFI_ERROR(MemoryStatus))*(UINT32 *)out=Value;return MemoryStatus;
}
static EFI_STATUS EFIAPI WriteMemory(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT8 b,UINT64 o,UINTN n,VOID *v){
  (void)p;(void)w;(void)b;(void)o;(void)n;(void)v;Writes++;return EFI_ACCESS_DENIED;
}
static void Reset(void){
  memset(&Pci,0,sizeof(Pci));memset(Config,0,sizeof(Config));Reads=Writes=0;ConfigStatus=MemoryStatus=EFI_SUCCESS;Bus=1;Value=0;
  Config[0]=0x24dc10de;Config[11]=0x3a5817aa;Config[1]=3;Config[2]=0x030000a1;Config[4]=0xd0000000;
  Pci.GetLocation=Location;Pci.Pci.Read=ReadConfig;Pci.Mem.Read=ReadMemory;Pci.Mem.Write=WriteMemory;
}
int main(void){
  ECC_READOUT R;
  Reset();CHECK(!EFI_ERROR(EccCollectReadout(&Pci,0x1000000,&R)) && R.Valid && !R.EnabledReadout && Reads==1 && !Writes);
  Reset();Value=0x10000;CHECK(!EFI_ERROR(EccCollectReadout(&Pci,0x1000000,&R)) && R.Valid && R.EnabledReadout && Reads==1 && !Writes);
  Reset();Value=0x20000;CHECK(!EFI_ERROR(EccCollectReadout(&Pci,0x1000000,&R)) && !R.EnabledReadout);
  Reset();Value=0xffffffff;CHECK(EFI_ERROR(EccCollectReadout(&Pci,0x1000000,&R)) && !R.Valid && !R.EnabledReadout);
  Reset();Value=0xbadf0010;CHECK(EFI_ERROR(EccCollectReadout(&Pci,0x1000000,&R)) && !R.Valid);
  Reset();MemoryStatus=EFI_DEVICE_ERROR;CHECK(EccCollectReadout(&Pci,0x1000000,&R)==MemoryStatus && !R.Valid);
  Reset();ConfigStatus=EFI_DEVICE_ERROR;CHECK(EccCollectReadout(&Pci,0x1000000,&R)==ConfigStatus && !Reads);
  Reset();Config[0]++;CHECK(EFI_ERROR(EccCollectReadout(&Pci,0x1000000,&R)) && !Reads);
  Reset();Config[11]++;CHECK(EFI_ERROR(EccCollectReadout(&Pci,0x1000000,&R)) && !Reads);
  Reset();Config[1]=0;CHECK(EFI_ERROR(EccCollectReadout(&Pci,0x1000000,&R)) && !Reads);
  Reset();Config[4]++;CHECK(EFI_ERROR(EccCollectReadout(&Pci,0x1000000,&R)) && !Reads);
  Reset();Bus=2;CHECK(EFI_ERROR(EccCollectReadout(&Pci,0x1000000,&R)) && !Reads);
  Reset();CHECK(EFI_ERROR(EccCollectReadout(&Pci,0x823817,&R)) && !Reads);
  Reset();CHECK(EFI_ERROR(EccCollectReadout(NULL,0x1000000,&R)) && !Reads);
  Reset();Pci.Mem.Read=NULL;CHECK(EFI_ERROR(EccCollectReadout(&Pci,0x1000000,&R)) && !Reads);
  CHECK(EFI_ERROR(EccCollectReadout(&Pci,0x1000000,NULL)) && !Writes);
  printf("ECC readout tests: %s (%u failures; 16 cases)\n",Failures?"FAIL":"PASS",Failures);return Failures?1:0;
}
