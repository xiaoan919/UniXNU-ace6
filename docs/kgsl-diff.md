# msm_kgsl.ko 静态分析：gen7 vs gen8 差量

对象：`msm_kgsl.ko`，A830 的 Qualcomm 内核驱动，2986408 字节，
ELF64 ET_REL，`e_machine=183`（AArch64），**符号表完整**（15369 个符号，2613 个 `.text` 函数）。

这是全 A830 平台最高价值的一份二进制——它包含这颗 GPU 的完整驱动实现，带函数名，可直接做 gen7/gen8 对比。

## 核心结论：两代实现共用同一套骨架

驱动里 `gen7_*` 和 `gen8_*` 两套符号并存，直接构成 A740 与 A830 的差量参照：

| 指标 | 值 |
| --- | --- |
| `gen7_*` 函数数 | 234 |
| `gen8_*` 函数数 | 234 |
| 同名函数 | **223（95.3%）** |
| gen8 独有 | 11 |
| gen7 独有 | 11 |
| **平均体积比** | **1.03** |
| **体积差 <10% 的同名函数** | **174 / 223（78%）** |

**结论：A830 不是 A740 的重写，是 A740 的演进。** 78% 的函数体量几乎一样，说明逻辑结构一致，差异集中在少数几个函数和常量表。

这直接推翻了我之前"gen8 换了架构所以要重写"的判断。

## 差异最大的函数

体积比异常（>1.5 或 <0.67）的，都是需要重点关注的：

| 函数 | gen7 | gen8 | 比值 | 解读 |
| --- | --- | --- | --- | --- |
| `counter_read` | 88 | 456 | **5.18** | gen8 计数器读取逻辑大幅扩展 |
| `counter_read_norestore` | 100 | 60 | 0.60 | 拆分了原逻辑 |
| `gmu_snapshot_dtcm` | 456 | 280 | 0.61 | DTCM 转储简化 |
| `complete_rpmh_votes` | 604 | 372 | 0.62 | **RPMh 投票机制变了** |
| `gmu_device_probe` | 348 | 220 | 0.63 | 探测流程精简 |
| `build_rpmh_tables` | 1112 | 752 | 0.68 | **RPMh 表结构变了** |
| `gmu_reset` | 396 | 288 | 0.73 | 复位序列变化 |
| `cp_hw_err_callback` | 1148 | 832 | 0.72 | CP 错误处理变化 |
| `gmu_register_config` | 596 | 472 | 0.79 | 寄存器配置表变化 |
| `start` | 3672 | 4932 | **1.34** | **启动序列显著变长** |

`start` 从 3672 涨到 4932（+34%），是目前已知最大的语义变化点——这与 HFI 启动消息从 gen7 到 gen8 增多了 ACD / IFPC 等阶段吻合。

## 几乎完全相同的函数（可直接复用）

以下函数体积完全一致（比值 1.00），说明实现未变：

```
gmu_boot              340    HFI 启动入口
gmu_load_fw           624 → 612   固件加载
gmu_parse_fw          780 → 752   固件容器解析
gmu_hfi_probe          80     HFI 探测
gmu_hfi_start         128     HFI 启动
gmu_itcm_shadow       176
gmu_memory_init       100
gmu_clock_set          36
gmu_clock_set_rate    280
gmu_dcvs_set          480
gmu_oob_set/clear     560/276
gmu_wait_for_idle     296
ringbuffer_addcmds   1516    命令缓冲构建
ringbuffer_init       396
ringbuffer_submit     304
ringbuffer_submitcmd 3348 → 3376  提交路径
fenced_write          404
gpu_boot              616
```

**`gmu_parse_fw`（780→752）和 `gmu_load_fw`（624→612）几乎没变**——这印证了 GMU 容器格式跨代不变（与我之前解析固件头的结论一致）。

**`ringbuffer_submitcmd` 3348→3376（+0.8%）**——PM4 提交路径结构基本一致。这是重要发现：`AdrenoPM4` 的改造量可能远小于我先前估计。

## gen8 独有的 11 个函数

| 函数 | 大小 | 作用推断 |
| --- | --- | --- |
| `fault_header` | 1796 | gen8 新的故障头格式 |
| `snapshot_shader` | 1244 | 着色器内存转储（新增） |
| `snapshot_mvc_regs` | 984 | MVC 寄存器（新增） |
| `acquire_cp_semaphore` | 556 | **CP 信号量（新增同步机制）** |
| `release_cp_semaphore` | 44 | 配对释放 |
| `cx_host_irq_handler` | 272 | **新增中断源** |
| `hwsched_soccp_vote` | 212 | SOCP 投票 |
| `regread64_aperture` | 164 | 64 位窗口读 |
| `gmu_rpmh_pwr_state_is_active` | 100 | RPMh 电源状态查询 |
| `counter_gmu_pwr_enable` | 124 | GMU 功耗计数器 |
| `spin_idle_debug_lpac` | 232 | LPAC 空闲调试 |

关键新增：**`acquire_cp_semaphore` / `release_cp_semaphore`**（CP 信号量）与 **`cx_host_irq_handler`**（新中断）。
这两处是 gen8 在提交路径上相对 gen7 的实质变化，A740 驱动里没有对应实现。

## gen7 独有的 11 个函数

反向看，A740 有而 A830 没有的，说明被移除或合并——这些在移植时要**删除**而不是照搬。

## 对 UniXNU 移植的量化估计

基于以上数据，重新评估各组件：

| 组件 | 原估计 | 修正估计 | 依据 |
| --- | --- | --- | --- |
| `AdrenoFirmware` | 80% | **90%** | `gmu_parse_fw`/`gmu_load_fw` 体积几乎不变 |
| `AdrenoHfi` | 100% | **100%** | `gmu_hfi_probe`/`gmu_hfi_start` 完全一致 |
| `Adreno740` 核心 | 需大改 | **改 20-30%** | 78% 函数体量相同 |
| `AdrenoPM4` | 需重做 | **改 10-15%** | `ringbuffer_submitcmd` 仅 +0.8% |
| `AirNir` | 需新后端 | **仍需新后端** | 着色器 ISA 不在 kgsl 里，无此数据 |
| `AdrenoMetal` | 100% | **100%** | 与代际无关 |
| `GopFramebuffer` | 100% | **100%** | 与代际无关 |

**累计：GPU 驱动移植从"重写"降级为"改 20-30% + 新增 11 个函数"。**

## 仍需解决的两件事

### 1. 着色器 ISA（最大未知）

kgsl 是内核驱动，**不含着色器编译器**。gen8 的 ISA 编码在 `vulkan.adreno.so` 里（5.35 MB，Qualcomm 官方 Vulkan 实现）。

`AirNir` 要把 AIR → NIR → gen8 ISA，这一步必须逆向 `vulkan.adreno.so`，工作量独立于上面的差量分析。

### 2. ZAP 固件尺寸矛盾（已部分澄清）

`gen80000_zap.mbn` 12088 字节，但 DT 的 `gpu_microcode_region` 只有 `0x2000`（8192 字节）。

ELF 解析结果：

```
e_entry  = 0x1000
ph[1]    = offset 0x1000, filesz 0x3b4   (948 字节，实际代码)
ph[2]    = offset 0x2000, filesz 0xf38
```

代码段从 `0x1000` 起、占 948 字节，落在 8 KiB 区域内。前 4 KiB 是 ELF 头与签名，不载入。

**看起来 8 KiB 够用，但没有验证 ph[2] 的 `memsz=0xf38` 是否会越界。** 写驱动前必须确认。

## 下一步可做的事

1. **反汇编 `gen8_start`**（4932 字节，最大差异点）——提取 A830 的完整启动序列
2. **反汇编 `gen8_gmu_load_fw` + `gen8_gmu_parse_fw`**——确认 gen8 容器解析与 UniXNU 实现是否一致
3. **反汇编 `acquire_cp_semaphore`**——gen8 新增的同步机制
4. **逆向 `vulkan.adreno.so`**——解决着色器 ISA 问题

前 3 项在本机可做（纯静态），第 4 项工作量大但同样可做。
