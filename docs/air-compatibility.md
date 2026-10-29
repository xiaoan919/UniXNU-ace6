# AIR 兼容性判定：结论与依据

目标：确定 `libllvm-qgl.so` 能否直接吃下 Apple AIR，从而决定 `AirNir` 是"写调用封装"还是"写转换层"。

## 已确认的事实

### 1. 指针模型：类型化（LLVM 15 之前）

`libllvm-qgl.so` 里存在三处 `getPointerElementType()` 调用：

```
ptrTy->isPointerTy() && ptrTy->getPointerElementType()->isVectorTy()
    && "indexing vector using non vector type"
callee->arg_begin()->getType()->isPointerTy()
    && (callee->arg_begin()->getType()->getPointerElementType()->isStructTy()
addPtr && (addPtr->getType()->isArrayTy()
    || addPtr->getType()->getPointerElementType()->isArrayTy())
```

`getPointerElementType()` 在 LLVM 15 被废弃，16 移除。它的存在说明：

- 该 LLVM 派生版本 **≤ 14**
- 使用**类型化指针**（`T*`），不是 opaque pointer（`ptr`）

**Apple AIR 的基线也长期停留在类型化指针时代**（Apple 的 clang 到 Xcode 15 才转向 opaque pointer）。
这一点上两边一致，是兼容的有利信号。

### 2. 输入格式：原生支持 SPIR-V

```
isSrcSpirV && "Assert Index:[2] unexpected input type"
Invalid SPIR-V module: magic number
Invalid SPIR-V module: size
Invalid SPIR-V module: ID bound
Invalid SPIR-V module: ends prematurely
Invalid SPIR-V module: numWords in instruction is 0
"Generate SPIR-V for offline Vulkan compilation"
GenerateSPIRV (false):
End of Spirv
```

**这是最重要的发现。** 编译器有一条明确的 **SPIR-V 输入路径**，并且会校验 SPIR-V 头
（magic / size / ID bound / 指令字长）。说明它把 SPIR-V 作为一等输入。

同时它也支持：

| 输入 | 证据 |
| --- | --- |
| GLSL 源码 | `GLSL Source String %d`、`GLSL Preprocessor is out of memory.` |
| SPIR-V 二进制 | 上述五条校验串 |
| DXIL | `dxilBitcode`、`dxilBitcodeSize` |
| DXBC + golden data | `Provide input signature to run DXBC with DX12 golden data` |

也就是说这是个**多前端编译器**，SPIR-V 是其中一条正式通路。

### 3. 输出能力：自带反汇编

```
DISASSEMBLED_SHADER
DISASSEMBLED_SHADERS_COLLECTIONS
llvm-mc (based on LLVM
Optimize Oxili BC instructions
```

`llvm-mc` 的存在说明库内含 LLVM 汇编器，可产出/反汇编 QGPU 汇编文本。

### 4. 编译器身份

```
====<CompilerVersion>E031.47.18.51</CompilerVersion>====
out/target/product/sun/gen/HEADER_LIBRARIES/oxili_gen_intermediates/...
```

`sun` = `ro.board.platform`，确认是这颗芯片的原生编译产物。

## 已验证的代码位置

用 ADRP+ADD 交叉引用在 `.text` 里定位到三处关键代码：

| 字符串 | 引用地址 | 性质 |
| --- | --- | --- |
| `isSrcSpirV && "Assert Index:[2] unexpected input type"` | `0x842428` | 输入类型分派的断言 |
| `Invalid SPIR-V module: magic number` | `0x65358c` | SPIR-V 头校验 |
| `QCC Parse Shader` | `0x425ff8` | QCC 前端入口 |

`0x842428` 处的反汇编：

```
0x0842428  adrp  x0, 0x66000          ; "isSrcSpirV && \"Assert Index:[2]..."
0x084242c  add   x0, x0, #0x76e
0x0842430  adrp  x1, 0x33000          ; 源文件路径
0x0842434  add   x1, x1, #0x19
0x0842438  mov   w2, #0x1b9           ; 行号 441
0x084243c  bl    0x399ec0             ; __assert_fail
0x0842440  bl    0x11d2558            ; abort / llvm_unreachable
```

这是标准的 `assert_has_index` 宏展开。**`Assert Index:[2]` 说明有一组按索引区分的输入类型枚举**，
索引 2 对应 SPIR-V。旁边的行号 441 与源文件路径也能提取出来，用于对照 Qualcomm 的源码版本。

`0x833500+0x19` 指向的源文件路径读出来是：

```
vendor/qcom/proprietary/graphics/adreno200/shadercompiler/
    HighLevelCompiler/lib/LA/gl_core/ContextImpl.cpp
```

**行号 441**，`ContextImpl.cpp`。

这是决定性的定位：**输入格式分派在 `HighLevelCompiler/lib/LA/gl_core/ContextImpl.cpp` 里**，
不在 LLVM 后端。也就是说，SPIR-V 的解析与 QGPU IR 的生成都在前端这一层完成，
后端（Oxili）只负责接收 QGPU IR 之后的阶段。

对照之前枚举到的前端文件：

```
HighLevelCompiler/lib/LA/gl_core/Context.cpp
HighLevelCompiler/lib/LA/gl_core/ContextImpl.cpp     ← 输入分派在这里
HighLevelCompiler/lib/LA/gl_core/MetaDataExportGL.cpp
HighLevelCompiler/lib/LA/gl_core/ModuleCache.cpp
HighLevelCompiler/include/LA/gl_core/Compiler.hpp
```

**`ContextImpl.cpp` 就是 `CompilerContext` 的实现文件**，与导出的
`_ZN15CompilerContextC1EPN12QGPUCompiler7ContextE`（`0x82a610`）对应。
两个地址相距 0x1de18，属于同一编译单元的概率很高。

这意味着：**要从 SPIR-V 走到 gen8 ISA，只需调用 `CompilerContext` 这一个对象**，
输入类型由 `ContextImpl.cpp` 内部按索引分派。

## 结论：AirNir 不需要碰 LLVM bitcode

原来的设想是：AIR（LLVM bitcode）→ 自己写转换 → QGPU IR → 自己写后端。

**实际情况给出的更短路径：**

```
Apple AIR (LLVM bitcode)
   ↓  [需要一层 bitcode → SPIR-V，或直接复用 Apple 的 AIR→SPIRV 通路]
SPIR-V
   ↓  [libllvm-qgl.so 原生支持]
QGPU IR
   ↓  [Oxili 后端，现成]
gen8 ISA
```

**关键判断：`AirNir` 的工作重心从"实现编译器"变成"准备 SPIR-V 输入"。**

三条可选路线，按工作量排序：

### 路线 A：让编译器直接读 AIR 的 bitcode（最省事，但要验证）

AIR 本质是 LLVM bitcode，只是带 Apple 的 metadata 与 address space 约定。
如果 `libllvm-qgl.so` 的 bitcode reader 能读它的版本，只要：
1. 把 AIR 的 triple 改写成 `qgpu` 系列
2. 补上 QGPU 需要的 metadata（`QGPU_METADATA_NAMED_ADDR_SPACE_BASES` 等）

风险：AirNir 若要复用，需确认 bitcode 版本匹配。

### 路线 B：AIR → SPIR-V → QGPU（最稳，多一步）

Apple 自己就有 AIR → SPIRV 的通路（Metal 工具链里的 `metal-objdump` / SPIRV-Cross 一类）。
拿到 SPIR-V 后，`libllvm-qgl.so` 的原生 SPIR-V 入口直接接管。

**这条路线的证据最充分**，因为 SPIR-V 输入路径是编译器正式支持的。

### 路线 C：AIR → NIR → 自己写 gen8 后端（最重，不建议）

只在 A、B 都失败时才考虑。工作量是重写 Oxili 后端。

## 仍需验证的两点

| 项 | 为什么没确认 | 怎么确认 |
| --- | --- | --- |
| bitcode 具体版本号 | 版本串是运行时格式化参数，不可直接读 | 需要实际喂一个已知版本的 bitcode 试 |
| SPIR-V 入口的调用约定 | 符号被打桩 | 从 `isSrcSpirV` 断言串做 xref 定位 |

`isSrcSpirV` 这条断言字符串在 `.rodata` 里，可以用它的地址反查引用它的代码，
从而定位到输入分派的 switch。这是下一步最值得做的事。

## 附带发现

```
Unable to parse ASICFamily value '
Unable to parse UCHETrapBaseLo/Hi value '
Unable to parse EnableA7pLDGASTGA value '
```

`ASICFamily` 参数说明编译器按 ASIC 家族配置，`EnableA7pLDGASTGA` 这种名字说明
A7p（A740 一代）和 A8x 的特性开关**同时存在于这个二进制里**。

含义：**同一个编译器同时支持 gen7 和 gen8**，通过 ASICFamily 参数选择。
这对我们极其有利——意味着 A830 与 A740 的差异被这个参数封装了，不需要两套编译器。
