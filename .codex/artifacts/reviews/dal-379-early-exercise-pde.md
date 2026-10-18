# DAL-379：提前行权 PDE 聚焦代码主审

## Findings

### P2：XLL 入口提前拒绝整块空 settings，违背原生默认值契约

位置：`dal-excel/src/__earlyexercisepde.cpp:20`。

在 Excel 中将全空的 `A1:C2` 传给
`EARLYEXERCISEPDESETTINGS.NEW("s",A1:C2)`，应按默认设置创建句柄。
active API 第 8 节规定整块空范围表示省略可选参数；新增
`dal-excel/tests/test_early_exercise_pde.cpp:160` 的
`TestEntireBlankOptionalRangeUsesDefaults` 也明确接受 `Matrix_<Cell_>(2,3)`。

但新增 markup 在通用矩阵转换前调用 `ValidateRiskRequestSettings`：

- `dal-excel/auto/MG_EarlyExercisePdeSettings_New_public.inc:14` 已生成同一调用。
- `dal-excel/src/__riskrequestinput.hpp:21` 先调用 `ValidateScriptSettingsRange`。
- `dal-excel/src/__scriptinput.hpp:57` 只展开 1×1 multi；2×3 的 `xltypeMulti`
  不会被 `ScriptInputBlank` 识别为全空。
- 随后的 `dal-excel/src/__scriptinput.hpp:76` 无条件要求两列，因而先报
  `expected 2 columns`，到不了 `Rows` 的整块空检测
  （`dal-excel/src/__earlyexercisepde.cpp:277`）。全空的多行单列范围同样受影响。

这是新入口与既定契约的静态可确定差异，不是 Windows 实测失败。
现有 portable 测试直接调用 typed 函数，绕过这段 `_WIN32` 包装，因而不会发现问题。
应在 P3 的原生输入验证中先识别整块空范围，再对非空范围执行形状和类型检查；
保留半空行、未知键和重复键的拒绝行为，通过 Machinist 重生成。
定向回归应输入真实 OPER 全空 2×3 / 多行单列范围并经过 XLL 入口，另保留非空坏形状反例；
只增加或重跑 typed portable 用例不能关闭此项。

### P3：独立 CRR oracle 的错误路径使用裸标准异常

位置：`dal-cpp/tests/math/pde/test_early_exercise_pde.cpp:219`。

无效风险中性概率直接 `throw std::runtime_error`，违反
`.codex/references/code-style.md` 的 DAL 宏异常约定，也丢失 DAL 的位置上下文。
改为 `REQUIRE` 保持当前严格拒绝行为即可；不要 clamp 概率或改变 oracle 算法。
这是非阻断风格项，没有证据表明已执行的 CRR 参照无效。

## Open Questions

没有新的 API 或数值方法决策需要作者回答。P2 有明确修复方向。
Windows XLL 的编译、注册、真实 worksheet/OPER 转换和上游句柄释放后的寿命仍需 CI
提供证据；本轮 Linux 静态审查及 portable 通过不能替代这些运行结果。

## Tests

审查精确范围为 `f721478ccb8284a48ea144eae828c37e43dc9142..0b903a023adaa414f4c268cfc472678d0ccdccc1`，
HEAD 确认为后者，分支为 `agent/dal-orchestrator/9f36be3b414c`。
完整阅读全部 64 个改动文件，包括 enum / Java / Excel 生成物、测试、构建注册及实施记录，
并阅读 active API、critique 的 B1 最终复核、implementation、validation 和评审/组件约束。
这是本地提交审查，没有 PR 元数据或 GitHub review 操作。

本轮核验：

- 用 Python `hashlib` / `git show` 逐文件核对：64 个工作副本文件全部匹配指定 HEAD blob；
  5 个 core/public/portable/Python/benchmark 二进制全部匹配 tester 的
  `build/dal379-validation/provenance-final-check.json`。
- API / critique SHA-256 仍分别为
  `67032d299b4386568d377a151c28f6b206d7052f8fccffce64657c1e3c7a46e0` /
  `cfadaca7366d0669b3d2fdd9b28f4a4c5ae00558fbf38f40ecbf3c5466a2f37a`。
- `git diff --check f721478c..0b903a02` 通过。P2 沿实际 markup → generated stub →
  原生 validator → typed 函数逐层核对；没有另写模拟算法来声称 Windows 通过或失败。

复用的通过证据：标准静态 Release，Linux/GCC 15.2、C++17、native AAD、Eigen OFF、
CPython 3.13.9 / pybind11 3.0.4；`build/dal379-accepted-ctest.log` 为
`100% tests passed, 0 tests failed out of 49`，`build/dal379-accepted-pytest.log` 为
`45 passed in 0.11s`。没有重跑这些集合或性能矩阵。

已核对独立解析/CRR 与小型 active-set 枚举、分离时空收敛和扩域、日期/carry、原始残差及失败无价格的
测试实现。A/B/CN-A 的实际容量账本和预算等号/少一字节/工作耗尽证据适用；CN-A 实际执行
31876 步，未将预算公式当作运行结果。core 关闭 public 时的示例/benchmark 构建与链接证据适用，
新 benchmark 只包含 core 头文件并链接 `dal_library`；没有反向 facade 依赖或示例迁移。
Machinist 一致性原日志通过，19 个新增 Excel 函数的生成声明、调用、返回和注册与源码对应。

tester 的 8 项原分类器 contact 探针及截断变异 RED 直接复用，未将注入的 scratch 数据当成金融价格证据。
四类新路径性能采样只有信息性绝对耗时，未声称历史性能回归通过。
shared/AAD 的相同环境当前/基线对照表明是既有混合链接缺陷，本轮无新增影响证据，不扩成 P3 修复项；
完整 shared 集合与 Windows XLL 仍未验证。

## Summary

核心实现未发现新增阻断：日期身份驱动事件计划，carry 段归属与投影先后符合契约；American 使用
固定未加 penalty 的归一化 LCP 残差和实际线性误差，work/迭代失败不发布价格；预检保留完整区域容量，
owning C++ / Python getter 与 GIL 前复制的寿命路径一致。上述结论只覆盖已审范围和适用证据，
不代表任意市场上的默认网格价格精度保证。

后续文档阶段必须同步的事实：

- `docs/methodology/pde/framework.md:344` 的 “American Obstacles Are Test-Only” 及没有公开
  Bermudan/American pricer 的说法已过时；`docs/methodology/pde/option-pricing.md:69` 将生产
  提前行权仅归于 LSM 的说法，以及 PDE 目录说明也需同步。原 `ThetaScheme_` 和独立 test-support
  helper 的职责仍应准确描述。
- 公共 C++、Python、Excel 导航/用法与 `CHANGELOG.md` 需记录这一新增能力，说明 ACT/365 精确日期、
  分段连续 carry、默认 fully implicit 的一阶时间精度、可选 CN 的预算代价、有限 upper 近似、
  残差与价格误差的区别、owning 与失败无价格语义，以及首版不提供 Greeks。
  按委托交由下一阶段 doc-writer 决定组织方式，本轮不修改文档产品内容。

本轮只新增本 review 并增量更新索引，保留既有未提交 API / critique / validation / README 内容；
不改产品代码、环境、分支或 PR，不提交 GitHub review，不触发其他角色。
父任务仍为 `in_progress`，交回 orchestrator 安排 P2 修复及后续文档收尾。

**Verdict: Request Changes**

## 2026-10-18：P2 定向复核

### Findings

本轮未发现新问题，**原 P2 代码 blocker 已解除**。本节仅审查
`0b903a023adaa414f4c268cfc472678d0ccdccc1..318ccc576590a2cbe9c59189c08d8574ee0d2e25`
及直接影响，当前 HEAD 确认为后者，沿用原分支；不重审整个 P3。
本节取代首轮 P2 的未关闭状态，首轮证据与原 verdict 保留。
原 CRR P3 风格项仍未修改，保持非阻断。

- `dal-excel/src/__earlyexercisepdeinput.hpp:9` 的专用 validator 对整个 multi 执行
  `all_of(ScriptInputBlank)`；nil、missing、空字符串及其混合范围才提前返回。
  数字零、false、非空文本或 error 均不算空，任一非空单元格使原严格 validator 继续运行。
  未裁剪物理范围，也未放宽共享 validator。
- `dal-excel/src/__earlyexercisepde.cpp:20` 与
  `dal-excel/auto/MG_EarlyExercisePdeSettings_New_public.inc:14` 均改为同一专用入口。
  整数归一化仍先执行，随后仍以 greedy 矩阵转换保留原矩形，再由 typed 层的整块空检测使用默认值。
  注册名、参数、返回和其他生成入口不变。
- `dal-excel/tests/test_script_raw.cpp:943` 覆盖真实 OPER 的全空 2×3 / 6×1，
  三种空类型分别通过 XLL constructor 与 getter 核对 401 / 500 / theta=1 默认值；
  `:976` 对两种形状逐格放入零、false、文本、error，并覆盖半空行、缺键/值、
  重复/未知键、非法类型和 NUL；`:1034` 检查 xltypeInt 和空 upper。
  它们调用 `:50` 的 `GetProcAddress` helper，实际指向 `dal_excel.xll` 的导出函数，
  并沿既有 `Output_` / `xlAutoFree12` 路径释放 OPER，未改成直接调用 typed 函数。

### Open Questions

无新的修复或 API 决策问题。上述三项 Windows 原生入口测试**尚未在 Windows 编译或运行**；
Linux SDK 声明 shim 的 syntax check / validator 探针和 portable 测试均不替代它们。
实际导出调用、注册、转换及释放行为仍待 Windows CI，**本轮不宣称整体 merge-ready**。

### Tests

核对全部五个改动文件与实施记录、探针源码/日志、生成物及既有 Windows 测试配置。
五个文件同时匹配精确 HEAD blob 与 `build/dal379-p2-probe/provenance.json` 的 SHA-256。
API / critique / validation 哈希未变；core / public / Python 的提交间 diff 为空。
`git diff --check 0b903a02..318ccc57` 通过。

直接复用实施者的适用证据，未重跑集合或创建构建环境：

- 原 validator 的全空 2×3 探针 RED 为 `expected 2 columns`；专用 validator 同输入 GREEN。
  扩展探针接受 8 个全空范围并拒绝 36 个非空坏形状。探针包含生产头文件与仓库 OPER 定义，
  shim 只提供 SDK 声明；它不是 Windows ABI 或 XLL 执行证据。
- `build/dal379-p2-probe/excel-ctest.log` 为 16/16 通过（0.13 s），包含提前行权、
  European Excel 和通用坏形状检查。当前 portable 二进制 SHA-256 仍为
  `e49586aa726164120e7cdb474bb8c1e894428f428cfbe6e72b2dbd3741d05965`，
  与首轮接受的二进制相同；新增 raw 用例在 Linux 被 `_WIN32` guard 排除。
- Machinist 首次更新一个 Excel stub，一致性检查通过；core 的临时重写由既有 normalization
  恢复，最终没有 core diff。原数值、预算、contact、性能及 standalone core-build 证据继续适用。

Windows 注册链已静态确认：`dal-excel/CMakeLists.txt:50` 的测试选项默认开启，
`:201` 的 glob 包含该 raw 文件，`:225` 链接 XLL，`:227` 通过 `gtest_discover_tests` 登记。
`.github/workflows/cmake-windows.yml:135` 开启 Excel，`:202` 的 CTest 筛选不会排除这三项测试。
这是配置/源码注册核对，不是 Windows 的实际发现清单或 CI 结果。待 CI 可定向执行：

```sh
ctest --test-dir build/Release-windows -C Release --output-on-failure --no-tests=error \
  -R '^ScriptExcelRawTest\.TestEarlyExercisePdeSettings'
```

### Summary

P2 修复与原契约一致，已补经过真实导出入口的回归并接入现有 Windows 测试目标；
无需为此重启 P3 数值/预算/性能验证。仅追加本节和更新索引，首轮 review 原文字节保留，
其他未提交产物及产品代码不变。交回 orchestrator 进入文档阶段；父任务保持 `in_progress`。

**本轮 Verdict: Approve**（仅 P2 修复及直接影响；Windows 原生验证仍待 CI）。

## 2026-10-18：benchmark 迁移与 Codacy 修复增量主审

### Findings

本轮未发现新增代码问题或生产 blocker。审查范围为
`0b5b675556287b9565564bea779a53dd0d879b77..1e2bc1c00b3a61fc35fa271fc7ea7271dd9649af`。
PR #602 的 `headRefOid`、本地 HEAD 和 15 个增量文件的 Git blob 均核对为后者；
沿用 `agent/dal-orchestrator/9f36be3b414c`，目标 `master`。
完整读取改动文件、PR 元数据/文件清单、active implementation/validation、当前正式方法指南，
并对照基线；没有重新开展旧功能全审。首轮与 P2 复核记录保留。

以下为本次结论的源码依据，不是新增 Findings：

- `dal-cpp/examples/methods/american_put_mc/american_put_mc.cpp:111` 改用 core
  American LCP；精确 evaluation/maturity、r/q/vol、601 节点、upper=300、600 ordinary
  steps、显式 CN/无 damping 均保持。预算明确容纳单调性细分和完整区域输出，失败立即拒绝价格。
  `dal-cpp/examples/methods/american_put_mc/CMakeLists.txt:4` 仍只链接 core `dal_library`；
  没有 facade include、反向依赖或搬迁。`dal-cpp/benchmarks/pde_perf/pde_perf.cpp:108`
  与 `:114` 的计时/诊断入口原已使用新 API，四条提前行权路径及独立 European 路径保持。
- `dal-python/benchmarks/dal_comparisons/exercise_pde.py:17` 用整数日长计算每段最小步数，
  `:44` 显式取八倍，`:45` 构造精确 dated Bermudan，`:49` 保留 maturity 结束的 r/q 段。
  `:52` 明确 theta=1、无 Rannacher，并单独给执行预算；`:59` 拒绝失败结果，没有 fallback、
  重试或放宽残差。canonical spot=strike、explicit upper=4K 对该 Bermudan 表面有效，
  调用者仍在实际 spot/bumped spot 插值。
- `dal-python/benchmarks/dal_comparisons/exercise_scenarios.py:79` 保留 546 天的 91/7 天日程；
  weekly “American” 比较行仍是相同日期上的 Bermudan。
  `dal-python/benchmarks/lsmc_rqmc_coverage.py:41` 保留跨 547 天的原始精确日期和不等区间；
  `dal-python/benchmarks/compare_lsmc_policy_sensitivities.py:66`、`:198` 的 PV/六个 bump
  均经该入口。European 解析行和 low-vol 独立 CRR 选择不变。全调用搜索未发现遗留的
  benchmark 提前行权 PDE 内嵌实现，独立 test-support oracle 没有接入生产 PDE。
- `dal-cpp/dal/math/pde/earlyexercisepde.cpp:152`、`:198`、`:206` 的计划与边界拆分保留
  exercise/carry 日期归属、候选点和运算顺序；`:286`、`:311` 保留三角回代及线性误差定义。
  `:499` 的 RHS、`:514` 的 payoff correction、`:588` 的原始未加 penalty LCP/线性残差
  及失败优先级与基线一致。`:597` 仍在 solve 前检查 work 并计入失败尝试，`:629` 只在成功
  状态下继续，`:847` 只在成功后发布价格。Contact/区域扫描 `:653`、`:661` 的条件、容量
  和 clipping 不变。私有按 style 特化没有改变公开类型、默认值或 owning/getter 契约。
- `dal-cpp/dal/math/pde/earlyexercisepde.cpp:467` 新固定 context 比旧 Scratch 多 88 bytes；
  `:770` 已计入它，`:774` 仍核验实际 workspace 不超过原计划包络。十个 double scratch
  数组、payload 和最坏区域 reserve 不变；`:461`、`:466` 限制两个临时 descriptor 的尺寸，
  使其落在原固定标量 allowance 内。
- `dal-excel/src/__earlyexercisepde.cpp:275` 的 `read` 原为 typed callback；改名
  `consumeRow` 后，`:286` 的两次矩阵访问仍受两列检查和行界限控制，没有删除边界保护或
  压制安全规则。`:357` 起的字段解析拆分保留原校验、必填/重复/未知键和验证后提交行为；
  `dal-excel/src/__earlyexercisepdeinput.hpp:19` 只提取原标量类型谓词，原 P2 专用空设置
  validator 保持。markup/生成入口没有本轮内容差异，原 Machinist 一致性证据仍适用。
- `dal-python/benchmarks/tests/test_exercise_comparisons.py:33`、`:65` 覆盖实际 API 调用、
  精确日期、carry、显式方法和不等区间密度；`:90` 的 policy 回归用非 canonical 查询 spot
  和线性合成表面核对 PV 加六个 bump 的七次 API 调用/参数/插值。
  绕过 API 但返回相同数值的变异会失败，故能保护本次迁移；合成值不充当金融精度证据。
  `dal-public/tests/test_early_exercise_pde.cpp:25` 起的独立小 LCP 仍自行组矩阵并枚举全部
  八个 active set，抽取 helper 后保留原断言和 fatal-failure 传播。
  `dal-cpp/tests/math/pde/test_early_exercise_pde.cpp:213` 的 CRR 仍独立计算；原裸异常已改为
  REQUIRE，首轮非阻断风格项随此次修复关闭。

### Open Questions

无新的代码/API 决策或 blocker。既有
`dal-python/benchmarks/dal_comparisons/compare.py:204` 仍输出旧 CN/每日 4 步说明，按委托
保留为非阻断文档收尾。文档需统一说明 explicit fully implicit、八倍输入密度、dense/coarse
实际至少 32/16 步每日；README 的 coarse “two-step-per-day” 用语宜明确是放大前请求值。
这是一组 benchmark 的精度配置，不代表一般市场误差保证。

已记录的 Codacy SUCCESS 属于实现修复提交 `6d09fc37`，不冒充最终 head required checks。
本轮 PR check 快照没有证明 `1e2bc1c0` 的全部原生 Windows 作业和三项真实 OPER/XLL
入口回归已通过；wheel、Linux shim、portable 都不能替代它们。
本结论不宣称整个 PR merge-ready。最终 head 的真实 CI、全部未解决线程及合并由
orchestrator 按既有授权统一审计；本轮不等待/轮询外部 CI。

### Tests

本轮补充的是只读来源/结果核对，没有重跑已验证集合或创建环境：

- 15 个增量文件逐一匹配精确 HEAD blob。delivery source manifest 在当前副本仅有预期的
  index 与 tester 新增路由测试两处变化；`6d09fc37..1e2bc1c0` 只有这两处与追加 validation，
  生产实现不变。当前六个测试/binding/基线/standalone 示例 binary 的 SHA-256 均匹配
  tester 的 `independent-audit.json`。路由测试文件哈希为
  `7b6367f7aded520aae9b2c4257bdcb6f2ac8d112c36e253b57b817e5e0e4fc0e`。
- 独立递归复核已重放的八组完整 JSON，共 58,836 个标量；除八处
  `execution.peak_workspace_bytes` 各加 88 外完全相同，包含成功/失败、CN、零/近零 vol、
  日期、carry、区域、残差、work 和失败 getter 状态。
  布局日志为 Scratch=248、Rollback=336、StepOperator=48、AssemblyArrays=64。
- AST 对照确认所有原有 benchmark test 函数保持，原 PV/Delta/Vega/Rho coarse-dense
  门槛 `0.0005/0.0001/0.005/0.005`、CRR 价格 `0.003` 和 European 解析 `0.001`
  没有放宽。原 dense CN 的严格线性误差失败在不可变基线上同样复现；显式隐式加密是配置
  选择，没有修改求解器容差或把失败输出当价格。

复用并核对本实现对应的原始日志：34/34 定向 CTest、11 binding pytest、20 benchmark
pytest；tester 新增 policy 路由 1 GREEN/绕过变异预期 RED，当前分类代码八个 contact
GREEN/截断变异预期 RED。复用 Lizard 原阈值、Flawfinder RED/GREEN、生成一致性和
standalone core-only 实际 build/link/run（示例 PDE=7.0264）证据。
capacity 日志中的 A/B/CN-A 实际 workspace 为 53232/85232/1308272 bytes，均在原包络内。

四条性能路径继续采用已独立归约的两轮 paired 原始输出，原每轮 4% gate 不变：Bermudan
`-10.716%/-11.162%`、American `-6.402%/-5.797%`、Daily `+2.183%/+0.029%`、
NearZero `-4.655%/-4.559%`，没有新增疑点要求再采样。
`git diff --check` 通过。shared/AAD 既有混合链接边界不扩入本增量。

### Summary

benchmark 调用迁移、组件方向、日期/carry 和显式数值配置与声明一致；数值函数拆分保留
原残差、失败、区域、预算语义，新测试能防止迁移被绕过。仅追加本 review 并更新其索引，
原记录逐字节保留，implementation/validation 与产品代码不改。交回 orchestrator 进行
必要文档收尾及最终 head 门禁审计；父任务保持 `in_progress`。

**本轮 Verdict: Approve**（仅上述增量；整体合并仍依赖最终 head 原生 CI 与收尾审计）。
