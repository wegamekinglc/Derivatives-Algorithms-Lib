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
`45 passed in 0.11s`，不采用旧 `test_output.txt`。原命令 / 配置见
[实施记录](../implementation/dal-379-early-exercise-pde.md)。其中高风险证据核对如下：

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
