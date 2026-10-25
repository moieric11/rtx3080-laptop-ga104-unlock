#include "probe_core.h"
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/GraphicsOutput.h>
#include <Guid/FileInfo.h>
#include <Guid/GlobalVariable.h>
#include <Protocol/DriverBinding.h>
#ifdef PROBE_HANDOFF_TEST
#include "discrete_handoff.h"
#endif

static EFI_GUID PciGuid = EFI_PCI_IO_PROTOCOL_GUID;
static EFI_GUID LoadedGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
static EFI_GUID PathGuid = EFI_DEVICE_PATH_PROTOCOL_GUID;
static EFI_GUID FsGuid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID GopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static EFI_GUID FileInfoGuid = EFI_FILE_INFO_ID;
static EFI_SYSTEM_TABLE *St;
/* Keep an absolute pointer relocation for firmware PE loaders. */
static EFI_SYSTEM_TABLE **volatile SystemTableSlot = &St;
static CHAR8 Log[262144];
static UINTN Used;
static BOOLEAN LogOverflow;

static VOID Text(const CHAR8 *S) {
  while (*S && Used + 1 < sizeof(Log)) Log[Used++] = *S++;
  if (*S) LogOverflow = TRUE;
  Log[Used] = 0;
}
static VOID Hex(const CHAR8 *Key, UINT64 Value) {
  static const CHAR8 Digits[] = "0123456789abcdef";
  INTN I;
  Text(Key); Text("=0x");
  for (I = 60; I >= 0; I -= 4) {
    CHAR8 S[2] = {Digits[(Value >> I) & 15], 0}; Text(S);
  }
  Text("\r\n");
}
static VOID Say(CHAR16 *S) {
  if (St->ConOut && St->ConOut->OutputString) St->ConOut->OutputString(St->ConOut, S);
}
static VOID ShowLog(VOID) {
  CHAR16 Line[128];
  UINTN I = 0;
  while (I < Used) {
    UINTN N = 0;
    while (I < Used && N < 127) Line[N++] = (CHAR16)(UINT8)Log[I++];
    Line[N] = 0; Say(Line);
  }
}

/* Bounded walk of firmware-owned device-path nodes, without opening controllers. */
static BOOLEAN PathHasUsb(EFI_HANDLE Handle, BOOLEAN Record) {
  EFI_DEVICE_PATH_PROTOCOL *Path = NULL;
  UINTN Total = 0, Nodes = 0;
  BOOLEAN Usb = FALSE;
  EFI_STATUS Status = St->BootServices->HandleProtocol(Handle, &PathGuid, (VOID **)&Path);
  if (Record) Hex("device_path_status", Status);
  if (EFI_ERROR(Status) || Path == NULL) return FALSE;
  while (Nodes++ < 64 && Total + 4 <= 4096) {
    UINTN Length = Path->Length[0] | ((UINTN)Path->Length[1] << 8);
    UINTN I;
    if (Length < 4 || Length > 4096 - Total) break;
    if (Path->Type == MESSAGING_DEVICE_PATH &&
        (Path->SubType == MSG_USB_DP || Path->SubType == MSG_USB_CLASS_DP ||
         Path->SubType == MSG_USB_WWID_DP)) Usb = TRUE;
    if (Record) {
      static const CHAR8 Digits[] = "0123456789abcdef";
      Text("path_node=");
      for (I = 0; I < Length; I++) {
        UINT8 B = ((UINT8 *)Path)[I];
        CHAR8 Pair[3] = {Digits[B >> 4], Digits[B & 15], 0}; Text(Pair);
      }
      Text("\r\n");
    }
    if (Path->Type == END_DEVICE_PATH_TYPE)
      return Usb && Length == 4 && Path->SubType == END_ENTIRE_DEVICE_PATH_SUBTYPE;
    Total += Length;
    Path = (EFI_DEVICE_PATH_PROTOCOL *)((UINT8 *)Path + Length);
  }
  return FALSE; /* Malformed or unterminated path is never an output destination. */
}

/* Only a USB-loaded image with our existing marker directory can create files. */
static EFI_FILE_PROTOCOL *OpenOutput(EFI_HANDLE Image) {
  EFI_LOADED_IMAGE_PROTOCOL *Loaded = NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs = NULL;
  EFI_FILE_PROTOCOL *Root = NULL, *Parent = NULL, *Run = NULL;
  EFI_STATUS Status;
  UINTN Slot;
  union { EFI_FILE_INFO Info; UINT8 Bytes[512]; } Metadata;
  UINTN MetadataSize = sizeof(Metadata);
  Status = St->BootServices->HandleProtocol(Image, &LoadedGuid, (VOID **)&Loaded);
  Hex("loaded_image_status", Status);
  if (EFI_ERROR(Status) || Loaded == NULL) return NULL;
  if (!PathHasUsb(Loaded->DeviceHandle, TRUE)) {
    Text("output=disabled: loaded image device is not a verified USB path\r\n");
    return NULL;
  }
  Status = St->BootServices->HandleProtocol(Loaded->DeviceHandle, &FsGuid, (VOID **)&Fs);
  Hex("usb_filesystem_status", Status);
  if (EFI_ERROR(Status) || Fs == NULL) return NULL;
  Status = Fs->OpenVolume(Fs, &Root);
  Hex("usb_open_volume_status", Status);
  if (EFI_ERROR(Status) || Root == NULL) return NULL;
  Status = Root->Open(Root, &Parent, L"\\RTX3080-PROBE",
                      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE, 0);
  Root->Close(Root);
  Hex("probe_directory_status", Status);
  if (EFI_ERROR(Status) || Parent == NULL) return NULL;
  Status = Parent->GetInfo(Parent, &FileInfoGuid, &MetadataSize, &Metadata);
  if (EFI_ERROR(Status) || MetadataSize < SIZE_OF_EFI_FILE_INFO ||
      !(Metadata.Info.Attribute & EFI_FILE_DIRECTORY)) {
    Text("output=disabled: marker is not a verified directory\r\n");
    Parent->Close(Parent); return NULL;
  }
  for (Slot = 0; Slot < 100; Slot++) {
    CHAR16 Name[] = L"RUN00";
    Name[3] = (CHAR16)('0' + Slot / 10); Name[4] = (CHAR16)('0' + Slot % 10);
    Status = Parent->Open(Parent, &Run, Name, EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(Status)) { Run->Close(Run); Run = NULL; continue; }
    if (Status != EFI_NOT_FOUND) break;
    Status = Parent->Open(Parent, &Run, Name,
      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, EFI_FILE_DIRECTORY);
    Hex("run_directory_status", Status); Hex("run_slot", Slot);
    if (EFI_ERROR(Status)) Run = NULL;
    break;
  }
  Parent->Close(Parent);
  return Run;
}

static EFI_STATUS Save(EFI_FILE_PROTOCOL *Directory, CHAR16 *Name,
                       VOID *Data, UINTN Size) {
  EFI_FILE_PROTOCOL *File = NULL;
  EFI_STATUS Status;
  UINTN Written = Size;
  if (!Directory) return EFI_NOT_FOUND;
  Status = Directory->Open(Directory, &File, Name, EFI_FILE_MODE_READ, 0);
  if (!EFI_ERROR(Status)) { File->Close(File); return EFI_ALREADY_STARTED; }
  if (Status != EFI_NOT_FOUND) return Status;
  Status = Directory->Open(Directory, &File, Name,
    EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
  if (EFI_ERROR(Status)) return Status;
  Status = File->Write(File, &Written, Data);
  if (!EFI_ERROR(Status) && Written != Size) Status = EFI_VOLUME_FULL;
  if (!EFI_ERROR(Status)) Status = File->Flush(File);
  File->Close(File);
  return Status;
}

static VOID GuidText(const EFI_GUID *Guid) {
  static const CHAR8 Digits[] = "0123456789abcdef";
  const UINT8 *Bytes = (const UINT8 *)Guid;
  Text("protocol_guid_bytes=");
  for (UINTN I = 0; I < sizeof(*Guid); I++) {
    CHAR8 Pair[] = {Digits[Bytes[I] >> 4], Digits[Bytes[I] & 15], 0}; Text(Pair);
  }
  Text("\r\n");
}

/* Information queries only. Never OpenProtocol(EXCLUSIVE) or disconnect here. */
static VOID RecordOwnership(EFI_HANDLE Handle) {
  EFI_BOOT_SERVICES *Bs = St->BootServices;
  EFI_GUID **Protocols = NULL; UINTN Count = 0, Total = 0;
  Text("[OWNERSHIP]\r\n"); Hex("controller_handle", (UINTN)Handle);
  if (!Bs->ProtocolsPerHandle || !Bs->OpenProtocolInformation) {
    Text("ownership=information services unavailable\r\n"); return;
  }
  EFI_STATUS Status = Bs->ProtocolsPerHandle(Handle, &Protocols, &Count);
  Hex("protocols_status", Status); Hex("protocol_count", Count);
  if (!EFI_ERROR(Status) && Protocols && Count <= 64) {
    for (UINTN I = 0; I < Count; I++) {
      EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *Entries = NULL; UINTN N = 0;
      if (!Protocols[I]) { Text("ownership=null protocol\r\n"); break; }
      GuidText(Protocols[I]);
      Status = Bs->OpenProtocolInformation(Handle, Protocols[I], &Entries, &N);
      Hex("open_information_status", Status); Hex("open_entry_count", N);
      if (!EFI_ERROR(Status) && (!N || Entries) && N <= 128 - Total) {
        Total += N;
        for (UINTN J = 0; J < N; J++) {
          EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *Entry = &Entries[J];
          Hex("agent_handle", (UINTN)Entry->AgentHandle);
          Hex("child_controller_handle", (UINTN)Entry->ControllerHandle);
          Hex("open_attributes", Entry->Attributes); Hex("open_count", Entry->OpenCount);
          if (Entry->Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER) {
            EFI_DRIVER_BINDING_PROTOCOL *Binding = NULL;
            EFI_GUID BindingGuid = EFI_DRIVER_BINDING_PROTOCOL_GUID;
            Status = Bs->HandleProtocol(Entry->AgentHandle, &BindingGuid, (VOID **)&Binding);
            Hex("binding_status", Status);
            if (!EFI_ERROR(Status) && Binding) {
              Hex("binding_handle", (UINTN)Binding->DriverBindingHandle);
              Hex("driver_image_handle", (UINTN)Binding->ImageHandle);
              Hex("binding_version", Binding->Version);
              EFI_LOADED_IMAGE_PROTOCOL *Loaded = NULL;
              Status = Bs->HandleProtocol(Binding->ImageHandle, &LoadedGuid, (VOID **)&Loaded);
              Hex("driver_loaded_image_status", Status);
              if (!EFI_ERROR(Status) && Loaded) {
                Hex("driver_image_bytes", Loaded->ImageSize);
                Hex("driver_code_type", Loaded->ImageCodeType);
              }
            }
          }
        }
      } else if (!EFI_ERROR(Status)) Text("ownership=invalid or oversized entries\r\n");
      if (Entries) Bs->FreePool(Entries);
      if (Total >= 128) { Text("ownership=entry limit reached\r\n"); break; }
    }
  }
  if (Protocols) Bs->FreePool(Protocols);
}

/* Record exact boot option bytes without loading an image or changing NVRAM. */
static VOID RecordBootOptions(VOID) {
  EFI_GUID GlobalGuid = EFI_GLOBAL_VARIABLE;
  UINT16 Order[64]; UINTN Bytes = sizeof(Order);
  Text("[BOOT OPTIONS]\r\n");
  if (!St->RuntimeServices || !St->RuntimeServices->GetVariable) {
    Text("boot_options=runtime query unavailable\r\n"); return;
  }
  EFI_STATUS Status = St->RuntimeServices->GetVariable(L"BootOrder", &GlobalGuid, NULL, &Bytes, Order);
  Hex("boot_order_status", Status);
  if (EFI_ERROR(Status) || Bytes > sizeof(Order) || (Bytes & 1)) return;
  static const CHAR16 HexDigits[] = L"0123456789ABCDEF";
  static const CHAR8 Digits[] = "0123456789abcdef";
  static UINT8 Data[4096];
  for (UINTN I = 0; I < Bytes / 2; I++) {
    CHAR16 Name[] = L"Boot0000"; UINTN Size = sizeof(Data);
    for (UINTN J = 0; J < 4; J++) Name[4+J] = HexDigits[(Order[I] >> ((3-J)*4)) & 15];
    Status = St->RuntimeServices->GetVariable(Name, &GlobalGuid, NULL, &Size, Data);
    Hex("boot_option", Order[I]); Hex("boot_option_status", Status);
    if (EFI_ERROR(Status) || Size > sizeof(Data)) continue;
    Text("boot_option_bytes=");
    for (UINTN J = 0; J < Size; J++) {
      CHAR8 Pair[] = {Digits[Data[J] >> 4], Digits[Data[J] & 15], 0}; Text(Pair);
    }
    Text("\r\n");
  }
}

static VOID RecordGop(VOID) {
  EFI_HANDLE *Handles = NULL;
  UINTN Count = 0, I;
  EFI_STATUS Status = St->BootServices->LocateHandleBuffer(ByProtocol, &GopGuid,
                                                        NULL, &Count, &Handles);
  Hex("gop_enumeration_status", Status);
  if (EFI_ERROR(Status)) return;
  for (I = 0; I < Count; I++) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = NULL;
    Status = St->BootServices->HandleProtocol(Handles[I], &GopGuid, (VOID **)&Gop);
    if (EFI_ERROR(Status) || !Gop || !Gop->Mode) continue;
    Text("[GOP]\r\n"); Hex("index", I);
    Hex("gop_handle", (UINTN)Handles[I]);
    Hex("mode", Gop->Mode->Mode);
    Hex("framebuffer_base", Gop->Mode->FrameBufferBase);
    Hex("framebuffer_size", Gop->Mode->FrameBufferSize);
    if (Gop->Mode->Info) {
      Hex("width", Gop->Mode->Info->HorizontalResolution);
      Hex("height", Gop->Mode->Info->VerticalResolution);
      Hex("pixels_per_scanline", Gop->Mode->Info->PixelsPerScanLine);
      Hex("pixel_format", Gop->Mode->Info->PixelFormat);
    }
    PathHasUsb(Handles[I], TRUE);
    RecordOwnership(Handles[I]);
  }
  if (Handles) St->BootServices->FreePool(Handles);
}

#ifdef PROBE_HANDOFF_TEST
static EFI_FILE_PROTOCOL *HandoffFile;
static BOOLEAN HandoffLogFailed;
static BOOLEAN HandoffLog(const char *Event, EFI_STATUS Status, UINT64 Value) {
  UINTN Previous=Used;
  Text(Event);Text("\r\n");Hex("status",Status);Hex("value",Value);
  if(!HandoffFile || HandoffLogFailed || LogOverflow)return FALSE;
  UINTN Expected=Used-Previous,Written=Expected;
  EFI_STATUS S=HandoffFile->Write(HandoffFile,&Written,Log+Previous);
  if(!EFI_ERROR(S) && Written==Expected)S=HandoffFile->Flush(HandoffFile);
  if(EFI_ERROR(S) || Written!=Expected)HandoffLogFailed=TRUE;
  return !HandoffLogFailed;
}
static VOID RunHandoff(EFI_HANDLE Image,EFI_HANDLE Target,EFI_FILE_PROTOCOL *Output) {
  EFI_STATUS Status;EFI_FILE_PROTOCOL *Existing=NULL;
  Status=Output->Open(Output,&Existing,L"HANDOFF.TXT",EFI_FILE_MODE_READ,0);
  if(!EFI_ERROR(Status)){if(Existing)Existing->Close(Existing);return;}
  if(Status!=EFI_NOT_FOUND)return;
  HandoffFile=NULL;HandoffLogFailed=FALSE;Used=0;Log[0]=0;LogOverflow=FALSE;
  Status=Output->Open(Output,&HandoffFile,L"HANDOFF.TXT",
    EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,0);
  if(EFI_ERROR(Status) || !HandoffFile)return;
  if(!HandoffLog("handoff-test-build-v0.3",EFI_SUCCESS,0)) {
    HandoffFile->Close(HandoffFile);HandoffFile=NULL;return;
  }
  DH_PLAN Plan;
  Status=DhPlan(St,Target,Image,&Plan,HandoffLog);
  if(EFI_ERROR(Status)) {
    HandoffLog("handoff-preflight-refused",Status,0);
    Say(L"Handoff preflight refused. Details saved in HANDOFF.TXT. No driver stopped.\r\n");
    HandoffFile->Close(HandoffFile);HandoffFile=NULL;return;
  }
  Say(L"\r\nOPTIONAL DISCRETE DISPLAY HANDOFF TEST - not an unlock\r\n");
  Say(L"T stops then reconnects the NVIDIA UEFI driver once. The screen may go black.\r\n");
  Say(L"If the screen stays black for over 60 seconds, power off with the power button.\r\n");
  Say(L"Press T to test, Esc to cancel; after 30 seconds this test cancels.\r\n");
  BOOLEAN Armed=DhConfirmTest(St);
  DH_RESULT Result={0};
  if(Armed && HandoffLog("manual-T",EFI_SUCCESS,1)) {
    Status=St->BootServices->SetWatchdogTimer(0,0,0,NULL);
    if(HandoffLog("watchdog-disable",Status,0) && !EFI_ERROR(Status)) {
      /* No console calls from DhRun or its log callback while GOPs are detached. */
      Status=DhRun(St,&Plan,&Result,HandoffLog);
      HandoffLog("handoff-final",Status,Result.DisplayRestored);
    }
  } else HandoffLog("handoff-cancelled",EFI_ABORTED,0);
  HandoffFile->Close(HandoffFile);HandoffFile=NULL;
  if(Result.Attempted && !Result.DisplayRestored) {
    /* Do not hand a broken graphics environment to Windows. No reset loop. */
    for(;;)St->BootServices->Stall(1000000);
  }
  if(DhConsoleReady(St))Say(L"Handoff test finished. HANDOFF.TXT is saved. The GPU was not unlocked.\r\n");
}
#endif

EFI_STATUS EFIAPI efi_main(EFI_HANDLE Image, EFI_SYSTEM_TABLE *Table) {
  EFI_BOOT_SERVICES *Bs;
  EFI_HANDLE *Handles = NULL, SelectedHandle = NULL;
  EFI_PCI_IO_PROTOCOL *Selected = NULL;
  EFI_FILE_PROTOCOL *Output;
  UINTN Count = 0, I, Matches = 0;
  EFI_STATUS Status;
  PROBE_RESULT Result;
  *SystemTableSlot = Table; Bs = St->BootServices; Used = 0; Log[0] = 0; LogOverflow = FALSE;
#ifdef PROBE_HANDOFF_TEST
  Say(L"\r\nRTX 3080 diagnostic v0.3 - optional Discrete driver handoff test\r\n");
#else
  Say(L"\r\nRTX 3080 read-only diagnostic v0.2 - Discrete ownership\r\n");
#endif
  Say(L"Collects BIOS and preboot state. This does not unlock the GPU.\r\n");
#ifdef PROBE_HANDOFF_TEST
  Text("RTX3080 diagnostic v0.3 - optional Discrete driver handoff test\r\n");
  Text("REPORT.TXT covers the read-only phase; driver test results are in HANDOFF.TXT\r\n");
#else
  Text("RTX3080 read-only diagnostic v0.2 - Discrete ownership\r\n");
#endif
  Text(PROBE_ID_TEXT);
  Hex("console_out_handle", (UINTN)St->ConsoleOutHandle);
  Hex("console_in_handle", (UINTN)St->ConsoleInHandle);
  Hex("standard_error_handle", (UINTN)St->StandardErrorHandle);
  Output = OpenOutput(Image);
  Status = Bs->LocateHandleBuffer(ByProtocol, &PciGuid, NULL, &Count, &Handles);
  Hex("pci_enumeration_status", Status);
  if (!EFI_ERROR(Status)) {
    for (I = 0; I < Count; I++) {
      EFI_PCI_IO_PROTOCOL *Pci = NULL;
      UINT32 Config[64];
      UINTN Segment = 0, Bus = 0, Device = 0, Function = 0;
      Status = Bs->HandleProtocol(Handles[I], &PciGuid, (VOID **)&Pci);
      if (EFI_ERROR(Status) || Pci == NULL) continue;
      Status = Pci->Pci.Read(Pci, EfiPciIoWidthUint32, 0, 64, Config);
      if (EFI_ERROR(Status) || (Config[0] & 0xffff) != 0x10de) continue;
      Text("[NVIDIA PCI]\r\n");
      Status = Pci->GetLocation(Pci, &Segment, &Bus, &Device, &Function);
      Hex("location_status", Status); Hex("segment", Segment); Hex("bus", Bus);
      Hex("device", Device); Hex("function", Function);
      Hex("identity", Config[0]); Hex("class_revision", Config[2]);
      Hex("subsystem", Config[11]);
      if (ProbeMatches(Config)) { Matches++; Selected = Pci; SelectedHandle = Handles[I]; }
    }
  }
  if (Handles) Bs->FreePool(Handles);
  Hex("matching_cards", Matches);
  RecordGop();
  RecordBootOptions();
  if (Matches == 1) {
    Say(PROBE_FOUND_TEXT);
    Text("[TARGET]\r\n"); PathHasUsb(SelectedHandle, TRUE);
    RecordOwnership(SelectedHandle);
    Status = ProbeCollect(Selected, Bs, &Result);
    Hex("collect_status", Status);
    if (!EFI_ERROR(Status)) {
      for (I = 0; I < 64; I++) { Hex("config_offset", I * 4); Hex("value", Result.Config[I]); }
      Hex("bar0_status", Result.BarStatus); Hex("bar0_length", Result.BarSize);
      Hex("memory_decode_enabled", Result.MemoryEnabled);
      for (I = 0; I < PROBE_SAMPLE_COUNT; I++) {
        PROBE_SAMPLE *Sample = &Result.Samples[I];
        Hex("register", Sample->Offset); Hex("status", Sample->Status); Hex("value", Sample->Value);
        if (!EFI_ERROR(Sample->Status) &&
            (Sample->Value == 0xffffffff || (Sample->Value & 0xffff0000U) == 0xbadf0000U))
          Text("interpretation=inaccessible, not a valid measurement\r\n");
      }
      Hex("rom_image_size", Selected->RomSize);
      if (Output && ProbeRomUsable(Selected->RomImage, Selected->RomSize)) {
        Status = Save(Output, L"ROMIMAGE.BIN", Selected->RomImage, (UINTN)Selected->RomSize);
        Hex("rom_image_save_status", Status);
      } else Text("rom_image=not saved (no writable USB or missing/invalid firmware ROM copy)\r\n");
      if (Output && Result.MemoryEnabled && !EFI_ERROR(Result.BarStatus) &&
          Result.BarSize >= PROBE_PROM_OFFSET + PROBE_PROM_SIZE) {
        VOID *Prom = NULL;
        Status = Bs->AllocatePool(EfiLoaderData, PROBE_PROM_SIZE, &Prom);
        Hex("prom_allocation_status", Status);
        if (!EFI_ERROR(Status) && Prom) {
          Say(L"Reading the 1 MiB PROM window; no ROM-enable writes are used.\r\n");
          Status = ProbeReadProm(Selected, &Result, Prom, PROBE_PROM_SIZE);
          Hex("prom_read_status", Status);
          if (!EFI_ERROR(Status) && ProbeRomUsable(Prom, PROBE_PROM_SIZE)) {
            Status = Save(Output, L"PROM.BIN", Prom, PROBE_PROM_SIZE);
            Hex("prom_save_status", Status);
          } else Text("prom=not saved (read error or no 55aa ROM signature)\r\n");
          Bs->FreePool(Prom);
        }
      }
    }
  } else {
    Text("target=missing or ambiguous; no GPU MMIO or ROM reads attempted\r\n");
    Say(L"Expected card missing or ambiguous. No GPU MMIO reads attempted.\r\n");
  }
  Text("GPU writes=none; GPU resets=none; boot-variable changes=none\r\n");
  if (LogOverflow) {
    static const CHAR8 Tail[] = "\r\nREPORT_TRUNCATED=1; additional ownership or boot data missing\r\n";
    UINTN N = sizeof(Tail)-1;
    if (Used >= N) for (UINTN J = 0; J < N; J++) Log[Used-N+J] = Tail[J];
    Say(L"WARNING: log buffer full; report is incomplete.\r\n");
  }
  Status = Save(Output, L"REPORT.TXT", Log, Used);
  if (EFI_ERROR(Status)) {
    Say(L"Could not save report. Diagnostic details follow:\r\n");
    Hex("report_save_status", Status); ShowLog();
  } else Say(L"Saved: USB \\RTX3080-PROBE\\RUNxx\\REPORT.TXT and available BIOS copies.\r\n");
#ifdef PROBE_HANDOFF_TEST
  if(!EFI_ERROR(Status) && Matches==1 && !LogOverflow && Output)RunHandoff(Image,SelectedHandle,Output);
#endif
  if (Output) Output->Close(Output);
  Say(L"Returning in 15 seconds. Choose Windows Boot Manager if a boot menu appears.\r\n");
  Bs->Stall(15000000);
  return EFI_SUCCESS;
}
