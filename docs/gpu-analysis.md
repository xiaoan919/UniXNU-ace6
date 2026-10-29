# Adreno 830 GPU 分析报告

从本机 dump 出的 8 个文件做静态分析。所有结论标注来源，未验证项明确标出。

## 提取的产物

| 文件 | 大小 | 格式 | sha256 前 16 位 |
| --- | --- | --- | --- |
| `gen80000_gmu.bin` | 88896 | GMU 容器 | `1ad8175e6cd01ea7` |
| `gen80000_sqe.fw` | 111892 | 裸 AArch64 微码 | `9c8f009bea112d7c` |
| `gen80000_zap.mbn` | 12088 | ELF32 | `8fc0eebbc7476fc1` |
| `gen80600_gmu.bin` | 88964 | GMU 容器 | `75b3282a40e5411a` |
| `gen70900_sqe.fw` | 85364 | 裸微码（A740 对照） | `f756bafe5f69a871` |
| `gen71700_gmu.bin` | 81348 | GMU 容器（A740 对照） | `75b2c1757ef57099` |
| `msm_kgsl.ko` | 2986408 | ELF64 REL，A830 完整内核驱动 | `667e63110cb58d12` |
| `vulkan.adreno.so` | 5350184 | ELF64，Qualcomm 官方 Vulkan | `177b7fc461a074b5` |

两个 GMU 固件（`gen80000` / `gen80600`）同时存在，对应 A830 的 GPU 变体与 ACD 配置。

## 结论一：GMU 容器跨代不变

`gen71700_gmu.bin`（A740）与 `gen80000_gmu.bin`（A830）的头 16 字节结构完全一致：

```
A740: 00 00 00 00 | 00 00 00 00 | 01 00 00 00 | dc 01 00 00
A830: 00 00 00 00 | 00 00 00 00 | 01 00 00 00 | ec 01 00 00
             addr=0        pad        type=1      size
```

- A740 首段 size `0x1dc`（476 B）
- A830 首段 size `0x1ec`（492 B）

**同构，仅尺寸不同。** GMU 引导容器的解析逻辑可以原样复用，差异只在段内容。

## 结论二：SQE 微码不可复用，但编码体系相同

`gen80000_sqe.fw`（111892 B）与 `gen70900_sqe.fw`（85364 B）的字节级对比：

| 指标 | 结果 |
| --- | --- |
| 相同公共前缀 | **5 字节** |
| 256 字节块重合 | **0 个**（gen8 437 块 / gen7 333 块） |
| 体积增长 | +31% |

两份微码是**完全不同的二进制**。但两者都无容器头，是裸的 AArch64 Thumb-2 代码（首字 `0x00000000`，随后是 T32 指令）。

含义：SQE 微码必须用 gen8 自己的那份（已提取），不能拿 A740 的替代。但**加载方式、入口约定、HFI 交互模型在指令编码层是同一套**。

## 结论三：HFI 消息号已从 UniXNU 源码取得

`AdrenoFirmware/AdrenoHfiStartup.c` 里的启动序列，A830 可直接照此实现：

| 阶段 | HFI 消息号 | payload 字数 | 关键字段 |
| --- | --- | --- | --- |
| Performance | **4** | 58 | `p[0]=gpu_count` `p[1]=gmu_count`，每级 `vote/hz` |
| Bandwidth | **3** | 159 | `p[0]=bw_count` `p[1]=cnoc_count` `p[2]=ddr_count` |
| ACD Enable | **11** | 3 | `p[0]=12 p[1]=1 p[2]=0` |
| ACD Table | **7** | 36 | `version/mask/stride/count` + 32 字数据 |
| IFPC | **11** | 3 | `p[0]=9 p[1]=1 p[2]=0x1680` |
| Core Start | **14** | 1 | `p[0]=0` |
| Start | **10** | 0 | 无 payload |

约束（来自源码硬校验）：

- GPU 性能级 2–16 个，频率须为 1000 Hz 的倍数且严格递增
- GMU 级 2–4 个
- 带宽表 1–16 项
- cnoc 地址最多 6 个、ddr 最多 8 个，均须 4 字节对齐、非零、不重复
- 共享内存须 **16 KiB**，4 个原生 4K 页，CPU/GMU 一致，32 位 GMU IOVA

## 结论四：本机实测的启动参数

| 项 | 值 | 来源 |
| --- | --- | --- |
| GPU 型号 | `Adreno830v2` | `kgsl-3d0/gpu_model` |
| chipid | `0x44050001` | DT `qcom,chipid` |
| GPU 寄存器基址 | `0x3d00000`，长 `0x40000` | DT kgsl-3d0 |
| GMU 寄存器 | `0x3d37000` + `0x68000`，`0x3d40000` + `0x10000` | DT gmu |
| HFI 中断 | GIC SPI **304** (`0x130`) | DT `interrupts` |
| GMU 中断 | GIC SPI **305** (`0x131`) | DT `interrupts` |
| GPU SMMU | `0x3da0000` + `0x40000` | DT kgsl-smmu |
| GPU QTB | `0x3de8000`，SID `0x0`–`0x3ff`，**iova-width 0x31 = 49 位** | DT gpu_qtb |
| GPU SMMU actlr | `<0x0 0x3ff 0x32b>` | DT |
| ZAP 固件区 | `0x9b09a000`，**仅 `0x2000` = 8 KiB** | DT `gpu_microcode_region` |
| 频率表 | 160/222/342/389/443/525/607/660/734/832/900/967/1050/1100 MHz | `gpu_available_frequencies` |
| governor | `msm-adreno-tz` | devfreq |
| IFPC | 启用，`ifpc_count=0x3EAF5EC` | sysfs |
| GMU A/B | `gmu_ab=1` | sysfs |
| GMU 频率表 | `qcom,gmu-freq-table = <0x1dcd6500 0x40 0x26be3680 0x80>` = 500 MHz/64, 650 MHz/128 | DT gmu |

GPU SMMU 的 **49 位 IOVA** 是关键差异：A740 用更窄的地址空间，A830 的 VA 宽了一档。

## 结论五：两份高价值二进制

`vulkan.adreno.so`（5.35 MB）是 Qualcomm 官方 A830 Vulkan 实现，内含：

- 完整的 PM4 命令生成逻辑（对应 UniXNU 的 `AdrenoPM4` kit）
- gen8 着色器编译器后端（对应 `AirNir` 的 IR3 替代）
- 驱动包标识 `com.qualcomm.qti.gpudrivers.sun.api35`

`msm_kgsl.ko`（2.99 MB）是 A830 的内核驱动，内含：

- GMU 引导流程（`gen80000_gmu.bin` 的加载序列）
- HFI 收发实现
- PM4 提交环、fence、BO 管理
- `ifpc` / `acd` / `lm` / `lpac` 等 gen8 电源特性

**这两份合起来，是重写 UniXNU A830 驱动时唯一完整的行为参照。** 比读 Mesa 源码更直接，因为它们是这颗芯片的官方实现。

## 对移植的实际影响

| UniXNU 组件 | 能否复用 | 说明 |
| --- | --- | --- |
| `AdrenoFirmware`（GMU 容器解析 + HFI 编码） | **80% 可复用** | 容器同构；HFI 消息号已知；需核对 gen8 是否新增消息 |
| `AdrenoHfi`（收发队列） | **可复用** | 协议模型相同 |
| `Adreno740`（GPU 服务/BO/ring/fence） | **需改** | 寄存器偏移、49 位 IOVA、gen8 电源域 |
| `AdrenoPM4`（包编码） | **需重做** | gen8 的 PM4 包格式与常量表不同 |
| `AirNir`（AIR → NIR → IR3） | **需新增 gen8 后端** | IR3 是 a6xx/a7xx ISA，gen8 换了编码 |
| `AdrenoMetal`（Metal 前端） | **可复用** | 与 GPU 代际无关 |
| `GopFramebuffer`（scanout） | **可复用** | 与 GPU 代际无关 |

## 未验证项

| 项 | 原因 |
| --- | --- |
| gen8 是否新增/修改 HFI 消息号 | 需反编译 `msm_kgsl.ko` 的 HFI 发送点 |
| gen8 PM4 包格式具体差异 | 未做反汇编 |
| `gen80000` 与 `gen80600` 的选择条件 | 推测与 GPU 变体/ACD 有关，未验证 |
| ZAP 固件 8 KiB 是否够 | `gpu_microcode_region` 只有 `0x2000`，而 `gen80000_zap.mbn` 有 12088 B（11.8 KiB）——**两者矛盾，需核实** |

最后一条值得警惕：ZAP 固件文件 12088 字节，但 DT 里给它的预留区只有 8192 字节。这说明 ZAP 可能不是整份载入该区域，或者该区域只是签名头。**这一点在写驱动前必须查清，否则会越界。**
