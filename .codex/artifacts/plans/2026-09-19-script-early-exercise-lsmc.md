# 脚本引擎提前行权（美式/百慕大 LSMC）执行方案（修订版 r2）

## 来源与背景

- 需求来源：DAL-279 周报提案③，用户批准进入规划；本版为评审修订轮（api-designer 5 blocker + 9 minor；reviewer Request Changes 5 blocker + 非阻塞项；协调器四项方向性裁定），全部 blocker 并集、两项强烈建议项（回归稳健性、#390 重锚定）与全部 minor 已并入，原开放问题按裁定写死。
- 基准：master `9ee82433`（2026-09-19，含 #390"移除 model bindings、FIX 观测按索引名绑定"）。现状锚点已按该基准复核改写（文件 + 符号，不含行号）。
- 决策链：与 FIX 机制（DAL-198…205/208）同构——同在事件日期网格上工作；FIX 处理历史观测冻结，本特性在同一网格上做后向归纳。

## 问题陈述

脚本引擎今天严格前向单遍求值（`ScriptProduct_::EvaluateImpl` 按事件顺序树走；`EvalCompiledEvents` 按事件顺序执行字节码），无跨路径通信（唯一跨路径步骤是末尾均值聚合），无按日期×路径的状态存储。因此美式/百慕大权证、可赎回债券、带回售的雪球等**持有方提前行权类产品无法表达与定价**。本方案在不破坏现有 FIX/编译/AAD 语义的前提下，为脚本引擎增加 `EXERCISE` 事件语义与 Longstaff–Schwartz LSMC 后向归纳估值。

## 目标

1. 新事件语句 `EXERCISE <expr> [IF <cond>]`，解析、校验、诊断、持久化边界完整。
2. double（硬决策）与 AAD（fuzzy 平滑决策）两种模式的 LSMC 估值，树走与编译求值器 parity（容差语义，见 S11）。
3. 回归策略确定性可重放：同种子下结果与线程数 bitwise 一致（含 AAD 模式；机制见 N9，任务与验收见 T2）。
4. 三端投影：C++/Python/Excel 各增加一个**仿真诊断入口**（X5）；既有 `Explain` 契约与全部现有签名冻结不动（X7）。
5. 测试以测试专用百慕大 PDE 基准、欧式极限与中期行权日基准为真值锚点。

## 非目标（v1）

- 连续行权的美式（仅事件日期网格上离散行权）。
- 评估日当天或过去的行权（校验报错；已行权存量合约不可表达）。
- 多资产回归因子（回归因子 = 脚本声明的唯一模型观测 EQ FIX 索引，#390 后按索引名绑定）。
- mrg32/irn 的确定性重放方案（被 S15 排除；列后续开放问题 F2）。
- ITM 过滤、非多项式基、分块重放内存优化（后续开放问题 F3/F4）。
- 除 X5/X7 明确列出的三端加性诊断入口（`ExplainScriptSimulation` / `ScriptSimulation_Explain` / `SCRIPTSIMULATION.EXPLAIN`）以外的新增输出面；以及任何现有函数签名/输出形状的变更。
- 隐含的二叉树或美式 PDE 库面（PDE 基准仅为测试代码）。

## 现状锚点（实现者必读，基准 `9ee82433`）

- 语句文法仅三种：`IF <cond> THEN … [ELSE …] END`、`<var> = <expr>`、`<var> PAYS <expr>`（`dal-cpp/dal/script/parser.cpp`，`Parser_::ParseStatement`）；保留字表在 parser.cpp 顶部；未知语句形式报 generic "statement without an instruction"。
- 事件表：输入行 `dates[i]/events[i]` 一一对应；预处理按日期入 `std::map` 排序、同日合并（`dal-cpp/dal/script/preprocessor.cpp`，`AppendEvent`）并记录 `SourceOrigin_`（row/offset/事件日期）；过去/未来事件由 `PartitionEvents` 划分，评估日当天事件留在未来侧，当日定盘来源由 `TodayFixingPolicy_` 决定（`dal-cpp/dal/script/settings.hpp`）。
- **payoff 闸门**：`dal-cpp/dal/script/preparation.cpp` 无条件 `REQUIRE2(product->HasPayoff(), "InvalidScriptStructure: dates/events has no PAYS payoff")`；`HasPayoff`/`ContainsPayoff`（`dal-cpp/dal/script/event.cpp`）只认 `NodePays_`。诊断侧 `payoff_index` 仅在 `HasPayoff() && !VarNames().empty()` 时输出（`dal-cpp/dal/script/diagnostics.cpp`）。
- **模型绑定（#390 后）**：`ScriptValuationSettings_::modelBindings_` 与 `ModelIndexBinding_` 已删除；引擎将模型 spot 输出绑定到脚本自身声明的未来 FIX 索引，多个不同未来索引报 `MultipleModelIndices`（`dal-cpp/dal/script/preparation.cpp` 的 `InferBinding`）；Python `model_bindings` 参数与 Excel `SCRIPTVALUATIONSETTINGS.NEW` 第三参数已移除。
- 准备管线：`PreparedScriptBuilder_::Prepare` 做设置解析→观测收集→索引绑定→模型计划→历史解析→过去事件重放；`ObservationPlan_` 封闭全部 FIX/SPOT 请求；执行模式/平滑/编译位在准备期冻结并以 `UnsupportedExecutionMode` 拒绝违约。
- 仿真：`dal-cpp/dal/script/simulation.hpp` 批大小上限 8192、`BatchPlan_` 的 batchSize = min(8192, ceil(nPaths/nThreads)) **随线程数变化**、全局 `ThreadPool_`、每 worker `ThreadState_`；AAD 结果按**线程槽** `simResults(threadNum)` 累加后 `AggregateAADResults`（浮点分组随线程数变化——现有机制不满足 bitwise 线程不变性，见 N9 的驱动层方案）。
- **RNG 重放语义（`docs/methodology/random.md`）**：`SkipTo` 行为因引擎而异——Sobol 直接重构状态（完整重放）；`MRG32k32a_` 用 matrix jump，"仅在偶数路径偏移重放 `FillUniform`，**不复放 normal paths**"；`ShuffledIRN_::SkipTo` 为 no-op。LSMC 的 Phase C 消费 normal paths，因此只有 Sobol 可安全重放（S15 的依据）。
- 编译求值器：`dal-cpp/dal/script/visitor/compiler.hpp`，栈式字节码（opcode 枚举 `NodeType_` 为 NTTP/稳定整数操作数的手写例外，opcode 只可**尾部追加**，分派按数值区间分层，legacy 流与 prepared 流经 `Prepared_` 模板旗标隔离）；fuzzy IF 走"双分支求值 + 变量混合"（`FuzzyEvaluator_` 与编译 `FuzzyIf` 互为镜像）。
- 线性代数：无 QR/SVD 最小二乘；`CholeskySolve`（`dal-cpp/dal/math/matrix/cholesky.hpp`）的默认正则化是"负主元残差截零 + 倒对角正则"（`docs/methodology/matrix.md`），**不构成显式 A+λI**——对 O(nPaths) 量级 Gram 元素数值上无效，LSMC 驱动必须自行做显式 ridge（N3）。
- 诊断：节点 JSON 判别键为 `"kind"`（`{"id","kind",…}`，`dal-cpp/dal/script/visitor/debugger.hpp`）；`"type"` 键被 Fix/Spot 观测子对象占用；Debugger 有 JSON、s-表达式文本、ASCII 树三套渲染。Describe = `dal.script-product/2`、Explain = `dal.script-valuation/1`，契约声明 Explain "不生成路径、不启动 worker、不创建 AAD 录制"（script_engine.md / public-api.md / excel-script-settings.md 三处）。
- archive：`ScriptProductData` 版本号在 `dal-cpp/dal/script/event.hpp` 的 Machinist storable 标记块（version 2，`manual`）；已存档产品在加载时**重新解析事件文本**。
- PDE：`Dal::PDE::Rollback_` θ-scheme 仅欧式（无障碍/PSOR/罚函数）；库内无美式测试、无二叉树。
- 平滑核：`CSpr(x, eps)`（平滑 `x>0` 斜坡）、`BFly` 在 `dal-cpp/dal/script/visitor/smoothing.hpp`；`smooth_` 为全局默认过渡宽度；比较的 `;eps` 选项仅在条件文法内可达（语句级无 eps 槽位）。

## 1. 语义规格（REQ-S）

- **S1 行权表达**：行权能力表达在事件层：一个事件至多一条 `EXERCISE` 语句，事件日期即行权观察日。行权日期必须**严格晚于评估日**，否则准备期报 `UnsupportedExerciseDate`（附输入行号与日期，见 S16）。
- **S2 语句语义**：`EXERCISE <expr> [IF <cond>]` 中 `<expr>` 为行权价值（估值单位、与 `PAYS` 同口径、不预先贴现）；可选 `<cond>` 限制该日允许行权的路径。行权判定 = 比较 `<expr>` 与回归 continuation 估计（S3）；`<cond>` 为假则该路径当日不可行权。
- **S3 行权 vs 持有判定（硬模式）**：在行权日 k，**持有价值** `H_k = p_k + D_{k,k+1}·W_{k+1}`（p_k = 当日全部 `PAYS` 之和，W 为后向现金流估计，D 为 numeraire 贴现比——当日 `PAYS` 计入持有侧，与 S4 的取代语义口径一致）；对条件为真的路径子集回归 `H_k` 得 `C_k(z)`。路径判定：条件为真且 `h_k > C_k(z_k)`（**严格大于**，平价持有）→ 行权，`W_k := h_k`（取代当日及之后全部支付）；否则 `W_k := H_k`。非行权事件 `W_k := p_k + D_{k,k+1}·W_{k+1}`；最后事件之后 `W = 0`。
- **S4 与 `PAYS` 的组合边界**：行权价值**取代**行权日当日及之后的全部支付（与语句书写顺序无关）；未行权路径当日 `PAYS` 照常。持有侧包含当日 `PAYS`（S3 的 `H_k`），消除"行权判定系统性偏向行权"的口径偏差；autocall 赎回价含票息由用户在 `<expr>` 中表达。
- **S5 未行权语义**：从未行权的路径 payoff = 全部 `PAYS` 之和（现行为）；不含 `EXERCISE` 的产品零影响（B4 保留字边界除外，见 X1）。
- **S6 与 FIX 的组合边界**：`ObservationPlan_` 语义不变——行权日及之后的 FIX 请求照常在准备期封闭解析；某路径行权后其后续观测只是不再被使用，不产生错误、不影响诊断与 `sample_dates`。历史 FIX 值与 past-replay AAD 种子机制（DAL-201）完全不变。
- **S7 与 `TodayFixingPolicy_` 的组合边界**：S1 排除评估日当天行权，当日策略只作用于非行权事件的 FIX，与本特性无交叠；文档明确该边界（对齐 DAL-208 模式）。
- **S8 历史区边界**：`EXERCISE` 不得出现在解析后日期 ≤ 评估日的事件中；评估日后移使行权日落入历史区时同样报错，用户需重建合约。v1 不支持"已行权"存量状态。
- **S9 fuzzy（AAD）模式判定——递归混合读法（裁定写死）**：决策度 `d_k = CSpr(h_k − C_k(z_k), eps)`（带条件时与条件度取代数积，对齐 `FuzzyAnd`；eps 来源见 S17）。路径价值按**递归混合**定义：`V_k = d_k · h_k + (1−d_k) · (p_k + D_{k,k+1}·V_{k+1})`，非行权事件 `V_k = p_k + D·V_{k+1}`，末事件后 `V = 0`——延续分支**递归包含其后各日的 fuzzy 行权决策**（非"纯 PAYS 前向收益"、非"冻结多项式单点代入"）。smooth→0 时 `d_k → {0,1}`，V 逐路径退化为硬模式 payoff（T4 模式一致性验收的前提）。`C_k` 冻结为 tape 常量的 envelope 论证见 N6。
- **S10 确定性与可重放（bitwise，机制见 N9）**：同一种子下，PV、`d_<param>`、回归系数 `C_k`、`exercise_rate` 与线程数**bitwise 一致**，double 与 AAD 两种模式均如此。LSMC 驱动自带与线程数无关的批布局与按批索引序归约（N9），不改动不含 `EXERCISE` 产品的现有仿真路径。
- **S11 parity 要求（容差语义）**：同合约/模型/设置下，树走与编译求值器：
  - double：PV 在既有 parity 容差内一致（对齐 script_engine.md 文档化的 association-noise 契约，不承诺 bitwise）；行权决策的逐路径差异仅允许出现在 `|h_k − C_k| ≤ 既有 parity 容差` 的边界路径上，且此类路径占比 ≤ 1e-4；决策逻辑（与同一冻结 `C_k` 的比较）为共享驱动代码。
  - AAD：PV 与 `d_<param>` 满足现有 parity 容差。
  - parity 与 fuzz 套件扩展覆盖 `EXERCISE` 产品（T3）。
- **S12 执行模式边界**：含 `EXERCISE` 的产品必须走带模型的准备管线；history-only 准备与 legacy `PreProcess` 管线遇到 `EXERCISE` 报 `UnsupportedExecutionMode`。
- **S13 模型与回归因子（#390 后措辞）**：公开估值门维持 BS/Dupire 不变；回归因子 = 该产品脚本报文中**唯一**的未来模型观测 EQ FIX 索引（引擎按索引名绑定模型 spot 输出；多于一个未来索引报 `MultipleModelIndices`，沿用 #390 语义，本特性不新增绑定面），在行权采样日观测。
- **S14 payoff 闸门与接收器（B2 修复，写死；路由谓词按"是否含 PAYS"）**：`ContainsPayoff` 扩展识别 `NodeExercise_`（`EXERCISE` 构成 payoff，`InvalidScriptStructure` 闸门对 EXERCISE-only 产品放行）。接收器路由谓词 = **合约是否含 `PAYS`**（而非"是否有变量"——如 `x = SPOT(); EXERCISE …` 有变量但无 `PAYS`，按现状 `payoffIdx_` 会默认指向最后一个变量而非接收器）：含 `PAYS` → 沿用现有 payoff 变量作持有侧累积器（`payoffIdx_` 现状语义不变），驱动层在行权路径上以 `h_k` 覆盖；**不含 `PAYS`（无论有无变量）** → 现状无接收器哨兵（`payoffIdx_ = -1`），LSMC 驱动自行聚合路径 payoff（行权 → `h_k·df`，从未行权 → 0），`PayOffIdx` 提取不适用于哨兵路径，`payoff_index` 诊断维持现状 **null 键输出**（非"省略"；script_engine.md 的 payoff_index 措辞随 X8 同步）。
- **S15 RNG 引擎限制（裁定 3 + 代码证据收严，写死）**：含 `EXERCISE` 的产品在准备期**仅允许 `rsg = "sobol"`**；`mrg32`/`irn` 报 `UnsupportedRsgForExercise`（附 rsg 名、定位信息与 "use method='sobol'" 提示）。依据（random.md 的 SkipTo 语义）：MRG32 的 matrix jump 仅在偶数 uniform 偏移重放且**不复放 normal paths**，IRN 为 no-op——LSMC Phase C 消费 normal paths，允许二者即复现 irn 同类静默错价。裁定原文允许 sobol/mrg32，本方案依上述文档化证据收严为 sobol-only；mrg32/irn 的确定性重放列后续开放问题 F2。
- **S16 错误定位契约**：本特性新增的**五个源定位错误**（`UnsupportedExerciseDate`、`UnsupportedExerciseNesting`、`DuplicateExercise`、`InvalidExerciseCondition`、`UnsupportedRsgForExercise`）必须携带输入行号/偏移/日期等定位上下文（复用 `SourceOrigin_`），格式对齐 FIX 先例并被测试锁定；`DuplicateExercise` 尤须覆盖"同日多行合并"场景的定位。X4 在 `ValidateSimulationSettings` 新增的 `InvalidSetting:` 类校验错误**不适用** `SourceOrigin_` 要求——它无产品/源文本上下文，沿用设置错误的字段名定位风格。
- **S17 平滑参数来源（对齐现有文法，写死）**：带条件的 `EXERCISE` 复用条件的 `;eps` 选项（无 `;eps` 落 `simulation.smooth_`），该 eps 同时服务条件平滑与决策平滑（一个 eps 两种用途，文档化）；无条件 `EXERCISE` 直接用 `simulation.smooth_`。v1 不引入语句级 eps 槽位（现有文法仅在条件内可达）。

## 2. 语法与公开 API（REQ-X）

- **X1 语法与保留字 breaking-change 声明（B4）**：保留字表新增 `EXERCISE`（大小写不敏感）。**这是隐性 breaking change**：现有以 `exercise` 为变量/宏/常量名的脚本与已存档产品（加载时重解析）在新构建下解析失败。处理对齐 FIX 先例：解析错误给出保留字改名迁移提示（含定位）；`CHANGELOG.md` 记 Breaking 条目（"Breaking: `EXERCISE` is reserved; variables or definitions named `exercise` must be renamed"）；script_engine.md 兼容边界节声明。验收基线相应修正：**不含 `EXERCISE` 且未使用保留字 `exercise` 的现有产品行为 bitwise 不变**（仓内无测试使用该名，CI 不会红，但承诺边界必须如实收窄）。
- **X2 AST 与节点渲染（触点清单完整版）**：`NodeExercise_ : ActNode_`，子节点 = [价值表达式] + 可选条件节点，fuzzy 元数据对齐 `CompNode_`。Debugger 三套渲染的完整触点（`dal-cpp/dal/script/visitor/debugger.hpp`）：JSON——判别键 `"kind":"exercise"`（`"type"` 被 Fix/Spot 观测子对象占用），**必须扩展 `JsonWriteFields` 分派链**（否则 eps/mode 平滑元数据静默缺失），字段形对齐 compare 节点 `"mode"/"eps"` 嵌套；s-表达式文本标签一项；ASCII 树——`TreePrec` 优先级表 + **`TreeInlineStatement`/`TreeBranchStatement` 两处语句分派**（未处理的 kind 会落入分号拼接或错误的 `^` 头渲染）。**legacy debug `/1` 闸门**：现有守卫只拦 FIX 类 `preparationError_` 拒绝，EXERCISE-only 产品会漏进 `/1`——须增加显式 `EXERCISE` 内容检查，拒绝消息带 Describe 迁移提示；T1 锁定树渲染黄金值。
- **X3 语句位置约束与二义性**：`EXERCISE` 只能作为事件顶层语句（嵌套 `IF` 报 `UnsupportedExerciseNesting`）；每事件至多一条（`DuplicateExercise`）。**dangling-IF 二义性**：`EXERCISE` 后的 `IF` 贪心绑定为条件引入词；条件解析尾部遇到语句边界 token（`THEN`/`END`/下一保留字）报专用 `InvalidExerciseCondition`（不做误导性的 generic 变量名冲突报错）。v1 维持贪心绑定 + 专用错误，不改文法。
- **X4 设置与三端校验落点**：`MonteCarloSettings_` 新增 `int lsmcBasisDegree_ = 3;`，校验为 1..8 整数（拒 bool/非整数/越界）。**C++ 原生落点**：`ValidateSimulationSettings`（`dal-cpp/dal/script/settings.cpp`）扩展该校验，并扩展 `dal-public/tests/test_script_contract.cpp` 对应断言；Python/Excel 侧校验对齐 DAL-204 先例。上界 8 的依据：z-归一化单项式基在 8 阶内 Gram 条件数典型可达 1e8–1e10，配合 N3 的显式 ridge 与条件数守卫可控；更高阶无收敛收益且数值风险陡增（Runge/Fisher 效应），T2 附 6 阶数值用例与 8 阶退化守卫用例。
- **X5 仿真诊断面（裁定 1 写死；`Explain` 契约不动）**：`exercise_events` **不进 `Explain`**（其契约"不生成路径、不启动 worker、无 AAD 录制"与无 nPaths 参数决定它承载不了回归系数/行权率）。新增携带 settings 的仿真诊断入口：C++ `ExplainScriptSimulation(product, modelData, numPath, valuation = {}, simulation = {})`（两个 settings 参数默认构造，对齐诊断入口惯例）→ JSON schema `dal.script-simulation/1`；Python `ScriptSimulation_Explain`、Excel `SCRIPTSIMULATION.EXPLAIN`（T5 投影；命名对齐既有 `ScriptValuation_Explain` ↔ `SCRIPTVALUATION.EXPLAIN` 镜像惯例）。该入口**显式运行完整三阶段估值**并在契约中声明成本（生成路径 + worker + 回归）。schema：`{schema, evaluation_date, simulation{rsg,use_bb,enable_aad,smooth,compiled,lsmc_basis_degree}, n_paths, exercise_events:[{event_id, date, basis_degree, regressor_index, num_cond_true_paths, num_coefficients, coefficients[], degenerate, degenerate_reason, exercise_rate}]}`。细节钉死：不含 `EXERCISE` 的产品返回 `exercise_events: []`（省略键与空数组二选一，取后者）；`regressor_index` = canonical 索引名字符串（与 Explain `requests[].index_canonical` 同一 join 空间）；退化日以 `degenerate: true` + `degenerate_reason`（值风格冻结为 PascalCase 多词 token，对齐 schema 既有 `ExplicitSnapshot`/`RequireHistorical` 先例：`ConditionPathsBelowMin` / `SigmaFloor` / `IllConditioned`）显式区分于真实低行权率；`n_paths` 显式入 schema（同时回应 R1）。
- **X6 archive 版本决策（维持不升版，措辞修正）**：v2 字段集无需扩展，**不升 v3**。兼容边界如实陈述：旧二进制读入含 `EXERCISE` 的已存档产品时，在**事件重解析期**得到 generic "statement without an instruction" 错误——**不含** EXERCISE/行号/版本专用提示（旧二进制无法预知新语句）；边界文档按"不能解析（generic 错误）"措辞而非"明确报错"。升级 v3 的逃生舱配方（标记块 version 3 → `dal_generate` → 提交 `MG_ScriptProductData_v3_{Read,Write}.inc` 并保留 v2 reader → 写端默认 v3 → 扩展 `test_script_archive.cpp` 与 script_engine.md 归档表）作为记录保留，非本里程碑动作。
- **X7 三端投影（签名冻结范围明确）**：**冻结**：`NewScriptProduct`、`ValueByMonteCarlo` 全部重载、`ExplainScriptValuation`、`MonteCarlo_Value`、`Product_New`、`MONTECARLO.VALUE(WITHSETTINGS)`、`SCRIPTVALUATION.EXPLAIN` 等既有签名与输出形状（含结果 map 的 `PV` + `d_<param>` 键集，**以及 `dal.script-valuation/1` 的 `simulation` echo 五字段 rsg/use_bb/enable_aad/smooth/compiled——X4 新增字段不进入该 echo：echo 序列化按字段显式列举，禁止反射式照抄 `MonteCarloSettings_`，防止冻结面静默漂移**）。**新增（加性）**：X5 的 `ExplainScriptSimulation`/`ScriptSimulation_Explain`/`SCRIPTSIMULATION.EXPLAIN`；`MonteCarloSettings_` 新字段自然透传（Python keyword-only `lsmc_basis_degree`、Excel `MONTECARLOSETTINGS.NEW` 新 key，未知 key 仍硬错，旧工作簿不受影响；storable 标记再生成 + `dal_check_generated` 门禁）。
- **X8 示例与文档面（清单补全）**：新示例 `dal-cpp/examples/american_put_mc/` 与 `dal-python/examples/013.exercise_bermudan.py`；文档面：`docs/methodology/script_engine.md`（Early Exercise 小节 + 兼容边界 + 既有 "null when no payoff exists" 的 `payoff_index` 措辞随 #390 后语义更新为"无 payoff 变量时"——S14 哨兵路径复用该 null 键语义）、`docs/excel-script-settings.md`（新 key + SCRIPTSIMULATION.EXPLAIN）、`docs/public-api.md`（新入口）、`docs/README.md` 索引、`docs/methodology/_cpp-example-style.md` 权威示例映射表（加 `american_put_mc/`）、`dal-python/README.md` 的 `MonteCarloSettings_` keyword-only 签名块（加 `lsmc_basis_degree`）、`CHANGELOG.md`（Breaking 条目 + 能力条目）。

## 3. 数值方法与数据流（REQ-N）

- **N1 Phase A（前向存储，double）**：按线程数无关批布局（N9）生成路径并前向求值，`EXERCISE` 对前向求值为 no-op，仅记录：(a) 每路径每事件支付额 `p_i`（稀疏存储：仅含 `PAYS` 的事件）；(b) 每路径每行权日三元组 `(x_k 回归因子观测, h_k 行权价值, 条件度[硬模式 0/1])`。
- **N2 Phase B（后向归纳 + 回归，逐事件现金流，B1 修复）**：自最后事件向首个事件回走，维护每路径现金流向量 W（表示"当前回走日期的价值"）：初始（最后事件之后）`W = 0`；每事件 i：`H_i = p_i + D_{i,i+1}·W`；若 i 为行权日：对**条件为真**的路径子集，以 `H_i` 为回归目标、z-归一化基（N3）累积正规方程矩并求解 `C_i`（N3 的 ridge 与守卫）；逐路径判定 `h_i > C_i(z_i)`（严格大于）→ `W := h_i`（S4 取代，含当日 p_i），否则 `W := H_i`。非行权事件 `W := H_i`。**当日 `PAYS` 计入持有侧**（H_i 定义），与 S4/S9 完全同口径；不存在"评估日贴现总量分解"或重复计入问题。输出全体 `C_i` 与退化标记。
- **N3 回归基、归一化与退化策略（回归稳健性，写死）**：基 = z 的 d 阶单项式，`z = (x − μ̂)/σ̂`，`σ̂` 下限保护 `max(σ̂, 1e-12·max(1,|μ̂|))`（σ̂=0 合法输入不产生 NaN）。**显式 ridge**：驱动层在调用 `CholeskySolve` 前自构 `A + λ·diag(A)`，`λ = 1e-12`（相对对角 ridge；内置默认正则化是 clip-倒对角机制、非显式位移，不依赖它）。**退化策略（择一写死）**：`nCondTrue < 10×(d+1)` 或条件数估计（对角比/分解残差）> 1e12 或触发 σ̂ 下限时，该日**退化为常数基**（仅截距回归，不行权判定退化为与常数比较），标记 `degenerate` 与原因入 X5 诊断；不崩溃、不静默换策略。
- **N4 Phase C（冻结策略重放，sobol-only）**：用 Sobol 的 `SkipTo` 确定性重生成同一批路径（S15 保证引擎面），再次前向求值；在每个行权日由**驱动层**计算决策：`h_k` 与条件度来自树走/编译求值段，`C_k(z_k)` 用冻结系数；硬模式按 S3 递归得路径 payoff，fuzzy 模式按 S9 递归混合。PV = 路径 payoff 均值；AAD 模式在本 Pass 录制伴随（N6）。同一比较与系数保证 Phase B/Phase C 决策一致（同路径集）。
- **N5 贴现（末步显式）**：日期间折算用现有样本网格 numeraire 比率（`SampleDef_`），与 `PAYS` 折现口径一致；S3/S9 的递归在各自日期时点计价；**PV = D_{eval,t₁} · W_{t₁}**——W 回走/重放到首事件后，乘"评估日 → 首事件"的 numeraire 贴现比是显式最后一步，不隐含在递归内。
- **N6 AAD 与 envelope 论证（裁定 2 写死）**：回归系数为 tape 常量（不做跨路径可微回归）。路径可微性来自 S9 的递归混合（`d_k`、`h`、`p`、贴现因子全部在 tape 上）；伴随是**冻结策略价格泛函**的精确梯度。总梯度与它的差 = `∂V/∂policy·∂policy/∂θ`；由包络定理，策略近优时 `∂V/∂policy = O(策略次优误差)`，该项为二阶小量。**T4 的 bump 测试按生产行为让 bump 重新生成策略**（测的是总导数），1e-3/5e-3 容差带显式吸收 envelope 余量；若 T4 发现可归因于 `∂C/∂θ` 缺项的越限，回退方案（系数再生成的种子化或收窄 smooth 带）在 T4 记录，不进 v1 范围。fuzzy 混合的挂接点（求值器内 vs 驱动层）为 T4 实现细节，不构成公共语义差异。
- **N7 内存模型（B1 后更新）**：峰值 ≈ `nPaths × nPaysEvents × 8B`（N1a 现金流存储）+ `nPaths × nExDates × (2–3) × 8B`（N1b 三元组，条件式才存第三元）+ `nPaths × 8B`（W 向量）+ 系数。例：2^20 路径、52 个含 PAYS 事件、12 行权日 ≈ 437 MB + 302 MB ≈ **0.74 GB**——文档化预算并给出行数×路径数预算表；超预算由用户降路径/日期，分块重放（按日期块经 `SkipTo` 重生成）为后备优化（F4，v1 不做）。
- **N8 性能预算**：含 `EXERCISE` 产品单次估值为不含行权产品的 ~2.2–2.6 倍（两次路径生成与求值 + 回归）；不含 `EXERCISE` 的产品零新路径。`script_mc_perf` 基准增加行权场景（head-only 新场景由 `check_benchmark_regressions.py` 自动记为 ungated new coverage，无需基线配对动作）。
- **N9 聚合与批布局（B2 修复：bitwise 线程不变性的机制，写死）**：LSMC 驱动**不沿用**现有 `BatchPlan_`（其 batchSize 随线程数变化）与线程槽累加（AAD 亦然）：(a) 批布局只依赖 nPaths——固定批大小 `min(8192, nPaths)`、批数 `ceil(nPaths/ batchSize)`，worker 从队列拉批；(b) double 与 AAD 的路径贡献均写入**按批索引**的槽位，回归矩按批索引序累积归约到逐日累加器；(c) 系数求解、W 向量更新在批归约完成后单点执行。既有仿真循环（不含 `EXERCISE` 的产品）零改动。验收在 T2（1 vs N 线程 bitwise，双模式）。

## 4. 里程碑 T1–T5

### T1 语法/语义骨架与诊断（约 2–3 人日）

- 范围：X1–X3（含 B4 声明与错误定位 S16、X3 二义性处理）、S1/S8/S12/S15 校验与错误、S14 的 `ContainsPayoff` 扩展与哨兵路径、X2 三渲染器；树走/编译求值遇到 `EXERCISE` 临时抛 `UnsupportedExecutionMode`（T2 解锁）。文件面：`dal-cpp/dal/script/{lexer,parser,event,preprocessor,settings,diagnostics}.{hpp,cpp}`、`node.hpp`、`preparation.{hpp,cpp}`、`dal-cpp/dal/math/random` 无改动（S15 的 rsg 闸门在 **preparation 层**实现——`ValidateSimulationSettings` 无产品知识，不在该层）。
- 验收标准：
  - 新套件 `ScriptExerciseParseTest` 全绿；`dal-public/tests/test_script_contract.cpp` 的 `ValidateSimulationSettings` 扩展断言全绿；
  - 含 `EXERCISE` 产品 Describe JSON 含 `"kind":"exercise"` 节点（三种渲染均可用）且 schema 仍为 `dal.script-product/2`；
  - EXERCISE-only 产品通过 payoff 闸门（不再 `InvalidScriptStructure`）；
  - 不含 `EXERCISE` 且未用保留字 `exercise` 的现有测试零变化（`ctest` 全量）。
- 聚焦测试（`dal-cpp/tests/script/test_exercise_parse.cpp`）：文法接受/拒绝（含 `;eps` 条件选项、保留字冲突、嵌套、重复、过期行权日、mrg32/irn 拒绝、dangling-IF 专用错误）；全部新错误的定位信息断言（含同日多行合并的 `DuplicateExercise`）；Describe JSON 黄金值与 ASCII 树渲染黄金值；legacy debug /1 拒绝并提示迁移（含 EXERCISE-only 产品不漏进 /1 的闸门用例）。

### T2 树走 LSMC（double，硬决策）+ 聚合重构 + 仿真诊断（约 7–10 人日）

- 范围：N1–N5、N7、**N9 聚合重构**、S3/S4/S10 落地；**X5 的 C++ 面**（core+public `ExplainScriptSimulation`，`exercise_events` 全字段，T2/T4 测试即用它断言 `exercise_rate` 与系数）；测试专用百慕大 PDE 基准。
- PDE 基准（test-support，测试专用）：现有 `Dal::PDE::Rollback_` θ-scheme 在行权日之间回滚 + 行权日障碍投影 `V = max(V, h)`；基准自证两组断言（欧式极限对 `BlackScholes`/`Distribution` 闭式；网格收敛阶）。
- 验收标准（`dal-cpp/tests/script/test_exercise_lsmc.cpp`，套件 `ScriptExerciseLSMCTest`）：
  - **欧式极限**：仅到期行权的 put 与欧式闭式之差 ≤ 3 × MC 标准误（Sobol 2^18）；
  - **中期行权日基准**（回归机制敏感，非仅到期）：单中期行权日 + 到期的 Bermudan put vs PDE 基准，|MC − PDE| ≤ 0.5% × spot（2^18、阶数 3）；
  - **美式 put 对拍**：周行权 52 日型网格，同容差；
  - **收敛行为（带宽/趋势，替代单调链）**：路径数 2^16/2^17/2^18 各满足 |MC − PDE| ≤ max(3·SE, 0.75%×spot)，且 |error(2^18)| ≤ |error(2^16)|（单点趋势比较，不要求逐级单调）；
  - **线程不变性（N9 验收，双模式）**：1 线程 vs 默认线程池，PV、系数、`exercise_rate` bitwise 相等（double；AAD 模式在 T4 补齐后纳入同一断言）；
  - **零行权退化**：条件恒假产品与去掉 `EXERCISE` 的普通产品 PV bitwise 相等；
  - **退化策略**：σ̂=0 输入（vol=0 合法模型）、条件真路径不足、高条件数三场景不崩溃且 `degenerate` 标记正确；
  - **阶数用例**：阶数 6 对拍 PDE 通过、阶数 8 触发守卫进入退化标记路径的行为锁定；
  - **PDE 基准自证**：欧式极限与网格收敛两组断言。
- 聚焦测试：`./build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='ScriptExerciseLSMCTest.*:ScriptExerciseParseTest.*'`。

### T3 编译求值器 parity（约 3–5 人日）

- 范围：S11 容差语义——编译模式接入同一驱动（行权价值与条件度用编译段求值；决策逻辑留驱动层）；新 opcode 仅可**尾部追加**，分派区间与 legacy/prepared 缝（`Prepared_` 旗标）不动；若新增扩散超出 `compiler.hpp` 舒适区，拆 `dal-cpp/dal/script/visitor/compilerexercise.hpp`（命名对齐 code-style 的 lowercase/无分隔符）。
- 验收标准：
  - `ScriptCompiledParityTest` 新增行权产品用例：double 模式 PV 在既有 parity 容差内一致；决策差异仅限边界路径（S11 的占比与带宽断言）；
  - `ScriptCompiledParityFuzzTest` fuzz 生成器扩展随机 `EXERCISE`（单事件/多事件/日程三组各不少于一例），容差内一致；
  - 现有 filter `'ScriptCompiledParityTest.*:ScriptCompiledParityFuzzTest.*'` 全绿；`script_mc_perf` 行权场景跑通。

### T4 AAD（fuzzy 递归混合 + 冻结策略重放）（约 4–6 人日）

- 范围：S9 递归混合、N6 envelope 落地与文档化、Phase C 的 AAD 录制、系数常量化、与 DAL-201 past-replay 组合（历史 FIX + 未来行权）；四 AAD 后端。
- 验收标准（并入 `ScriptExerciseLSMCTest`）：
  - **AAD vs 中心差分（生产行为：bump 重生成策略）**：同种子 CRN，spot/vol/rate/div 中心 bump（spot 步长 0.05、其余 1e-3 相对），`d_<param>` 相对偏差目标 ≤ 1e-3、硬上限 5e-3（envelope 余量显式计入；超限须归因分析并记录回退决策）；
  - **smooth→0 收敛（S9 读法验收）**：fuzzy PV 随 smooth 递减（0.1→0.01→0.001）收敛至硬模式 PV，带宽断言；
  - **AAD 线程不变性**：纳入 T2 的 bitwise 断言（N9 双模式承诺补全）；
  - **FIX × EXERCISE 组合**：历史定盘（全局 store 与显式快照）+ 未来行权产品，参数风险经历史值链路保留（对齐 `test_past_replay.cpp` 断言风格）；
  - CI 四后端（AADet/XAD/Adept/CoDiPack）矩阵全绿。
- 聚焦测试：`--gtest_filter='ScriptExerciseLSMCTest.*'` + `test_past_replay.cpp` 回归。

### T5 绑定与文档（约 2–3 人日）

- 范围：X7 新增面（Python `ScriptSimulation_Explain`、Excel `SCRIPTSIMULATION.EXPLAIN` + `MONTECARLOSETTINGS.NEW` 新 key + storable 再生成）、X8 全部文档与示例、`CHANGELOG.md`（Breaking + 能力条目）、benchmark 门禁确认。
- 验收标准：
  - Python：`lsmc_basis_degree` 校验与 keyword 断言**并入既有 `dal-python/tests/test_script_settings.py`**；`ScriptSimulation_Explain` 的 e2e 断言（`exercise_events` 解析、`[]` 语义、`n_paths` 字段）新增用例；
  - Excel：新 key 解析与未知 key 硬错在 `dal-excel/tests/test_script_settings.cpp`；`SCRIPTSIMULATION.EXPLAIN` 契约测试放 `dal-excel/tests/test_script_valuation.cpp`（既有 `SCRIPTVALUATION.EXPLAIN` 契约测试所在文件）（portable）；
  - `check_docs.py` 与 doc-integrity CI 绿；X8 清单全部文件就位；两个示例可运行且数值与 T2 基准一致；
  - 四后端 + Windows CI 全绿；benchmark 门无回归。
- 总工期：T1–T5 约 **18–28 人日**（含 N9 聚合重构与仿真诊断面）。

## 5. 风险与依赖

- **R1 内存规模**（N7 公式与预算示例）：大路径数 × 多事件产品内存线性增长；v1 以 `n_paths`/事件数入诊断与文档预算表缓解，分块重放为后备（F4）。
- **R2 回归偏差与基选择敏感**：低阶基对陡峭 continuation 可能欠拟合；缓解：阶数可设 + 显式 ridge/条件数守卫 + 退化标记 + T2 阶数用例与中期行权日锚点。
- **R3 fuzzy 行权平滑偏置**：过渡带混合与 envelope 缺项（N6）——T4 以 bump-重生成策略的容差带量化；smooth→0 收敛断言锁定读法。
- **R4 `compiler.hpp` 复杂度**：决策逻辑留驱动层、opcode 尾部追加、必要时拆 `compilerexercise.hpp`。
- **R5 PDE 基准正确性**：基准自证断言与对拍同级必过。
- **R6 Excel 设置再生成**：storable/UDF 标记再生成 + `dal_check_generated` + Windows CI。
- **R7 N9 聚合重构的实现风险**：驱动层自有批布局与按批归约为新代码，验收（双模式 bitwise 1 vs N 线程）在 T2 门禁化；若 AAD 按批归约遇到 tape/槽位工程困难，回退为"double 模式 bitwise + AAD 模式容差 1e-12 相对"并在方案与 CHANGELOG 记录（该回退不解除验收，只放宽 AAD 一项的相等语义——须显式评审通过）。
- **依赖**：`CholeskySolve`（现有 + 驱动层显式 ridge）、Sobol `SkipTo`（现有）、fuzzy IF 混合机制（现有）、`ThreadPool_`（现有）。无新外部依赖。

## 6. 已裁定事项（原开放问题，全部写死）

- **A1（原 Q1/archive）**：不升 v3；generic 解析错误的如实措辞与逃生舱配方（X6）。
- **A2（原 Q2/大 eps）**：v1 不警告，文档说明 eps 双用途（S17）。
- **A3（原 Q3/基函数）**：v1 单项式全路径回归 + 显式 ridge + 退化策略（N3）；ITM/其他基按 T2 数据在后续里程碑再议（F3）。
- **A4（原 Q4/分块重放）**：v1 不做（F4）。
- **A5（原 Q5/当日行权）**：维持排除（S1）；有需求时走 REQUIREHISTORICAL 确定性决策流程再立项。
- **A6（评审裁定 1）**：`exercise_events` 移至 `dal.script-simulation/1` 仿真诊断面，`Explain` 契约与 X7 冻结面不动（X5）。
- **A7（评审裁定 2）**：S9 递归混合读法 + 冻结系数 envelope 论证入方案（N6）。
- **A8（评审裁定 3，证据收严）**：sobol-only（S15）——裁定原文为 sobol/mrg32，random.md 证实 MRG32 `SkipTo` 不复放 normal paths，故收严；mrg32/irn 重放列 F2。
- **A9（评审裁定 4）**：基准重锚定 `9ee82433`，S13 与锚点按 #390 语义改写（本文档已执行）。
- **A10（S10/S11 线程不变性）**：保持 **bitwise** 承诺，机制与任务为 N9（驱动层线程无关批布局 + 按批索引归约，AAD 含在内），验收在 T2/T4 门禁化；parity（S11）单独降为文档化容差语义。

## 7. 后续开放问题（不阻塞本里程碑）

- **F1** `NodeCollect_`（未使用节点）与本特性的关系——无关，不动。
- **F2** mrg32/irn 的确定性重放（需 normal-path 级 seek 或路径缓存），实现后可放宽 S15。
- **F3** ITM 过滤与非单项式基族（依 T2 数据决定是否引入开关）。
- **F4** 分块重放内存优化（大路径数 × 多事件产品的 N7 预算后备）。

## 8. 特性级验收汇总

1. T1–T5 各节验收标准全绿（filters：`ScriptExerciseParseTest.*`、`ScriptExerciseLSMCTest.*`、`ScriptCompiledParityTest.*:ScriptCompiledParityFuzzTest.*`、`test_script_contract.cpp` 扩展断言）。
2. 全量 `ctest`（Linux）无回归；**不含 `EXERCISE` 且未使用保留字 `exercise` 的现有产品行为 bitwise 不变**（X1 边界）。
3. CI：四 AAD 后端矩阵、Windows（含 Excel 构建）、doc-integrity、benchmark 回归门全绿。
4. 三端可用性：C++/Python/Excel 均能表达并估值百慕大行权产品，且 `ExplainScriptSimulation`/`ScriptSimulation_Explain`/`SCRIPTSIMULATION.EXPLAIN` 可输出 `exercise_events` 诊断（示例落地 X8）。
5. 文档与 CHANGELOG（含 Breaking 条目）就位；本方案在特性合入后按 artifacts 惯例退役，现行状态并入 `docs/methodology/script_engine.md`。
