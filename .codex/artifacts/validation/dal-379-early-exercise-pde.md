P3 独立定向验证，2026-10-18。验证提交 `0b903a023adaa414f4c268cfc472678d0ccdccc1`，
对照基线 `f721478ccb8284a48ea144eae828c37e43dc9142`；沿用
`agent/dal-orchestrator/9f36be3b414c`。未改生产代码、正式测试集合或生成文件，
未创建分支、工作树、提交或 PR。新增内容只有本记录、索引和隔离构建中的运行时验证探针。
API / critique 的既有未提交文件保留。

## 独立结论

- 标准静态 Release 的 P3 数值、原始残差、失败无价格、预算及兼容性证据可以复用。
  附件内的 49 项 CTest / 45 项 Python 均通过；本轮没有重跑该集合或性能矩阵。
- 原 shared/AAD 错误已归因：**基线已有的默认混合链接配置及 DSO tape 身份分裂，
  不是本次 P3 回归**。未设置 `BUILD_SHARED_LIBS` 时，旧 European 最小 pytest
  在当前与基线各失败 1 项，错误与原 17 项失败日志相同。
  显式 `BUILD_SHARED_LIBS=ON` 的干净构建中，该用例在两边各通过 1 项。
  这些结果不能扩写成整个 shared 集合通过。
- 直接执行原生产分类器的 8 个合成探针通过，覆盖多段、最坏 run 容量、空接触、
  clipping、括区、每层索引 / offset、零 payoff 与阈值边界。
  运行时模拟“只保留第一段”时断言失败，撤去该变异后通过；没有为测试改生产实现。
- 可交回 orchestrator 安排 reviewer / doc-writer。未设共享开关的 Linux Python
  配置仍有实际失败，需要生产 / 构建修复时由 orchestrator 派 implementer。
  Windows XLL 编译、注册、真实类型转换及句柄寿命仍缺本地运行环境，留后续 CI。
  父任务保持 `in_progress`。

## 复用证据及来源核验

先读 active API、B1 最终 critique、实施记录及附件 `dal379-evidence.zip`
（attachment ID `01a14e23-8df9-7e00-832b-6013ec37df2b`）。通过 CLI 下载的附件与
本地原始 zip 字节相同，SHA-256 为
`6e37012e758cd2da7cc65b9b52d216794c34c81d005f0a1438c531d958f6ae65`。

`dal379-executable-identities.json` 的 64 个源码哈希全部同时匹配工作副本及
上述精确提交的 Git blob；5 个测试 / Python / benchmark 二进制哈希也全部匹配。
控制记录 API SHA-256 为 `67032d299b4386568d377a151c28f6b206d7052f8fccffce64657c1e3c7a46e0`，
critique 为 `cfadaca7366d0669b3d2fdd9b28f4a4c5ae00558fbf38f40ecbf3c5466a2f37a`，均保持不变。
核验结果保留在新证据包的 `reused-identities-check.json`、`provenance-final-check.json`。

采用 `dal379-accepted-ctest.log` 的新鲜采集摘要
`100% tests passed, 0 tests failed out of 49` 及 `dal379-accepted-pytest.log` 的
`45 passed in 0.11s`，不采用旧 `test_output.txt`。原命令 / 配置保存在 Git 历史的
`.codex/artifacts/implementation/dal-379-early-exercise-pde.md`
（完整控制记录快照 `a9e6077913e288954085b24b345942fac3873b35`）；
该已完成记录已退役。以下高风险证据和验证结论保持不变：

- CRR 常 carry put / yield call 和对齐日期 Bermudan：oracle 自行构造树和最优停止，
  无生产 PDE 系数、boundary 或 penalty 调用；有多树深、奇偶带宽及价格阈值。
  public 小网格 LCP 使用独立 active-set 枚举。
- 日期 / carry 解析极限、原始 LCP 与线性残差、penalty / WORK_LIMIT 等失败无价格、
  负利率 / 近零 vol、分离空间 / 时间收敛和扩域均在上述精确二进制的 49 项内。
  `dal-cpp/test-support/bermudan_pde.hpp` 的 Git blob 在当前和基线均为
  `d203a9b0d4ecfcf97c42118d70fd24436ff78e19`，新生产路径不调用它。
- 预算测试核对 A / B 默认案例、显式 CN-A、相等预算、少 1 byte / 少 1 步拒绝及
  途中 work failure；capacity / 实际 payload / 工作区账本未当作 RSS 测量。
  A、B、CN-A 的 `M / region_capacity` 分别为 `500 / 100500`、
  `500 / 200500`、`31876 / 6407076`，与 API 的整数算式及原 JSON 一致。
  原 `TestBudgetEnvelopeExactBoundaryAndWorkLimit` 确实包含 CN 执行，非只算公式。
- 四个性能用例的 executable SHA-256 为
  `f811e65cdfa5d70273a63f466349189f8d6aac589936ccd48ef9e5432ca25464`，
  与原两轮各 10 进程的采样一致。继续仅作新增路径的绝对耗时信息，没有历史回归结论。
- standalone core-build 的实际目标 / 链接证据仍适用：public 关闭时构建 core 示例
  和 benchmark；没有把 CTest 注册检查误写成示例已经运行。

## 运行已有测试：shared / AAD 最小对照

环境为 Linux x86_64、GCC 15.2.0、CMake 4.2.3、C++17、Release / O3、native AAD、
Eigen OFF、native arch OFF，CPython 3.13.9、系统 pybind11 3.0.4。
当前和基线均从 `git archive <SHA>` 提取到 `build/dal379-validation/src-current`
和 `src-baseline`，无分支 / 工作树。外部子模块目录只读复用同一已初始化源码；
GoogleTest `d72f9c8a`、rapidjson `ea152ded`、machinist `e76b0ef2`、
vendored pybind11 `8a099e44` 未更改；后者未用于 Python 编译。

两边首先使用完全相同的命令，下面 `NAME` 分别替换为 `current`、`baseline`：

```sh
cmake -S build/dal379-validation/src-NAME -B build/dal379-validation/shared-NAME \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
  -DDAL_BUILD_PUBLIC=ON -DDAL_BUILD_PYTHON=ON -DDAL_BUILD_EXCEL=OFF \
  -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DDAL_CPP_BUILD_TESTS=OFF \
  -DDAL_PUBLIC_BUILD_TESTS=OFF -DDAL_CPP_BUILD_EXAMPLES=OFF \
  -DDAL_PUBLIC_BUILD_EXAMPLES=OFF -DDAL_CPP_BUILD_BENCHMARKS=OFF \
  -DDAL_USE_EIGEN=OFF -DDAL_ENABLE_NATIVE_ARCH=OFF
cmake --build build/dal379-validation/shared-NAME --target _dal -j 4
PYTHONPATH=build/dal379-validation/shared-NAME/dal-python python3 -m pytest \
  build/dal379-validation/src-NAME/dal-python/tests/test_european_pde_risk.py::test_owning_small_grid_matches_accepted_financial_prices_and_every_risk -q
```

当前 `1 passed in 0.03s`，基线 `1 passed in 0.06s`。所有构建均前台收集完成。
两边相关的五个旧 AAD / European 源码 / 测试文件哈希完全相同，见
`shared-source-identities.json`。这里仅运行原失败列表中的最小一项，没有扩至 17 项。

继而找到原配置差异，保留 all-shared cache / flags / link / 二进制身份后，
在同一隔离目录只删除共享开关并重建；其他配置保持相同：

```sh
cmake -S build/dal379-validation/src-NAME -B build/dal379-validation/shared-NAME -UBUILD_SHARED_LIBS
cmake --build build/dal379-validation/shared-NAME --target _dal -j 4
# 再执行上面完全相同的一个 pytest
```

当前和基线均 `1 failed in 0.02s`，均在 `recording.cpp:51` 报
`RecordingScope: tape must be the calling thread's default tape`。

原因链已用产物和运行时地址验证：

1. `dal-cpp/CMakeLists.txt` 在变量未定义时只在其子目录作用域执行
   `set(BUILD_SHARED_LIBS ${UNIX})`。core 成为 shared；public 的
   `add_library(dal_public ...)` 在父作用域变量仍未定义时成为 static。
   该构建逻辑基线已有。两边 linker 命令均证实 `_dal` 链接
   `libdal_public.a` 和 `libdal_cpp.so.1.0.0`。
2. `_dal` 使用 pybind11 的 `-fvisibility=hidden -flto=auto`。`AAD::Tape()`
   是头文件内联函数，内部有函数静态 thread-local tape；core DSO 公开其 GNU UNIQUE
   TLS，而嵌入 `_dal` 的代码持有另一份 tape。`/proc/self/maps`、ELF NEEDED
   和链接记录排除了加载旧安装或另一 checkout 的 DAL 库。
3. 不改库代码，以 GDB 在实际 `RecordingScope_(Tape_*)` 入口读取 x86_64 参数。
   同进程 core TLS 地址 `0xa9e038`，构造参数 `0xb1d0c0`，明确不等；调用栈的
   `EvaluateEuropeanPdeRisk` 来自 `_dal`，constructor 来自当前隔离 core DSO。
   然后原身份断言失败。完整记录为 `current-mixed-tape-identity.log`。
4. 显式 ON 时 public 也成为 shared，最小测试通过；标准静态配置也有原通过证据。
   因而不能将“shared”笼统写成失败或通过，失败配置是上述混合链接。

新增 P3 的最小独立日期 / owning pytest 在 all-shared 和混合配置各通过 1 项：

```sh
PYTHONPATH=build/dal379-validation/shared-current/dal-python python3 -m pytest \
  dal-python/tests/test_early_exercise_pde.py::test_exact_date_independent_price_and_detached_owning_results -q
```

这只证明该 P3 路径的 smoke 检查。未修改 AAD、未放宽旧断言、未声称旧 17 项或
完整 shared 集合已经通过。证据包分别保存 `allshared-*` / `mixed-*` 的 cache、
flags、链接、SHA-256、实际 DSO 路径和原始 pytest 日志；构建目录最终停留在混合配置。

## 新增验证探针：合成 contact 编码和括区

生产分类逻辑是 `EvaluateEarlyExercisePde` 内联局部 lambda 和 run 扫描，
没有可输入任意 continuation / payoff 的 API。没有提取另一份算法来冒充验证，
也没有增加生产测试开关或修改金融实现来制造多段市场用例。

`contact_probe.cpp` 用 GoogleTest 调用原 API；同一生产 `.cpp` 以 `-O0 -g -DNDEBUG`
重编译供调试，其余依赖沿用已核验静态库。GDB 在真实 run 扫描前仅向 scratch
注入数值，随后由**原分类器和原结果存储代码**生成结果。每次运行必须命中两个
实际可行权层，否则驱动失败。探针不作价格、LCP 收敛或优化构建性能证据。

```sh
c++ -std=c++17 -O0 -g -DNDEBUG -I dal-cpp \
  -I dal-cpp/externals/googletest/googletest/include \
  build/dal379-validation/contact_probe.cpp dal-cpp/dal/math/pde/earlyexercisepde.cpp \
  build/dal379-implementation/dal-cpp/libdal_cpp.a \
  build/dal379-implementation/lib/libgtest_main.a \
  build/dal379-implementation/lib/libgtest.a -pthread \
  -o build/dal379-validation/contact_probe
python3 build/dal379-validation/run_contacts.py
```

探针先读取 API 与附近已有测试。RED 是 GDB 在运行时模拟扫描仅保留首个 run：
expected 8 条区域而实际 2 条，精确断言失败，证明可击穿单边界 / 丢段违约；
未修改任何生产源码。去掉变异后，Bermudan / American 各四种场景全部 GREEN：

- 非均匀四段 `{0..1, 3..4, 7, 10}`，同时包含左右贴边、内部单节点及多个括区。
- 11 节点交替六段，两层共 12 段，恰好填满计划区域 capacity，没有截断或扩容。
- 全零 payoff 的无差别接触输出空区域，终值也不制造行权区域。
- 精确可表示的 `epsilon=0.125`，检查 gap / dual / continuation 的相等边界，
  排除 gap 超限、负 dual 超限 / continuation 超限及零 payoff 候选。

逐项断言每层 `timeIndex`、从 0 重置的 `regionIndex`、逆回滚存储的正确 offset、
闭区间节点、clipped 布尔值，以及两侧相邻物理网格括区。
这补充的是原作者明确未覆盖的合成分类缺口；未将这些非金融 scratch 数据解释为
真实 vanilla 市场存在多段最优行权，也未把调试探针纳入常规 CTest。

原探针测试最初因 `relative_tolerance=0` 被合法入参检查拒绝，这是探针设置错误，
已改为合法二进制分数使总 epsilon 精确等于 0.125；没有产品失败修复。
GDB 17.1 及依赖通过 `apt-get download` / `dpkg-deb -x` 解压到隔离构建目录，
未安装或更改系统软件。驱动、源码、编译日志、mutation RED、8 份 GREEN 及
源码 / executable SHA-256 全在新证据包中，可复现，不依赖私有生产 hook。

## 修复与交接边界

本轮运行了已有测试、编写了独立运行时验证探针，**没有修复生产或既有测试失败**。
未设 `BUILD_SHARED_LIBS` 的配置是一个真实的基线缺陷；若交付要覆盖这一路径，
由 implementer 修复开关作用域 / 链接一致性，并以本最小用例回归，不能靠删身份检查。
显式统一 ON 或标准 preset 静态配置可作为已经验证的定向配置，不是默认配置已经修复。

Windows XLL 未运行项目保持原边界；portable 通过不能替代 Windows 证据。
本轮不重复已适用的数值、预算、组件构建或性能集合，不触发其他角色、发布或合并。
父任务尚有 reviewer / doc-writer，保持 `in_progress`。

## 2026-10-18 增量验证：benchmark 迁移与数值重构

本节追加于原记录，不覆盖首轮证据。核验精确提交
`6d09fc37ca3455e6387d65e4669ba0b90a6f89b9`，增量基线
`0b5b675556287b9565564bea779a53dd0d879b77`，沿用原分支与 PR #602。
进入本轮时工作树干净，本地 / origin head 一致。**本轮独立定向验证通过，
没有新增数值或生产 blocker；Windows 原生运行仍待精确 head CI。**

仅新增一项 benchmark 路由测试、追加本节并更新索引；未改生产实现、默认值、容差、
构建配置或生成文件，没有提交 / 推送、分支 / 工作树 / PR、合并或触发其他角色。
父任务保持 `in_progress`，交回安排增量 reviewer 与最终精确 head 门禁。

### 来源、复用与本轮补证范围

先读实施记录 `implementation/dal-379-benchmark-ci-repair.md`，通过 CLI 下载
附件 `01a14ea7-9ed3-7291-ab12-f71758ac5064`。下载包与本地
`dal379-benchmark-ci-evidence.zip` 字节相同，SHA-256 为
`96fcd0bbc49a2bbdf9d2e9955e92ebce6bb2f6d6e1433f3c2dcc17741824f8c5`。
作者的 delivery provenance 中 14 个文件全部匹配精确提交的 Git blob；新增测试前也
全部匹配工作副本。最终与基线两个性能 binary 哈希均与 delivery / paired manifest 匹配。
包内另有最初 `0b903a02` 的历史 identity / accepted 日志，未误用它们证明当前重构通过。

复用本 head 的 `dal379-delivery-ctest.log`（**34/34**，4.39s）、
`dal379-delivery-binding-pytest.log`（**11 passed**，0.05s）、
`dal379-delivery-benchmark-pytest.log`（**20 passed, 21 deselected**，13.75s），
以及真实 core-only 构建、示例运行与 RQMC / policy 协议 smoke 日志。
未变范围继续采用原证据，没有重跑这些集合或性能矩阵。

delivery manifest 只列性能 binary 身份，未列最终测试 / Python binary 的哈希。
为补来源缺口，确认对应 targets 均 up-to-date、没有编译 / relink，捕获本轮二进制身份，
并仅重放原八个小型完整结果快照。基线 Python binary 的哈希
`d9df3097915323526400a0ca1eb50fc8282d0e308c6dbd7a86e8851bf5554d6e`
匹配最初保存的不可变 binary；其 core 源码与 `0b5b6755` 的对应 blob 相同。
当前 Python binary 为
`30e27b927c8115ef6de0a666584032f414e5c0eb581daa492b8dd8dc8c0d2a0a`。
完整测试、示例与 binding 哈希见新证据的 `independent-audit.json`。

```sh
cmake --build build/dal379-implementation \
  --target dal_cpp_tests dal_public_tests dal_excel_portable_tests _dal -j 8
cmake --build build/dal379-core-boundary --target pde_perf american_put_mc -j 8
PYTHONPATH=build/dal379-ci-base/dal-python DAL_NUM_THREADS=4 python3 \
  build/dal379-numeric-snapshot.py build/dal379-validation-benchmark/numeric-base.json
PYTHONPATH=build/dal379-implementation/dal-python DAL_NUM_THREADS=4 python3 \
  build/dal379-numeric-snapshot.py build/dal379-validation-benchmark/numeric-head.json
```

以上构建检查全部成功且没有改变任何已交付 binary。
两个重放快照分别与作者 before / delivery 文件完整相等。八组包括 Bermudan、American、
显式 CN、零 / 近零 vol、WORK_LIMIT、MAX_ITERATIONS 和线性精度失败；递归检查
**58,836 个标量字段**，仅八处 `execution.peak_workspace_bytes` 各增加 88，其余完全相同。
比较包括十六进制浮点、全部日期 / 时间层 / 区域 / 残差 / work、价格与失败 getter 状态，
没有用价格相近代替完整结果等价。

### 调用和合约语义核对

- `dal-cpp/benchmarks/pde_perf/pde_perf.cpp` 的四条提前行权路径原已调用新 core API，
  两处 Evaluate（计时和诊断）均保持。默认 European Theta 路径没有被误迁移。
- `dal-cpp/examples/methods/american_put_mc/american_put_mc.cpp` 现在通过 core
  `EvaluateEarlyExercisePde` 获取 American LCP 价格，覆盖 evaluation `2026-09-20`
  至 maturity `2028-03-20`、S=K=100、vol=.20、r=.05、q=0。
  原 601 节点、upper=300、600 ordinary steps、CN / 无 Rannacher 配置保持；
  明确给预算允许单调性细分和完整区域输出。原 sample spot 正好是网格节点，
  取消旧 cubic 插值不会在该调用增加插值误差。
  American 连续行权 PDE 与 weekly MC 近似的语义仍有区别，原独立 MC / 解析断言保留。
  core-only 的实际 build / link 和示例输出 PDE=7.0264 均有交付证据。
- Python 的 comparison → `exercise_scenarios.pde_surface` / `pde_price` →
  `exercise_pde.put_surface` → public `EarlyExercisePdeResult_New` 路径完整。
  2025-01-15 起的 546 天合约、91 天 / 7 天日程及 maturity 不变，均明确请求
  **Bermudan on those dates**；周频 “American” 行没有意外切成连续 American。
- RQMC → `bermudan_put_pde` → 同一适配器，policy 的 `case_reference`、
  `add_reference_greeks` 的六个 bump 也经该入口。原 2026-09-20 / 2027-09-20 /
  2028-03-20 的精确日期、ACT/365 与 r/q/vol/strike 保持；这些日期覆盖 547 天，
  没有错误复用 comparison 的 546 天常量。
  European policy 行仍用解析，low-vol policy 的独立 CRR 选择保持，PDE diagnostic
  本身也通过新 API。
- 适配器固定 canonical market spot=strike 和 explicit upper=4K，使缓存表面不依赖
  query spot；价格依旧在调用者的实际 spot / bumped spot 上插值。
  rate / vol bump 会建立相应的新表面；carry 是 maturity 结束的同一确定性 r/q 段。
- 全部 benchmark / 受影响示例搜索没有发现遗漏的美式 / 百慕大式 PDE 旧实现。
  `script_mc_perf` 只有 MC，没有 PDE 调用；test-support oracle 保持独立，
  未将它接到生产 API。没有新增 facade example 或反向组件依赖。

### Python fully implicit 与精度门槛

复核 dense CN 原始失败和不可变基线复现：两边均为 `LINEAR_SOLVE_FAILURE`，
首步 residual `6.319554597791948e-6`、threshold `1e-12`、time index `279551`。
这是新 API 严格求解检查拒绝该配置的证据，不能把旧 NumPy CN 输出当作同格式成功。
本轮直接采用这组已定位证据，没有再计算 27 万时间层。

适配器现在始终显式请求 theta=1，没有运行时 fallback 或重试。对各 interval 最小步数，
取 `8 * max(ceil(requested_steps_i * total_days / interval_days_i))`，native 的
按日期分配确保每段不少于原请求的 8 倍。comparison dense / coarse 实际为至少
32 / 16 步每日，空间仍为 2401 / 1201；RQMC 不等区间的 allocation 也经过原新增测试。
8 倍是这组 benchmark 的精度配置，不是所有市场的误差保证；调高执行预算没有放宽
任何线性 / LCP 残差阈值。

独立 AST 比较证实原 benchmark 测试文件的 13 个已有函数的 AST 完全相同，
只有实现者新增两个路由 / 日期测试；因此下列原门槛没有被放宽：

- dense / coarse PV、Delta、Vega、Rho 差分别不超过
  `0.0005 / 0.0001 / 0.005 / 0.005`；两种离散行权日程都覆盖。
- dense PDE 对更细 CRR 的 PV 差不超过 `0.003`，European PDE 对解析差 `0.001`。
- 原全部 bump 点的两树深 CRR 差仍为 `0.003`，解析 European / 行权溢价检查保持。

这些金融准入属于已通过的 20 项，继续复用。CRR 自行构造树与 payoff，未调用生产
PDE 系数；core 的独立 CRR 只是抽出 payoff 并把非法概率的裸异常改为 REQUIRE。
public 小 LCP 仍自行组矩阵、枚举全部 8 个 active set，原断言均保留。
没有把共用生产 PDE 的 MC 一致性当成独立 PDE 价格参照。

### 实际存储与性能证据

直接编译包含当前生产 translation unit 的只读布局探针，结果为
`Scratch=248 Rollback=336 Added=88 StepOperator=48 AssemblyArrays=64`。
两个 exercise style 的 context 同大小；十个 N 长 double scratch 不变。
额外 88 字节来自 request / carry 引用、输出 descriptor、容量和 scale 等固定上下文，
确实进入 `fixedWorkspace`；per-step operand / buffer descriptor 有静态尺寸边界，
占用原固定标量 allowance。计划包络、区域 reserve 和 payload 不变。

```sh
c++ -std=c++17 -O3 -DNDEBUG -I dal-cpp \
  build/dal379-validation-benchmark/workspace_layout.cpp \
  build/dal379-core-boundary/dal-cpp/libdal_cpp.a -pthread \
  -o build/dal379-validation-benchmark/workspace_layout
build/dal379-validation-benchmark/workspace_layout
```

核对最终 delivery capacity 日志，而非重构中间版本：A / B / CN-A 的步数是
500 / 500 / 31876，region capacity 是 100500 / 200500 / 6407076，payload 是
2141144 / 4147544 / 136053912 bytes；workspace 是
53232 / 85232 / 1308272 bytes，分别小于计划 138880 / 241280 / 2146944。
payload 也小于原计划 3418672 / 6625072 / 217277488。
恰好 / 少 1 byte 的预算与失败隔离测试包含在本 head 的 34 项内。
这些数字是原生 capacity 账本，不是 RSS / allocator 元数据测量。

性能只复核作者最终 paired 原始日志，没有重新采样。base / head binary 分别是
`f811e65cdfa5d70273a63f466349189f8d6aac589936ccd48ef9e5432ca25464` /
`c342fb19b71530a5b7a9a76f5f08ef09baf28b313a3e96ca8c5a7045e7d8dfdc`。
独立从 40 个进程输出重新归约四条路径的每轮 min，确认每进程 10 repetitions、
源码保留 2 warmup、每侧每轮 10 进程、两轮、4% 原门槛不变。
两轮 delta 依次为 Bermudan `-10.716% / -11.162%`、American
`-6.402% / -5.797%`、Daily `+2.183% / +0.029%`、NearZero
`-4.655% / -4.559%`，均通过。没有忽略初始 RED，shared-host 噪声边界保持。

### 新增 / 更新的最窄检查

新增 `test_policy_reference_uses_public_surface_at_every_greek_bump`，补原测试仅覆盖
comparison / RQMC 路由而未直查 policy 六个 bump 的缺口。
它用一个非 canonical spot=70 和已知线性合成表面，要求 PV 加六个 bump 全部调用
public API，并核对实际插值 spot、精确日期、Bermudan 类型、carry、vol / rate bump
及固定网格。合成数据用于验证路由 / 插值契约，不充当金融价格准确性证据。
最小变异控制让旧式替身给出完全相同的 PV / Greeks 却绕过 API，测试因 0 次调用失败；
真实路径 **1 passed, 41 deselected**。没有生产行为修复。

```sh
PYTHONPATH=build/dal379-implementation/dal-python DAL_NUM_THREADS=4 python3 -m pytest \
  dal-python/benchmarks/tests/test_exercise_comparisons.py \
  -k test_policy_reference_uses_public_surface_at_every_greek_bump -q
```

contact 分类代码已从局部 lambda 移到 `Rollback_::Contact / RecordExercise`，
原八个探针的旧 binary 不足以单独证明当前 refactor。
因此保留原测试数据 / 断言，只更新调试定位到当前方法、重编译当前 `.cpp`，
**8 个 GREEN + 1 个预期 truncation mutation RED** 再次通过。
它仍直接执行生产分类 / 区域存储代码，不引入复制算法或生产 hook。
命令如下；采用原 `-O0 -g -DNDEBUG` 调试配置，不作性能或金融收敛证明。

```sh
c++ -std=c++17 -O0 -g -DNDEBUG -I dal-cpp \
  -I dal-cpp/externals/googletest/googletest/include \
  build/dal379-validation-benchmark/contact_probe.cpp dal-cpp/dal/math/pde/earlyexercisepde.cpp \
  build/dal379-implementation/dal-cpp/libdal_cpp.a \
  build/dal379-implementation/lib/libgtest_main.a \
  build/dal379-implementation/lib/libgtest.a -pthread \
  -o build/dal379-validation-benchmark/contact_probe
python3 build/dal379-validation-benchmark/run_contacts.py
```

未扩大到完整 CTest / Python / 性能集合。

### 余项和交接

非阻断文字遗留：`dal-python/benchmarks/dal_comparisons/compare.py:204`
仍把参考解描述为 “Independent Crank-Nicolson PDE” 和每日 4 步，
实际已是 public fully implicit / 至少 32 步。README 与 CONVENTIONS 已更新，
但 CLI 输出说明未同步；交后续 reviewer / 收尾修正，不更改数值结论。

本轮只取一次精确 head 的真实 GitHub check-runs 快照：Codacy check
`117655729769` SUCCESS，core-only examples SUCCESS；四个 MSVC 原生 build job
仍为 `in_progress`，configure-only SUCCESS 不等于真实 OPER / XLL 测试已执行。
没有把 Windows wheel、Linux shim 或 portable 成功替代原生入口证据，
没有等待 / 轮询 CI。快照保留为 `ci-snapshot.json`，最终 exact-head 门禁由 orchestrator 收集。
本轮新增测试 / 记录尚未提交，未来提交的门禁须匹配新的最终 head，不能沿用 `6d09fc37`
的 SUCCESS 冒充其通过。既有混合链接问题不扩为修复。
