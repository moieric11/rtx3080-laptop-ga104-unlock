/* Test the actual app callback with the retained core's simulated EFI fixture. */
#define main retained_fixture_main
#include "../unlock/tests/test_efi_transaction.c"
#undef main
#include "../ga104_unlock_app.c"

static unsigned exclusive_checks;
BOOLEAN GuLogReady(VOID){return TRUE;}
BOOLEAN DhCheckExclusive(EFI_SYSTEM_TABLE *st,EFI_HANDLE target,EFI_HANDLE image) {
  CHECK(st==Table && target==Gu.handle && image==Gu.image && Gu.owned);
  exclusive_checks++;
  return fake.mode!=DISPLAY_CHANGED || exclusive_checks<2;
}
static EFI_STATUS EFIAPI callback_bar(EFI_PCI_IO_PROTOCOL *p,UINT8 index,UINT64 *support,VOID **out) {
  CHECK(p==&pci && !index && !support);
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *d=NULL;
  EFI_STATUS s=allocate_pool(EfiLoaderData,sizeof(*d),(VOID **)&d);if(EFI_ERROR(s))return s;
  d->Desc=ACPI_ADDRESS_SPACE_DESCRIPTOR;d->Len=sizeof(*d)-3;d->ResType=ACPI_ADDRESS_SPACE_TYPE_MEM;
  d->AddrRangeMin=GU_BOARD_BAR0;d->AddrRangeMax=0xffffff;d->AddrLen=0x1000000;d->AddrSpaceGranularity=32;
  *out=d;return EFI_SUCCESS;
}
int main(int argc,char **argv) {
  CHECK(argc==4);load_fixture_assets(argv[1],argv[2]);load_capture(argv[3]);
  EFI_SYSTEM_TABLE table={0};table.BootServices=&bs;Table=&table;
  for(enum scenario mode=SUCCESS;mode<SCENARIO_COUNT;mode++) {
    setup(mode);Gu.image=(EFI_HANDLE)(UINTN)2;Gu.handle=(EFI_HANDLE)(UINTN)1;
    pci.GetBarAttributes=callback_bar;exclusive_checks=0;
    DH_PLAN before={0};before.Target=Gu.handle;before.Image=Gu.image;memcpy(before.Config,config,sizeof(config));
    BOOLEAN safe=FALSE;EFI_STATUS s=Unlock(&pci,&before,&safe);
    CHECK(!(config[4]&4));
    if(mode==SUCCESS || mode==FWSEC_RUNNING || mode==PIO_UNAVAILABLE) {
      CHECK(!EFI_ERROR(s) && safe && Gu.flrs==1 && Gu.started && Gu.quiesced);
      CHECK(registers[SS0/4]==0x88888888 && registers[SS1/4]==8);
    }
    if(safe)CHECK(!Gu.allocations && !Gu.dma_count && !Gu.retained && !Gu.master_attributes_pending);
    else CHECK(Gu.retained || Gu.started || Gu.master_attributes_pending);
    printf("GA104 actual unlock callback %s: safe=%u PASS\n",case_names[mode],safe);
    dispose_host_fixture();
  }
  const UINT32 observed[]={0x61014100U,0x60014100U,0x61004100U};
  for(UINTN i=0;i<3;i++) {
    setup(SUCCESS);Gu.image=(EFI_HANDLE)(UINTN)2;Gu.handle=(EFI_HANDLE)(UINTN)1;
    pci.GetBarAttributes=callback_bar;exclusive_checks=0;registers[SS0/4]=observed[i];
    DH_PLAN before={0};before.Target=Gu.handle;before.Image=Gu.image;memcpy(before.Config,config,sizeof(config));
    BOOLEAN safe=FALSE;EFI_STATUS s=Unlock(&pci,&before,&safe);
#ifdef GU_COLD_SELECTOR_VARIANTS
    CHECK(!EFI_ERROR(s) && safe && Gu.started && Gu.flrs==1);
    CHECK(registers[SS0/4]==0x88888888 && registers[SS1/4]==8);
#else
    if(i)CHECK(s==EFI_NOT_READY && safe && !Gu.started && !Gu.flrs);
    else CHECK(!EFI_ERROR(s) && safe && Gu.started);
#endif
    dispose_host_fixture();
  }
  const UINT32 rejected[]={0U,0xffffffffU,0xbadf0000U,0x88888888U,0x61014108U,0x61014101U};
  for(UINTN i=0;i<sizeof(rejected)/sizeof(rejected[0]);i++) {
    setup(SUCCESS);registers[SS0/4]=rejected[i];
    CHECK(CheckCold()==EFI_NOT_READY && !Gu.started && !Gu.flrs);
    dispose_host_fixture();
  }
  /* A matching selector must never bypass the remaining cold gates. */
  const UINT32 offsets[]={0U,0x1183a4U,PLM,0x1fa828U,SS1,GU_GFW_BOOT_OFFSET};
  const UINT32 bad[]={0U,0U,0xffffffffU,0x10U,8U,0U};
  for(UINTN i=0;i<sizeof(offsets)/sizeof(offsets[0]);i++) {
    setup(SUCCESS);registers[offsets[i]/4]=bad[i];
    CHECK(CheckCold()==EFI_NOT_READY && !Gu.started && !Gu.flrs);
    dispose_host_fixture();
  }
  puts("15 selector and cold-state regression scenarios passed.");
  for(UINTN i=0;i<5;i++)free((VOID *)GuAssets[i].firmware.data);
  puts("20 actual GA104 app callback scenarios passed; hardware and firmware execution simulated.");
  return 0;
}
