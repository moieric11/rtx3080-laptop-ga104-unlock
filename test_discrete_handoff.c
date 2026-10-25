#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef NULL
#include "discrete_handoff.h"

#define H(n) ((EFI_HANDLE)(UINTN)(n))
static EFI_BOOT_SERVICES Bs;
static EFI_SYSTEM_TABLE St;
static EFI_PCI_IO_PROTOCOL Pci;
static EFI_DRIVER_BINDING_PROTOCOL Binding;
static EFI_LOADED_IMAGE_PROTOCOL Loaded;
static EFI_GRAPHICS_OUTPUT_PROTOCOL Gop;
static EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE Mode;
static EFI_GRAPHICS_OUTPUT_MODE_INFORMATION Info;
static EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL Console;
static EFI_SIMPLE_TEXT_INPUT_PROTOCOL Input;
static CHAR16 FreshKey,OldKey;
static unsigned KeyResets,KeyReads,Stalls;
static BOOLEAN ResetError,ReadError;
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID,PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID,
  BindingGuid=EFI_DRIVER_BINDING_PROTOCOL_GUID,LoadedGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID,
  GopGuid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,TextGuid=EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;
static UINT8 RootPath[]={2,1,12,0,0xd0,0x41,3,10,0,0,0,0,1,1,6,0,1,1,1,1,6,0,0,0,127,255,4,0};
static UINT8 ChildPath[]={2,1,12,0,0xd0,0x41,3,10,0,0,0,0,1,1,6,0,1,1,1,1,6,0,0,0,2,3,8,0,0,4,1,128,127,255,4,0};
static BOOLEAN Connected,Exclusive;
static unsigned Disconnects,Connects,Claims,Closes,Queries,Writes,Failures,EventCount,Operations;
static EFI_TPL Tpl;
static UINT32 Config[64];
static const char *Case;
#define CHECK(c) do {if(!(c)){fprintf(stderr,"FAIL [%s] line %d: %s\n",Case,__LINE__,#c);Failures++;}}while(0)
static BOOLEAN Is(const char *s){return strcmp(Case,s)==0;}
static EFI_HANDLE Driver(void){return Is("owner-race") && Queries>=3?H(4):H(3);}
static BOOLEAN Guid(const EFI_GUID *a,const EFI_GUID *b){return !memcmp(a,b,sizeof(*a));}
static EFI_STATUS EFIAPI Stop(EFI_DRIVER_BINDING_PROTOCOL *b,EFI_HANDLE h,UINTN n,EFI_HANDLE *c){(void)b;(void)h;(void)n;(void)c;CHECK(0);return EFI_UNSUPPORTED;}
static EFI_STATUS EFIAPI Read(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 o,UINTN n,VOID *out){
  CHECK(p==&Pci && w==EfiPciIoWidthUint32 && !o && n==64);
  memcpy(out,Config,sizeof(Config));return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Write(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 o,UINTN n,VOID *in){
  (void)p;(void)w;(void)o;(void)n;(void)in;Writes++;return EFI_ACCESS_DENIED;
}
static EFI_STATUS EFIAPI Location(EFI_PCI_IO_PROTOCOL *p,UINTN *s,UINTN *b,UINTN *d,UINTN *f){
  CHECK(p==&Pci);*s=0;*b=1;*d=*f=0;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Output(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *p,CHAR16 *s){(void)p;(void)s;CHECK(0);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI InputReset(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *p,BOOLEAN extended){
  CHECK(p==&Input && !extended);KeyResets++;OldKey=0;return ResetError?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI InputRead(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *p,EFI_INPUT_KEY *key){
  CHECK(p==&Input);KeyReads++;
  if(ReadError)return EFI_DEVICE_ERROR;
  CHAR16 C=OldKey?OldKey:FreshKey;OldKey=FreshKey=0;
  if(!C)return EFI_NOT_READY;
  key->UnicodeChar=C;key->ScanCode=0;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Stall(UINTN us){CHECK(us==100000);Stalls++;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Handle(EFI_HANDLE h,EFI_GUID *g,VOID **out){
  *out=NULL;
  if(h==H(1) && Guid(g,&PciGuid)){*out=&Pci;return EFI_SUCCESS;}
  if(h==H(1) && Guid(g,&PathGuid)){*out=RootPath;return EFI_SUCCESS;}
  if((h==H(10)||h==H(11)) && Guid(g,&PathGuid)){
    *out=ChildPath;return EFI_SUCCESS;
  }
  if(h==Driver() && Guid(g,&BindingGuid)){Binding.DriverBindingHandle=Driver();*out=&Binding;return EFI_SUCCESS;}
  if(h==H(5) && Guid(g,&LoadedGuid)){*out=&Loaded;return EFI_SUCCESS;}
  if(h==H(10) && Guid(g,&GopGuid) && Connected){*out=&Gop;return EFI_SUCCESS;}
  if(h==H(20) && Guid(g,&TextGuid) && Connected && !Is("lost-console")){*out=&Console;return EFI_SUCCESS;}
  return EFI_NOT_FOUND;
}
static EFI_STATUS EFIAPI Protocols(EFI_HANDLE h,EFI_GUID ***out,UINTN *n){
  CHECK(h==H(1));Queries++;*n=2;*out=malloc(2*sizeof(**out));
  (*out)[0]=&PciGuid;(*out)[1]=&PathGuid;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Opens(EFI_HANDLE h,EFI_GUID *g,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **out,UINTN *n){
  CHECK(h==H(1));*out=NULL;*n=0;
  if(!Connected) {
    if(Exclusive && Guid(g,&PciGuid)) {
      *n=1;*out=calloc(1,sizeof(**out));(*out)->AgentHandle=H(2);
      (*out)->Attributes=EFI_OPEN_PROTOCOL_EXCLUSIVE;(*out)->OpenCount=1;
    }
    return EFI_SUCCESS;
  }
  if(Guid(g,&PciGuid)){
    *n=1;*out=calloc(1,sizeof(**out));(*out)[0].AgentHandle=Driver();
    (*out)[0].ControllerHandle=H(1);(*out)[0].Attributes=EFI_OPEN_PROTOCOL_BY_DRIVER;(*out)[0].OpenCount=1;
    if(Is("bad-attributes"))(*out)[0].Attributes=EFI_OPEN_PROTOCOL_EXCLUSIVE;
  } else if(Guid(g,&PathGuid)){
    *n=2;*out=calloc(2,sizeof(**out));
    for(unsigned i=0;i<2;i++){
      (*out)[i].AgentHandle=Is("alien-child-owner")?H(9):Driver();
      (*out)[i].ControllerHandle=H(10+i);(*out)[i].Attributes=EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER;(*out)[i].OpenCount=1;
    }
    if(Is("console-child"))(*out)[1].ControllerHandle=St.ConsoleOutHandle;
    if(Is("duplicate-child"))(*out)[1].ControllerHandle=(*out)[0].ControllerHandle;
  }
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Free(VOID *p){free(p);return EFI_SUCCESS;}
static EFI_STATUS EFIAPI Locate(EFI_LOCATE_SEARCH_TYPE type,EFI_GUID *g,VOID *key,UINTN *n,EFI_HANDLE **out){
  (void)key;CHECK(type==ByProtocol && Guid(g,&GopGuid));*n=1;*out=malloc(sizeof(**out));(*out)[0]=H(10);return EFI_SUCCESS;
}
static EFI_TPL EFIAPI Raise(EFI_TPL t){EFI_TPL old=Tpl;Tpl=t;return old;}
static VOID EFIAPI Restore(EFI_TPL t){Tpl=t;}
static EFI_STATUS EFIAPI Disconnect(EFI_HANDLE h,EFI_HANDLE driver,EFI_HANDLE child){
  CHECK(Tpl==TPL_APPLICATION && h==H(1) && driver==H(3) && !child);Disconnects++;
  if(!Is("stop-noop"))Connected=FALSE;
  return Is("partial-stop")?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Connect(EFI_HANDLE h,EFI_HANDLE *drivers,EFI_DEVICE_PATH_PROTOCOL *p,BOOLEAN recursive){
  CHECK(Tpl==TPL_APPLICATION && h==H(1) && drivers[0]==H(3) && !drivers[1] && !p && recursive);Connects++;
  if(Is("reconnect-error"))return EFI_DEVICE_ERROR;
  Connected=TRUE;
  if(Is("framebuffer-changed"))Mode.FrameBufferBase++;
  if(Is("bar-changed"))Config[4]++;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Open(EFI_HANDLE h,EFI_GUID *g,VOID **out,EFI_HANDLE agent,EFI_HANDLE c,UINT32 attributes){
  CHECK(Tpl==TPL_NOTIFY && h==H(1) && Guid(g,&PciGuid) && agent==H(2) && !c && attributes==EFI_OPEN_PROTOCOL_EXCLUSIVE);
  CHECK(!Connected && !Exclusive);Claims++;
  if(Is("claim-error"))return EFI_ACCESS_DENIED;
  *out=Is("wrong-interface")?(VOID *)&Loaded:(VOID *)&Pci;Exclusive=TRUE;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI Close(EFI_HANDLE h,EFI_GUID *g,EFI_HANDLE agent,EFI_HANDLE c){
  CHECK(Tpl==TPL_NOTIFY && h==H(1) && Guid(g,&PciGuid) && agent==H(2) && !c && Exclusive);Closes++;Exclusive=FALSE;
  return Is("close-error")?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static BOOLEAN Log(const char *event,EFI_STATUS status,UINT64 value){
  (void)status;(void)value;EventCount++;
  CHECK(Tpl==TPL_APPLICATION);
  return !((Is("log-intent") && !strcmp(event,"disconnect-intent")) ||
           (Is("log-after-stop") && !strcmp(event,"disconnect-result")) ||
           (Is("operation-log-intent") && !strcmp(event,"operation-intent")) ||
           (Is("operation-log-result") && !strcmp(event,"operation-result")) ||
           (Is("log-final") && !strcmp(event,"display-restored")));
}
static void Reset(const char *name){
  Case=name;Connected=TRUE;Exclusive=FALSE;Disconnects=Connects=Claims=Closes=Queries=Writes=EventCount=0;Tpl=TPL_APPLICATION;
  Operations=0;
  memset(&Bs,0,sizeof(Bs));memset(&St,0,sizeof(St));memset(&Pci,0,sizeof(Pci));memset(Config,0,sizeof(Config));
  Config[0]=PROBE_PCI_ID;Config[11]=PROBE_SUBSYSTEM_ID;Config[2]=0x030000a1;Config[1]=3;Config[4]=0xd0000000;Config[6]=0xf8;
  if(Is("bus-master"))Config[1]|=4;
  Bs.HandleProtocol=Handle;Bs.ProtocolsPerHandle=Protocols;Bs.OpenProtocolInformation=Opens;Bs.FreePool=Free;
  Bs.LocateHandleBuffer=Locate;Bs.RaiseTPL=Raise;Bs.RestoreTPL=Restore;Bs.DisconnectController=Disconnect;Bs.ConnectController=Connect;
  Bs.OpenProtocol=Open;Bs.CloseProtocol=Close;
  St.BootServices=&Bs;St.ConOut=&Console;St.ConsoleOutHandle=H(20);St.ConsoleInHandle=H(21);St.StandardErrorHandle=H(22);
  Console.OutputString=Output;Pci.Pci.Read=Read;Pci.Pci.Write=Write;Pci.GetLocation=Location;
  Binding.DriverBindingHandle=H(3);Binding.ImageHandle=H(5);Binding.Stop=Stop;Binding.Version=0x6000a;
  Loaded.ImageBase=(VOID *)(UINTN)0x100000;Loaded.ImageSize=0x2be40;Loaded.ImageCodeType=EfiBootServicesCode;
  Mode.Info=&Info;Mode.SizeOfInfo=sizeof(Info);Mode.FrameBufferBase=0xf800000000ULL;Mode.FrameBufferSize=0x1900000;
  Info.HorizontalResolution=2560;Info.VerticalResolution=1600;Info.PixelsPerScanLine=4096;Info.PixelFormat=PixelBlueGreenRedReserved8BitPerColor;Gop.Mode=&Mode;
  ChildPath[22]=0;
  if(Is("foreign-child-path"))ChildPath[22]=1;
  if(Is("wrong-driver-version"))Binding.Version++;
}
static EFI_STATUS Operation(EFI_PCI_IO_PROTOCOL *pci,const DH_PLAN *plan,BOOLEAN *safe) {
  Operations++;CHECK(pci==&Pci && plan->Target==H(1) && Tpl==TPL_APPLICATION && Exclusive && !Connected);
  CHECK(DhCheckExclusive(&St,H(1),H(2)));
  *safe=!Is("operation-unsafe");
  return Is("operation-error-safe")?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
int main(void){
  const char *cases[]={"success","partial-stop","stop-noop","claim-error","wrong-interface","close-error",
    "reconnect-error","framebuffer-changed","bar-changed","log-intent","log-after-stop","log-final","owner-race",
    "bad-attributes","alien-child-owner","console-child","duplicate-child","bus-master","foreign-child-path","wrong-driver-version","lost-console"};
  for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++){
    Reset(cases[i]);DH_PLAN Plan;DH_RESULT Result={0};
    EFI_STATUS S=DhPlan(&St,H(1),H(2),&Plan,Log);
    BOOLEAN PreflightBad=i>=13;
    if(PreflightBad){CHECK(EFI_ERROR(S));CHECK(!Disconnects && !Connects && !Claims);}
    else {
      CHECK(!EFI_ERROR(S));S=DhRun(&St,&Plan,&Result,Log);
      if(Is("success")){CHECK(!EFI_ERROR(S) && Result.DisplayRestored && Result.Claimed && Closes==1);}
      else CHECK(EFI_ERROR(S));
      if(Is("log-intent") || Is("owner-race")){CHECK(!Result.Attempted && !Disconnects && !Connects);}
      else {
        CHECK(Result.Attempted && Disconnects==1 && Connects==1);
        CHECK(Result.DisplayRestored!= (Is("reconnect-error") || Is("framebuffer-changed") || Is("bar-changed")));
      }
    }
    CHECK(Tpl==TPL_APPLICATION && !Writes && !Exclusive);
    printf("Discrete handoff %s: checked\n",Case);
  }
  const char *operations[]={"operation-success","operation-unsafe","operation-error-safe","operation-log-intent","operation-log-result"};
  for(unsigned i=0;i<sizeof(operations)/sizeof(operations[0]);i++) {
    Reset(operations[i]);DH_PLAN Plan;DH_RESULT Result={0};
    CHECK(!EFI_ERROR(DhPlan(&St,H(1),H(2),&Plan,Log)));
    EFI_STATUS S=DhRunOperation(&St,&Plan,&Result,Log,Operation);
    CHECK(Disconnects==1 && Claims==1 && !Writes && Tpl==TPL_APPLICATION);
    if(Is("operation-unsafe")) {
      CHECK(EFI_ERROR(S) && Result.Retained && Exclusive && !Closes && !Connects && !Result.DisplayRestored && Operations==1);
    } else {
      CHECK(!Result.Retained && !Exclusive && Closes==1 && Connects==1 && Result.DisplayRestored);
      CHECK(Operations==(Is("operation-log-intent")?0U:1U));
      CHECK(EFI_ERROR(S)!=Is("operation-success"));
    }
    printf("Discrete callback %s: checked\n",Case);
  }
  Case="manual-T";St.ConIn=&Input;Bs.Stall=Stall;Input.Reset=InputReset;Input.ReadKeyStroke=InputRead;
  KeyResets=KeyReads=Stalls=0;FreshKey='T';CHECK(DhConfirmTest(&St));CHECK(KeyResets==1 && KeyReads==1 && !Stalls);
  FreshKey='t';CHECK(DhConfirmTest(&St));
  FreshKey=27;CHECK(!DhConfirmTest(&St));
  Stalls=0;OldKey='T';CHECK(!DhConfirmTest(&St));CHECK(Stalls==300);
  ResetError=TRUE;FreshKey='T';CHECK(!DhConfirmTest(&St));ResetError=FALSE;
  ReadError=TRUE;CHECK(!DhConfirmTest(&St));ReadError=FALSE;FreshKey=0;
  St.ConIn=NULL;CHECK(!DhConfirmTest(&St));
  puts("Manual T / Esc / timeout / stale key / input failures: checked");
  printf("Discrete handoff tests: %s (%u failures)\n",Failures?"FAIL":"PASS",Failures);
  return Failures?1:0;
}
