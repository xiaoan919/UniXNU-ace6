# A830 着色器编译器：重大发现

## 结论先行

**Adreno 的着色器编译器不在 `vulkan.adreno.so` 里，是一个独立的 LLVM 派生库。**
我已经把它从设备上完整提取出来，并确认它的后端名称、源文件结构和可用的调试能力。

这彻底改变了 `AirNir` 的移植方案：**不需要从零写 gen8 后端，只需要驱动现成的编译器。**

## 提取到的编译器库

| 文件 | 大小 | 作用 |
| --- | --- | --- |
| `libllvm-qgl.so` | **21,567,072 B** | **Vulkan/GL 着色器编译器（OpenGL 与 Vulkan 共用）** |
| `libllvm-glnext.so` | 1,993,712 B | GL 的 next-gen 前端 |
| `libadreno_compiler_cl.so` | 32,978,480 B | OpenCL 编译器（未提取，体积最大） |
| `libgsl.so` | 2,284,136 B | 图形支撑层 |
| `libadreno_utils.so` | 134,224 B | 工具函数 |
| `libGLESv2_adreno.so` | 6,151,056 B | GLES 入口 |

全部来自 `/vendor/lib64/`，全部带 `.gnu_debugdata`（XZ 压缩的迷你符号表）。

## 编译器后端：Oxili

`libllvm-qgl.so` 的字符串里暴露了完整的源码路径，后端名叫 **`Oxili`**：

```
vendor/qcom/proprietary/graphics/adreno200/shadercompiler/llvm/lib/Target/Oxili/...
```

这是 Adreno 的 LLVM Target。169 个源文件路径可见，分属：

| 目录 | 文件数 | 内容 |
| --- | --- | --- |
| `Target/Oxili` | **78** | 后端本体 |
| `Transforms` | 36 | LLVM 通用优化（InstCombine 等） |
| `CodeGen` | 22 | 机器码生成框架 |
| `Analysis` | 10 | 分析 pass |
| `Support` | 9 | 基础设施 |
| `VMCore` | 8 | LLVM 核心 |
| `IREmitter` | 2 | QGPU IR 发射 |
| `IRUpdater` | 1 | IR 更新 |
| `QGPU` | 1 | TargetTransformInfo |
| `Bitcode` | 1 | 位码写入 |
| `llclib` | 1 | — |

## Target/Oxili 的 78 个文件（完整清单）

后端结构完全暴露，可以据此判断各阶段职责：

### 指令选择
```
QGPUFastISel.h / QGPUFastIsel.cpp
QGPUInstructionSelector.cpp
QGPUISelLowering.cpp / .h
QGPUIselPrepare.cpp
QGPUIntrinsicLowering.cpp
QGPULiteralLowering.cpp
QGPUInstMap.h
QGPUInstrInfo.cpp / .h / Base.h
QGPUInstructionValidator.cpp
```

### 寄存器分配（多种策略并存）
```
QGPUConstRegAlloc.cpp
QGPUGlobalRegAlloc.cpp
QGPUGraphColoring.cpp
QGPURegAllocBase.cpp
QGPURegRenamer.cpp
QGPURegRewriter.cpp
QGPURegisterInfo.cpp / .h
QGPULMSpiller.cpp              (线性扫描溢出)
QGPULiveRangeEvacuate.cpp / .h
QGPURematerialization.cpp
QGPUUGPRPromote.cpp            (统一通用寄存器提升)
```

### 调度与延迟
```
QGPUScheduler.cpp
QGPUListSchedulingSelector.cpp
QGPULatencyHandler.cpp / .h
QGPUPostLatencyPass.cpp
QGPUPreLatencyPass.cpp
QGPULiveRangeEvacuate.cpp
```

### 编码（生成的机器码在这一层）
```
QGPUMachineEncoder.cpp
QGPUMachineObjWriter.cpp
QGPUTargetObjGen.cpp
QGPUGenAsmWriter.inc           (表驱动汇编输出)
Assembler/Encoder.cpp
Assembler/AsmParser.cpp
Assembler/MetadataEncoder.cpp
Assembler/SemanticAnalyzer.cpp
Assembler/Diagnostic.cpp
InstPrinter/QGPUInstPrinter.cpp
```

### QGPU 特有优化
```
QGPUCombiner.cpp
QGPUCrossCFGHoist.cpp
QGPUEarlyPreamble.cpp
QGPUPreamblePreload.cpp / .h
QGPUFenceInsertion.cpp
QGPUGlobalTexHoist.cpp / .h
QGPUPixelInterpolatorOptimizer.cpp
QGPUBreakComplexExpr.cpp / Utils.cpp
QGPUI64Emulate.cpp / .h         (64 位整数模拟)
QGPUScalarization.cpp
QGPUVectorize.cpp
QGPUMemoryVectorizer.cpp
QGPUPostRAVectorize.cpp
QGPUMIROptimizations.cpp
QGPUMIRInterface.cpp
QGPUResumeCount.cpp
QGPUXfrGraph.cpp
QGPUGrouping.cpp
QGPUOptimize.cpp
QGPUBackedgeFixups.cpp
QGPUPostSchedFixups.cpp
QGPUFrameLowering.cpp
QGPULocalAddrAlloc.cpp
QGPUTargetMachine.cpp / .h
QGPUCallGraphSCCPass.cpp
QGPUPreRAMIRVerifier.cpp / .h
QGPUFastISel.h
QGPUValueAnalysis.h
QGPUAttrInserter.cpp
QGPUProfilingInstrumentation.cpp
```

## 关键字符串证据

```
libllvm-qgl.so                                   自引用
vendor/.../shadercompiler/llvm/lib/IRUpdater/LLVMIRUpdater.cpp
vendor/.../shadercompiler/llvm/lib/IREmitter/IREmitter.cpp
vendor/.../shadercompiler/llvm/lib/IREmitter/BCTool.cpp
vendor/.../shadercompiler/llvm/lib/Target/Oxili/QGPUPreEmit.cpp
vendor/.../shadercompiler/llvm/lib/Target/Oxili/QGPULatencyHandler.h
vendor/.../shadercompiler/llvm/lib/Target/Oxili/InstPrinter/QGPUInstPrinter.cpp
vendor/.../shadercompiler/llvm/lib/Target/Oxili/QGPUMIROptimizations.cpp

mCurrentLLVMModule && "Init Func: LLVM module doesn't exist"
pMainFunction && "Issue obtaining main function from LLVM Module!"
theTriple.isQGPUArch()
qgpu-preemit
llvm.qgpu.emit.vertex
DISASSEMBLED_SHADER
DISASSEMBLED_SHADERS_COLLECTIONS
"After PreEmit passes"
"Use TSort Algorithm for Sorting BasicBlock Order"
"Disable Advance Code Sinking for Normal Instruction."
```

`DISASSEMBLED_SHADER` 说明编译器**自带反汇编输出能力**，调试时可直接看生成的 ISA。

## 两层编译器架构（关键）

`libllvm-qgl.so` 内部是**双目录**结构，对应 Qualcomm 的两层编译：

```
shadercompiler/
  HighLevelCompiler/     ← 前端：文本/字节码 → QGPU IR
  llvm/                  ← 后端：LLVM 优化 + Oxili 代码生成
```

### 前端 HighLevelCompiler（96 个源文件路径可见）

```
include/LA/gl_core/Compiler.hpp
include/LA/gl_core/CodeGenHelper.hpp
include/LA/gl_core/ConstantUnion.h
include/LA/gl_core/Symbol.hpp
include/LA/gl_core/es_sl_Types.h           ← ES 着色语言类型
include/LA/opengl/gl_parser/MachineIndependent/SymbolTable.h
include/LA/opengl/gl_parser/ShHandle.h     ← GLSL 解析器
include/common/HLC/hlc.h                   ← HLC 主接口
include/common/HLC/QGPUSymbolInfo.h
include/common/HLC/QGPUNamedPointer.h
include/common/HLC/GlUtils.hpp
include/common/QCC/Metadata.h              ← QCC 元数据
include/common/QGPU/Assembler/DumpISADefs.h
include/common/QGPU/Assembler/ShaderObject.h
include/common/QGPU/QGPUModuleInfo.h
include/common/raytracing/StateFunctionTransform.h
lib/LA/gl_core/Context.cpp
lib/LA/gl_core/ContextImpl.cpp
lib/LA/gl_core/MetaDataExportGL.cpp
lib/LA/gl_core/ModuleCache.cpp
```

这说明前端负责：GLSL/ESSL 解析、符号表、QGPU IR 生成、QCC 元数据。

### 后端 llvm/（Oxili Target）

见上文 78 个 `Target/Oxili` 文件。

## 实际的 ABI 边界

`.dynsym` 里 `shndx != 0`（即本库自己定义）且非 std 的符号只有 6 个：

| 符号 | 地址 | 大小 | 含义 |
| --- | --- | --- | --- |
| `_ZN15CompilerContextC1EPN12QGPUCompiler7ContextE` | `0x82a610` | 204 | `CompilerContext::CompilerContext(QGPUCompiler::Context*)` 构造 |
| `_ZN15CompilerContextC2EPN12QGPUCompiler7ContextE` | `0x82a610` | 204 | 同上（C2 是构造别名） |
| `_ZN15CompilerContextD1Ev` | `0x82c610` | 108 | 析构 |
| `_ZN15CompilerContextD2Ev` | `0x82c610` | 108 | 同上 |
| `LoadQCC` | `0x118de50` | 116 | **加载 QCC 模块** |
| `LoadLLC` | `0xcffe10` | 88 | **加载 LLC 模块** |

`LoadQCC` / `LoadLLC` 的存在说明：**这个 .so 是一个外壳，实际编译逻辑可能在运行时才装配。**

对应字符串证据：

```
"Force llclib to codegen on serialized module"
"llclib: target does not support generation of this"
"llclib: error: could not find triple for target '"
"llclib: Warning: Minimum Register footprint expected is "
vendor/.../shadercompiler/llvm/lib/llclib/llclib.cpp
"Unrecognized QCC Context option '"
"false && \"invalid qcc_constantexpressionopcode\""
```

`llclib` = LLVM Codegen library 的封装层；`QCC` = Qualcomm Compiler Collection 的上下文。

## 对 AirNir 方案的重新评估

原来的假设：`AirNir` 要自己实现 AIR → NIR → gen8 ISA 的完整编译链。

**实际情况：**

| 环节 | 原计划 | 实际情况 |
| --- | --- | --- |
| AIR 解析 | 自己做 | Apple 的 AIR 是 LLVM bitcode 变体 |
| 中端优化 | 自己做 | **`libllvm-qgl.so` 已含完整 LLVM Transforms** |
| 指令选择 | 自己写 gen8 后端 | **`Oxili` 后端已在库中** |
| 寄存器分配 | 自己写 | **5 种策略已实现** |
| 编码 | 自己写 | **`QGPUMachineEncoder` 已实现** |

**结论：`AirNir` 的角色从"实现编译器"变成"调用编译器"。**

新的技术路线：

1. 从 `libllvm-qgl.so` 解析出编译入口（需要找出对外导出的函数）
2. 把 AIR（LLVM bitcode）喂进去，让它以 QGPU triple 编译
3. 取出生成的机器码，交给 `Adreno740` 的提交路径

这条路比自写后端短得多，而且用的是 Qualcomm 自己的编译器，语义一定正确。

## 逆向难点

**所有 28056 个函数名都被符号打桩。**

```python
symbols 28057, funcs 28056, stubbed 28056, named 0
```

符号名是 `!!!0000!6f7c12360b6e6ab273c577d1650287!a97e8c1cc4!` 这种形式，
把真实名字替换成了哈希。这是 Qualcomm 的反逆向措施。

`.dynsym` 只有 743 条，且都是导出的 Vulkan API 入口（`vkCmdBeginPerTileExecutionQCOM` 等），
不包含编译器内部函数。

**因此不能靠符号名定位，必须靠：**

1. 字符串交叉引用（`.rodata` 里的路径字符串 → 找到引用它的代码）
2. 调用图分析（从导出的入口往下追）
3. 结构特征（LLVM 的 `PassRegistry` 初始化表）

## 下一步

优先级从高到低：

1. **定位编译器入口**：从 `libllvm-qgl.so` 的 `.init_array` 和导出符号入手，
   找到 `CompileShader` 类函数的地址
2. **确认 AIR 兼容性**：AIR 是 Apple 的 LLVM bitcode 变体，
   需要确认这个 LLVM 能否直接读。版本很关键（LLVM 的 bitcode 有版本兼容性）
3. **提取 QGPU 指令编码表**：`QGPUGenAsmWriter.inc` 是表驱动的，
   表数据在 `.rodata` 里，可以提取出完整的 ISA 描述
4. **确认 `libadreno_compiler_cl.so`（32.9 MB）是否更容易逆向**：
   OpenCL 编译器通常暴露更完整的 API

## 附带发现

`libllvm-qgl.so` 里的 `a8x` 相关字符串：

```
vendor/qcom/proprietary/graphics/adreno200/sharedlibs/rtlib/gpubvh/hwl/a8x/a8xbvhkernelloader.cpp
```

说明 A8x 的**光线追踪 BVH 内核**也是为 gen8 准备的，进一步确认这个驱动栈是原生 A830 版本，不是兼容模式。
