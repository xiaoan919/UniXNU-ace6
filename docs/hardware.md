# OnePlus Ace 6 (PLQ110 / ktm-g_ti) 硬件移植基线

从原厂 `dtbo.img`（sha256 `fb60c2f0…`，25165824 字节）实测提取。所有数值均来自
解包后的 DTB，不含推测。供 `UniXNU-<device>` 设备仓库的 `board.toml` / `dts/` 使用。

## 身份

| 项 | 值 | 来源 |
| --- | --- | --- |
| 型号 | `PLQ110` | `ro.product.model` |
| 设备名 | `OP6113L1` | `ro.product.device` |
| 项目号 | `24851` | `oplusboot.prjname` |
| 板级 project | `0x6113` | DTBO `platform_support_project` |
| 板型 model | `Sun MTP,ktm-g_ti` | `/proc/device-tree/model` |
| compatible | `qcom,sun-mtp` `qcom,sun` `qcom,sunp-mtp` `qcom,sunp` `qcom,mtp` | DT |
| 板级 ID | `0x50008 0x00` | DTBO `qcom,board-id` |
| msm-id | `0x26a`(618) / `0x27f`(639)，各带 `0x10000`/`0x20000` 域 | DTBO |
| SoC | SM8750 (`ro.board.platform=sun`) | getprop |
| GPU | Adreno830v2 | `/sys/class/kgsl/kgsl-3d0/gpu_model` |

## DTBO 结构

`DT_TABLE_MAGIC 0xd7b7ab1e`，`header_size=32`，`entry_size=32`，`count=10`，
`entries_offset=32`，`page_size=4096`，`version=0`，`total_size=10611178`。
每个 overlay 约 `0x103000`（≈1.03 MiB），10 个全部落在同一 `board-id=0x50008`。

## 面板：AA605_P_7_A0020（tianma）

节点 `qcom,mdss_dsi_panel_AA605_P_7_A0020_dsc_cmd`，`phandle = 0x218`。

| 属性 | 值 | 十进制 |
| --- | --- | --- |
| panel-width | `0x4f8` | 1272 |
| panel-height | `0xaf0` | 2800 |
| 帧率档 | `0x78` / `0x5a` / `0x3c` / `0x90` / `0xa5` | 120 / 90 / 60 / 144 / 165 |
| clockrate @120Hz | `0x41fb7ac0` | 1107.0 MHz |
| clockrate @144Hz | `0x5140c800` | 1363.2 MHz |
| h-front-porch | `0x1a` | 26 |
| h-back-porch | `0x1a` | 26 |
| h-pulse-width | `0x02` | 2 |
| h-sync-skew | `0x00` | 0 |
| v-front-porch | `0x38` | 56 |
| v-back-porch | `0x18` | 24 |
| v-pulse-width | `0x02` | 2 |
| bpp | `0x1e` | 30 |
| panel-type | `dsi_cmd_mode` | — |
| traffic-mode | `burst_mode` | — |
| lane-map | `lane_map_0123` | 4 lane |
| phy-timings @120 | `[00 24 0a 0a 1a 18 0a 0a 09 02 04 00 1e 0f]` | — |
| phy-timings @144 | `[00 2c 0c 0c 1d 1a 0c 0c 0b 02 04 00 24 11]` | — |
| 物理尺寸 | `0x47` × `0x9d` mm | 71 × 157 |

### DSC

`compression-mode = "dsc"`，`version = 0x12`(18)，`bit-per-pixel = 8`，
`bit-per-component = 10`，`slice-width = 0x27c`(636)，`slice-height = 0x14`(20)，
`slice-per-pkt = 2`，`encoders = 2`（双编码器）。

### 电源时序

`oplus,panel-power-on-sequence = "1","gpio1","1","vddio","3","gpio2","3","vci","10"`
`oplus,panel-power-off-sequence = "1","vci","3","gpio2","3","vddio","1","gpio1","1"`
电压 `vddi`/`vddr` 目标 `0x1b7740`(1800000) 典型，`0x1c3a90`(1850000)/`0x1dc130`(1950000) 上下界。

| 信号 | 绑定 |
| --- | --- |
| gpio1 | `&pm8550ve_g_gpios` idx `4` |
| gpio2 | `&pm8550ve_f_gpios` idx `7` |
| platform-reset-gpio | `&tlmm` pin `0x62`(98) |
| platform-te-gpio | `&tlmm` pin `0x56`(86) |
| 亮度 | `bl_ctrl_dcs`，max `0xffe`(4094)，default `0x666`(1638) |
| 背光模式 | `qcom,mdss-bl-high2bit`，`bl-inverted-dbv` |

## 触控：Synaptics S3910（与 OnePlus 13T 同型号）

**活跃节点**（`compatible = "synaptics-tcm"`，`status = "okay"`，`phandle = 0x8b`）：

| 属性 | 值 |
| --- | --- |
| chip_name | `synaptics-s3910` |
| resolution | `<0x4f80 0xaf00>` = 20352 × 44800 |
| irq-gpio / interrupts | `&tlmm` `0xa2`(162)，flags `0x2008` |
| 挂在 | `&hbp_spi_bus0` → `qupv3_se4_spi` |
| vddi-supply | `&L4B`，`0x1b7740`–`0x1b7740`（1.8 V 固定） |
| pinctrl-0 | `ts_int_active` + `ts_reset_active` |
| pinctrl-3 (resume) | `qupv3_se4_spi_miso/mosi/clk/cs_active` |
| 特性 | `create_with_power_on_support` |

**HBP 传输层**（`synaptics,tcm-spi-hbp`，`status = "disabled"`，仅作备用）：

| 属性 | 值 | 换算 |
| --- | --- | --- |
| reg | `0x00` | CS0 |
| spi-max-frequency | `0x121eac0` | 19.0 MHz |
| qcom,rt | — | 实时优先级 |
| chip-name | `S3910` | — |
| panel-coords | `<0x4f80 0xaf00>` | 20352 × 44800 |
| tx-rx-num | `<0x12 0x28>` | **18 × 40** |
| panel_type | `0x07` | — |
| firmware_name | `AA605` | — |
| platform_support_project | `0x6113` | = OP6113 |
| reset-gpio | `&tlmm` `0xa1`(161)，flags `0x01` |
| irq-gpio | `&tlmm` `0xa2`(162)，flags `0x2008` |
| avdd-gpio | `&pm8550ve_i_gpios` idx `3`，flags `0x01` |
| vdd-supply | `&L4B` | — |
| reset-active-ms | `0x0a` | 10 ms |
| reset-delay-ms | `0x50` | 80 ms |
| power-delay-ms | `0xc8` | 200 ms |
| spi-mode | `0x00` | mode 0 |
| spi-byte-delay-us | `0x00` | 0 |
| spi-block-delay-us | `0x00` | 0 |
| power-on-state | `0x01` | — |
| irq-on-state | `0x00` | — |
| reset-on-state | `0x00` | — |

`hbp_spi_bus0`：`compatible = "oplus,hbp_spi_bus"`，`spi-cs-setup = <0x32 0x00>`（50），
`bus,rx-dummy = 0`，`bus,tx-dummy = 0`，`phandle = 0x8a`。

### 与 OnePlus 13T 的关键差异

13T 的记录写明其触控是 S3910，且排错最终收敛到 SPI 传输层。本机同为 S3910，
`tx-rx = 18×40`，`spi-max-frequency = 19 MHz`，mode 0。**但 SE 索引不同**：
本机触控走 `qupv3_se4_spi`（`se4`），而 13T 的 `PaganiInput` 绑定的是它自己的 SE。
移植时必须改 `QcomGeniSpi` 的 SE 基址、对应 DMA 通道与 `pinctrl` 引用。

## 触控 SPI 控制器：qupv3_se4_spi（已从运行时 DTB 取得）

节点 `qupv3_se4_spi: spi@a90000`，`phandle = 0x524`。

| 属性 | 值 | 说明 |
| --- | --- | --- |
| compatible | `qcom,spi-geni` | GENI SPI |
| reg | `<0xa90000 0x4000>` | **基址 0xA90000，长度 16 KiB** |
| reg-names | `se_phys` | — |
| interrupts | `<0x00 0x165 0x04>` | SPI = 357，SPI 类型 |
| clock-names | `se-clk` | — |
| clocks | `<0x34 0x60>` | GCC_SE4 |
| interconnects | `qup-core` / `qup-config` / `qup-memory` | — |
| dmas | `<0x276 0x00 0x04 0x01 0x40 0x02>`, `<0x276 0x01 0x04 0x01 0x40 0x02>` | tx / rx |
| dma-names | `tx`, `rx` | — |
| spi-max-frequency | `0x2faf080` | 50 MHz（控制器上限） |
| status | `ok` | — |

所属 SE wrapper：`qcom,qupv3_1_geni_se@ac0000`（`qupv3_1`）。
同组 `qupv3_se4_i2c`（`i2c@a90000`）在 overlay 中被 `status = "disabled"`，
即该 SE 由 SPI 独占。

## 运行时 DTB（最权威来源）

`/sys/firmware/fdt` → 1401819 字节，反编译 2137271 字节。
根节点：

```
model = "Qualcomm Technologies, Inc. Sun MTP,ktm-g_ti";
compatible = "qcom,sun-mtp", "qcom,sun", "qcom,sunp-mtp", "qcom,sunp", "qcom,mtp";
qcom,msm-id = <0x100026a 0x20000 0x100026a 0x10000 0x100027f 0x10000 0x100027f 0x20000>;
qcom,board-id = <0x50008 0x00>;
```

`/sys/devices/soc0/soc_id = 618`（`0x26a`），`machine = SUN`，`revision = 2.0`，
`platform_version = 65536`。

`vendor_boot` 内嵌的是 **13 个 DTB 的合集**（6821771 字节），分别对应：
Kera(`0x293`) / KeraP(`0x2ae`) / Sun(`0x26a`) / SunP(`0x27f`) / Tuna(`0x28f`) /
Tuna-7(`0x2a9`) / TunaP(`0x2b6`)，其中 Sun/SunP 各有 v1/v2 与 Alt.Thermal 变体。
本机实际选中 `Sun`（`0x26a`）。

## 运行时 bootargs（完整）

来自 merged DT，注意与 `/proc/cmdline` 的差异：merged DT 内为 base 串，
aboot 追加的 `oplusboot.*` 在 `/proc/cmdline` 中。关键项：

```
buildvariant=user
oplusboot.prjname=24851          oplusboot.sku=7
oplusboot.serialno=c28b95a3      oplus_region=151
oplusboot.rpmb_enabled=1         oplusboot.secure_type=3
oplusboot.verifiedbootstate=orange
oplus.avbkeysha256=1799b97e6a3d4daa0e76710d7492aaccc97ea12d9673fc58e72ce7d3ab16d8c6
device_info.ddr_info=6|8|1|5|12
msm_drm.dsi_display0=qcom,mdss_dsi_panel_AA605_P_7_A0020_dsc_cmd:PanelID-0x009A0501:PanelSN-0x0E09170F13171008
oplus_bsp_tp_custom.dsi_display0=mdss_dsi_panel_AA605_P_7_A0020_dsc_cmd:synaptics-s3910
touch_panel0=0:synaptics-s3910:tianma
msm_kgsl.preempt_enable=0        console=ttynull
qcom_geni_serial.con_enabled=0
```

`PanelID-0x009A0501` / `PanelSN-0x0E09170F13171008` 是这块面板的实物标识。

> 注意：触控 HBP 节点里的 `platform_support_project_commandline` 写的是
> `..._P_3_A0020_...`，但这台机器实际用的是 `P_7`。判断面板型号应以
> bootargs 的 `dsi_display0` 和 DT 内实际存在的节点为准，不能照抄该字段。

## CPU 拓扑

8 核两组，`cpu-map`：

| cluster | core | cpu phandle |
| --- | --- | --- |
| cluster0（medium，`0x10`–`0x15`） | 0–5 | cpu0–cpu5 |
| cluster1（large，`0x16`–`0x17`） | 0–1 | cpu6–cpu7 |

实测频率：cpu0/cpu2/cpu4 = 3532800 kHz，cpu6/cpu7 = 4320000 kHz。
`soc0/platform_version = 65536`。PSCI 由 `idle-states` 的
`arm,psci-suspend-param` 表述（`MEDIUM_OFF_C4 = 0x04`，`LARGE_OFF_C4` 等）。

## 内存

`memory/reg` 为多段（非单一连续块），首段起点 `0x81960000`，
`MemTotal = 11384616 kB`（≈10.85 GiB）。`ddr-regions` 三段：

```
region0 = <0x00 0x80000000 0x00 0x80000000 ...>
region1 = <0x08 0x80000000 0x01 ...>
region2 = <0x09 0x80000000 0x01 0x80000000 ...>
```

`ddr_device_type = <0x08>`（LPDDR5X）。

## UFS

`ufshc_mem: ufshc@1d84000`，`phandle = 0x7d`。

| 属性 | 值 |
| --- | --- |
| compatible | `qcom,ufshc` |
| reg | `<0x1d84000 0x3000 0x1d88000 0x18000 0x1da5000 0x2000 0x1da4000 0x10>` |
| reg-names | `ufs_mem`, `ice`, `mcq_sqd`, `mcq_vs` |
| interrupts | `<0x00 0x109 0x04>`（265） |
| iommus | `<&apps_smmu 0x60 0x00>` → **SID 0x60** |
| qcom,iommu-dma | `fastmap` |
| lanes-per-direction | `2` |
| freq-table-hz | `100000000`–`403000000` |
| reset-gpios | `&tlmm 0xd7(215)` |
| vcc / vccq max | `1300000` / `1200000` µA |
| qcom,max-cpus | `8` |

设备上 `/dev/block/by-name/sda` 为 UFS LUN0，143 个分区。

## apps-smmu

`apps_smmu: apps-smmu@15000000`，`phandle = 0x7e`。

| 属性 | 值 |
| --- | --- |
| compatible | `qcom,qsmmu-v500` |
| reg | `<0x15000000 0x100000>`（1 MiB） |
| #iommu-cells | `2` |
| qcom,use-3-lvl-tables | 是 |
| qcom,handoff-smrs | `<0x800 0x02>` |
| #global-interrupts | `1` |
| dma-coherent | 是 |

QTB 端口与 stream-id 范围：

| QTB | 基址 | stream-id-range |
| --- | --- | --- |
| `anoc_1_qtb` | `0x16f2000` | `0x000`–`0x3ff` |
| `anoc_2_qtb` | `0x171b000` | `0x400`–`0x7ff` |
| `nsp_qtb` | `0x7d3000` | `0xc00`–`0xfff` |
| `lpass_qtb` | `0x7b3000` | `0x1000`–`0x13ff` |
| `cam_hf_qtb` | `0x17d2000` | `0x1c00`–`0x1fff` |

## reserved-memory（64 个区域，合计 1305.5 MiB）

固定地址（`no-map`）大块：

| 区域 | 基址 | 大小 |
| --- | --- | --- |
| `splash_region` | `0xfc800000` | 43.0 MiB |
| `mpss_region` | `0x8ba00000` | 246.0 MiB |
| `adspslpi_region` | `0x9ee00000` | 80.5 MiB |
| `oem_vm_region` | `0xf7c00000` | 76.0 MiB |
| `dsm_partition_1_region` | `0x84a00000` | 73.0 MiB |
| `trust_ui_vm_region` | `0xf3800000` | 68.0 MiB |
| `qdss_apps_region` | `0x82800000` | 32.0 MiB |
| `cdsp_region` | `0x9d200000` | 25.0 MiB |
| `gunyah_hyp_region` | `0x80000000` | 18.0 MiB |
| `dsm_partition_2_region` | `0x89300000` | 10.5 MiB |
| `llcc_lpi_region` | `0xff800000` | 8.0 MiB |
| `gunyah_hyp_region1` | `0xbd930000` | 8.0 MiB |
| `video_region` | `0x9c300000` | 8.0 MiB |
| `camera_2_region` | `0x9bb00000` | 8.0 MiB |
| `camera_region` | `0x9b300000` | 8.0 MiB |
| `cvp_region` | `0x9cb00000` | 7.0 MiB |
| `tz_merged_region` | `0xd8000000` | 6.0 MiB |
| `cpucp_scandump_region` | `0x82000000` | 3.5 MiB |
| `hwfence-shmem` | `0xd4e23000` | 2.9 MiB |
| `smem_region` | `0x81d00000` | 2.0 MiB |
| `cpucp_pdp_region` | `0x81200000` | 2.0 MiB |
| `spss_region` | `0x9b0a0000` | 1.9 MiB |
| `xbl_ramdump_region` | `0xb8000000` | 1.8 MiB |
| `soccp_region` | `0x9ec00000` | 1.5 MiB |
| `tz_stat_region` | `0x82700000` | 1.0 MiB |
| `global_sync_region` | `0x82600000` | 1.0 MiB |
| `pvm_fw_region` | `0x824a0000` | 1.0 MiB |
| `pdp_ns_shared_region` | `0x81f00000` | 1.0 MiB |
| `q6_adsp_dtb_region` | `0x9ed80000` | 0.5 MiB |
| `q6_cdsp_dtb_region` | `0x9eb00000` | 0.5 MiB |
| `q6_mpss_dtb_region` | `0x9b000000` | 0.5 MiB |
| `pmic_minii_dump_region` | `0x823e0000` | 0.5 MiB |
| `aop_tme_uefi_merged_region` | `0x81c80000` | 0.5 MiB |
| `lost_reg_mem2` | `0x9eb80000` | 0.5 MiB |
| `aop_image_region` | `0x81c00000` | 0.4 MiB |
| `spu_shared@9b2c0000` | `0x9b2c0000` | 0.2 MiB |
| `spu_shared@9b280000` | `0x9b280000` | 0.2 MiB |
| `soccp_sdi_region` | `0x823a0000` | 0.2 MiB |
| `xbl_dtlog_region` | `0x81a00000` | 0.2 MiB |
| `adsp_mhi_region` | `0x82380000` | 0.1 MiB |
| `aop_cmd_db_region` | `0x81c60000` | 0.1 MiB |
| `ipa_fw_region` | `0x9b080000` | 0.1 MiB |
| `botloader_mem` | `0x81ce4000` | 0x10000 |
| `ipa_gsi_region` | `0x9b090000` | 0x6000 |
| `device_info_mem` | `0xa4f00000` | 0x8000 |
| `lost_reg_mem` | `0x9b09c000` | 0x4000 |
| `gpu_microcode_region` | `0x9b09a000` | **0x2000**（8 KiB） |

动态分配（`alloc-ranges = <0 0 0 0xffffffff>`）：

| 区域 | 大小 | 用途 |
| --- | --- | --- |
| `non_secure_display_region` | 200.0 MiB | 显示 |
| `dump_mem_region` | 90.0 MiB | 崩溃转储 |
| `secure_cdsp_region` | 72.0 MiB | cdsp |
| `linux,cma` | 36.0 MiB | CMA |
| `qseecom_ta_region` | 32.0 MiB | TEE |
| `cnss_wlan_region` | 32.0 MiB | Wi-Fi |
| `qseecom_region` | 20.0 MiB | TEE |
| `va_md_mem_region` | 16.0 MiB | — |
| `sp_region` | 16.0 MiB | — |
| `qmc_dma_region` | 16.0 MiB | — |
| `adsp_heap_region` | 12.0 MiB | — |
| `vm_comm_mem_region` | 8.0 MiB | — |
| `cdsp_eva_region` | 4.0 MiB | — |
| `ramoops-region` | 2.2 MiB | pstore |
| `debug_kinfo_region` | 0.0 MiB | — |

`demura_region_0` / `demura_region_1` 的 `reg` 为 `<0 0 0 0>`（空），由 UEFI/XBL
在运行时填入，节点存在但无静态地址。

## 分区实测

| 分区 | 设备 | 字节 | 内容 |
| --- | --- | --- | --- |
| `boot_b` | `/dev/block/sde49` | 100663296 | `ANDROID!` v4，kernel 36866560，ramdisk 0 |
| `init_boot_b` | `/dev/block/sde66` | 8388608 | `ANDROID!` v4，ramdisk 3200887 |
| `vendor_boot_b` | `/dev/block/sde60` | 100663296 | `VNDRBOOT` v4，dtb 6821771，ramdisk 18480607 |
| `dtbo_b` | `/dev/block/sde53` | 25165824 | `DT_TABLE` 10 entries |
| `vbmeta_b` | `/dev/block/sde52` | 65536 | — |
| `vbmeta_system_b` | `/dev/block/sda7` | 65536 | — |

`vendor_boot` header：`page_size = 4096`，`header_size = 2128`，
`dtb_addr = 0x1f00000`。GKI 布局：kernel 在 `boot`，ramdisk 在 `init_boot`。

## 仍缺

| 数据 | 原因 | 取法 |
| --- | --- | --- |
| Adreno 830 GMU 固件 / SMMU stream 分配 | 无任何 UniXNU 支持 | 需逆向 `vulkan.adreno.so` + `kgsl` 侧信息 |
| `demura` 运行时地址 | 由 XBL 动态填 | 需在 Android 侧读 `msm_drm` debugfs |
| PMIC（`pm8550ve`）完整 gpio 映射 | 未逐项导出 | 反编译 dtbo 的 `fragment@` 电源段 |
| 面板初始化命令序列 | 在 dtbo 面板节点内，量大 | 已有原始 `.dts`，按需 grep `qcom,mdss-dsi-on-command` |
