#pragma once
#include <Uefi.h>
#include <Protocol/PciIo.h>
#include <IndustryStandard/Acpi10.h>

#ifdef PROBE_LENOVO_GA104
#define PROBE_PCI_ID 0x24dc10deU
#define PROBE_SUBSYSTEM_ID 0x3a5817aaU
#define PROBE_ID_TEXT "Expected PCI 10de:24dc subsystem 17aa:3a58\r\n"
#define PROBE_FOUND_TEXT L"Lenovo RTX 3080 Laptop GA104 found. Reading existing state...\r\n"
#else
#define PROBE_PCI_ID 0x220610deU
#define PROBE_SUBSYSTEM_ID 0x38901462U
#define PROBE_ID_TEXT "Expected PCI 10de:2206 subsystem 1462:3890\r\n"
#define PROBE_FOUND_TEXT L"Expected MSI RTX 3080 found. Reading its existing state...\r\n"
#endif

#define PROBE_SAMPLE_COUNT 16
#define PROBE_PROM_OFFSET 0x00300000ULL
#define PROBE_PROM_SIZE 0x00100000U
#define PROBE_ROM_LIMIT 0x01000000U

typedef struct {
  UINT32 Offset;
  UINT32 Value;
  EFI_STATUS Status;
} PROBE_SAMPLE;

typedef struct {
  UINT32 Config[64];
  UINT64 BarSize;
  EFI_STATUS BarStatus;
  BOOLEAN MemoryEnabled;
  PROBE_SAMPLE Samples[PROBE_SAMPLE_COUNT];
} PROBE_RESULT;

BOOLEAN ProbeMatches(const UINT32 *Config);
EFI_STATUS ProbeCollect(EFI_PCI_IO_PROTOCOL *Pci, EFI_BOOT_SERVICES *Bs,
                       PROBE_RESULT *Result);
EFI_STATUS ProbeReadProm(EFI_PCI_IO_PROTOCOL *Pci, const PROBE_RESULT *Result,
                        VOID *Buffer, UINTN Size);
BOOLEAN ProbeRomUsable(const VOID *Buffer, UINT64 Size);
