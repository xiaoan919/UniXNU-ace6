# A830 GMU 启动序列（从 msm_kgsl.ko 提取）

来源：`gen8_start`，`msm_kgsl.ko`，`.text` 偏移 `0x4e7e0`，文件偏移 `0x767e0`，4932 字节，1233 条指令。

提取方法：capstone 反汇编 + 解析 `.rela.text` 重定位（148 条）还原真实调用目标，
再回溯 `w1`/`w2` 立即数与寄存器读取，还原 `kgsl_regmap_write(base, reg, val)` 的实参。

## 前提：这是 GMU 寄存器空间的偏移

所有 `reg` 值都是 **GMU 寄存器块内偏移**，基址为：

```
GMU base = 0x3d37000   (DT: gmu reg[0], 长 0x68000)
```

注意不是 GPU 寄存器块（`0x3d00000`）。A740 与 A830 这里的基址相同。

## 第一段：初始化清除（0x4e878 – 0x4e8b8）

```
write 0x00019e, 0xffffffff        reg 0x19e 置全 1（清中断/状态）
write 0x000f08, [x19+0x1bd8]      参数化：来自 device 结构
write 0x000f09, [x19+0x1bdc]      参数化
write 0x00ae10, [x19+0x1bd8]      参数化
write 0x00ae11, [x19+0x1bdc]      参数化
```

`0xf08`/`0xf09` 与 `0xae10`/`0xae11` 成对出现，读取同一对源字段，
说明是两组同构配置（推测为 ACD 的两个实例，对应 `gen80000` 与 `gen80600` 两个 GPU 变体）。

## 第二段：时钟与电源域解锁（0x4e8e4 – 0x4e99c）

```
write 0x00081c, 0x00001000        reg 0x81c bit12 置位
write 0x008e0a, 0x0c000000
write 0x008e0a, 0x0c000000        重复写（等待稳定）
write 0x000f0a, [x19+0x1bd8]
write 0x000f0b, [x19+0x1bdc]
write 0x00ae12, [x19+0x1bd8]
write 0x00ae13, [x19+0x1bdc]
write 0x000e08, 0xfffff000        -0x1000，即清低 12 位（地址对齐）
write 0x000e09, 0x0001ffff
write 0x000e06, 0xfffff000
write 0x000e07, 0x0001ffff
```

`0xe06`–`0xe09` 四连写 + `-0x1000`/`0x1ffff` 是典型的 **地址窗口基址/长度设置**。
`0x81c` 反复被写 `0x1000`/`0x2000`/`0x3000`，是一个**选择器寄存器**：
先选窗口号，再写窗口内容。这是重定位窗口的经典模式。

## 第三段：核心使能（0x4e9c0 – 0x4ea08）

```
write 0x000460, 0x00000001
write 0x000500, 0x00000001        两个独立使能位
write 0x00fc41, 0x00000004
rmw   0x00ff00, 0x0000fc41        read-modify-write，把 0xfc41 写进 0xff00
```

`0xfc41` 与 `0xff00` 的 rmw 组合，推测是**中断路由/屏蔽配置**：先把掩码写进 `0xfc41`，
再用 rmw 把它合并到 `0xff00` 的控制字里。

## 第四段：窗口重定位序列（0x4eaf4 – 0x4ec4c）

这是最长的一段，模式高度规律：

```
write 0x00081c, 0x00002000        选窗口 2
write 0x008700, <val>             写窗口 2 内容
write 0x00081c, 0x00001000        选窗口 1
write 0x008700, <val>
write 0x00081c, 0x00001000        选窗口 1
write 0x008e08, <val>
write 0x00081c, 0x00001000        选窗口 1
write 0x008f00, <val>
write 0x00081c, <val>             选窗口（值来自寄存器）
write 0x00ae02, <val>
write 0x00b604, <val>
```

**`0x81c` 是窗口选择器**，`0x8700` / `0x8e08` / `0x8f00` / `0xae02` / `0xb604` 是窗口内的目标寄存器。
这是 gen8 GMU 的**分页寄存器访问机制**，A740 没有这么复杂的序列。

## 第五段：中断与总线（0x4ec78 – 0x4f1ac）

```
write 0x000e11, 0x00000080
write 0x00081c, [x19+0x3c08]
write 0x00002f, 0x40000000
write 0x00058f, 0x40000000
write 0x00081c, 0x00001000
write 0x008e50, 0x00000001
write 0x008e50, 0x00000001
write 0x003b0e, 0xff000000        -0x1000000
rmw   0x0000ff, 0xff000000
write 0x0000ff, 0x00000001
write 0x00081c, 0x00001000
write 0x00084e, 0xfff0000b        -0xfff5
write 0x00081c, 0x00002000
write 0x00084e, 0xfff0000b
write 0x00081c, 0x00003000
write 0x00084e, 0xfff0000b
write 0x00084e, 0xfff0000b        三次窗口 + 同一目标
```

`0x84e` 在窗口 1/2/3 下各写一次相同值 `0xfff0000b`，是**逐窗口广播配置**。
`0x2f` 与 `0x58f` 同写 `0x40000000`，是一对控制位。

## 与 UniXNU 现有实现的对照

| UniXNU 组件 | 对应关系 |
| --- | --- |
| `AdrenoFirmware`（GMU 容器） | `gmu_parse_fw` 780→752，逻辑基本不变 |
| `AdrenoHfi`（HFI 收发） | `gmu_hfi_probe` 80、`gmu_hfi_start` 128，**体积完全一致** |
| `Adreno740` 的 GMU 部分 | 需要按上表重写寄存器序列 |
| `AdrenoPM4` | `ringbuffer_submitcmd` 3348→3376，仅 +0.8% |

## 需要带入驱动的数值

从本机实测，直接可用：

| 项 | 值 |
| --- | --- |
| GMU 寄存器基址 | `0x3d37000`，长 `0x68000` |
| GMU AO 块 | `0x3d40000`，长 `0x10000` |
| HFI 中断 | GIC SPI 304 |
| GMU 中断 | GIC SPI 305 |
| GMU 频率表 | `qcom,gmu-freq-table = <0x1dcd6500 0x40 0x26be3680 0x80>` |
| GPU SMMU | `0x3da0000`，长 `0x40000` |
| GPU QTB | `0x3de8000`，SID 0..0x3ff，iova 49 位 |
| ZAP 区 | `0x9b09a000`，`0x2000` |

## 未完成的部分

上表只是 `gen8_start` 的**寄存器写序列**，还缺：

1. **HFI 消息注入点** — `gen8_hwsched_hfi_start`（1464 字节）里才有完整消息序列
2. **绑定/映射顺序** — `_map_gmu` 在 `0x54b10`，负责 IOVA 分配
3. **等待点** — `gmu_wait_for_lowest_idle`、`gmu_wait_for_active_transition` 的轮询条件
4. **窗口选择器 `0x81c` 的语义** — 需对照 A740 确认是否同构

下一步应分析 `gen8_hwsched_hfi_start`，那里才有与 UniXNU `AdrenoHfiStartupEncode` 的
七个阶段（消息号 4/3/11/7/11/14/10）一一对应的实际调用。
