/* Experimental Lenovo GA104 path. Real post-FLR GOP recovery is unverified. */
#include "discrete_handoff.h"
#include "unlock/app_io.h"
#include "unlock/checks.h"
#include "unlock/compat/compat.h"
#include "unlock/sha256.h"
#include "unlock/vendor/unlock_610.h"
#include "unlock/vendor/gfw_eval.h"
#include <Guid/GlobalVariable.h>
#include <IndustryStandard/Acpi.h>
#ifdef GU_ECC_READOUT_VARIANT
#include "ecc_readout.h"
#endif

static EFI_SYSTEM_TABLE *Table;
static DH_PLAN Plan;
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID,GlobalGuid=EFI_GLOBAL_VARIABLE;
#ifdef GU_ECC_READOUT_VARIANT
static VOID LogEccReadout(const char *Phase) {
  ECC_READOUT R;
  EccCollectReadout(Gu.pci,Gu.bar_size,&R);
  GuLog("ECC_READOUT phase=%s address=%#x status=%#llx raw=%#x valid=%u enabled_readout=%u\n",
    Phase,GA104_ECC_READOUT_OFFSET,(unsigned long long)R.Status,R.Value,R.Valid,R.EnabledReadout);
  GuLogText("ECC observation only: this readout does not establish ECC capability or data protection; no ECC setting changed.\n");
}
#endif
static BOOLEAN Equal(const VOID *A,const VOID *B,UINTN N){return !memcmp(A,B,N);}
static BOOLEAN LogEvent(const char *Event,EFI_STATUS Status,UINT64 Value) {
  GuLog("handoff %s status=%#llx value=%#llx\n",Event,(unsigned long long)Status,(unsigned long long)Value);
  if(!strcmp(Event,"disconnect-intent") && GuLogReady())Gu.console_suspended=TRUE;
  return GuLogReady();
}
static BOOLEAN DetachedDisplay(VOID) {
  return Gu.owned && DhCheckExclusive(Table,Gu.handle,Gu.image);
}
static EFI_STATUS ReadConfig(UINT32 Config[64]) {
  for(UINTN I=0;I<64;I++)if(EFI_ERROR(GuConfigRead(EfiPciIoWidthUint32,(UINT32)I*4,&Config[I])))return EFI_DEVICE_ERROR;
  return EFI_SUCCESS;
}
static EFI_STATUS CheckBar(VOID) {
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *D=NULL;
  EFI_STATUS S=Gu.pci->GetBarAttributes(Gu.pci,0,NULL,(VOID **)&D);
  if(!EFI_ERROR(S) && (!D || D->Desc!=ACPI_ADDRESS_SPACE_DESCRIPTOR || D->Len!=sizeof(*D)-3 ||
      D->ResType!=ACPI_ADDRESS_SPACE_TYPE_MEM || D->AddrRangeMin!=GU_BOARD_BAR0 ||
      D->AddrLen!=0x1000000 || D->AddrTranslationOffset || D->AddrSpaceGranularity!=32 ||
      (D->AddrRangeMax!=GU_BOARD_BAR0+0xffffff && D->AddrRangeMax!=0xffffff)))S=EFI_UNSUPPORTED;
  if(D)GuFree(D);
  if(!EFI_ERROR(S)){Gu.bar_base=GU_BOARD_BAR0;Gu.bar_size=0x1000000;}
  return S;
}
static EFI_STATUS CheckCold(VOID) {
  UINT32 Chip=GuRead32(0),Plm=GuRead32(0x823804),Hi=GuRead32(0x1fa828);
  UINT32 Ss0=GuRead32(0x82381c),Ss1=GuRead32(0x823820),Fb=GuRead32(0x1183a4);
  struct gpu_unlock_gfw_sample S={.config_ok=true,.plm=GuRead32(GU_GFW_PLM_OFFSET),
    .boot=GuRead32(GU_GFW_BOOT_OFFSET),.cpuctl=GuRead32(GU_GFW_CPUCTL_OFFSET)};
  GuLog("GA104 cold chip=%#x fb_mb=%#x plm=%#x WPR2_HI=%#x SS0=%#x SS1=%#x\n",Chip,Fb,Plm,Hi,Ss0,Ss1);
  GuLog("GA104 cold checks fault=%u chip=%u fb=%u plm=%u wpr=%u ss0=%u ss1=%u gfw=%d gfw_raw=%#x/%#x/%#x\n",
    Gu.fault,Chip==GU_BOARD_CHIP,Fb==GU_BOARD_FB_MB,Plm==0xffffff8f,!(Hi>>4),
    GU_BOARD_COLD_SS0_MATCH(Ss0),GU_BOARD_COLD_SS1_MATCH(Ss1),gpu_unlock_gfw_assess(&S),S.plm,S.boot,S.cpuctl);
  if(Gu.fault || Chip!=GU_BOARD_CHIP || Fb!=GU_BOARD_FB_MB || Plm!=0xffffff8f || (Hi>>4) ||
     !GU_BOARD_COLD_SS0_MATCH(Ss0) || !GU_BOARD_COLD_SS1_MATCH(Ss1) || gpu_unlock_gfw_assess(&S))return EFI_NOT_READY;
  return EFI_SUCCESS;
}
static EFI_STATUS CheckProm(VOID) {
  VOID *Prom=GuAlloc(1048576);if(!Prom)return EFI_OUT_OF_RESOURCES;
  GuCopyFromIo(Prom,(VOID *)(UINTN)(Gu.bar_base+0x300000),1048576);
  BOOLEAN Match=!Gu.fault && GuHashMatches(Prom,1048576,GU_BOARD_PROM_SHA256);
  GuFree(Prom);GuLog("GA104 captured PROM match=%u\n",Match);
  return Match?EFI_SUCCESS:EFI_CRC_ERROR;
}
static EFI_STATUS RestoreDecode(const DH_PLAN *Before) {
  UINT32 Current[64],Normalized[64],After[64];
  EFI_STATUS S=ReadConfig(Current);if(EFI_ERROR(S))return S;
  memcpy(Normalized,Current,sizeof(Current));
  UINT32 Missing=Before->Config[1]&~Current[1]&3U;Normalized[1]|=Missing;
  for(UINTN I=0;I<64;I++)if(Current[I]!=Before->Config[I])
    GuLog("post-stop config[%#x] before=%#x after=%#x\n",(unsigned)(I*4),Before->Config[I],Current[I]);
  if(!Equal(Normalized,Before->Config,sizeof(Current)))return EFI_UNSUPPORTED;
  if(!Missing)return EFI_SUCCESS;
  if(!Gu.pci->Attributes)return EFI_UNSUPPORTED;
  UINT64 Need=(Missing&1?EFI_PCI_IO_ATTRIBUTE_IO:0)|(Missing&2?EFI_PCI_IO_ATTRIBUTE_MEMORY:0),Supported=0;
  S=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationSupported,0,&Supported);
  if(EFI_ERROR(S) || (Supported&Need)!=Need)return EFI_UNSUPPORTED;
  GuLog("restore original IO/MEM decode only: missing=%#x\n",Missing);
  if(!GuLogReady())return EFI_DEVICE_ERROR;
  S=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationEnable,Need,NULL);
  if(EFI_ERROR(S))return S;
  S=ReadConfig(After);if(EFI_ERROR(S))return S;
  return Equal(After,Before->Config,sizeof(After))?EFI_SUCCESS:EFI_COMPROMISED_DATA;
}
static EFI_STATUS Unlock(EFI_PCI_IO_PROTOCOL *Pci,const DH_PLAN *Before,BOOLEAN *Safe) {
  *Safe=FALSE;Gu.pci=Pci;Gu.owned=TRUE;Gu.armed=TRUE;Gu.display_ok=DetachedDisplay();
  EFI_STATUS S=Gu.display_ok?RestoreDecode(Before):EFI_ACCESS_DENIED;
  if(!EFI_ERROR(S))S=CheckBar();
  if(!EFI_ERROR(S))S=CheckCold();
  if(!EFI_ERROR(S))S=CheckProm();
  int R=0;struct gpu_unlock_610_shared Shared={0};
  if(!EFI_ERROR(S) && GuLogReady()) {
    static UINT64 Mask;
    static struct pci_dev Device;
    Mask=0xffffffffULL;
    Device=(struct pci_dev){.dev={.dma_mask=&Mask,.coherent_dma_mask=0xffffffffULL},.pm_cap=Gu.pm_cap,.refs=1};
    GuLogText("START: one GA104 unlock transaction; display driver is detached.\n");
    if(!GuLogReady())S=EFI_DEVICE_ERROR;
    else {
      R=gpu_unlock_610_load_shared(&Device,&Shared);
      if(!R)R=gpu_unlock_610_unlock(&Device,&Shared);
      S=R?EFI_DEVICE_ERROR:EFI_SUCCESS;
    }
  } else if(!EFI_ERROR(S))S=EFI_DEVICE_ERROR;
  EFI_STATUS Contained=GuClearMaster();
  *Safe=!EFI_ERROR(Contained) && GuSafeToReturn() && !Gu.allocations && !Gu.dma_count && !(Gu.started && EFI_ERROR(S));
  GuLog("GA104 transaction result=%d status=%#llx containment=%#llx started=%u quiesced=%u fault=%u retained=%u FLR=%u safe=%u\n",
    R,(unsigned long long)S,(unsigned long long)Contained,Gu.started,Gu.quiesced,Gu.fault,Gu.retained,Gu.flrs,*Safe);
  *Safe=*Safe && GuSafeToReturn();
  if(*Safe){gpu_unlock_610_release_shared(&Shared);Gu.armed=FALSE;}
  return !EFI_ERROR(Contained)?S:Contained;
}
static EFI_STATUS FindTarget(VOID) {
  EFI_HANDLE *Handles=NULL;UINTN N=0,Matches=0;
  EFI_STATUS S=Gu.bs->LocateHandleBuffer(ByProtocol,&PciGuid,NULL,&N,&Handles);
  if(!EFI_ERROR(S) && (!Handles || !N || N>256))S=EFI_COMPROMISED_DATA;
  for(UINTN I=0;!EFI_ERROR(S) && I<N;I++) {
    EFI_PCI_IO_PROTOCOL *Pci=NULL;UINT32 Id=0;
    S=Gu.bs->HandleProtocol(Handles[I],&PciGuid,(VOID **)&Pci);
    if(!EFI_ERROR(S) && Pci)S=Pci->Pci.Read(Pci,EfiPciIoWidthUint32,0,1,&Id);
    if(!EFI_ERROR(S) && Pci && Id==GU_BOARD_ID){Gu.handle=Handles[I];Gu.pci=Pci;Matches++;}
  }
  if(Handles)GuFree(Handles);
  return !EFI_ERROR(S) && Matches==1?EFI_SUCCESS:EFI_UNSUPPORTED;
}
static EFI_STATUS Arm(VOID) {
  GuConsole("\nEXPERIMENTAL GA104 UNLOCK - one attempt, hardware result unverified.\n");
  GuConsole("The screen may go black. If it stays black over 3 minutes, power off.\n");
  GuConsole("Press U to attempt; Esc or 30 seconds cancels. Do not remove USB.\n");
  if(!GuLogReady() || !Table->ConIn || !Table->ConIn->Reset || !Table->ConIn->ReadKeyStroke)return EFI_UNSUPPORTED;
  EFI_STATUS S=Table->ConIn->Reset(Table->ConIn,FALSE);if(EFI_ERROR(S))return S;
  for(UINTN I=0;I<300;I++) {
    EFI_INPUT_KEY K;S=Table->ConIn->ReadKeyStroke(Table->ConIn,&K);
    if(!EFI_ERROR(S)) {
      if(K.ScanCode==0x17 || K.UnicodeChar==27)return EFI_ABORTED;
      if(K.UnicodeChar=='u' || K.UnicodeChar=='U')return GuLogReady()?EFI_SUCCESS:EFI_DEVICE_ERROR;
    } else if(S!=EFI_NOT_READY)return S;
    GuStall(100000);
  }
  return EFI_TIMEOUT;
}
static VOID Halt(VOID) {
  Gu.retained=TRUE;
  GuLogText("STOP: cleanup or display recovery unproven. POWER OFF; do not continue to Windows.\n");
  for(;;)Gu.bs->Stall(1000000);
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE Image,EFI_SYSTEM_TABLE *St) {
  if(!St || !St->BootServices || !St->RuntimeServices)return EFI_INVALID_PARAMETER;
  Table=St;GuInit(St->BootServices,NULL);Gu.image=Image;Gu.display_check=DetachedDisplay;
  EFI_STATUS S=GuIoOpen(Image,St);DH_RESULT Result={0};const char *Stage="usb-log";
  if(EFI_ERROR(S))goto Finish;
  GuLogText("LENOVO GA104 DISCRETE experiment v0.4. Driver handoff tested; post-FLR recovery unverified.\n");
#ifdef GU_COLD_SELECTOR_MASK
  GuLogText("COLD SELECTOR mask v2: SS0 nonzero, all override enables clear; SS1=6 and other cold gates unchanged. No ECC readout.\n");
#elif defined(GU_COLD_SELECTOR_VARIANTS)
  GuLogText("COLD SELECTOR variant v1: three observed SS0 states accepted; other cold gates unchanged. No ECC readout.\n");
#endif
#ifdef GU_ECC_READOUT_VARIANT
  GuLogText("ECC READOUT variant v1: ECC is observed only. U runs the existing compute unlock, not ECC activation.\n");
#endif
  UINT8 Secure=0xff;UINTN Bytes=1;Stage="secure-boot";
  S=St->RuntimeServices->GetVariable(L"SecureBoot",&GlobalGuid,NULL,&Bytes,&Secure);
  if(EFI_ERROR(S) || Bytes!=1 || Secure){S=EFI_SECURITY_VIOLATION;goto Finish;}
  Stage="target";S=FindTarget();if(EFI_ERROR(S))goto Finish;
  Stage="handoff-plan";S=DhPlan(St,Gu.handle,Image,&Plan,LogEvent);if(EFI_ERROR(S))goto Finish;
  GU_CAPS Caps;Stage="pci-config";
  if(!GuCheckConfig(Plan.Config,0,1,0,0,&Caps)){S=EFI_UNSUPPORTED;goto Finish;}
  Gu.pm_cap=Caps.pm;Gu.pcie_cap=Caps.pcie;Gu.pcie_version=Caps.pcie_version;Gu.msi_cap=Caps.msi;Gu.msix_cap=Caps.msix;
  Stage="bar0";S=CheckBar();if(EFI_ERROR(S))goto Finish;
  Stage="cold-state";S=CheckCold();if(EFI_ERROR(S))goto Finish;
#ifdef GU_ECC_READOUT_VARIANT
  LogEccReadout("cold-before-compute-unlock");
#endif
  Stage="prom";S=CheckProm();if(EFI_ERROR(S))goto Finish;
  Stage="assets";S=GuLoadAssets();if(EFI_ERROR(S))goto Finish;
  Stage="manual-U";S=Arm();if(EFI_ERROR(S))goto Finish;
  GuLogText("Manual U confirmed. One transaction; no automatic retry.\n");
  if(!GuLogReady()){S=EFI_DEVICE_ERROR;goto Finish;}
  Stage="watchdog";S=Gu.bs->SetWatchdogTimer(0,0,0,NULL);if(EFI_ERROR(S))goto Finish;
  Stage="discrete-unlock";S=DhRunOperation(St,&Plan,&Result,LogEvent,Unlock);
  if(Result.Retained || (Result.Attempted && !Result.DisplayRestored))Halt();
  Gu.owned=FALSE;
  if(DhConsoleReady(St))Gu.console_suspended=FALSE;
  if(!EFI_ERROR(S) && Result.OperationAttempted) {
    EFI_PCI_IO_PROTOCOL *Fresh=NULL;
    S=Gu.bs->HandleProtocol(Gu.handle,&PciGuid,(VOID **)&Fresh);
    if(!EFI_ERROR(S) && Fresh){Gu.pci=Fresh;UINT32 Ss0=GuRead32(0x82381c),Ss1=GuRead32(0x823820);
      GuLog("after GOP reconnect SS0=%#x SS1=%#x\n",Ss0,Ss1);
      if(Gu.fault || Ss0!=0x88888888 || Ss1!=8)S=EFI_NOT_READY;
#ifdef GU_ECC_READOUT_VARIANT
      if(!Gu.fault)LogEccReadout("after-compute-unlock-and-gop");
#endif
    } else S=EFI_DEVICE_ERROR;
  }
Finish:
  GuLog("FINAL GA104 status=%#llx stage=%s attempted=%u operation=%u display_restored=%u retained=%u\n",
    (unsigned long long)S,Stage,Result.Attempted,Result.OperationAttempted,Result.DisplayRestored,Result.Retained);
  if(!GuSafeToReturn())Halt();
  GuReleaseAssets();
  EFI_STATUS Closed=GuIoClose();if(EFI_ERROR(Closed))S=Closed;
  if(!EFI_ERROR(S))GuConsole("Unlock selectors survived GOP reconnect. Choose Windows Boot Manager WITHOUT REBOOTING. Windows performance must be measured.\n");
  else GuConsole("Unlock cancelled, rejected or not preserved. Check RTX3080-UNLOCK/RUNxx/REPORT.TXT.\n");
  Gu.bs->Stall(15000000);
  return S;
}
