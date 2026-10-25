/* Offline only: uses captured ROM and firmware files, never device access. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../unlock/vendor/vbios_610.h"
#include "../unlock/vendor/prep_610.h"

static unsigned char *load(const char *path, size_t *size) {
  FILE *f = fopen(path, "rb");
  if (!f) { perror(path); exit(2); }
  if (fseek(f, 0, SEEK_END)) exit(2);
  long n = ftell(f);
  if (n <= 0 || n > 128 * 1024 * 1024 || fseek(f, 0, SEEK_SET)) exit(2);
  unsigned char *data = malloc((size_t)n);
  if (!data || fread(data, 1, (size_t)n, f) != (size_t)n) exit(2);
  fclose(f); *size = (size_t)n; return data;
}

int main(int argc, char **argv) {
  if (argc != 4) return 2;
  size_t rom_size, gsp_size, desc_size;
  unsigned char *rom = load(argv[1], &rom_size);
  unsigned char *gsp = load(argv[2], &gsp_size);
  unsigned char *desc = load(argv[3], &desc_size);
  struct gpu_unlock_fwsec fw = {0};
  struct gpu_unlock_span image = {0}, signature = {0};
  struct gpu_unlock_boot_desc boot = {0};
  struct gpu_unlock_layout_in in = {0};
  struct gpu_unlock_layout_out out = {0};
  int r = gpu_unlock_vbios_fwsec(rom, rom_size, &fw);
  if (r || fw.engine_mask != 0x400) { fprintf(stderr, "FWSEC incompatible: %d mask=%x\n", r, fw.engine_mask); return 1; }
  if (gpu_unlock_elf_section(gsp,gsp_size,".fwimage",&image) ||
      gpu_unlock_elf_section(gsp,gsp_size,".fwsignature_ga10x",&signature) ||
      signature.size != 4096 ||
      gpu_unlock_boot_desc_parse(desc,desc_size,24576,&boot)) return 1;
  /* Captured FB=0x4000 MiB, VGA=1. MMU lock does not end at FB top. */
  in.usable_fb = 16384ULL << 20;
  in.vga_base = in.usable_fb - (1ULL << 20);
  in.frts_size = in.nonwpr_size = 1ULL << 20;
  in.boot_size = boot.image_size; in.image_size = image.size;
  in.heap_request = 88ULL << 20;
  if (gpu_unlock_layout(&in,&out) || out.nonwpr_start < in.usable_fb-(256ULL<<20)) return 1;
  printf("{\"fwsec_image_offset\":%u,\"fwsec_image_size\":%u,\"engine_mask\":%u,"
         "\"ucode_id\":%u,\"signature_versions\":%u,\"signature_count\":%u,"
         "\"gsp_image_bytes\":%llu,\"gsp_signature_bytes\":%llu,"
         "\"nonwpr_start\":%llu,\"wpr_start\":%llu,\"wpr_end\":%llu}\n",
         fw.image_off,fw.image_size,fw.engine_mask,fw.ucode_id,fw.sig_versions,fw.sig_count,
         (unsigned long long)image.size,(unsigned long long)signature.size,
         (unsigned long long)out.nonwpr_start,(unsigned long long)out.wpr_start,(unsigned long long)out.wpr_end);
  free(rom); free(gsp); free(desc); return 0;
}
