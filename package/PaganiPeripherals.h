// SPDX-License-Identifier: BSD-2-Clause-Patent
// OnePlus Ace 6 (PLQ110) board constants; installed as kits/ace6.
//
// Every value below was measured from this device:
//   runtime DT  /sys/firmware/fdt  (1401819 B)
//   overlays    dtbo_b             (board-id 0x50008)
//   base DTBs   vendor_boot_b      (dtb @ 0x11a1000, 13 concatenated FDTs)
//   GPT         sda                (4096 B logical sectors)
//
// Differences from the OnePlus 13T (PaganiPeripherals.h) are marked DIFF.
#pragma once

// ---- clocks -------------------------------------------------------------
#define PAGANI_GCC_BASE 0x100000ULL                      // same as 13T
#define PAGANI_GPUCC_BASE 0x3d90000ULL
#define PAGANI_GXR_CLKCTL 0x3d68024ULL

// ---- touch: Synaptics S3910 on qupv3_se4_spi ----------------------------
// DIFF: 13T used ldob4/ldob16 rails; this board uses a single fixed L4B rail.
#define PAGANI_TOUCH_SE 0xa90000ULL                      // same as 13T
#define PAGANI_TOUCH_WRAPPER 0xac0000ULL                 // same as 13T
#define PAGANI_TOUCH_SE_SIZE 0x4000                      // reg = <0xa90000 0x4000>
#define PAGANI_TOUCH_IRQ 0x165                           // GIC SPI 357
#define PAGANI_TOUCH_SMMU 0x15000000ULL
#define PAGANI_TOUCH_SMMU_SIZE 0x100000
// No SPI node on this SoC declares iommus, so the bypass SID is not readable
// from the device tree. The 13T hard-coded 0xa3 cannot be trusted here.
//
// PAGANI_TOUCH_SMMU_PROBE  1 = configure a translation context (needs a valid SID)
//                        0 = the controller is on the bypass path, use physical DMA
// PAGANI_TOUCH_SMMU_SID    SID to program when probing; override at build time
// PAGANI_TOUCH_SMMU_OPTIONAL 1 = fall back to physical DMA if configuration fails
//
// Measurement instructions are in docs/touch-bringup.md.
#define PAGANI_TOUCH_SMMU_PROBE 1
#ifndef PAGANI_TOUCH_SMMU_SID
#define PAGANI_TOUCH_SMMU_SID 0xa3
#endif
#define PAGANI_TOUCH_SMMU_OPTIONAL 1

// tlmm pins, measured from qupv3_se4_spi_*_active pinctrl nodes.
#define PAGANI_TLMM_GPIO(n) (0xf100000ULL + 0x1000ULL * (n))
#define PAGANI_TOUCH_MOSI 49
#define PAGANI_TOUCH_MISO 48
#define PAGANI_TOUCH_CLK  50
#define PAGANI_TOUCH_CS   51
#define PAGANI_TOUCH_RESET 161                           // 0xa1
#define PAGANI_TOUCH_ATTN  162                           // 0xa2

// DIFF: measured drive-strength values, not the 13T {2,1,2,1,3,3}.
// Order: MISO(48), MOSI(49), CLK(50), CS(51), RESET, ATTN
#define PAGANI_TOUCH_PINS {48, 49, 50, 51, PAGANI_TOUCH_RESET, PAGANI_TOUCH_ATTN}
#define PAGANI_TOUCH_DRIVES {6, 4, 6, 4, 3, 3}

#define PAGANI_TOUCH_RESET_MS 10                         // reset-active-ms
#define PAGANI_TOUCH_STARTUP_MS 80                       // reset-delay-ms
#define PAGANI_TOUCH_POWER_MS 200                        // power-delay-ms
#define PAGANI_TOUCH_HZ 19000000                         // 0x121eac0
#define PAGANI_TOUCH_CS_DELAY 50                         // spi-cs-setup
#define PAGANI_TOUCH_WORD_DELAY 0

// Touch capacity, measured: touchpanel,tx-rx-num = <0x12 0x28>
#define PAGANI_TOUCH_TX 18
#define PAGANI_TOUCH_RX 40
#define PAGANI_TOUCH_RES_X 20352                         // 0x4f80
#define PAGANI_TOUCH_RES_Y 44800                         // 0xaf00
#define PAGANI_TOUCH_EDGE_PIXELS 48

// DIFF: this board's touch node declares only power,vddi-supply = <&L4B>,
// a fixed 1.8 V rail (regulator-pm-humu-l4, phandle 0x401).
// There is no equivalent of the 13T 3.1 V vcc rail in the active node.
#define PAGANI_TOUCH_VDD "L4B"
#define PAGANI_TOUCH_VDD_MV 1800
#define PAGANI_TOUCH_NO_VCC 1
// L4B is a fixed 1.8 V pm_humu_l4 rail with no cmd-db resource name, so it has
// no VRM address to look up and is already enabled at PMIC default.
// Set to 1 only if a board revision is found to use a programmable RPMh rail.
#define PAGANI_TOUCH_HAS_VRM 0

// --- RPMh resources, needed only when PAGANI_TOUCH_HAS_VRM is 1 ---
// The 13T values are kept as documented defaults so the conditional branch
// still compiles; they are NOT valid for this board.
//
// Ace6 runtime DT: apps DRV-2 lives in cmd-db at 0x81c60000. L4B
// (regulator-pm-humu-l4) has no qcom,resource-name, so neither a VRM address
// nor a resource name exists for the touch rail on this board.
#define PAGANI_TOUCH_RSC 0x16520000ULL
#define PAGANI_TOUCH_RSC_SIZE 0x10000
#define PAGANI_TOUCH_VDD_VRM 0x40800
#define PAGANI_TOUCH_VCC_VRM 0x41900
#define PAGANI_TOUCH_VCC "ldob16"
#define PAGANI_TOUCH_VCC_MV 3100

// ---- cmd-db (shared with 13T) ------------------------------------------
#define PAGANI_CMD_DB 0x81c60000ULL
#define PAGANI_CMD_DB_SIZE 0x20000

// ---- SPMI / PMIC -------------------------------------------------------
#define PAGANI_SPMI_CORE 0xc400000ULL
#define PAGANI_SPMI_CNFG 0xc42d000ULL
#define PAGANI_SPMI_OBS 0xc440000ULL
#define PAGANI_SPMI_EE 0

// ---- buttons -----------------------------------------------------------
// pmic_resin / pmic_pwrkey / gpio-keys are present; PPIDs inherited from the
// shared PM8550-family topology. Unverified against this board's keymap.
#define PAGANI_PON_PPID 0x013
#define PAGANI_GPIO_PPID 0x18d
#define PAGANI_PON_POWER 0x80
#define PAGANI_PON_VOLUME_UP 0x40
#define PAGANI_GPIO_VOLUME_DOWN 1

// ---- display -----------------------------------------------------------
// qcom,mdss_dsi_ctrl0@ae94000 reg = <0xae94000 0x1000 ...>
#define PAGANI_DSI_BASE 0xae94000ULL                     // same as 13T
#define PAGANI_DSI_PHY 0xae95000ULL

// DIFF: brightness range. Ace6 declares bl-max-level = 0xffe (4094) and
// oplus normal max 0xdbb (3515) with default 0x666 (1638).
// 13T used max 0xdbb / default 0x599.
#define PAGANI_BRIGHTNESS_MAX 0xffe
#define PAGANI_BRIGHTNESS_NORMAL_MAX 0xdbb
#define PAGANI_BRIGHTNESS_DEFAULT 0x666

// Panel: AA605_P_7_A0020 (tianma). 13T used AA594_P_7_A0020 — different panel.
#define PAGANI_PANEL_NAME "AA605 P 7 A0020 dsc cmd mode panel"
#define PAGANI_PANEL_W 1272
#define PAGANI_PANEL_H 2800
#define PAGANI_PANEL_REFRESH 120
#define PAGANI_PANEL_RESET_GPIO 98                       // tlmm 0x62
#define PAGANI_PANEL_TE_GPIO 86                          // tlmm 0x56

// ---- GPU ---------------------------------------------------------------
// Adreno830v2. No UniXNU driver exists; these are recorded for reference only.
#define PAGANI_GPU_BASE 0x3d00000ULL
#define PAGANI_GPU_CHIPID 0x44050001
#define PAGANI_GMU_BASE 0x3d37000ULL
#define PAGANI_GMU_HFI_IRQ 304                           // 0x130
#define PAGANI_GMU_IRQ 305                               // 0x131
#define PAGANI_GPU_SMMU_BASE 0x3da0000ULL
#define PAGANI_GPU_QTB 0x3de8000ULL
#define PAGANI_GPU_IOVA_WIDTH 0x31                       // 49 bit, measured

// ---- storage -----------------------------------------------------------
#define PAGANI_UFS_BASE 0x1d84000ULL                     // reg <0x1d84000 0x3000 ...>
#define PAGANI_UFS_ICE 0x1d88000ULL
#define PAGANI_UFS_MCQ_SQD 0x1da5000ULL
#define PAGANI_UFS_MCQ_VS 0x1da4000ULL
#define PAGANI_UFS_IRQ 0x109                             // 265
#define PAGANI_UFS_SID 0x60                              // iommus = <&apps_smmu 0x60 0>
#define PAGANI_UFS_RESET_GPIO 215                        // tlmm 0xd7
#define PAGANI_UFS_LANES 2

// ---- memory ------------------------------------------------------------
// Two usable high windows, both measured:
//   base 0x980000000 size 0x180000000 (6 GiB)  -- identical to the 13T node
//   base 0x8c0000000 size 0x0c0000000 (3 GiB)  -- additional, 13T lacks this
#define PAGANI_DRAM_SIZE 0x300000000ULL                  // 12 GiB physical
#define PAGANI_MEM_WINDOW_A_BASE 0x980000000ULL
#define PAGANI_MEM_WINDOW_A_SIZE 0x180000000ULL
#define PAGANI_MEM_WINDOW_B_BASE 0x8c0000000ULL
#define PAGANI_MEM_WINDOW_B_SIZE 0x0c0000000ULL
#define PAGANI_OWNED_WINDOW 0x100000000ULL               // keep 4 GiB until requalified

// ---- reserved memory (selected) ----------------------------------------
#define PAGANI_SPLASH_BASE 0xfc800000ULL
#define PAGANI_SPLASH_SIZE 0x2b00000ULL                  // 43 MiB
#define PAGANI_GPU_MICROCODE_BASE 0x9b09a000ULL
#define PAGANI_GPU_MICROCODE_SIZE 0x2000ULL              // 8 KiB only
#define PAGANI_APP_SMMU_BASE 0x15000000ULL
#define PAGANI_APP_SMMU_SIZE 0x100000ULL

// ---- partition ---------------------------------------------------------
// GPT uses 4096-byte logical sectors; entry 15 is userdata.
#define PAGANI_USERDATA_FIRST_LBA 4602604ULL
#define PAGANI_USERDATA_LAST_LBA 61655034ULL
#define PAGANI_LBA_BYTES 4096
