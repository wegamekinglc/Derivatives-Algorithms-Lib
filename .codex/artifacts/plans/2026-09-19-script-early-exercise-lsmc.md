# 脚本引擎提前行权（美式/百慕大 LSMC）执行方案

## 来源与背景

- 需求来源：DAL-279 周报提案③，用户已批准进入规划；协调器要求以编号规格深度输出可开工的执行方案（本文），实现阶段另行授权。
- 基准：master `7eff696e`（2026-09-19，含 PR #389 模型绑定推断）。文中引用的代码位置均为该基准下的现状锚点（文件 + 符号，不含行号）。
- 决策链：与刚落地的 FIX 机制（DAL-198…205/208，PR #366–#378）同构——都在事件日期网格上工作；FIX 处理历史观测冻结，本特性在同一网格上做后向归纳。

## 问题陈述

脚本引擎今天严格前向单遍求值（`ScriptProduct_::EvaluateImpl` 按事件顺序树走；`EvalCompiledEvents` 按事件顺序执行字节码），无跨路径通信（唯一跨路径步骤是末尾均值聚合），且无任何按日期×路径的状态存储。因此美式/百慕大权证、可赎回债券、带回售的雪球等**持有方提前行权类产品无法表达与定价**。本方案在不破坏现有 FIX/编译/AAD 语义的前提下，为脚本引擎增加 `EXERCISE` 事件语义与 Longstaff–Schwartz 最小二乘蒙特卡洛（LSMC）后向归纳估值。

## 目标

1. 新事件语句 `EXERCISE <expr> [IF <cond>]`，解析、校验、诊断、持久化边界完整。
2. double（硬决策）与 AAD（fuzzy 平滑决策）两种模式的 LSMC 估值，树走与编译求值器 parity。
3. 回归策略确定性可重放：同种子结果与线程数无关（bitwise 一致）。
4. 三端投影：C++（dal-public 签名不变）、Python（`MonteCarloSettings_` 新 keyword）、Excel（`MONTECARLOSETTINGS.NEW` 新 key）。
5. 测试以测试专用百慕大 PDE 基准与欧式极限为真值锚点。

## 非目标（v1）

- 连续行权的美式（仅在事件日期网格上离散行权）。
- 评估日当天或过去的行权（校验报错；已提前行权的存量合约不可表达）。
- 多资产回归因子（受现有单 EQ 观测/样本约束，回归因子 = 绑定的唯一模型观测 EQ 指数）。
- ITM 过滤回归、非多项式基（Hermite、样条）、分块重放内存优化（列为开放问题）。
- 新增 Excel 工作表函数或改变任何现有函数签名/输出形状。
- 隐含的二叉树或美式 PDE 库面（PDE 基准仅为测试代码）。

## 现状锚点（实现者必读）

- 语句文法只有三种：`IF <cond> THEN … [ELSE …] END`、`<var> = <expr>`、`<var> PAYS <expr>`（`dal-cpp/dal/script/parser.cpp`，`Parser_::ParseStatement`）；保留字表在 parser.cpp 顶部。
- 事件表：输入行 `dates[i]/events[i]` 一一对应；预处理按日期入 `std::map` 排序、同日合并（`dal-cpp/dal/script/preprocessor.cpp`，`AppendEvent`）；过去/未来事件由 `PartitionEvents` 划分，评估日当天事件留在未来侧，当日定盘来源由 `TodayFixingPolicy_` 决定（`dal-cpp/dal/script/settings.hpp`）。
- AST 与 visitor 扩散面：新节点需要覆盖 `dal-cpp/dal/script/visitorlist.hpp` 的 MODIFY/CONST 两个 visitor 列表中所有相关 visitor；DAL-198 的 `NodeFix_` 是最近模板。
- 准备管线：`PreparedScriptBuilder_::Prepare`（`dal-cpp/dal/script/preparation.cpp`）做设置解析→观测收集→绑定校验→模型计划→历史解析→过去事件重放；`ObservationPlan_` 封闭全部 FIX/SPOT 请求；执行模式/平滑/编译位在准备期冻结并以 `UnsupportedExecutionMode` 拒绝违约。
- 仿真：`dal-cpp/dal/script/simulation.hpp` 批大小 8192、`BatchPlan_`、全局 `ThreadPool_`、每 worker `ThreadState_`（RNG/树求值器/编译求值器）；double 与 AAD 两个循环；RNG 支持 `SkipTo(pathIndex)` 确定性重放；AAD 每 worker 录制 + `RewindToMark`/`PayoffRoot`，历史状态经 `SetHistoricalSeed` 成为 tape 输入（DAL-201）。
- 编译求值器：`dal-cpp/dal/script/visitor/compiler.hpp`，栈式字节码（opcode 枚举 `NodeType_`、每事件 `nodeStream_`/`constStream_`），fuzzy IF 走"双分支求值 + 变量混合"（`FuzzyEvaluator_` 与编译 `FuzzyIf` 互为镜像）。
- 线性代数：无 QR/SVD 最小二乘；`Dal::CholeskyDecomposition` / `CholeskySolve(SquareMatrix_<>* a, Vector_<Vector_<>>* b, double regularization = Dal::EPSILON)`（`dal-cpp/dal/math/matrix/cholesky.hpp`）是现成正则化正规方程求解器。
- PDE：`Dal::PDE::Rollback_` θ-scheme 仅欧式，无障碍/PSOR/罚函数；库内无美式测试、无二叉树（grep 证实）。
- 诊断：Describe = `dal.script-product/2`、Explain = `dal.script-valuation/1`，FIX 阶段采用**加键不升版**先例。
- archive：`ScriptProductData` 版本号在 `dal-cpp/dal/script/event.hpp` 的 Machinist storable 标记块（现为 version 2，`manual`）；v1→v2 的"保留旧 reader + 升版 writer"模式由 `dal-public/tests/test_script_archive.cpp` 与 script_engine.md 固定。
- 平滑核：`CSpr(x, eps)`（平滑 `x>0` 斜坡）、`BFly`（平滑等于）在 `dal-cpp/dal/script/visitor/smoothing.hpp`；`smooth_` 为全局默认过渡宽度，比较可带 `;eps` 覆盖。

## 1. 语义规格（REQ-S）

- **S1 行权表达**：行权能力表达在事件层：一个事件至多一条 `EXERCISE` 语句，事件日期即行权观察日。行权日期必须**严格晚于评估日**，否则准备期报 `UnsupportedExerciseDate`（信息含输入行号与日期）。
- **S2 语句语义**：`EXERCISE <expr> [IF <cond>]` 中 `<expr>` 为行权价值（估值单位、与 `PAYS` 同口径、不预先贴现）；可选 `<cond>` 限制该日允许行权的路径。行权判定 = 比较 `<expr>` 与回归 continuation 估计（见 S3），`<cond>` 为假则该路径当日不可行权。
- **S3 行权 vs 持有判定（硬模式）**：在行权日 k，若条件成立且 `h_k > C_k(x_k)`（h 为行权价值、C 为冻结的回归多项式、x 为回归因子观测），则该路径行权：路径最终 payoff = h_k 贴现至评估日，**当日与之后的全部事件对该路径不再执行**；否则按原前向语义继续。
- **S4 与 `PAYS` 的组合边界**：行权价值**取代**行权日当日及之后的全部支付（与语句书写顺序无关，避免顺序依赖语义）；未行权路径当日 `PAYS` 照常。典型用法（autocall 赎回价含票息）由用户在 `<expr>` 中自行表达。
- **S5 未行权语义**：从未行权的路径 payoff = 全部 `PAYS` 之和（现行为），即本特性对不含 `EXERCISE` 的产品零影响。
- **S6 与 FIX 的组合边界**：`ObservationPlan_` 语义不变——行权日及之后的 FIX 请求照常在准备期封闭解析；某路径行权后，其后续观测只是不再被使用，不产生错误、不影响诊断与 `sample_dates`。历史 FIX 值与 past-replay AAD 种子机制（DAL-201）完全不变。
- **S7 与 `TodayFixingPolicy_` 的组合边界**：因 S1 排除评估日当天行权，当日策略只作用于非行权事件的 FIX，与本特性无交叠；文档明确该边界（对齐 DAL-208 的 SPOT 边界文档模式）。
- **S8 历史区边界**：`EXERCISE` 不得出现在解析后日期 ≤ 评估日的事件中（S1 的校验）；同一合约在评估日后移使行权日落入历史区时同样报错，用户需重建合约。v1 不支持"已行权"存量状态（非目标）。
- **S9 fuzzy（AAD）模式判定**：行权决策按现有 fuzzy IF 机制平滑：决策度 `d = CSpr(h_k − C_k(x_k), eps)`（eps 取语句 `;eps` 或 `simulation.smooth_`，同现有比较语义）；带条件时决策度 = 条件度与值比较度的代数积（对齐 `FuzzyAnd`）。路径 payoff = `d · h_k·df + (1−d) · 前向延续 payoff`（延续部分用冻结多项式 C_k 的不同iable代入），与现有"双分支求值 + 变量混合"一致，保证路径可微。
- **S10 确定性与可重放**：同一种子（同路径集合）下，PV、`d_<param>`、`exercise_rate` 与线程数**无关（bitwise 一致）**；回归矩按批次索引顺序归约（对齐现有 `simResults[batchIndex]` 聚合模式）。
- **S11 parity 要求**：同合约/模型/设置下，树走与编译求值器：
  - double（smooth=0）：逐路径行权决策**完全一致**（同一系数、同一比较 → 同一布尔结果），PV bitwise 一致；
  - AAD：PV 与 `d_<param>` 满足现有 parity 容差；
  - 现有 parity/fuzz 套件扩展覆盖 `EXERCISE` 产品（见 T3）。
- **S12 执行模式边界**：含 `EXERCISE` 的产品必须走带模型的准备管线（continuation 回归需要模拟）；history-only 准备与 legacy `PreProcess` 管线遇到 `EXERCISE` 报 `UnsupportedExecutionMode`（消息风格对齐现状）。
- **S13 模型范围**：公开估值门维持 BS/Dupire 不变；回归因子 = 该产品绑定的唯一模型观测 EQ 指数（由现有单索引校验/推断保证），在行权采样日观测。

## 2. 语法与公开 API（REQ-X）

- **X1 语法**：保留字表新增 `EXERCISE`；语句文法 `EXERCISE <expr> [IF <cond>]`（条件文法复用 `ParseCond`，含 `;eps`/`:eps` 选项）。无赋值形式——行权不写脚本变量。
- **X2 AST**：`NodeExercise_ : ActNode_`，子节点 = [价值表达式] + 可选条件节点，携带 fuzzy 元数据（`eps_`/`isDiscrete_`，字段对齐 `CompNode_`）。`Debugger_` 输出节点 JSON：`{"type": "exercise", "value": …, "condition": …, "eps": …}`（加键，不升版）。
- **X3 语句位置约束**：`EXERCISE` 只能作为事件的顶层语句，不得嵌套于 `IF`（报 `UnsupportedExerciseNesting`）；每事件至多一条（报 `DuplicateExercise`）。理由：回归需要按日期收集 (x, h, 条件度) 三元组，嵌套语义在 v1 无法良定义。
- **X4 设置**：`MonteCarloSettings_` 新增字段 `int lsmcBasisDegree_ = 3;`，校验为 1..8 的整数（拒 bool/非整数/越界，校验风格对齐 DAL-204 的输入验证）。v1 不设基类型开关（N3）。
- **X5 诊断**：Explain（`dal.script-valuation/1`，加键不升版）新增 `exercise_events: [{event_id, date, basis_degree, regressor_index, num_coefficients, coefficients[], exercise_rate}]`；`exercise_rate` 为重放 Pass 各行权日实际行权路径占比。Describe（`dal.script-product/2`）的 statements 自然包含 X2 节点 JSON。legacy debug `/1` 继续拒绝含 `EXERCISE` 的产品（消息给出 Describe 迁移提示，对齐 FIX 先例）。
- **X6 archive 版本决策**：`ScriptProductData` v2 字段集（name/dates/events/default_index）无需扩展——`EXERCISE` 活在事件文本里，回归设置属估值设置而非合约。**推荐：不升 v3**；旧二进制读入新合约在解析期得到明确错误，兼容边界按 DAL-208 模式写入 `docs/methodology/script_engine.md`。若评审倾向显式版本门控，则按 v1→v2 配方升 v3（event.hpp 标记块 version 3 → `dal_generate` → 提交 `MG_ScriptProductData_v3_{Read,Write}.inc` 并保留 v2 reader → 写入端默认 v3 → 扩展 `test_script_archive.cpp` 与 script_engine.md 归档表）。见开放问题 Q1。
- **X7 三端投影**：
  - dal-public：`NewScriptProduct`/`ValueByMonteCarlo`/`ExplainScriptValuation` 签名不变，`MonteCarloSettings_` 新字段自然透传；结果 map 仍为 `PV` + `d_<param>`。
  - Python：`MonteCarloSettings_` 新 keyword-only `lsmc_basis_degree`（默认 3，同 X4 校验）；`MonteCarlo_ValueWithSettings`/`ScriptValuation_Explain` 自然支持；legacy 位置参数 `MonteCarlo_Value` 不变（默认阶数）。
  - Excel：`MONTECARLOSETTINGS.NEW` 新 key `lsmc_basis_degree`（未知 key 仍硬错，旧工作簿不受影响）；`MONTECARLO.VALUEWITHSETTINGS`、`SCRIPTVALUATION.EXPLAIN` 自然支持；输出形状不变。涉及 `__scriptsettings.cpp` key 解析与 storable 标记再生成（Windows CI 验证）。
- **X8 示例与文档面**：新示例 `dal-cpp/examples/american_put_mc/`（硬 + fuzzy AAD 两种模式）与 `dal-python/examples/013.exercise_bermudan.py`；`docs/methodology/script_engine.md` 增"Early Exercise"小节、`docs/excel-script-settings.md` 增 key 说明、`docs/public-api.md` 与 `docs/README.md` 索引、`CHANGELOG.md` 条目（T5）。

## 3. 数值方法与数据流（REQ-N）

三阶段数据流，全部运行在现有批式并行框架上，不引入跨路径同步原语：

- **N1 Phase A（前向存储，double）**：按现有批结构生成路径并前向求值，`EXERCISE` 语句对前向求值为 no-op，仅记录每路径每行权日三元组 `(x_k 回归因子观测, h_k 行权价值, 条件度)`；同时正常累积 `PAYS` 至到期，得到初始现金流向量（贴现至评估日，numeraire 口径同现状）。
- **N2 Phase B（后向归纳 + 回归）**：自最后一个事件日期向首个事件日期逐日回走（行权日与非行权日都走）：行权日 k 先对**条件为真**的路径子集累积正规方程矩 `A = ZᵀZ`、`b = Zᵀy`（y = 该路径 k 日后现金流估计贴现至 k；Z 为归一化基，见 N3），`CholeskySolve` 求系数 `C_k`；随后逐路径应用 S3 硬决策更新现金流向量（行权则现金流 = h_k 贴现，丢弃其后支付）。非行权日仅把当日 `PAYS` 折入现金流向量。输出全体 `C_k`。矩阵条件差（秩亏/样本过少，阈值 `nCondTrue < 10 × nCoeff`）时该日退化为常数基或不行权（不崩溃），由 `exercise_rate` 诊断暴露。
- **N3 回归基与归一化**：v1 基 = 归一化变量的 d 阶单项式 `{1, z, …, z^d}`，`z = (x − μ̂)/σ̂`（逐日路径样本均值/标准差，改善正规方程条件数），d = `lsmcBasisDegree_`；不做 ITM 过滤（对全条件为真路径回归）。正则化用 `CholeskySolve` 默认 `Dal::EPSILON`。
- **N4 Phase C（冻结策略重放）**：用 RNG 的 `SkipTo` 确定性重生成同一批路径，再次前向求值；在每个行权日由**驱动层**（非逐路径字节码）计算决策：h_k 与条件度来自树走/编译求值段，C_k(z_k) 用冻结系数，硬模式布尔比较、fuzzy 模式按 S9 混合。PV = 路径 payoff 均值；AAD 模式在本 Pass 重放录制伴随（见 N6）。策略冻结保证路径可微与 S10 确定性。
- **N5 贴现**：日期间折算用现有样本网格 numeraire 比率（`SampleDef_` 的 numeraire），与 `PAYS` 折现口径一致。
- **N6 AAD 模式**：Phase C 完整复用现有每 worker tape 模式（`InitModel4ParallelAAD`/`RewindToMark`/`PayoffRoot`/`SetHistoricalSeed`）；回归系数为 tape 常量；fuzzy 决策混合复用现有变量混合存储（`varStore0_`/`varStore1_` 模式）——挂接细节（求值器内 vs 驱动层）在 T4 设计时定，不构成公共语义差异。输出 `d_<param>` 覆盖模型参数与脚本常量（含经历史 FIX 的 DAL-201 风险链）。
- **N7 内存模型**：峰值 ≈ `nPaths × nExDates × 3 × 8B`（Phase A 三元组）+ `nPaths × 8B`（现金流向量）+ 系数。例：2^20 路径 × 12 行权日 ≈ 300 MB。超预算时用户降路径数/日期数；分块重放（按日期块重生成路径）为后续优化（Q4，v1 不做）。
- **N8 性能预算**：含 `EXERCISE` 产品单次估值为不含行权产品的 ~2.2–2.6 倍（Phase A + Phase C 两次路径生成与求值 + 回归开销）；不含 `EXERCISE` 的产品零回归路径。`script_mc_perf` 基准增加行权场景并纳入 CI 回归门（T3/T5）。

## 4. 里程碑 T1–T5

每个里程碑：范围（引用 REQ）→ 验收标准（可执行）→ 聚焦测试 → 预估工作量。

### T1 语法/语义骨架与诊断（约 2–3 人日）

- 范围：X1–X3、S1/S8/S12 的校验与错误、X2 节点 JSON、X5 Describe/legacy debug 行为；树走/编译求值遇到 `EXERCISE` 临时抛 `UnsupportedExecutionMode`（T2 解锁）。文件面：`dal-cpp/dal/script/{lexer,parser,event,preprocessor,settings,diagnostics}.{hpp,cpp}`、`node.hpp`、`preparation.{hpp,cpp}` 及 visitorlist 中相关 visitor 的最小 `Visit` 扩散。
- 验收标准：
  - 新套件 `ScriptExerciseParseTest` 全绿（下述用例）；
  - 含 `EXERCISE` 产品 Describe JSON 包含 exercise 节点且 schema 仍为 `dal.script-product/2`；
  - 不含 `EXERCISE` 的全部现有测试零变化（`ctest` 全量）。
- 聚焦测试（`dal-cpp/tests/script/test_exercise_parse.cpp`）：文法接受 `EXERCISE expr` / `EXERCISE expr IF cond` / `;eps` 选项；拒绝保留字冲突（变量名 `exercise`）、嵌套 IF、同事件重复、行权日期 ≤ 评估日（`UnsupportedExerciseDate`）、history-only 准备（`UnsupportedExecutionMode`）；Describe JSON 黄金值；legacy debug /1 拒绝并提示迁移。

### T2 树走 LSMC（double，硬决策）（约 5–8 人日）

- 范围：N1–N5、N7 驱动实现（树走求值）；S3/S4/S10；测试专用百慕大 PDE 基准（见下）；Explain 的 `exercise_events` 键（系数在 T2 即输出，`exercise_rate` 来自 Phase C）。
- PDE 基准（test-support，测试专用）：基于现有 `Dal::PDE::Rollback_` θ-scheme 在行权日之间回滚 + 行权日障碍投影 `V = max(V, h)` 的百慕大求解器；**基准自证**：单行权日在到期 → 与 `Distribution`/`AAD::BlackScholes` 欧式闭式一致（沿用 `test_thetascheme.cpp` 对拍先例）；网格收敛阶检验。
- 验收标准（`dal-cpp/tests/script/test_exercise_lsmc.cpp`，套件 `ScriptExerciseLSMCTest`，全部 ASSERT 风格对齐单测规范）：
  - **欧式极限**：仅到期行权的 put 与欧式 put 闭式之差 ≤ 3 × MC 标准误（Sobol 2^18）；
  - **美式 put 对拍**：BS(spot=100, K=100, vol=20%, r=5%/年, q=0)，周行权 52 日型网格，2^18 路径、阶数 3，|MC − PDE 基准| ≤ 0.5% × spot；
  - **百慕大收敛**：路径数 2^16/2^17/2^18/2^20 下 |MC − PDE| 单调不增（弱断言，同种子 CRN）；
  - **线程不变性**：1 线程 vs 默认线程池，PV bitwise 相等（S10）；
  - **零行权退化**：条件恒假的 `EXERCISE` 产品与去掉该语句的普通产品 PV bitwise 相等（S5）；
  - **PDE 基准自证**：欧式极限与网格收敛两组断言。
- 聚焦测试命令：`./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptExerciseLSMCTest.*:ScriptExerciseParseTest.*'`。

### T3 编译求值器 parity（约 3–5 人日）

- 范围：S11——编译模式接入同一驱动：行权价值与条件度用编译段求值（新增最少 opcode：行权表达式的段求值与决策数据回传；决策比较逻辑全部在驱动层，不进字节码）；`compiler.hpp` 若因新增扩散超过单文件复杂度舒适区，将行权相关发射/执行拆至 `dal-cpp/dal/script/visitor/compiler_exercise.hpp`（对齐无重复规则）。
- 验收标准：
  - `ScriptCompiledParityTest` 新增行权产品用例：double 模式树走 vs 编译 PV bitwise 相等、逐路径决策一致（S11）；
  - `ScriptCompiledParityFuzzTest` 的 fuzz 生成器扩展随机生成 `EXERCISE` 语句（单事件/多事件/日程模板三组各不少于一例），parity 容差内一致；
  - 现有 filter `'ScriptCompiledParityTest.*:ScriptCompiledParityFuzzTest.*'` 全绿。
- 聚焦测试：上述 filter + `script_mc_perf` 基准新增行权场景并跑通（回归门接入在 T5）。

### T4 AAD（fuzzy 决策 + 冻结策略重放）（约 4–6 人日）

- 范围：S9、N6——fuzzy 决策混合、Phase C 的 AAD 录制、系数常量化、与 DAL-201 past-replay 的组合（历史 FIX + 未来行权的产品）；四 AAD 后端验证。
- 验收标准（并入 `ScriptExerciseLSMCTest`）：
  - **AAD vs 中心差分**：同种子 CRN（同路径集合），对 spot/vol/rate/div 中心 bump（spot 步长 0.05、其余 1e-3 相对），`d_<param>` 相对偏差目标 ≤ 1e-3、硬上限 5e-3（fuzzy 过渡带与平滑核差异的余量，超限在测试中注明原因并收紧 smooth 复验）；
  - **决策平滑性**：`exercise_rate` 随参数微扰连续（相邻 bump 的 rate 变化有界）；
  - **FIX × EXERCISE 组合**：历史定盘（全局 store 与显式快照两源）+ 未来行权产品，AAD 参数风险经历史值链路保留（对齐 `test_past_replay.cpp` 断言风格）；
  - **模式一致性**：AAD 模式 PV 与 double 模式（smooth→0 极限）在容忍带内一致；
  - CI 四后端（AADet/XAD/Adept/CoDiPack）矩阵全绿（提交触发，无需本地全跑）。
- 聚焦测试：`--gtest_filter='ScriptExerciseLSMCTest.*'` + `test_past_replay.cpp` 回归。

### T5 绑定与文档（约 2–3 人日）

- 范围：X7 三端投影（Python keyword + 校验、Excel key + storable 再生成）、X8 示例与文档、`CHANGELOG.md`、benchmark 回归门接入（`check_benchmark_regressions.py` 清单）、X6 兼容边界文档。
- 验收标准：
  - `dal-python` 新测试（`dal-python/tests/test_exercise.py`）：`lsmc_basis_degree` 校验（默认/合法/非法值，bool 拒绝）、含行权产品 `MonteCarlo_ValueWithSettings` 出 PV、`ScriptValuation_Explain` 解析出 `exercise_events`；
  - `dal-excel/tests/test_script_settings.cpp`：`MONTECARLOSETTINGS.NEW` 新 key 解析、未知 key 仍硬错（portable 合同测试）；
  - `check_docs.py` 与 doc-integrity CI 绿；两个新示例可运行且数值与 T2 基准一致；
  - 四后端 + Windows CI 全绿；benchmark 门无回归（N8 预算内）。
- 总工期估计：T1–T5 合计约 16–25 人日（不含评审往返）。

## 5. 风险与依赖

- **R1 内存规模**（N7 公式）：大路径数 × 多行权日期产品内存线性增长；v1 以文档与诊断（`exercise_events` 暴露 nPaths/nExDates）缓解，分块重放为后备（Q4）。依赖：无新第三方库。
- **R2 回归偏差与基选择敏感**：低阶多项式对陡峭 continuation 可能欠拟合；缓解：阶数可设（X4）、`exercise_rate`/系数诊断、T2 收敛断言；ITM 过滤与更优基列为 Q3。
- **R3 fuzzy 行权平滑偏置**：过渡带 d∈(0,1) 的混合可能轻微抬高/压低 PV（S9）；缓解：默认 smooth=0.01 相对行权价值尺度很小、T4 AAD-vs-bump 容差带量化偏置；大 `;eps` 的交互列入 Q2。
- **R4 `compiler.hpp` 复杂度**（现约 950 行）：新增扩散若失控则拆 `compiler_exercise.hpp`（T3 范围内）；决策逻辑留在驱动层，字节码只新增最少发射，控制复杂度增量。
- **R5 PDE 基准的正确性**：基准错误会误导全部对拍；T2 将"基准自证"（欧式极限 + 网格收敛）设为与对拍同级的必过断言。
- **R6 Excel 设置再生成**：storable/UDI 标记再生成涉及 `dal_generate` 与 `dal_check_generated`、Windows CI 验证（PR #380 后该链路健康）。
- **R7 Phase A 存储的并行局部性**：三元组由驱动层集中存储，批间写偏移固定，无锁；若 T2 基准显示带宽瓶颈，按批块布局优化（实现细节，不阻塞）。
- **依赖**：`CholeskySolve`（现有）、RNG `SkipTo` 重放（现有）、fuzzy IF 混合机制（现有）、`ThreadPool_`（现有）。无新外部依赖。

## 6. 开放问题

- **Q1 archive 是否升 v3**（X6）——当前倾向：不升，解析期明确报错 + 边界文档。
- **Q2 用户显式大 `;eps` 与行权决策的偏置提示**——当前倾向：v1 不警告，文档说明。
- **Q3 基函数族与 ITM 过滤**（N3）——当前倾向：v1 单项式全路径回归；按 T2 数据再议。
- **Q4 分块重放内存优化**（N7）——当前倾向：v1 不做，预留驱动层接口。
- **Q5 评估日当天行权**（S1 排除）——当前倾向：维持排除；有真实需求时允许 `REQUIREHISTORICAL` 下的确定性决策。
- **Q6 `EXERCISE` 与未使用节点 `NodeCollect_` 的关系**——无关，不动。

## 7. 特性级验收汇总

全部满足即视为特性交付：

1. T1–T5 各节验收标准全绿（filters：`ScriptExerciseParseTest.*`、`ScriptExerciseLSMCTest.*`、`ScriptCompiledParityTest.*:ScriptCompiledParityFuzzTest.*`）。
2. 全量 `ctest`（Linux）无回归；不含 `EXERCISE` 的现有产品行为 bitwise 不变。
3. CI：四 AAD 后端矩阵、Windows（含 Excel 构建）、doc-integrity、benchmark 回归门全绿。
4. 三端可用性：C++/Python/Excel 均能表达并估值一个百慕大行权产品（示例落地 X8）。
5. 文档与 CHANGELOG 就位；本方案文档在特性合入后按 artifacts 惯例退役，现行状态并入 `docs/methodology/script_engine.md`。
