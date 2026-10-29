# 触控 bring-up 流程（S3910 / Ace 6）

本机触控链路已全部实测定位，唯一未知量是 SMMU 旁路 SID。本文给出在真机上把它测出来的方法。

## 已知条件

| 项 | 值 | 来源 |
| --- | --- | --- |
| 芯片 | Synaptics S3910 | `device,chip_name` |
| SE 基址 | `0xA90000`，长 `0x4000` | `spi@a90000` 的 `reg` |
| wrapper | `0xAC0000` | `qupv3_1_geni_se@ac0000` |
| GIC SPI | 357 (`0x165`) | `interrupts = <0 0x165 4>` |
| SPI 时钟 | 19 MHz | `0x121eac0` |
| CS setup | 50 | `spi-cs-setup = <0x32 0x00>` |
| 引脚 | MISO 48 / MOSI 49 / CLK 50 / CS 51 | pinctrl |
| 复位 | 161 (`0xa1`) | `reset-gpio` |
| 中断 | 162 (`0xa2`) | `irq-gpio` |
| 电源 | `L4B` 固定 1.8 V | `power,vddi-supply` |
| 容量 | 18 × 40，20352 × 44800 | `tx-rx-num`, `resolution` |

## 未知量：SMMU SID

13T 用 `0xa3`。本机 DT 里 `spi@a90000` **没有** `iommus` 属性，全机 SPI 节点也都没有。
说明 GENI SPI 在该平台上不经过 apps-smmu 的 per-device SMR，或由 wrapper 静态代理。

`PaganiInput.cpp` L118-126 的用法：

```c
if (!smmu.map(PAGANI_TOUCH_SMMU, PAGANI_TOUCH_SMMU_SIZE)) return -12;
auto plan = planUfsSmmu(PAGANI_TOUCH_SMMU_SID, physical, 2*bytes, read, PAGANI_TOUCH_SMMU_SIZE);
const char *error = configureUfsSmmu(plan, PAGANI_TOUCH_SMMU_SID, read, write);
```

三条可能路径，按优先级：

### 路径 A：不需要 SMMU（优先验证）

若 GENI SPI 用物理地址直通，则 `planUfsSmmu` 可以整个跳过，DMA 直接用物理地址。
**验证方法**：先按"无 SMMU"编译，跑起来看 `s3910_start()` 是否成功。
如果首包就读到正确的 identification header（`a5` 帧），说明不需要。

13T 的记录显示它最初也遇到 `T0 00 header, r=ffffff92`——即首包全零/超时，
最后是通过 **SE DMA + SMMU extent** 修好的。所以更可能需要 SMMU。

### 路径 B：从原厂 Android 侧读实际 SID

设备已解锁且有 KernelSU root，可以直接查：

```sh
# 1. GENI SE 的 DMA 实际走哪条 SID：看 iommu 的 attach 记录
adb shell "su -c 'cat /sys/kernel/debug/iommu/attributes 2>/dev/null | head -40'"

# 2. 从 kgsl/arm-smmu 的 SMR 表反推
adb shell "su -c 'cat /sys/kernel/debug/arm-smmu/15000000.apps-smmu/*' 2>/dev/null | grep -i -A2 cb"

# 3. 直接从触控驱动的 dmesg 里找（OnePlus 原厂驱动会打印 SID）
adb shell "su -c 'dmesg | grep -iE \"smmu|sid|stream.?id|cb=\" | head -30'"

# 4. 从 device tree 的 geni wrapper 找 iommu 域
adb shell "su -c 'find /proc/device-tree -name iommus | head -20'"
```

### 路径 C：扫描 SID

`apps-smmu` 的 CB 数量与 `qcom,actlr` 表已实测：

```
qcom,actlr = <0x1c00 0x00 0x01  0x800 0x02 0x01  0x801 0x00 0x01
             0xc01 0x40 0x303  0xc02 0x20 0x303 ...>
```

SID 高 8 位是 SMMU 实例，低 8 位是 CB 号。13T 的 `0xa3` → SMMU 0xa、CB 3。
可以在 `0xa0`–`0xaf` 范围内逐一试 `planUfsSmmu`，观察哪个能成功读到 `a5` 帧。

## 引脚配置（已实测，可直接照写）

```
qupv3_se4_spi_miso_active  pins=gpio48  func=qup1_se4_l0  drive=0x06  bias-pull-down
qupv3_se4_spi_mosi_active  pins=gpio49  func=qup1_se4_l1  drive=0x04  bias-pull-down
qupv3_se4_spi_clk_active   pins=gpio50  func=qup1_se4_l2  drive=0x06  bias-pull-down
qupv3_se4_spi_cs_active    pins=gpio51  func=qup1_se4_l3  drive=0x04  bias-pull-down
qupv3_se4_spi_sleep        pins=gpio48  func=gpio         drive=0x04  bias-pull-down
```

注意：13T 的 `PAGANI_TOUCH_DRIVES = {2,1,2,1,3,3}` 与本机不同，
本机是 `{6,4,6,4,3,3}`。驱动强度过弱会导致高速 SPI 通信丢包。

## 电源：与 13T 结构不同

13T 走 RPMh VRM 双轨（`ldob4` 1.8 V + `ldob16` 3.1 V）。
本机只有 `L4B` 一条固定 1.8 V 轨，DT 里 `min == max == 0x1b7740`，
且无 `qcom,resource-name`，说明由 PMIC 上电默认值供电，不需要 RPMh 命令。

```c
// Ace6 版本：跳过 VRM，只校验 L4B 已上电
// 原 13T 代码：
//   rpmh_vrm_address(&db, ..., PAGANI_TOUCH_VDD, &addresses[0]);
//   sm8750_rpmh_vrm_enable(&power, addresses[0], 1800, &result);
// Ace6：
//   ACE6_TOUCH_NO_VCC 已定义，跳过第二轨
//   第一轨改为读 PMIC 状态确认，不下发 enable 命令
```

## 上电时序（实测）

```
power-on-state   = 1
reset-on-state   = 0
irq-on-state     = 0
reset-active-ms  = 10      复位拉低保持 10 ms
reset-delay-ms   = 80      释放后等 80 ms
power-delay-ms   = 200     上电后等 200 ms
```

对比 13T：`reset-delay-ms` 是 80，`power-delay-ms` 是 200，`reset-active-ms` 是 10
——**三项完全一致**。只有驱动强度与电源轨不同。

## 验收标准

按 13T 的记录，判断顺序应当是：

1. `s3910_start()` 返回 0，读到有效的 identification header
2. `s3910_poll()` 返回 1 且 `contact[i].active` 在按下时非零、抬起时归零
3. `s3910_hid_report()` 产生 62 字节报告，`IOHIDDevice` 提交返回 `r=0`
4. 锁屏能滑动

13T 在 build-56 达到第 3 步（`r=0, observed n=430`，接触掩码正确），
build-57 才让 UI 触摸真正可用。差别在于**触控描述符**与**事件驱动匹配**，
不在传输层。若本机传输层先通了但 UI 无响应，应查这两处而不是 SPI。
