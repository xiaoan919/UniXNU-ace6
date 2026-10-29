# 驱动可复用性分析

对象：`IssacL04/UniXNU-Pagani-Drivers`（238626 B，14 个驱动 + 2 个 kit），
对照本机实测硬件。

## 结论

**触控可以几乎零成本移植，面板可以低成本移植，其余全部原样可用。**

原因在架构：所有板级数值集中在 `package/PaganiPeripherals.h` 一个 44 行头文件里，
驱动本体只消费宏。把该头文件换成 `PaganiPeripherals.h` 即可。

| 驱动 | 体积 | 可复用度 | 说明 |
| --- | --- | --- | --- |
| `S3910.c` / `S3910.h` | 12901 B | **100% 原样** | 纯协议，零板级依赖 |
| `PaganiPanel` | 6291 B | **~95%** | 只改基址与亮度常量 |
| `PaganiInput` | 20113 B | ~85% | 改 SID 与电源轨两处 |
| `NativeUfs` | 229095 B | 原样 | SID 0x60 由宏传入 |
| `NativeAES` / `NativeKeyStoreEvents` | 38204 B | 原样 | 无板级耦合 |
| `NativeMobileDisplay` | 93602 B | 原样 | 消费面板 DTS |
| `PaganiArmCpu` / `PaganiArmDTPlatform` | 54651 B | 原样 | 8 核拓扑一致 |
| `PaganiDarwinBoot` | 77287 B | 原样 | 加载器 |
| `ShengPlatformPower` | 8313 B | 原样 | PMIC 框架 |
| `BootProgress` / `Stage7Probe` | 68981 B | 原样 | 诊断 |

## 一、S3910 触控：100% 可复用

`S3910.h` 用 `struct s3910_bus` 把 I/O 完全抽象：

```c
struct s3910_bus {
    void *context;
    int (*read)(void *, uint8_t *, size_t);
    int (*write)(void *, const uint8_t *, size_t);
    void (*sleep_ms)(void *, unsigned);
};
```

`S3910.c` 内**不含任何板级常量**——没有 `0xa90000`、没有 `ldob`、没有 GPIO 号。
分辨率也不是硬编码：

```
L197: s->max_x=le16(s->payload+32); s->max_y=le16(s->payload+34);
```

从设备自身读最大值，然后归一化：

```
L231: frame.contact[id].x=(uint16_t)((uint32_t)x*32767/max_x);
```

所以本机 `20352 × 44800` 和 13T 的分辨率不同完全不影响。

**直接原样复制 `S3910.c` + `S3910.h`，一个字节不改。**

## 二、PaganiPanel：只改三个常量

通读 `PaganiPanel.cpp`（111 行）后确认，它**不涉及面板时序、DSC、刷新率**，
只做两件 DSI 事：

| 动作 | DCS 命令 | 位置 |
| --- | --- | --- |
| 亮度 | `0x51` | L49 |
| 灭屏 / 亮屏 | `0x28` / `0x29` | L44 |

板级依赖只有三处：`PAGANI_DSI_BASE`、`PAGANI_BRIGHTNESS_MAX`、`PAGANI_BRIGHTNESS_DEFAULT`。

已确认本机 DSI 基址**与 13T 相同**：

```
qcom,mdss_dsi_ctrl0@ae94000  reg = <0xae94000 0x1000 0xaf0f000 0x04 0xae36000 0x300>
```

需要改的只有亮度范围：

| 常量 | 13T | Ace 6 实测 | 来源 |
| --- | --- | --- | --- |
| `BRIGHTNESS_MAX` | `0xdbb` (3515) | **`0xffe` (4094)** | `qcom,mdss-dsi-bl-max-level` |
| `BRIGHTNESS_NORMAL_MAX` | — | `0xdbb` (3515) | `oplus,dsi-bl-normal-max-level` |
| `BRIGHTNESS_DEFAULT` | `0x599` (1433) | **`0x666` (1638)** | `oplus,dsi-brightness-default-level` |

> 面板名不同：13T 是 **AA594**_P_7_A0020，本机是 **AA605**_P_7_A0020。
> 同代工（`P_7`）、同供应商批次（`A0020`），但型号序列不同，属不同面板。
> `PaganiPanel.cpp` 本身不含面板名，所以不影响编译；差异在 DTS 与驱动初始化序列。

## 三、PaganiInput：两处必须改

`PaganiInput.cpp`（20113 B）的板级耦合点全部通过宏：

| 行 | 耦合点 | 用到的宏 | Ace 6 状态 |
| --- | --- | --- | --- |
| L118-126 | SMMU 映射与 S2CR 配置 | `TOUCH_SMMU`, `TOUCH_SMMU_SID` | **SID 需重新测** |
| L137-179 | SPMI + RPMh 电源轨 | `SPMI_*`, `TOUCH_VDD/VCC*` | **电源轨完全不同** |
| L183-188 | GCC / SE / wrapper 映射 | `GCC_BASE`, `TOUCH_SE`, `TOUCH_WRAPPER` | ✅ 全部相同 |
| L204-206 | SPI 时钟与 CS 延时 | `TOUCH_HZ`, `CS_DELAY`, `WORD_DELAY` | ✅ 全部相同 |
| L208-212 | tlmm 引脚与驱动强度 | `TOUCH_PINS`, `TOUCH_DRIVES`, `TLMM_GPIO` | 引脚同，强度不同 |

### 3.1 SMMU SID：必须重新测

13T 用 `PAGANI_TOUCH_SMMU_SID = 0xa3`。本机**无法从 DT 得出**：

```
Ace6 runtime DT: spi@a90000 有 iommus 属性吗？ False
全机所有 spi 节点带 iommus 的：0 个
```

SM8750 的 GENI SPI 走的是非 SMMU 旁路路径（或由 GENI wrapper 统一代理），
DT 层不声明 per-SE 的 iommus。13T 的 `0xa3` 是它自己实测出来的旁路 SID。

**处置**：`PaganiPeripherals.h` 里保留 `PAGANI_TOUCH_SMMU_SID_UNKNOWN 1` 而非编造数值，
并在驱动里加运行期探测。实测方法见 `docs/touch-bringup.md`。

### 3.2 电源轨：结构不同

13T 用两条 RPMh VRM 轨：

```
PAGANI_TOUCH_VDD "ldob4"   VRM 0x40800  1800 mV
PAGANI_TOUCH_VCC "ldob16"  VRM 0x41900  3100 mV
```

本机触控节点只声明一条**固定 1.8 V** 轨：

```
power,vddi-supply = <&L4B>;      /* phandle 0x401 */
power,vddi_volt   = <0x1b7740 0x1b7740>;
```

`L4B` 的 DT 定义：

```
L4B: regulator-pm-humu-l4 {
    regulator-name = "pm_humu_l4";
    regulator-min-microvolt = <0x1b7740>;
    regulator-max-microvolt = <0x1b7740>;
    qcom,init-voltage = <0x1b7740>;
    qcom,init-mode = <0x04>;
};
```

**固定电压、无 VRM 地址**，说明它由 PMIC 默认状态供电，不需要 RPMh 命令。
3.1 V 那条轨在本机活动节点里不存在（SM8750 的 S3910 只需要 1.8 V VDDI）。

**处置**：`PaganiPeripherals.h` 定义 `PAGANI_TOUCH_NO_VCC 1`；驱动里跳过 VRM 使能，
改为仅确认 `L4B` 已上电（读 PMIC 状态）后直接进入 GPIO 复位序列。

### 3.3 tlmm 引脚：号同、强度不同

引脚号完全一致（`48,49,50,51` + reset `161` + attn `162`），但驱动强度不同：

| 引脚 | 13T drive | Ace 6 实测 drive |
| --- | --- | --- |
| MISO (48) | 2 | **0x06 (6)** |
| MOSI (49) | 1 | **0x04 (4)** |
| CLK (50) | 2 | **0x06 (6)** |
| CS (51) | 1 | **0x04 (4)** |
| RESET (161) | 3 | 3 |
| ATTN (162) | 3 | 3 |

来源：`qupv3_se4_spi_{miso,mosi,clk,cs}_active` 的 `drive-strength` 属性。
已写入 `PAGANI_TOUCH_DRIVES {6, 4, 6, 4, 3, 3}`。

## 四、完全可直接用的（零改动）

这些驱动的板级输入全部来自 DTS 或宏，本机值与 13T 相同或已由宏覆盖：

- `NativeUfs`（229 KB）——SID `0x60` 本机实测一致
- `NativeAES`、`NativeKeyStoreEvents`
- `NativeMobileDisplay`（93 KB）——读 DTS 里的面板节点
- `NativeRamBlock`、`NativeVirtioBlock`
- `PaganiArmCpu`、`PaganiArmDTPlatform`——8 核拓扑一致
- `PaganiDarwinBoot`、`ShengPlatformPower`、`BootProgress`、`Stage7Probe`
- `kits/NativePrelink`、`kits/PaganiVerifiedRoot`

## 五、移植动作清单

1. 复制 `drivers/` 全部到 `UniXNU-ace6/drivers/`
2. 复制 `kits/` 全部
3. `package/PaganiPeripherals.h` → 替换为 `package/PaganiPeripherals.h`（已生成）
4. 全局替换源码里的 `PAGANI_` 前缀为 `PAGANI_`，以及 `Pagani` 类名 → `Ace6`
5. `PaganiInput.cpp` 的 `startTouchPower()` 改为跳过 VRM，只校验 L4B
6. `PaganiInput.cpp` 的 SMMU 段改为运行期探测 SID
7. `PaganiPanel` 用 `PaganiPanel.cpp` 为模板，改三个亮度常量

## 六、未验证项

| 项 | 状态 |
| --- | --- |
| 驱动能否在本机内核上加载 | 未测，需要 XNU 先启动 |
| SMMU SID 实际值 | **未知**，需运行期探测 |
| `L4B` 是否默认已上电 | 未测 |
| AA605 面板初始化序列 | 只有 DT 参数，DCS 序列需从原厂驱动提取 |
| 按钮 PPID | 沿用共享拓扑，未对本机校验 |
