# UniXNU 移植差异报告：Ace 6 (PLQ110) vs OnePlus 13T (PKX110)

对比对象：
- **A** = 本机运行时合并 DT `fdt_dts/runtime_merged.dts`（1401819 B DTB → 2137271 B dts），
  来源 `/sys/firmware/fdt`
- **B** = 上游 `IssacL04/UniXNU-sm8750-common` @ `dts/sm8750.dtsi`（4191 B，129 行）
  与 `IssacL04/UniXNU-Pagani_SM8750` @ `dts/oneplus13t.dts`（648 B，23 行）

## 结论摘要

上游 `sm8750.dtsi` 只描述 8 个节点，全部 `status = "disabled"`，是为 UniXNU 自建
最小设备树准备的**骨架**，不是对 Qualcomm 完整 DTS 的移植。它的价值在于给出准确
的寄存器基址与中断号，其余一律要自己写。

| 类别 | 数量 | 说明 |
| --- | --- | --- |
| 上游已给且**地址完全一致** | 8/8 | 可直接复用 |
| 上游未给但本机 DT 已有 | 数百 | 需按需裁剪 |
| 必须重写的板级专属 | 6 类 | 面板、触控、RAM、分区、SMMU 配额、ADT |

## 一、上游节点逐条核对（8/8 全部吻合）

| 上游 label | 本机实际节点 | 上游 reg | 本机 reg | 判定 |
| --- | --- | --- | --- | --- |
| `gic` | `intc: interrupt-controller@16000000` | `0x16000000+0x10000`, `0x16080000+0x200000` | 同 | ✅ |
| `gcc` | `clock-controller@100000` | `0x100000+0x1f4200` | 同 | ✅ |
| `ufs` | `ufshc_mem: ufshc@1d84000` | `0x1d84000+0x3000` | 同（另有 ice/mcq 段） | ✅ |
| `smmu` | `apps_smmu: apps-smmu@15000000` | `0x15000000+0x100000` | 同 | ✅ |
| `gpu` | `msm_gpu: qcom,kgsl-3d0@3d00000` | 4 段 | **7 段**（多 3 个 QDSS 段） | ⚠️ 需补 |
| `gmu` | `gmu: qcom,gmu@3d37000` | `0x3d37000+0x68000`, `0x3d40000+0x10000` | 同 | ✅ |
| `gpucc` | `gpucc: clock-controller@3d90000` | `0x3d90000+0x9800` | 同 | ✅ |
| `gpu_smmu` | `kgsl_smmu: kgsl-smmu@3da0000` | `0x3da0000+0x40000` | 同 | ✅ |

GPU 的 QDSS 附加段（上游缺）：
```
+ 0x10900000 0x80000   (qdss_gfx)
+ 0x10048000 0x8000    (qdss_etr)
+ 0x10b05000 0x1000    (qdss_tmc)
```
`reg-names` 上游只给 4 项，本机为 7 项。UniXNU 只需前 4 项（
`kgsl_3d0_reg_memory`, `rscc`, `cx_dbgc`, `cx_misc`），QDSS 段可不带。

## 二、13T 的 DTS 与本机的高度重合

13T 的 `dts/oneplus13t.dts` 只有 23 行，其中内存节点：

```
memory@980000000 {
    reg = <9 0x80000000 1 0x80000000>;   // base 0x980000000, size 0x180000000 (6 GiB)
}
```

本机 `memory/reg` 第 11 段：`base 0x980000000  size 0x180000000  6144.0 MiB`

**完全一致**。同一 SoC 的 LPDDR 高地址段布局相同，这行可以照抄。

13T 的 GPU 标识：`qcom,chipid = <0x44050001>`、`qcom,gpu-model = "Adreno830v2"`。
本机 `kgsl` 报 `Adreno830v2`，DT 内同样是 `chipid = 0x44050001`。**同一颗 GPU**。

## 三、本机内存布局（16 段，合计 11426.7 MiB）

| # | base | size | MiB | 用途推断 |
| --- | --- | --- | --- | --- |
| 1 | `0x081960000` | `0xa0000` | 0.6 | 低端碎片 |
| 2 | `0x081a40000` | `0x1c0000` | 1.8 | 低端碎片 |
| 3 | `0x081cf4000` | `0xc000` | 0.05 | 低端碎片 |
| 4 | `0x082460000` | `0x40000` | 0.25 | 低端碎片 |
| 5 | `0x0be360000` | `0x16ac3000` | 362.8 | 主段 A |
| 6 | `0x0d5100000` | `0x2f00000` | 47.0 | 主段 B |
| 7 | `0x0d8600000` | `0x0` | 0 | 空 |
| 8 | `0x0e3720000` | `0x1c0e0000` | 448.9 | 主段 C |
| 9 | `0x0880000000` | `0x2f8fe000` | 761.0 | 高段 A |
| 10 | `0x08b0000000` | `0x7500000` | 117.0 | 高段 B |
| 11 | **`0x0980000000`** | **`0x180000000`** | **6144.0** | **与 13T 同** |
| 12 | `0x08c0000000` | `0xc0000000` | 3072.0 | 高段 C |
| 13 | `0x082800000` | `0x2200000` | 34.0 | 低端 |
| 14 | `0x089d80000` | `0x1c80000` | 28.5 | 低端 |
| 15 | `0x0a3e80000` | `0x14180000` | 321.5 | 中段 |
| 16 | `0x0b81c0000` | `0x5770000` | 87.4 | 中段 |

最高地址 `0xb00000000` = 44 GiB。`MemTotal = 11384616 kB` = 10.85 GiB。

物理 DRAM 12 GiB（`ddr_info=6|8|1|5|12` 末位），其余被 1305.5 MiB
reserved-memory 与 carveout 占用。

> **13T 的 `dram_size = 0x180000000`（6 GiB）对本机偏小**。本机可用高段除
> `0x980000000` 那 6 GiB 外还有 `0x8c0000000` 的 3 GiB。建议
> `update dram_size` 到 `0x400000000`（16 GiB 窗口）或按 DarwinBoot 的实际
> owned 窗口调整，**不要照抄 6 GiB**。

## 四、必须重写的板级项

| 项 | 13T 值 | 本机值 | 差异性质 |
| --- | --- | --- | --- |
| 面板节点 | `PaganiPanel`（6.32" 2640×1216） | `AA605_P_7_A0020`（6.83" 2800×1272） | 完全不同 |
| 面板 DSI 时序 | — | hfp/hbp/hpw = 26/26/2，vfp/vbp/vpw = 56/24/2 | 需照本机 |
| 面板 DSC | — | v18, bpp8/bpc10, slice 636×20, 2 enc | 需照本机 |
| 触控 SE | 13T 自己的 | `qupv3_se4_spi` @ `0xA90000`, IRQ 357 | **须改** |
| 触控 SPI 频率 | — | 19 MHz (controller 50 MHz) | — |
| 触控 IRQ/复位 | — | tlmm 162 / 161 | — |
| UFS SID | — | `apps_smmu 0x60` | 可能同 |
| GPT userdata | `first_lba=44928768`(64 GiB 机) | `first_lba=4602604, last=61655034`(217.64 GiB) | **须改** |
| `[adt.product.string] model` | `PKX110` | `PLQ110` | **须改** |
| `[payload] volume` | `UNIXNU13T` | 建议 `UNIXNUACE6` | **须改** |
| `[firmware.defines] DEVICE_MODEL` | `oneplus13t` | `ace6` | **须改** |

## 五、本机独有的关键寄存器（13T 的 DTS 不含，需自建）

### 触控 SPI
```
qupv3_se4_spi: spi@a90000 {
    compatible = "qcom,spi-geni";
    reg = <0xa90000 0x4000>;
    interrupts = <0 0x165 4>;        // 357
    clocks = <&gcc 0x60>;
    dmas = <&gcc_dma 0 4 1 0x40 2>, <&gcc_dma 1 4 1 0x40 2>;
    spi-max-frequency = <0x2faf080>;  // 50 MHz
};
```

### UFS 完整
```
ufshc@1d84000 {
    reg = <0x1d84000 0x3000 0x1d88000 0x18000 0x1da5000 0x2000 0x1da4000 0x10>;
    interrupts = <0 0x109 4>;         // 265
    iommus = <&apps_smmu 0x60 0>;
    qcom,iommu-dma = "fastmap";
    lanes-per-direction = <2>;
    reset-gpios = <&tlmm 0xd7 1>;     // 215
};
```

### GPU SMMU（唯一 QTB）
```
kgsl-smmu@3da0000 {
    reg = <0x3da0000 0x40000>;
    #iommu-cells = <2>;
    qcom,use-3-lvl-tables;
    qcom,actlr = <0x0 0x3ff 0x32b>;
    gpu_qtb@3de8000 {
        reg = <0x3de8000 0x1000>;
        qcom,stream-id-range = <0x0 0x400>;
        qcom,iova-width = <0x31>;     // 49 位
        qcom,num-qtb-ports = <2>;
    };
};
```

### apps-smmu QTB 配额
| QTB | 基址 | SID 范围 | iova |
| --- | --- | --- | --- |
| `anoc_1_qtb` | `0x16f2000` | `0x000`+`0x400` | 0x24 |
| `anoc_2_qtb` | `0x171b000` | `0x400`+`0x400` | 0x24 |
| `nsp_qtb` | `0x7d3000` | `0xc00`+`0x400` | 0x20 |
| `lpass_qtb` | `0x7b3000` | `0x1000`+`0x400` | 0x20 |
| `cam_hf_qtb` | `0x17d2000` | `0x1c00`+`0x400` | 0x24 |

### Adreno 830 GMU
```
gmu@3d37000 {
    reg = <0x3d37000 0x68000 0x3d40000 0x10000>;
    interrupts = <0 0x130 4 0 0x131 4>;   // hfi=304, gmu=305
    qcom,iommu-dma = "disabled";
    iommus = <&kgsl_smmu 0x5 0>;
    qcom,gmu-freq-table = <0x1dcd6500 0x40 0x26be3680 0x80>;  // 500MHz/2, 650MHz/4
};
```
`zap-shader { memory-region = <&gpu_microcode_mem>; }` →
`gpu_microcode_region@9b09a000`，**仅 0x2000（8 KiB）**。

## 六、分区（GPT 为 4096 B 逻辑扇区，32 项）

`disk GUID 609d71dd-b5cb-f755-5883-9b4397a67208`，`first_usable=6`，
`last_usable=61655034`，`backup_lba=61655039`。

| # | name | first_lba | last_lba | size |
| --- | --- | --- | --- | --- |
| 13 | `hybridswap` | 123628 | 385771 | 1.00 GiB |
| 14 | `super` | 385772 | 4602603 | 16.09 GiB |
| 15 | `userdata` | **4602604** | **61655034** | **217.64 GiB** |

`userdata` UUID `eff62697-59e0-6ec7-fa06-cd5720c1d0e5`，
type `1b81e7e6-f50d-419b-a739-2aeef8da3335`。

> 13T 的 `[userdata] first_lba = 44928768 / last_lba = 61705983` 是 64 GiB 机器的
> 值。本机必须换成 `4602604` / `61655034` 才不越界。

## 七、可直接复用 / 必须重写 清单

### 直接复用（改 `-D` 与 identity 即可）
- `sm8750.dtsi` 全部 8 个节点的 `reg` / `interrupts`
- `memory@980000000` 那一段（6 GiB，完全同）
- CPU 拓扑：cluster0 = cpu0-5，cluster1 = cpu6-7，8 核全 `okay`
- `kgsl-3d0` 前 4 个 reg 段与 `chipid = 0x44050001`
- `gmu@3d37000` 全部
- `gpucc@3d90000`、`kgsl-smmu@3da0000`
- `apps-smmu@15000000` 与 5 个 QTB 配额
- `ufshc@1d84000`（含 SID 0x60）
- `board.toml` 的整体结构、`[kernel.prelink]` 的 roots 列表、
  `[payload] kernel_args` 的绝大多数选项

### 必须重写
- `dts/<board>.dts`：`model` / `compatible` / 内存窗口（按本机 12 GiB 重算）
- 面板组件（`PaganiPanel` 的替代品），全部 DSI 时序与 DSC 参数
- 触控组件（`PaganiInput` 的替代品），SE 基址换成 `0xA90000`
- `[userdata]` 的 LBA
- `[adt.product.string] model = "PLQ110"`
- `[firmware]` 的 dsc / patch / defines
- `[payload] volume`、`title`

### 完全无解
- Adreno 830 GPU 加速（13T 亦为 `planned`）
- 13T 的 `PaganiPanel` / `PaganiInput` 驱动代码不可直接复用，只能参考结构

## 八、立即可做的验证

```sh
# 1. 确认 13T 的 sm8750.dtsi 能在本机 DT 上编译通过
#    （只验证语法与地址不冲突，不做启动）
./unixnu defconfig oneplus13t_defconfig -D ../UniXNU-pagani
./unixnu status --json

# 2. 对比本机 runtime DT 与 13T 编译产物
dtc -I dtb -O dts out/oneplus13t/.../dtb | head -50
```
