#include "discrete_handoff.h"

static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID, PathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID,
  BindingGuid=EFI_DRIVER_BINDING_PROTOCOL_GUID, LoadedGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID,
  GopGuid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID, TextGuid=EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID;

static BOOLEAN Equal(const VOID *A,const VOID *B,UINTN Size) {
  const UINT8 *P=A,*Q=B;for(UINTN I=0;I<Size;I++)if(P[I]!=Q[I])return FALSE;return TRUE;
}
static BOOLEAN Passive(UINT32 Attributes) {
  return Attributes==EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL ||
    Attributes==EFI_OPEN_PROTOCOL_GET_PROTOCOL || Attributes==EFI_OPEN_PROTOCOL_TEST_PROTOCOL;
}
static EFI_STATUS PciTarget(EFI_BOOT_SERVICES *Bs,EFI_HANDLE Target,EFI_PCI_IO_PROTOCOL **Out,UINT32 Config[64]) {
  UINTN S,B,D,F;EFI_STATUS Status=Bs->HandleProtocol(Target,&PciGuid,(VOID **)Out);
  if(EFI_ERROR(Status) || !*Out || !(*Out)->GetLocation || !(*Out)->Pci.Read)return EFI_NOT_FOUND;
  Status=(*Out)->GetLocation(*Out,&S,&B,&D,&F);
  if(EFI_ERROR(Status) || S || B!=1 || D || F)return EFI_UNSUPPORTED;
  Status=(*Out)->Pci.Read(*Out,EfiPciIoWidthUint32,0,64,Config);
  if(EFI_ERROR(Status))return Status;
  return ProbeMatches(Config)?EFI_SUCCESS:EFI_UNSUPPORTED;
}
/* The GPU root path and display-child suffix are firmware-owned, bounded walks. */
static UINTN PathLength(const EFI_DEVICE_PATH_PROTOCOL *Path) {
  const UINT8 *P=(const UINT8 *)Path;UINTN Offset=0;
  if(!P)return 0;
  for(UINTN I=0;I<32 && Offset+4<=256;I++) {
    UINTN Size=P[Offset+2]+((UINTN)P[Offset+3]<<8);
    if(Size<4 || Size>256-Offset)return 0;
    if(P[Offset]==END_DEVICE_PATH_TYPE)
      return P[Offset+1]==END_ENTIRE_DEVICE_PATH_SUBTYPE && Size==4?Offset+4:0;
    Offset+=Size;
  }
  return 0;
}
static BOOLEAN ChildPath(EFI_BOOT_SERVICES *Bs,EFI_HANDLE Target,EFI_HANDLE Child) {
  EFI_DEVICE_PATH_PROTOCOL *Root=NULL,*Path=NULL;
  if(EFI_ERROR(Bs->HandleProtocol(Target,&PathGuid,(VOID **)&Root)) ||
     EFI_ERROR(Bs->HandleProtocol(Child,&PathGuid,(VOID **)&Path)))return FALSE;
  UINTN R=PathLength(Root),N=PathLength(Path);
  if(R<4 || N<=R || !Equal(Root,Path,R-4))return FALSE;
  const UINT8 *P=(const UINT8 *)Path;
  for(UINTN O=R-4;O<N-4;) {
    UINTN Size=P[O+2]+((UINTN)P[O+3]<<8);
    if(P[O]!=ACPI_DEVICE_PATH || P[O+1]!=ACPI_ADR_DP || Size<8 || (Size&3) || Size>N-4-O)return FALSE;
    O+=Size;
  }
  return TRUE;
}
/* Mode 0: bound driver; 1: detached; 2: our exclusive claim only. */
static EFI_STATUS Snapshot(EFI_SYSTEM_TABLE *St,DH_PLAN *Plan,UINTN Mode) {
  EFI_BOOT_SERVICES *Bs=St->BootServices;EFI_GUID **Protocols=NULL;UINTN Count=0,Drivers=0,Total=0,Exclusive=0;
  Plan->Driver=NULL;Plan->ChildCount=0;
  EFI_STATUS Status=Bs->ProtocolsPerHandle(Plan->Target,&Protocols,&Count);
  if(!EFI_ERROR(Status) && (!Protocols || !Count || Count>32))Status=EFI_COMPROMISED_DATA;
  for(UINTN I=0;!EFI_ERROR(Status) && I<Count;I++) {
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *Entries=NULL;UINTN N=0;
    if(!Protocols[I]){Status=EFI_COMPROMISED_DATA;break;}
    Status=Bs->OpenProtocolInformation(Plan->Target,Protocols[I],&Entries,&N);
    if(!EFI_ERROR(Status) && ((N && !Entries) || N>128-Total))Status=EFI_COMPROMISED_DATA;
    if(!EFI_ERROR(Status))for(UINTN J=0;J<N;J++) {
      EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *E=&Entries[J];Total++;
      if(Passive(E->Attributes))continue;
      if(Mode==2 && E->Attributes==EFI_OPEN_PROTOCOL_EXCLUSIVE && E->AgentHandle==Plan->Image &&
         !E->ControllerHandle && E->OpenCount==1 && Equal(Protocols[I],&PciGuid,sizeof(PciGuid))) {Exclusive++;continue;}
      if(Mode || !E->AgentHandle || E->OpenCount!=1){Status=EFI_ACCESS_DENIED;break;}
      if(E->Attributes==EFI_OPEN_PROTOCOL_BY_DRIVER && Equal(Protocols[I],&PciGuid,sizeof(PciGuid)) &&
         E->ControllerHandle==Plan->Target) {
        Plan->Driver=E->AgentHandle;Drivers++;
      } else if(E->Attributes==EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER &&
                Equal(Protocols[I],&PathGuid,sizeof(PathGuid))) {
        if(!E->ControllerHandle || E->ControllerHandle==Plan->Target ||
           E->ControllerHandle==St->ConsoleOutHandle || E->ControllerHandle==St->ConsoleInHandle ||
           E->ControllerHandle==St->StandardErrorHandle || Plan->ChildCount>=DH_MAX_CHILDREN) {Status=EFI_ACCESS_DENIED;break;}
        for(UINTN K=0;K<Plan->ChildCount;K++)if(Plan->Children[K]==E->ControllerHandle)Status=EFI_COMPROMISED_DATA;
        if(EFI_ERROR(Status))break;
        /* Validate owner after collecting the PCI owner, regardless of GUID order. */
        Plan->Children[Plan->ChildCount++]=E->ControllerHandle;
      } else {Status=EFI_ACCESS_DENIED;break;}
    }
    if(Entries && EFI_ERROR(Bs->FreePool(Entries)))Status=EFI_DEVICE_ERROR;
  }
  if(Protocols && EFI_ERROR(Bs->FreePool(Protocols)))Status=EFI_DEVICE_ERROR;
  if(EFI_ERROR(Status))return Status;
  if(Mode)return Drivers==0 && Plan->ChildCount==0 && Exclusive==(Mode==2?1U:0U)?EFI_SUCCESS:EFI_ACCESS_DENIED;
  if(Drivers!=1 || Plan->Driver==Plan->Image || Plan->Driver==Plan->Target || Plan->ChildCount!=2)return EFI_UNSUPPORTED;
  /* Re-read the specific DevicePath opens to bind every child to the PCI owner. */
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *Entries=NULL;UINTN N=0;
  Status=Bs->OpenProtocolInformation(Plan->Target,&PathGuid,&Entries,&N);
  if(!EFI_ERROR(Status) && ((N && !Entries) || N>128))Status=EFI_COMPROMISED_DATA;
  if(!EFI_ERROR(Status))for(UINTN I=0;I<N;I++)
    if(Entries[I].Attributes==EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER && Entries[I].AgentHandle!=Plan->Driver)Status=EFI_ACCESS_DENIED;
  if(Entries && EFI_ERROR(Bs->FreePool(Entries)))Status=EFI_DEVICE_ERROR;
  if(EFI_ERROR(Status))return Status;
  for(UINTN I=0;I<Plan->ChildCount;I++)if(!ChildPath(Bs,Plan->Target,Plan->Children[I]))return EFI_UNSUPPORTED;
  EFI_DRIVER_BINDING_PROTOCOL *Binding=NULL;EFI_LOADED_IMAGE_PROTOCOL *Image=NULL;
  Status=Bs->HandleProtocol(Plan->Driver,&BindingGuid,(VOID **)&Binding);
  if(EFI_ERROR(Status) || !Binding || Binding->DriverBindingHandle!=Plan->Driver || !Binding->Stop ||
     !Binding->ImageHandle || Binding->ImageHandle==Plan->Image)return EFI_UNSUPPORTED;
  Status=Bs->HandleProtocol(Binding->ImageHandle,&LoadedGuid,(VOID **)&Image);
  if(EFI_ERROR(Status) || !Image || !Image->ImageBase || Image->ImageSize!=0x2be40 ||
     Image->ImageCodeType!=EfiBootServicesCode || Binding->Version!=0x6000a)return EFI_UNSUPPORTED;
  Plan->DriverImage=Binding->ImageHandle;Plan->DriverBase=Image->ImageBase;
  Plan->DriverSize=Image->ImageSize;Plan->DriverVersion=Binding->Version;Plan->Stop=Binding->Stop;
  return EFI_SUCCESS;
}
BOOLEAN DhConsoleReady(EFI_SYSTEM_TABLE *St) {
  EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *Text=NULL;
  return St && St->BootServices && St->ConOut && St->ConOut->OutputString &&
    !EFI_ERROR(St->BootServices->HandleProtocol(St->ConsoleOutHandle,&TextGuid,(VOID **)&Text)) && Text==St->ConOut;
}
static BOOLEAN DisplayReady(EFI_SYSTEM_TABLE *St,EFI_HANDLE Target) {
  EFI_BOOT_SERVICES *Bs=St->BootServices;EFI_HANDLE *Handles=NULL;UINTN Count=0,Matching=0;
  EFI_STATUS Status=Bs->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&Count,&Handles);
  if(!EFI_ERROR(Status) && (!Handles || !Count || Count>16))Status=EFI_COMPROMISED_DATA;
  if(!EFI_ERROR(Status))for(UINTN I=0;I<Count;I++) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop=NULL;
    if(!ChildPath(Bs,Target,Handles[I]))continue;
    EFI_STATUS S=Bs->HandleProtocol(Handles[I],&GopGuid,(VOID **)&Gop);
    if(EFI_ERROR(S) || !Gop || !Gop->Mode || !Gop->Mode->Info ||
       Gop->Mode->SizeOfInfo<sizeof(*Gop->Mode->Info))continue;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *M=Gop->Mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info=M->Info;
    if(M->FrameBufferBase!=0xf800000000ULL || M->FrameBufferSize!=0x1900000 ||
       Info->HorizontalResolution!=2560 || Info->VerticalResolution!=1600 ||
       Info->PixelsPerScanLine!=4096 || Info->PixelFormat!=PixelBlueGreenRedReserved8BitPerColor)continue;
    Matching++;
  }
  if(Handles && EFI_ERROR(Bs->FreePool(Handles)))Status=EFI_DEVICE_ERROR;
  return !EFI_ERROR(Status) && Matching==1 && DhConsoleReady(St);
}
EFI_STATUS DhPlan(EFI_SYSTEM_TABLE *St,EFI_HANDLE Target,EFI_HANDLE Image,DH_PLAN *Plan,DH_LOG Log) {
  if(!St || !St->BootServices || !Target || !Image || !Plan || !Log)return EFI_INVALID_PARAMETER;
  EFI_BOOT_SERVICES *Bs=St->BootServices;
  if(!Bs->ProtocolsPerHandle || !Bs->OpenProtocolInformation || !Bs->DisconnectController ||
     !Bs->ConnectController || !Bs->OpenProtocol || !Bs->CloseProtocol || !Bs->RaiseTPL || !Bs->RestoreTPL ||
     !Bs->HandleProtocol || !Bs->LocateHandleBuffer || !Bs->FreePool)return EFI_UNSUPPORTED;
  *Plan=(DH_PLAN){0};Plan->Target=Target;Plan->Image=Image;EFI_PCI_IO_PROTOCOL *Pci=NULL;
  EFI_STATUS Status=PciTarget(Bs,Target,&Pci,Plan->Config);
  if(!EFI_ERROR(Status) && ((Plan->Config[1]&7)!=3 || Plan->Config[4]!=0xd0000000 ||
     Plan->Config[6]!=0xf8 || Plan->Config[11]!=0x3a5817aa))Status=EFI_UNSUPPORTED;
  if(!EFI_ERROR(Status))Status=Snapshot(St,Plan,FALSE);
  if(!Log("target-driver",Status,(UINTN)Plan->Driver))return EFI_DEVICE_ERROR;
  for(UINTN I=0;I<Plan->ChildCount && I<DH_MAX_CHILDREN;I++) {
    EFI_DEVICE_PATH_PROTOCOL *Path=NULL;
    EFI_STATUS S=Bs->HandleProtocol(Plan->Children[I],&PathGuid,(VOID **)&Path);
    if(!Log("display-child",S,(UINTN)Plan->Children[I]))return EFI_DEVICE_ERROR;
    UINTN N=EFI_ERROR(S)?0:PathLength(Path);
    if(!Log("child-path-bytes",S,N))return EFI_DEVICE_ERROR;
    for(UINTN O=0;O<N;O+=8) {
      UINT64 Value=0;const UINT8 *Bytes=(const UINT8 *)Path;
      for(UINTN J=0;J<8 && O+J<N;J++)Value|=(UINT64)Bytes[O+J]<<(J*8);
      if(!Log("child-path-qword",EFI_SUCCESS,Value))return EFI_DEVICE_ERROR;
    }
  }
  if(!EFI_ERROR(Status) && !DisplayReady(St,Target))Status=EFI_UNSUPPORTED;
  if(!Log("plan",Status,Plan->ChildCount))return EFI_DEVICE_ERROR;
  return Status;
}
BOOLEAN DhConfirmTest(EFI_SYSTEM_TABLE *St) {
  if(!St || !St->BootServices || !St->BootServices->Stall || !St->ConIn ||
     !St->ConIn->Reset || !St->ConIn->ReadKeyStroke || EFI_ERROR(St->ConIn->Reset(St->ConIn,FALSE)))return FALSE;
  for(UINTN I=0;I<300;I++) {
    EFI_INPUT_KEY Key;
    EFI_STATUS Status=St->ConIn->ReadKeyStroke(St->ConIn,&Key);
    if(!EFI_ERROR(Status)) {
      if(Key.ScanCode==0x17 || Key.UnicodeChar==27)return FALSE;
      if(Key.UnicodeChar=='t' || Key.UnicodeChar=='T')return TRUE;
    } else if(Status!=EFI_NOT_READY)return FALSE;
    St->BootServices->Stall(100000);
  }
  return FALSE;
}
static BOOLEAN SamePlan(const DH_PLAN *A,const DH_PLAN *B) {
  if(A->Target!=B->Target || A->Driver!=B->Driver || A->DriverImage!=B->DriverImage ||
     A->Image!=B->Image || A->Stop!=B->Stop || A->DriverBase!=B->DriverBase ||
     A->DriverSize!=B->DriverSize || A->DriverVersion!=B->DriverVersion || A->ChildCount!=B->ChildCount ||
     !Equal(A->Config,B->Config,sizeof(A->Config)))return FALSE;
  for(UINTN I=0;I<A->ChildCount;I++) {
    BOOLEAN Found=FALSE;for(UINTN J=0;J<B->ChildCount;J++)if(A->Children[I]==B->Children[J])Found=TRUE;
    if(!Found)return FALSE;
  }
  return TRUE;
}
BOOLEAN DhCheckExclusive(EFI_SYSTEM_TABLE *St,EFI_HANDLE Target,EFI_HANDLE Image) {
  DH_PLAN Plan={0};Plan.Target=Target;Plan.Image=Image;
  return !EFI_ERROR(Snapshot(St,&Plan,2));
}
EFI_STATUS DhRunOperation(EFI_SYSTEM_TABLE *St,const DH_PLAN *Plan,DH_RESULT *Result,DH_LOG Log,DH_OPERATION Operation) {
  if(!St || !Plan || !Result || !Log)return EFI_INVALID_PARAMETER;
  *Result=(DH_RESULT){0};DH_PLAN Fresh;
  EFI_STATUS Status=DhPlan(St,Plan->Target,Plan->Image,&Fresh,Log);
  if(!EFI_ERROR(Status) && !SamePlan(Plan,&Fresh))Status=EFI_ABORTED;
  if(EFI_ERROR(Status))return Status;
  if(!Log("disconnect-intent",EFI_SUCCESS,Plan->ChildCount))return EFI_DEVICE_ERROR;
  /* No file IO in the final snapshot-to-disconnect window. */
  EFI_TPL Old=St->BootServices->RaiseTPL(TPL_NOTIFY);
  EFI_PCI_IO_PROTOCOL *Before=NULL;
  Status=PciTarget(St->BootServices,Plan->Target,&Before,Fresh.Config);
  if(!EFI_ERROR(Status))Status=Snapshot(St,&Fresh,FALSE);
  St->BootServices->RestoreTPL(Old);
  if(EFI_ERROR(Status) || !SamePlan(Plan,&Fresh))return EFI_ABORTED;
  EFI_BOOT_SERVICES *Bs=St->BootServices;Result->Attempted=TRUE;
  Result->DisconnectStatus=Bs->DisconnectController(Plan->Target,Plan->Driver,NULL);
  BOOLEAN Logged=Log("disconnect-result",Result->DisconnectStatus,0);
  Status=Result->DisconnectStatus;
  if(!EFI_ERROR(Status) && Logged) {
    DH_PLAN Detached={0};Detached.Target=Plan->Target;Detached.Image=Plan->Image;
    EFI_PCI_IO_PROTOCOL *Pci=NULL;UINT32 Config[64];
    Old=Bs->RaiseTPL(TPL_NOTIFY);
    Status=PciTarget(Bs,Plan->Target,&Pci,Config);
    if(!EFI_ERROR(Status) && (Config[1]&4))Status=EFI_ACCESS_DENIED;
    if(!EFI_ERROR(Status))Status=Snapshot(St,&Detached,TRUE);
    if(!EFI_ERROR(Status)) {
      EFI_PCI_IO_PROTOCOL *Exclusive=NULL;
      Result->ClaimStatus=Bs->OpenProtocol(Plan->Target,&PciGuid,(VOID **)&Exclusive,Plan->Image,NULL,EFI_OPEN_PROTOCOL_EXCLUSIVE);
      Status=Result->ClaimStatus;
      if(!EFI_ERROR(Status)) {
        Result->Claimed=TRUE;
        if(Exclusive!=Pci)Status=EFI_COMPROMISED_DATA;
      }
    }
    Bs->RestoreTPL(Old);
    if(Result->Claimed && !EFI_ERROR(Status) && Operation) {
      if(!Log("operation-intent",EFI_SUCCESS,0))Status=EFI_DEVICE_ERROR;
      else {
        BOOLEAN Safe=FALSE;Result->OperationAttempted=TRUE;
        Status=Operation(Pci,Plan,&Safe);
        if(!Log("operation-result",Status,Safe))Logged=FALSE;
        if(!Safe) {
          Result->Retained=TRUE;
          return EFI_ERROR(Status)?Status:EFI_DEVICE_ERROR;
        }
      }
    }
    if(Result->Claimed) {
      Old=Bs->RaiseTPL(TPL_NOTIFY);
      Result->CloseStatus=Bs->CloseProtocol(Plan->Target,&PciGuid,Plan->Image,NULL);
      Bs->RestoreTPL(Old);
      if(EFI_ERROR(Result->CloseStatus))Status=Result->CloseStatus;
    }
    if(!Log("exclusive-roundtrip",Status,Result->Claimed))Logged=FALSE;
  }
  if(!Logged && !EFI_ERROR(Status))Status=EFI_DEVICE_ERROR;
  /* Reconnect exactly once, even if Stop partially failed or logging failed. */
  EFI_HANDLE Drivers[2]={Plan->Driver,NULL};
  Result->ReconnectStatus=Bs->ConnectController(Plan->Target,Drivers,NULL,TRUE);
  if(!Log("reconnect-result",Result->ReconnectStatus,0))Logged=FALSE;
  Result->DisplayRestored=!EFI_ERROR(Result->ReconnectStatus) && DisplayReady(St,Plan->Target);
  if(Result->DisplayRestored) {
    EFI_PCI_IO_PROTOCOL *Pci=NULL;UINT32 Config[64];
    EFI_STATUS PciStatus=PciTarget(Bs,Plan->Target,&Pci,Config);
    Result->DisplayRestored=!EFI_ERROR(PciStatus) && Config[4]==Plan->Config[4] && Config[6]==Plan->Config[6];
    if(!Log("pci-after-reconnect",PciStatus,EFI_ERROR(PciStatus)?0:Config[1]))Logged=FALSE;
  }
  if(Result->DisplayRestored) {
    DH_PLAN Rebound={0};Rebound.Target=Plan->Target;Rebound.Image=Plan->Image;
    EFI_STATUS ReboundStatus=Snapshot(St,&Rebound,FALSE);
    Result->DisplayRestored=!EFI_ERROR(ReboundStatus) && Rebound.Driver==Plan->Driver && Rebound.DriverImage==Plan->DriverImage;
  }
  if(!Log("display-restored",Result->DisplayRestored?EFI_SUCCESS:EFI_DEVICE_ERROR,Result->DisplayRestored))Logged=FALSE;
  if(!Logged)return EFI_DEVICE_ERROR;
  if(EFI_ERROR(Status))return Status;
  return Result->DisplayRestored?EFI_SUCCESS:EFI_DEVICE_ERROR;
}
EFI_STATUS DhRun(EFI_SYSTEM_TABLE *St,const DH_PLAN *Plan,DH_RESULT *Result,DH_LOG Log) {
  return DhRunOperation(St,Plan,Result,Log,NULL);
}
