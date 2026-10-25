#pragma once
#include "probe_core.h"
#define GA104_ECC_READOUT_OFFSET 0x00823814U
#define GA104_ECC_READOUT_MASK 0x00010000U
typedef struct {
  EFI_STATUS Status;
  UINT32 Value;
  BOOLEAN Valid, EnabledReadout;
} ECC_READOUT;
/* Observes the readout state; does not establish support or correct operation. */
EFI_STATUS EccCollectReadout(EFI_PCI_IO_PROTOCOL *Pci,UINT64 BarSize,ECC_READOUT *Out);
