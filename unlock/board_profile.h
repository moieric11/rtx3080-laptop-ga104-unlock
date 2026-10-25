#pragma once

/* Profiles describe captures; they do not establish that display handoff works. */
#ifdef GU_PROFILE_LENOVO_GA104
#define GU_BOARD_ID 0x24dc10deU
#define GU_BOARD_SUBSYSTEM 0x3a5817aaU
#define GU_BOARD_CHIP 0xb74000a1U
#define GU_BOARD_BAR0 0xd0000000ULL
#define GU_BOARD_FB_MB 0x4000U
#define GU_BOARD_COLD_SS0 0x61014100U
#ifdef GU_COLD_SELECTOR_VARIANTS
/* Observed on this same Lenovo in RUN01, RUN03 and RUN04. All override
 * enable bits (0x88888888) remain clear. Do not accept zero/inaccessible
 * reads or arbitrary selectors; the other cold-state gates still apply. */
#define GU_BOARD_COLD_SS0_MATCH(value) ((value) == 0x61014100U || (value) == 0x60014100U || (value) == 0x61004100U)
#else
#define GU_BOARD_COLD_SS0_MATCH(value) ((value) == GU_BOARD_COLD_SS0)
#endif
#define GU_BOARD_COLD_SS1 6U
#define GU_BOARD_COLD_SS1_MATCH(value) ((value) == 6U)
#define GU_BOARD_PROM_SHA256 "3b0aa7e610e24c01b91c6267fac86d5a8ff430a5864f927102041da5bbbd486f"
/* A live build needs a tested discrete-GOP release/reconnect path. */
#if defined(GU_UEFI_PORT) && !defined(GU_OFFLINE_TEST) && !defined(GU_DISCRETE_EXPERIMENTAL)
#error Lenovo GA104 live unlock is not ready: discrete display handoff unresolved
#endif
#else
#define GU_BOARD_ID 0x220610deU
#define GU_BOARD_SUBSYSTEM 0x38901462U
#define GU_BOARD_CHIP 0xb72000a1U
#define GU_BOARD_BAR0 0x84000000ULL
#define GU_BOARD_FB_MB 0x2800U
#define GU_BOARD_COLD_SS0 0x46254040U
#define GU_BOARD_COLD_SS0_MATCH(value) ((value) == GU_BOARD_COLD_SS0)
#define GU_BOARD_COLD_SS1 3U
#define GU_BOARD_COLD_SS1_MATCH(value) ((value) == 2U || (value) == 3U)
#define GU_BOARD_PROM_SHA256 "f3dc8f07b0b8c9856d192df172dfa7f7f84ac17ef6edfdb448ad7fa94efd2789"
#endif
