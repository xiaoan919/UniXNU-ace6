# A830 HFI 启动阶段（从 msm_kgsl.ko 提取）

来源：`gen8_hwsched_hfi_start`，`.text` 偏移 `0x61644`，1464 字节。
调用目标由 `.rela.text` 重定位还原。

## 实测的完整启动顺序

按调用顺序，A830 的 HFI 启动是：

| # | 调用 | 作用 |
| --- | --- | --- |
| 1 | `gen8_gmu_hfi_start` | HFI 队列初始化 |
| 2 | `gen8_hwsched_warmboot_init_gmu` | 热启动 GMU |
| 3 | `gen8_hfi_send_get_value` | 读取固件版本等 |
| 4 | `gen8_hfi_send_generic_req_v5`（id=`0x9a`） | 专有请求 |
| 5 | `gen8_hfi_send_gpu_perf_table` | **性能表（对应 UniXNU msg 4）** |
| 6 | `gen8_hfi_send_generic_req` | 通用请求 |
| 7 | `gen8_hfi_send_acd_feature_ctrl` | ACD 特性 |
| 8 | `gen8_hfi_send_bcl_feature_ctrl` | **BCL 特性（UniXNU 无）** |
| 9 | `gen8_hfi_send_clx_feature_ctrl` | **CLX 特性（UniXNU 无）** |
| 10 | `gen8_hfi_send_ifpc_feature_ctrl` | IFPC 特性 |
| 11–15 | `gen8_hfi_send_feature_ctrl` × 5 | 五个通用特性开关 |
| 16–17 | `gen8_hfi_send_set_value` × 2 | 参数设置 |
| 18 | `gen8_hfi_send_core_fw_start` | **核心固件启动（对应 msg 14）** |
| 19–20 | `gen8_hfi_send_feature_ctrl` × 2 | 启动后再开关两个特性 |
| 21 | `gen8_hfi_send_generic_req` | 收尾请求 |
| 22 | `gmu_core_regwrite` | 最后一次寄存器写 |
| 23 | `kgsl_pwrctrl_setup_default_votes` | 建立默认功耗票 |

## feature_ctrl 与 set_value 的完整参数表

从调用点回溯 `w0`/`w1`/`w2`/`w3` 还原：

| 调用地址 | 类型 | id | enable | data | 条件保护 |
| --- | --- | --- | --- | --- | --- |
| `0x0618d0` | feature_ctrl | `0x01` | 1 | 0 | 无 |
| `0x0618ec` | feature_ctrl | `0x10` | 1 | 0 | 无 |
| `0x061914` | feature_ctrl | `0x17` | 1 | 0 | 无 |
| `0x061938` | feature_ctrl | `0x1a` | **0** | 0 | 无 |
| `0x061994` | set_value | `0x77` | 0 | 1 | `cbz w8` — w8 非零才发 |
| `0x0619ac` | set_value | `0x6f` | 0 | `[x20+0x1174]` | w3 非零才发 |
| `0x061a00` | feature_ctrl | `0x18` | 1 | 0 | `cbz w8` |
| `0x061a28` | feature_ctrl | `0x1d` | 1 | 0 | `tbz w8,#2` — bit2 为 0 才发 |

**两个容易抄错的点：**

1. `0x1a` 的 `enable` 是 **0**，这是**关闭**某个特性，不是开启。照抄成 1 会导致行为不同。
2. 最后四条（`0x77`、`0x6f`、`0x18`、`0x1d`）都有**条件保护**，依赖 `x19+0x...` 里的
   配置字段。这些字段来自 device tree 或 board 配置，不是恒定发送。

`0x9a` 那条 `generic_req_v5` 的 payload 里 `[sp+4] = 0x9a`、`[sp+0xc] = 0`，
说明它是带一个 u32 参数的私有请求，在 `gpu_perf_table` 之前发送。

## 与 UniXNU 现有实现的差异

UniXNU `AdrenoHfiStartupEncode` 只有七个阶段：

```
Performance(4) → Bandwidth(3) → AcdEnable(11) → AcdTable(7) → Ifpc(11) → CoreStart(14) → Start(10)
```

A830 实际需要但 UniXNU 没有的：

| 缺失项 | 说明 |
| --- | --- |
| **BCL** | 电池电流限制（Battery Current Limit），gen8 新增特性 |
| **CLX** | gen8 新增特性，具体语义待查 |
| **feature_ctrl 通用通道** | UniXNU 把每类特性硬编码成独立消息，A830 走统一的 feature 开关 |
| **set_value / get_value** | 参数读写通道，UniXNU 无对应 |
| **ACD 的 `0x9a` 专有请求** | 在 perf table 之前发送 |

**含义：UniXNU 的 `AdrenoFirmware` kit 不能直接给 A830 用，需要扩展至少 5 类消息。**
这比我上一轮估计的 90% 复用率要低——协议层框架可复用，但消息集要补。

## `gen8_start` 的寄存器序列（已提取）

见 `gmu-start-sequence.md`，要点：

- `0x81c` 是**窗口选择器**，值 `0x1000`/`0x2000`/`0x3000` 选不同窗口
- `0x84e` 在三窗口下各写一次 `0xfff0000b`，是逐窗口广播
- `0xf08`/`0xf09` 与 `0xae10`/`0xae11` 成对，从 `[x19+0x1bd8]`/`[x19+0x1bdc]` 取值
- `0xfc41` + `0xff00` 的 rmw 组合是中断路由配置

A740 没有 `0x81c` 窗口选择机制，这是 gen8 GMU 的实质新增。

## 累计评估

| UniXNU 组件 | 复用度 | 依据 |
| --- | --- | --- |
| `AdrenoHfi`（收发队列） | **100%** | `gmu_hfi_probe` 80、`gmu_hfi_start` 128 体积完全一致 |
| `AdrenoFirmware`（容器解析） | **85%** | `gmu_parse_fw`/`gmu_load_fw` 几乎不变 |
| `AdrenoFirmware`（HFI 消息集） | **60%** | 缺 BCL/CLX/set_value/get_value/0x9a |
| `Adreno740` GMU 寄存器序列 | **需重写** | `0x81c` 窗口机制是 gen8 独有 |
| `Adreno740` 提交路径 | **95%** | `ringbuffer_submitcmd` +0.8% |
| `AdrenoPM4` | **85%** | 同上 |
| `AdrenoMetal` / `GopFramebuffer` | **100%** | 与代际无关 |
| `AirNir` | **需新后端** | 着色器 ISA 在 vulkan.adreno.so，未分析 |

## 下一步

1. 提取剩余两个 `feature_ctrl` 的 id（`0x61a00`、`0x61a28`）
2. 反汇编 `gen8_hfi_send_bcl_feature_ctrl` / `clx` / `acd`，确定各自的消息号与 payload 结构
3. 反汇编 `gen8_hfi_send_generic_req_v5`，看 `0x9a` 请求的 payload
4. 分析 `_map_gmu`（`0x54b10`），确认 49 位 IOVA 的分配方式
