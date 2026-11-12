# DAL-523 普通数学函数分派归并

已实现协调器选定的 C1/D1：LOG/SQRT/EXP/MIN/MAX 的五组工厂与参数范围初始化改为文件私有的五项不可变描述表。characterization 先在原实现 GREEN，之后最小重构及同一聚焦回归 GREEN。尚未推送或创建 PR；独立 tester 完整验证与 reviewer 仍待协调器串行安排，父 Issue 保持 `in_progress`。

## 基线、范围与设计

- 复用共享 checkout 和 `agent/dal-orchestrator/fbbeeafab8cb`；基线为 `da3f7b0ad40d58f5f8fc97513e43a09fbdd5b8a7`，未另建 checkout/分支。
- characterization 提交：`6213bceb9e7654ee1df0696d4cbdbb25fc9e43bd`，仅修改 `dal-cpp/tests/script/test_parser.cpp`。
- 最小重构与本报告的对应提交 ID 在 Issue 交回评论中给出；生产源码身份为 parser blob `4c171d3fa881f9a37c804aea02ca1eff503180c2`。
- 本次变更仅三个文件：`dal-cpp/dal/script/parser.cpp`、`dal-cpp/tests/script/test_parser.cpp`、本报告。质量/缺陷调查文件及其临时构建保留，均未混入提交。

按 AGENTS、code-style、unit-test-style、component-boundaries、benchmark-workflow、DAL-398 DSL 大小写/源文本合同、质量方案及当前 script_engine 方法说明实施。公开接口与错误契约保持，遵从协调器决定跳过新 spec/API/critic 阶段。

`BuiltinFunction_` 仅含名字、`Expression_ (*)()` 工厂及 unsigned min/max。五行数据严格为 LOG/SQRT/EXP→1..1，MIN/MAX→2..1000，复用 `MakeBaseNode`。`FindBuiltinFunction` 一个线性 for/if，按原 `String_` 规则比较，miss 返回 nullptr；没有新增依赖、可变注册状态、std::function、枚举或堆分配。

SPOT → 五项描述表 → DCF 的选择顺序与原语义相同；此前的 FIX、索引、向量识别和数字路由，以及之后的参数解析、错误文字、FoldVariadicExtremum 和 ParseVar 回退均保持。D2 比较节点、未使用的 `empty` 变量和已有说明注释均未清理。没有公共头、生成代码、序列化、数值算子或默认值变化。

## characterization 与 GREEN 证据

新增四个普通 TEST，无 fixture：

- `TestParseUnaryFunctionCaseArityAndErrorText`：三种 unary 的大写/小写/混合拼写，0/1/2 参数、缺括号、嵌套 AST；错误类型为 ScriptError_，校验原错误 payload 后缀，包含输入函数原拼写，不固定编译器附加的文件/行号前缀。
- `TestParseExtremumFunctionCaseArityAndLeftFold`：MIN/MAX 三种大小写，各自 0/2/3/1000/1001 参数、原 arity 错误文字；从根向左逐层验证全部 binary 节点及每个数值叶子的源顺序。
- `TestParseSingleExtremumArgumentKeepsVectorRouting`：MIN/MAX 单个预定义向量、单个普通名字、单个数字和单个索引参数。保留单数字走向量名错误、单索引走 arity 错误的现有区别。
- `TestParseBuiltinLookupKeepsSpecialFormsAndVariableFallback`：混合大小写 SPOT 的 source row/date/offset、原 arity 错误，普通前缀变量和绑定常量，DCF 原错误文字，FIX 的 literal/date/source/preparation 信息。

复用既有 `TestParserDCF` 正常日期结果、所有 parser/vector/exercise 测试及 ExtremumOrderTest。后者覆盖 source-left tie、NaN/有符号零、tree/compiled/AAD、variadic AST 和导数，未修改测试或 evaluator。

本轮在当前源码上新建独立临时验证构建，直接编译完整 core 源码、仓库原 `test_main.cpp` 和选中的九个测试源文件，复用只读、干净且与当前 gitlink 一致的第三方源码。未初始化或改写共享 checkout 的 submodule。临时 CMake driver、命令日志及原始测试输出在交回证据包内；它不是根工程完整 CTest/install 矩阵。

执行命令（`VALIDATION` 为证据包的 `dal523-parser-validation`，`DAL_DEPS` 含指定版本的 Eigen/GoogleTest/RapidJSON）：

```bash
cmake -S "$VALIDATION" -B "$VALIDATION/build" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DDAL_ROOT="$PWD" -DDAL_DEPS="$DAL_DEPS"
cmake --build "$VALIDATION/build" --target dal523_parser_tests dal523_script_perf -j8
DAL_NUM_THREADS=4 "$VALIDATION/build/dal523_parser_tests" --gtest_color=no \
  --gtest_filter='ScriptTest.TestParseUnaryFunctionCaseArityAndErrorText:ScriptTest.TestParseExtremumFunctionCaseArityAndLeftFold:ScriptTest.TestParseSingleExtremumArgumentKeepsVectorRouting:ScriptTest.TestParseBuiltinLookupKeepsSpecialFormsAndVariableFallback'
DAL_NUM_THREADS=4 "$VALIDATION/build/dal523_parser_tests" --gtest_color=no
```

原始结果：

```text
# 原实现 + 新 characterization（characterization-baseline.log），exit 0
[==========] 4 tests from 1 test suite ran. (2 ms total)
[  PASSED  ] 4 tests.

# 原实现聚焦基线（focused-baseline.log），exit 0
[==========] 172 tests from 8 test suites ran. (1609 ms total)
[  PASSED  ] 172 tests.

# 描述表重构后，同一测试二进制范围（focused-head.log），exit 0
[==========] 172 tests from 8 test suites ran. (1583 ms total)
[  PASSED  ] 172 tests.
```

八个 suite 为 ScriptTest、ScriptLexerTest、ExtremumOrderTest、NativeVectorTest、ScriptExerciseParseTest、ScriptCompiledParityTest、ScriptCompiledParityFuzzTest、ScriptFixingPreparationTest。characterization 4 项包含在后两次 172 项中，不重复累计。按质量路线不要求故意制造 RED；没有削弱测试或放松门槛。

## 同口径结构实测

沿用质量报告的符号切片、去除注释/字面量后统计 if/for 等关键字的方法。新查找 helper 一并计数，不把移出的分支排除在指标之外。

| Metric                      | Baseline | Implemented                      |
|-----------------------------|----------|----------------------------------|
| 普通工厂/arity 可执行设置点           | 5 个分支    | 1 个应用点 + 5 行数据                   |
| 内建选择分支                      | 7        | 3：SPOT / descriptor / DCF        |
| ParseVarConstFunc if        | 18       | 14                               |
| 主函数 + lookup 显式控制关键字        | 18       | 16：主函数 14 if + lookup 1 for/1 if |
| ParseVarConstFunc body_span | 73       | 60                               |
| ParseVarConstFunc 非空行       | 69       | 56                               |
| lookup body_span            | 0        | 6                                |
| 向量识别嵌套 if 深度                | 4        | 4                                |
| 新增公共字段/API                  | 0        | 0                                |

这些是局部源码结构代理，不是圈复杂度或全仓性能指标。`parser.cpp` 整体净增 6 行，未声称全文件缩短。原始数据见 `structure-before.json`/`structure-after.json`；复核脚本口径与质量报告一致。

## 性能映射、采样与限制

改动位于 parse/AST 构造：`ParseVarConstFunc → Parser_::Parse → ScriptProduct_::ParseEvents`，公开产品构造最终也调用此 core 路径。选择现有 `script_perf` 的 parse(stage 2) 与 construct(stage 1+2)，保留原 2000 iterations，覆盖 SPOT、MAX hit、普通变量/常量 miss 及事件表构造。

排除：预处理算法、lowering、tree/compiled 每路径数值求值、AAD reverse、MC、PDE、curve/rate-risk 稳态聚合、RNG、矩阵/插值等都没有生产改动。九项正式 gate 没有本次改变的 parser 热路径，不以其门禁结果或先前 PDE 调查替代本次验证。未新建 benchmark、未修改 driver、未扩大公共 API。已有 script_perf 没有逐个 unary/MIN 的独立计时用例；本次其行为由 characterization 验证，未来如需专门 throughput 证据应扩展既有目标。

正确性 GREEN 后，使用同机 Release、static/native AAD、GCC 15.2.0、CMake 4.2.3、Eigen ON、native-arch OFF、DAL_NUM_THREADS=4。CPU 为 i9-13900HX，环境为 WSL2，共享宿主存在噪声，不能保证宿主完全空闲；没有与采样并行的本轮构建或测试。

遵从单 checkout 限制：基线源码构建完成并通过测试后，先冻结 baseline 静态测试/benchmark 可执行文件，再改 parser、重建并冻结 head。未另建 source worktree；保留两个 parser 源码快照、两个二进制和源码身份，测量前后检查 benchmark SHA256 未漂移。这个隔离方式及非正式 gate 的性质显式保留，不能冒充根工程的正式性能验收。

按仓库采样口径执行两轮、每轮每侧 10 个独立进程，交错并交替首发侧、保留全部 40 次输出，归约各轮 min；没有改变 iteration 或用单次 smoke 出具结论。parse/construct 的信息性观测如下，单位 us/iter：

| Case                | Round | Base min | Head min | Delta  |
|---------------------|-------|----------|----------|--------|
| parse stage 2       | 1     | 156.450  | 155.123  | -0.85% |
| parse stage 2       | 2     | 157.929  | 156.679  | -0.79% |
| construct stage 1+2 | 1     | 252.288  | 253.320  | +0.41% |
| construct stage 1+2 | 2     | 255.246  | 257.623  | +0.93% |

未观察到这两个用例的持续显著退化；小幅波动不作为性能提升结论。script_perf 不属于九项配对 gate，不套用或新增硬门槛，也不声明正式性能门禁已通过。完整原始数据为 `paired-raw/`、`paired-results.json`、`paired-measurement.log`，采样脚本为 `measure.py`。

身份：baseline script_perf SHA256 `8aeebe1d1d11265a324d3bec999aece756382046cadafb0f2fd4a259249518c2`；head `e901bf54c135dbe308c6a322d7c86525205700afb47eaab666ae48597cb55759`。dependency SHAs、parser/test binary hashes、完整 flags/compile commands 见证据包 `identity.json` 与构建配置。

## 交回与剩余验证

没有设计或行为范围偏移；本机未配置 Git author，提交命令以本轮明确的 Cheng Li 身份用 `git -c` 指定，未改全局或共享 Git 配置。

当前 `docs/methodology/script_engine.md` 已描述大小写、向量/标量极值和 source-left/左折叠。此次仅归并私有注册，未出现真实公开文档缺口；按仓库规则 routine refactor 不需要 CHANGELOG，因此本轮无 docs/CHANGELOG 改动。

未执行根工程完整本地验证矩阵、public/Python/Excel runtime/install、CI、独立 reviewer 或 PR/merge。需由协调器先串行安排 tester 完整验证和 reviewer；本阶段不自行触发成员、不推送、不打开 PR。所有调查产物保留，父 Issue 继续 `in_progress`。
