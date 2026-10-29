# 移植进度评估

目标：在这台 OnePlus Ace 6（PLQ110）上运行 iOS。

## 当前结论

**整条链路里，软件侧的准备大约完成了 15%。真正的阻塞点不是代码，是三件尚未发生的事：编译、刷入、第一次启动。**

现在已经做到"不需要再猜任何硬件数值"。剩下的全部是工程实现和真机迭代。

## 完整链路拆解

从加电到 iOS 桌面，Ace 6 需要这 12 层全部打通：

| # | 层 | 状态 | 说明 |
| --- | --- | --- | --- |
| 1 | **解锁 BL + 可刷写** | ✅ **已完成** | KSU root 在位，`verifiedbootstate=orange` 实测 |
| 2 | **原厂固件提取** | ✅ **已完成** | boot/init_boot/vendor_boot/dtbo 全部 dump |
| 3 | **设备树逆向** | ✅ **已完成** | 运行时 merged DT 1401819 B 已反编译 |
| 4 | **板级常量** | ✅ **已完成** | 89 个宏，全部有实测来源 |
| 5 | **UEFI 固件包** | ⬜ **未开始** | 需从 paganiPkg 派生 ace6Pkg |
| 6 | **BootShim 注入** | ⬜ **未开始** | 需按本机 ABL 计算基址 |
| 7 | **UniXNU 引导器** | 🟡 **源码已有** | 主仓可直接用，未编译 |
| 8 | **XNU 内核编译** | ⬜ **未开始** | 需 macOS + IPSW，未做过 |
| 9 | **prelink 合并** | ⬜ **未开始** | 工具就绪，未跑过 |
| 10 | **驱动加载** | 🟡 **代码已移植** | 未编译、未上机 |
| 11 | **存储/显示/输入** | 🟡 **参数就绪** | 驱动未验证 |
| 12 | **GPU 加速** | ⬜ **无解** | Adreno 830 无任何实现 |

## 逐项细节

### 已完成（1-4）

第 1 层你早就做好了。第 2-4 层这一轮全部完成，产出：

- `runtime_merged.dts` 2.1 MB，最权威的设备树
- `ace6-hardware.md` 362 行，含 64 个 reserved-memory 区域、16 段内存映射
- `PaganiPeripherals.h` 89 个宏，每个都标注了实测来源
- `board.toml` / `ace6.dts` / `ace6_defconfig`，静态验证全通过

这部分的价值在于：**后续任何环节出问题，都有唯一可信的参照，不需要回原厂固件反复挖。**

### 第 5 层：UEFI 固件（下一步的起点）

刚发现 `uefi-sheng` 里已有 **`paganiPkg`**，是 SM8750 的完整 UEFI 平台包：

```
Platforms/OnePlus/paganiPkg/
  pagani.dsc          2359 B  平台定义
  pagani.fdf          4534 B  固件布局
  DeviceBuild.py      8081 B  构建脚本
  Include/APRIORI.inc 5511 B  驱动加载顺序
  Include/DXE.inc     5468 B  DXE 阶段驱动
  Include/RAW.inc    18739 B  原始二进制
  Library/MemoryMapLib/MemoryMapLib.c   4643 B  内存映射
  Library/ConfigurationMapLib/...      1854 B
```

**但它是给 13s（CPH2723, board 24875, 1216×2640）的，不是 13T，也不是 Ace 6。**

内存映射对比结果：40 条里 4 条完全一致、14 条部分匹配、9 条无对应。

必须改的关键项：

| 项 | pagani (13s) | Ace 6 实测 | 风险 |
| --- | --- | --- | --- |
| `DXE_Heap` 大小 | `0x19CA0000` | **`0x16AC3000`** | **溢出 49.9 MiB，覆盖 hwfence 共享内存** |
| `FrameBuffer` | 1216×2640 | **1272×2800** | 显示错位 |
| `SmbiosSystemModel` | `13s` | `Ace 6` | 仅标识 |
| `RetailSku` | `CPH2723` | `PLQ110` | 仅标识 |
| `BoardModel` | `24875` | `24851` | 仅标识 |
| `Display_Demura` | `0xA3500000+0x2C80000` | 运行时由 XBL 填 | 需实测 |

`DXE_Heap` 那条最危险：pagani 的值覆盖到 `0xD8000000`，而 Ace 6 的可用内存在 `0xD4E23000` 就结束了，`0xD4E23000`–`0xD5103000` 是 `hwfence-shmem`（2.9 MiB），后面 `0xD5100000` 起才是下一段可用内存。照抄会让 UEFI 堆写进硬件 fence 共享区。

### 第 6 层：BootShim

需要按本机 ABL 计算嵌入基址。`uefi-sheng` 的 `BootShim/` 目录有现成实现，但基址与大小要按 Ace 6 的 `boot_b`（100663296 B，kernel 36866560 B）重新算。

### 第 7-9 层：内核

这是最大的一段未知。需要：

1. 一台 macOS（或 Hackintosh / 云 Mac）编译 XNU
2. iPadOS 17.7.11 (21H461) 的 IPSW
3. 跑 `./unixnu build kernel` → `prelink` → `payload` → `bootimg`

13T 的记录显示单个 build 的完整链接 + prelink 需要 **17-21 分钟**，而它迭代了 63 个 build 才让触控可用。这台机器还没跑过第 1 个 build。

### 第 12 层：GPU

Adreno 830（chipid `0x44050001`）。UniXNU 只有 Adreno 740 驱动。13T 用同一颗 830，它的 GPU 状态到最新一版仍是 `planned`。

**这意味着即使其他全部打通，iOS 也只能软件渲染。** 能进系统、能触摸，但动画会卡。这是整个项目里唯一没有已知解法的一环。

## 现实的时间与工作量估计

以 13T 为参照（63 个 build、每轮 17-21 分钟编译 + 人工上机验证）：

| 里程碑 | 13T 用了多久 | Ace 6 预估 |
| --- | --- | --- |
| 编译出第一个能启动的 boot.img | build 48-49 | 2-4 周（含环境搭建） |
| 进锁屏 | build 50 | +1-2 周 |
| 触控可用 | build 57 | +3-6 周 |
| 显示正常 | 尚未完全达标 | 未知 |
| GPU 加速 | 未实现 | 无解 |

前提是：你能搞定 macOS 编译环境，且愿意每轮手动 `fastboot boot` 验证。

## 现在最该做的三件事

按依赖顺序，不能跳：

**第一件：派生 ace6Pkg**

从 `paganiPkg` 复制，改上面表格里的 6 项。这是纯手工活，1-2 小时，不需要真机。

**第二件：搭编译环境**

需要 macOS 或 Linux + 交叉工具链。`unixnu` 的构建流程文档在 `unixnu.com/building`。这一步能否跑通决定整个项目是否可行。

**第三件：第一次编译 XNU**

先不追求启动，只求 `./unixnu build kernel` 能过。这一步会暴露真正的技术障碍（补丁冲突、依赖缺失），比继续读源码更有价值。

## 一句话回答"到哪了"

**硬件侦察阶段 100% 完成，软件实现阶段刚开始，物理验证阶段 0%。**

现在这台机器的每一个硬件参数都有实测依据，仓库骨架和驱动移植也已就位。但还没有一行代码在这台设备上跑过。下一次真正有意义的进展，是第一次成功编译出 `boot.img` 并用 `fastboot boot` 看到 UniXNU 的启动画面——那才是从"纸面"跨到"真机"的分界线。
