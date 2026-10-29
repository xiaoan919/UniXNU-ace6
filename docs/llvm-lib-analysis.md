# libllvm-qgl.so 结构分析

## 基本信息

| 项 | 值 |
| --- | --- |
| 文件 | `libllvm-qgl.so`（`/vendor/lib64/`） |
| 大小 | 21,567,072 B |
| sha256 | `9e2b7e3c2aa4ff468ba0a10153273773888e8385fdadd1b268c3107175cff07a` |
| ELF | AArch64 DYN，30 sections |
| `.text` | 偏移 `0x164000`，大小 `0x341280`（3.4 MB） |
| `.rodata` | 偏移 `0x15f50`，大小 `0x11b453`（1.1 MB） |
| `.dynsym` | 286 条，其中本库定义 35 条 |
| `.gnu_debugdata` | 有，解压后 2,120,744 B，28,057 符号 |
| 符号状态 | **28,056 个函数名全部被打桩**（`!!!0000!<hash>!`） |

## 两层编译架构

```
shadercompiler/
  HighLevelCompiler/     前端：GLSL/ESSL → QGPU IR（96 个源文件路径）
  llvm/                  后端：LLVM 优化 + Oxili 代码生成（169 个源文件路径）

  llvm/lib/Target/Oxili/       78 文件，QGPU 后端
  llvm/lib/llclib/llclib.cpp   1 文件，LLVM Codegen 封装
```

## 对外 ABI（本库定义的 6 个非 std 符号）

| mangled name | 地址 | 大小 | 解 |
| --- | --- | --- | --- |
| `_ZN15CompilerContextC1EPN12QGPUCompiler7ContextE` | `0x82a610` | 204 | `CompilerContext::CompilerContext(QGPUCompiler::Context*)` |
| `_ZN15CompilerContextC2EPN12QGPUCompiler7ContextE` | `0x82a610` | 204 | 同上 |
| `_ZN15CompilerContextD1Ev` | `0x82c610` | 108 | 析构 |
| `_ZN15CompilerContextD2Ev` | `0x82c610` | 108 | 同上 |
| `LoadQCC` | `0x118de50` | 116 | 装载 QCC 模块 |
| `LoadLLC` | `0xcffe10` | 88 | 装载 LLC 模块 |

## 版本与构建标识

```
====<CompilerVersion>E031.47.18.51</CompilerVersion>====
llvm-mc (based on LLVM <version>
out/target/product/sun/gen/HEADER_LIBRARIES/oxili_gen_intermediates/llvm/lib/Target/Oxili/QGPUGenAsm...
```

| 项 | 值 |
| --- | --- |
| Qualcomm 编译器版本 | **`E031.47.18.51`** |
| 构建产品名 | **`sun`**（与 `ro.board.platform` 一致，确认是 A830 原生版本） |
| 工具 | `llvm-mc`（LLVM 汇编器，说明编译器自带汇编/反汇编能力） |

`sun` 这个产品名是决定性证据：**这个编译器就是为这颗 GPU 编译的**，不是通用兼容版本。

## QGPU 内建函数（intrinsic）

`.rodata` 里可见 911 条 `qgpu` 相关字符串，其中一批是 LLVM intrinsic 名：

```
llvm.qgpu.atomic64.add.oob
llvm.qgpu.atomic64.umax.oob
llvm.qgpu.bfrev
llvm.qgpu.clamp
llvm.qgpu.clzs
llvm.qgpu.demote.to.helper.invocation
llvm.qgpu.fcombine10
llvm.qgpu.fcombine31
llvm.qgpu.fcombine55
llvm.qgpu.fcreate.map55
llvm.qgpu.emit.vertex
...
```

这就是 **QGPU IR 的语义集合**。把这份列表提取全，等于拿到了 AIR/QGPU IR 与 gen8 硬件之间的语义映射表。

`fcombine10` / `fcombine31` / `fcombine55` / `fcreate.map55` 这类名字对应 gen8 新增的**浮点运算合并指令**，
A740 的 IR3 里没有——这正是 ISA 层面的代际差异所在。

## CompilerContext 布局（从构造函数推导）

`0x82a610` 的 204 字节反汇编显示，对象总大小约 **0x338 字节**，初始化为：

```
this+0x00  = {0, ctx}            // QGPUCompiler::Context* 指针
this+0x10  = alloc(8)  -> 存指针 // 二级指针，指向 0x68 字节的结构
this+0x58  = 零 (q0)
this+0x68  = 零 (q0)
this+0x78  = &this+0x78          // 自引用/哨兵
this+0x204 = &this+0x204         // 自引用/哨兵
this+0x2b0 = &this+0x78
this+0x2b8 = &this+0x204
this+0x2c0 = 零 (q0)
this+0x2d0 = 0 (byte)
this+0x2d8 = 0
this+0x2e0 = 零 (q0)
this+0x2f0 = ctx->[0x48]
this+0x2f8 = ctx->[0x00]
this+0x300 = &ctx+0x50
this+0x308 = 零 (q0 x2)
this+0x328 = 0
this+0x331 = 0 (halfword)
```

两个自引用指针（`+0x78`、`+0x204`）是典型的 **intrusive list 头**（`std::__list_node` 风格的环）。
随后 `alloc(0x68)` 并调用一个初始化函数（`0x82a6e0`），参数为 `(w1=0, w2=0x8000, w3=0x10)`。

`0x8000` 与 `0x10` 这对参数，可能是初始缓冲区大小与对齐。

## 关键：编译器由运行时装配

`LoadQCC` / `LoadLLC` 加上以下字符串，说明真正的编译逻辑是**运行时装载**的：

```
"Force llclib to codegen on serialized module"
"llclib: target does not support generation of this"
"llclib: error: could not find triple for target '"
"llclib: Warning: Minimum Register footprint expected is "
"Unrecognized QCC Context option '"
```

这解释了为什么 `.so` 里只有两个加载函数作为 ABI 边界——它是个**外壳加调度器**。

## 对 AirNir 的意义

原来 `AirNir` 要做：AIR → NIR → IR3（或 gen8 ISA）。工作量是自写一个编译器后端。

现在已知：

| 环节 | 现成资源 |
| --- | --- |
| LLVM 中端优化 | `libllvm-qgl.so` 内 `Transforms/` 36 个 pass |
| QGPU 指令选择 | `Target/Oxili/` 的 `QGPUInstructionSelector` |
| 寄存器分配 | 5 套策略（图着色、线性扫描、全局、常量、UGPR） |
| 机器码编码 | `QGPUMachineEncoder` + 表驱动 `QGPUGenAsmWriter.inc` |
| ISA 反汇编输出 | `DISASSEMBLED_SHADER` 字符串，编译器自带 |

**`AirNir` 可以从"实现编译器"降级为"调用编译器"。**

## 与技术难点的对应

| 难点 | 现状 |
| --- | --- |
| gen8 ISA 编码 | **已解决方向**：在 `QGPUGenAsmWriter.inc` 的表里，可从 `.rodata` 提取 |
| 指令选择规则 | 在 `QGPUInstructionSelector` 里，需逆向 |
| AIR 兼容性 | **未验证**：AIR 是 Apple 的 LLVM bitcode 变体，需确认版本兼容 |
| 编译器调用入口 | **已定位** `CompilerContext` 构造 + `LoadQCC`/`LoadLLC` |
| 函数名 | 全部打桩，只能靠字符串 xref 与调用图 |

## 下一步具体动作

1. **提取 QGPU ISA 表**：`QGPUGenAsmWriter.inc` 是 `.inc` 表驱动，
   实际数据在 `.rodata`。找到 `QGPUInstPrinter` 的字符串表可还原指令助记符。
2. **验证 AIR 版本**：读 `libllvm-qgl.so` 的 `LLVM_VERSION` 字符串，
   与 Apple AIR 的 bitcode 版本比对。
3. **追踪 `LoadQCC`**：`0x118de50` 只有 116 字节，会跳到真正的加载逻辑，
   顺着看能定位 QCC 上下文的初始化路径。
4. **考虑 `libadreno_compiler_cl.so`（32.9 MB）**：OpenCL 编译器通常暴露
   更完整的 C API，比逆向 C++ 类容易。

## 未提取但可能有用

| 文件 | 大小 | 说明 |
| --- | --- | --- |
| `libadreno_compiler_cl.so` | 32,978,480 B | OpenCL 编译器，体积最大 |
| `libGLESv2_adreno.so` | 6,151,056 B | GLES 入口，可能暴露编译 API |
| `libadreno_utils.so` | 134,224 B | 工具函数，可能有 ISA 辅助 |
