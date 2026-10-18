P3 实施完成，待独立 tester / reviewer 验收。基线 `f721478ccb8284a48ea144eae828c37e43dc9142`；复用分支 `agent/dal-orchestrator/9f36be3b414c`，未另建分支或工作树、未推送 / 合并，也未触发其他角色。本记录与实现放在同一本地提交，精确提交号由交付评论给出。

**控制记录及设计**

沿用 active API 和 B1 最终 critique，无契约放宽。API SHA-256 为 `67032d299b4386568d377a151c28f6b206d7052f8fccffce64657c1e3c7a46e0`，critique 为 `cfadaca7366d0669b3d2fdd9b28f4a4c5ae00558fbf38f40ecbf3c5466a2f37a`；原文件及 README 的既有未提交修改均保留。

新增独立 core theta 路径：ACT/365 日期锚点、分段 carry、默认 fully implicit、显式 CN / Rannacher 和有界单调性二分。Bermudan 只在精确日程投影；American 以 penalty 解原始归一化 LCP，每次求解检查物理行 backward error，逐步保留最大值。没有投影式 American fallback、penalty 倍增或容差放宽。结果 owning；失败无价格和估值层 getter。

空间与时间计划均先核算；区域按全部可行权层的最坏 run 容量预留。静态布局断言和 capacity 账本分别校验 P/W 包络。工作区峰值是实际容器 capacity、固定临时结构的 sizeof 与有界标量余量的保守核算；不代表 RSS，也不包含 allocator 元数据、字符串和语言包装。没有保留 N×M 价格面。

数值实现使用二进制幂统一缩放线性未知量，避免短 rollback 的深 OTM 尾部下溢污染相对残差；该因子在物理行 backward-error 比值中抵消。American 使用相对 payoff 的修正变量，Bermudan 直接求解价格，避免深 OTM 相消。不依赖扩展精度数组；全部十个 N 长度 scratch 数组为 double，未改变既定 penalty 方法。端点仅按边界 / 投影分类，不伪造端点 dual residual。

**改动文件与验证映射**

| 受影响路径                                                      | 定向正确性验证                                                     | 性能用例                                  |
|------------------------------------------------------------|-------------------------------------------------------------|---------------------------------------|
| `dal-cpp/dal/math/pde/earlyexercisepde.*`                  | `EarlyExercisePdeTest.*`（16 项）、独立 CRR / BS / 确定性解析          | `pde_perf --early-exercise` 四例        |
| `dal-public/src/earlyexercisepde.hpp`                      | `PublicEarlyExercisePdeTest.*`（2 项）、独立小 LCP 枚举、并发 / 非空 tape | 同一 core owning 执行路径                   |
| `dal-python/src/bindings/earlyexercisepde.cpp` / 注册和 CMake | `test_early_exercise_pde.py`（11 项）、旧 European Python 34 项   | 原生调用计时；语言包装不计入原生预算                    |
| `dal-excel/src/__earlyexercisepde*` / 生成 stubs             | `ExcelEarlyExercisePdeTest.*`（5 项）、旧 European Excel 10 项    | native 算法已覆盖；XLL 计时未运行                |
| `dal-cpp/benchmarks/pde_perf/*`                            | 四个新 case 均检查成功状态，保留原 default driver                         | 两轮各十个进程，每进程 2 warmup + 10 repetitions |

另包括 `dal-cpp/tests/math/pde/test_early_exercise_pde.cpp`、`dal-public/tests/test_early_exercise_pde.cpp`、`dal-excel/tests/test_early_exercise_pde.cpp` 和 `dal-python/tests/test_early_exercise_pde.py`。新增 3 个 Machinist enum 的 9 个生成文件及 19 个 worksheet 函数的 38 个生成文件；只通过 `dal_generate` 生成，没有直接编辑 auto 文件。

测试参照 `dal-cpp/test-support/bermudan_pde.hpp` 未修改，工作副本和基线 blob 均为 `d203a9b0d4ecfcf97c42118d70fd24436ff78e19`。新生产代码不调用该 helper。core 示例位置、目标及依赖方向保留。

**RED / GREEN**

初始 focused 编译使用 core-only 构建的静态库与 vendored GoogleTest；完整编译命令保留在运行记录，关键执行如下：

- RED：`build/dal379-focused --gtest_filter=EarlyExercisePdeTest.TestIrregularDateProjectionHasIndependentDeterministicPrice`，报 “not implemented”；最小实现后同命令 GREEN。
- RED：`build/dal379-focused '--gtest_filter=EarlyExercisePdeTest.*'`，捕获 American 中间层行权标记遗漏和细时间网格深 OTM 线性残差失败；修正标记及缩放求解后 GREEN。未放宽价格或残差阈值。
- 低 penalty 的常正 r=q 初值本身可满足 LCP、合法 0 次求解，因此用分段 r=q（前 0.1、后 -0.01）强制实际求解，并以独立标量递推核对失败步 obstacle gap。默认 penalty 通过，penalty=1 返回 LCP_RESIDUAL_LIMIT；另验证 MAX_ITERATIONS、WORK_LIMIT、严格线性精度失败及失败价格隔离。
- RED：`build/dal379-excel-focused --gtest_filter=ExcelEarlyExercisePdeTest.TestEntireBlankOptionalRangeUsesDefaults`，整块空区域错误拒绝；实现整块空检测后 GREEN，半空行仍拒绝。
- RED：`build/dal379-excel-time-red --gtest_filter=ExcelEarlyExercisePdeTest.TestIndependentPriceAndEveryOwningTable`，缺 TimeIndex 列；补齐输出后 GREEN。
- RED：`build/dal379-focused-current --gtest_filter=EarlyExercisePdeTest.TestInvalidScalarReportsFieldValueAndConstraint`，错误缺实际数值；补齐字段 / 数值 / 约束后 GREEN。
- Python RED：`PYTHONPATH=build/dal379-implementation/dal-python python3 -m pytest dal-python/tests/test_early_exercise_pde.py -x -q`，缺少新类型；绑定完成后 11 项 GREEN。

独立数值准入包括：137/365 不规则日程与 carry 顺序的确定性解析；有溢价的 put / yield call CRR 多树深及奇偶带宽；Bermudan 对齐树层；分段 carry 的解析 European、近零 vol；独立 3×3 LCP active-set 枚举；共同时间网格上的机会单调性；American 和 European 的三档空间 / 时间分离收敛、固定局部步长两次扩域；负利率、零 vol 两个 drift 方向、闰日、短到期、深 ITM / OTM、高 vol 及段内边界驻点。

**最终构建与验证**

Linux x86_64，GCC 15.2.0，CMake 4.2.3，C++17，Release / O3，native AAD，Eigen OFF，native arch OFF；CPython 3.13.9、系统 pybind11 3.0.4。配置采用仓库 `Release-linux` preset，`BUILD_SHARED_LIBS=off`，开启 public、Python 和 Excel portable tests。

```sh
cmake --preset=Release-linux -S . -B build/dal379-implementation \
  -DDAL_BUILD_PYTHON=ON -DDAL_BUILD_EXCEL_PORTABLE_TESTS=ON \
  -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_PUBLIC_BUILD_EXAMPLES=OFF -DDAL_USE_EIGEN=OFF
cmake --build build/dal379-implementation --target dal_cpp_tests dal_public_tests dal_excel_portable_tests _dal -j 8
ctest --test-dir build/dal379-implementation --output-on-failure \
  -R '^(EarlyExercisePdeTest|ThetaSchemeTest|PublicEarlyExercisePdeTest|EuropeanPdeRiskTest|ExcelEarlyExercisePdeTest|ExcelEuropeanPdeRiskTest)\.|^ScriptExerciseLSMCTest\.TestPdeBenchmark'
PYTHONPATH=build/dal379-implementation/dal-python python3 -m pytest \
  dal-python/tests/test_early_exercise_pde.py dal-python/tests/test_european_pde_risk.py -q
cmake --build build/dal379-implementation --target dal_check_generated -j 8
```

结果：49 项 CTest 全通过（含 core 新 16 项、public 新 2 项、Excel 新 5 项及旧回归）；45 项 Python 全通过。Machinist drift 检查通过，core / Excel 均 Wrote 0 files；staged diff 检查通过。最后一次 generation 没有内容变化，复用内容完全相同的构建 / 测试证据。

首次临时配置未采用 preset、使用 shared 库，旧 European Python 的 17 项遇到 tape 身份错误；该失败日志保留。本轮没有基线 shared 对照，因此不判定其引入时间，也不声称该配置已通过；此 shared/AAD 问题未定位或修复。最终按标准静态配置重建后全部 45 项通过，未修改 AAD 代码或弱化旧测试。初次依赖缺失通过初始化锁定版本的 GoogleTest / pybind11 子模块解决，未改指针。

另在 `build/dal379-core-boundary` 实际构建 `european_fd`、`american_put_mc`、`pde_perf`，配置 `DAL_BUILD_PUBLIC=OFF`、静态 Release；链接记录仅 core 静态库 / pthread，没有 public。CTest -N 确认原两项示例和新 benchmark 注册；示例本身未执行。新增 public header 由现有安装目录规则覆盖，未变更安装目标。

**预算实测**

| 案例                      | M / 实际区域 capacity | P_plan / 实际 payload (bytes) | W_plan / 保守实际峰值 (bytes) |
|-------------------------|-------------------|-----------------------------|-------------------------|
| A: N=401, sigma=0.50    | 500 / 100500      | 3418672 / 2141144           | 138880 / 53144          |
| B: N=801, sigma=0.20    | 500 / 200500      | 6625072 / 4147544           | 241280 / 85144          |
| CN-A: N=401, sigma=0.50 | 31876 / 6407076   | 217277488 / 136053912       | 2146944 / 1308184       |

A/B 计划与 B1 完整公式一致；CN-A 显式配置 payload=217277488、max_work_units=1271852400。预算恰等 P_plan/W_plan 通过，少 1 byte 及 M-1 上限均预检拒绝；实际不足的 work budget 返回无价格失败。上述内存是原生数值容量核算，不是价格误差保证。

**定向性能**

只测新增四条路径；未重跑旧九目标回归矩阵。没有历史生产早行权 PDE 基线，不给加速率或回归通过结论。共享本地主机可能受同时编译影响，结果为信息性绝对耗时。

每例保留每进程 2 warmup / 10 repetitions；两轮各十个进程、按 min 归约，DAL_NUM_THREADS=4。可执行 SHA-256 在采样前后均为 `f811e65cdfa5d70273a63f466349189f8d6aac589936ccd48ef9e5432ca25464`。

| 用例          | 轮 1 min (ms) | 轮 2 min (ms) | 实际步 / 求解次数 | work / payload / workspace (bytes) |
|-------------|--------------|--------------|------------|------------------------------------|
| Bermudan    | 4.569        | 4.96         | 501 / 501  | 199899 / 115416 / 53184            |
| American    | 12.475       | 13.184       | 500 / 1549 | 618051 / 2141144 / 53144           |
| Daily       | 37.76        | 40.495       | 730 / 730  | 291270 / 1641616 / 62344           |
| NearZero CN | 0.672068     | 0.726429     | 32 / 64    | 25536 / 143720 / 34424             |

前三例 N=401、ordinary_steps=500；Daily 为每日日程，ceil 分配产生 730 步。NearZero 为 sigma=1e-8、r=-0.2、q=-0.1、theta=0.5、ordinary_steps=1、无阻尼，实际二分为 32 步，覆盖 upwind 与 CN 单调性细分。原始日志与 results.json 保留在交付 evidence zip 中。

**未运行项目与交接边界**

- 不运行全量测试 / benchmark 矩阵；AAD、校准、MC、RNG 等未变更算法的性能用例排除。旧 Theta 和独立 test-support 的两个 PDE 自检已定向运行。
- 本地没有 Windows / Office XLL 运行条件：XLL 编译、实际注册、原始 xltypeInt / worksheet 转换和 Excel 句柄寿命仍需后续 Windows CI。portable 成功不替代这些证据。
- 未单独构造 synthetic 多段 contact 分类输入；代码按所有 run 编码，不假定唯一边界，当前独立 vanilla 例覆盖空 / 单段、贴边及非贴边括区。独立 tester 可继续补充该定向边界。
- 默认网格不是所有市场的精度保证；原始残差与价格误差分别验证。Greeks、局部波动率、自动网格和方法 fallback 未纳入本次实现。
- 当前方法文档、CHANGELOG 和对外示例由已安排的后续 doc-writer 阶段处理；本轮只交付实施与验证记录。标准静态 P3 路径没有本地阻断；shared/AAD 归因及 Windows XLL 仍待进一步验证，交回独立 tester / reviewer。
