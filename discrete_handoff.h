#pragma once
#include "probe_core.h"
#include <Protocol/DriverBinding.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/DevicePath.h>

#define DH_MAX_CHILDREN 8
typedef BOOLEAN (*DH_LOG)(const char *Event, EFI_STATUS Status, UINT64 Value);
typedef struct {
  EFI_HANDLE Target, Driver, DriverImage, Image;
  EFI_DRIVER_BINDING_STOP Stop;
  VOID *DriverBase;
  UINT64 DriverSize;
  UINT32 DriverVersion;
  EFI_HANDLE Children[DH_MAX_CHILDREN];
  UINTN ChildCount;
  UINT32 Config[64];
} DH_PLAN;
typedef struct {
  EFI_STATUS DisconnectStatus, ClaimStatus, CloseStatus, ReconnectStatus;
  BOOLEAN Attempted, Claimed, DisplayRestored;
  BOOLEAN OperationAttempted, Retained;
} DH_RESULT;
/* SafeToReconnect must remain FALSE unless DMA and firmware cleanup are proven. */
typedef EFI_STATUS (*DH_OPERATION)(EFI_PCI_IO_PROTOCOL *Pci,const DH_PLAN *Plan,BOOLEAN *SafeToReconnect);

/* Plan uses information queries only. Run tests driver handoff, never unlocks. */
EFI_STATUS DhPlan(EFI_SYSTEM_TABLE *St, EFI_HANDLE Target, EFI_HANDLE Image, DH_PLAN *Plan, DH_LOG Log);
EFI_STATUS DhRun(EFI_SYSTEM_TABLE *St, const DH_PLAN *Plan, DH_RESULT *Result, DH_LOG Log);
BOOLEAN DhConsoleReady(EFI_SYSTEM_TABLE *St);
BOOLEAN DhConfirmTest(EFI_SYSTEM_TABLE *St);
BOOLEAN DhCheckExclusive(EFI_SYSTEM_TABLE *St,EFI_HANDLE Target,EFI_HANDLE Image);
EFI_STATUS DhRunOperation(EFI_SYSTEM_TABLE *St,const DH_PLAN *Plan,DH_RESULT *Result,DH_LOG Log,DH_OPERATION Operation);
