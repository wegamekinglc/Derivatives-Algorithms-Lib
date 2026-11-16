# DAL-538：市场 IV → 非平坦 F/D local-vol MC API 决策

状态：S0 冻结合同，等待独立 review；这是后续 S1–S5 的实施控制，不代表产品已经实现或数值验收通过。日期：2026-11-16。对应 DAL-539；父任务 DAL-538。受众为 core/public C++ 开发者、Python/Excel 绑定作者、独立数值鉴证和性能/交付负责人。

[执行计划](../plans/dal-538-market-iv-local-vol-mc.md) 的 R1–R7、N1–N7、P1–P3、B1–B5 继续生效；本决策冻结其 D1–D6、精确接口及执行细节。若草案名称/默认/采样上限不同，以本文为准。用户已授权六阶段、每阶段独立任务及 PR、自主 review/修复/合并及可运行示例和 benchmark；S0 只交付文档 PR，由协调器安排 reviewer，审查合并后才启动 S1。

## 1. 基线与既有接口

规划源码基线 `47138fb89949029813ccdf0e4bcdf70d298e3e1e`。S0 实际 checkout、`origin/master` 与 `git ls-remote origin refs/heads/master` 均为 `c4f65377650c07f29c8b26852aeb5f8a5ef932b9`。专属分支 `agent/dal-api-designer/d90a1fab312d`，初始干净，PR base `master`。唯一增量为 OIS native evaluation PR #697：23 个文件，734 增/88 删，涉及 OIS lowering/observation/script、IR parser、core 示例及测试/文档；没有修改下列 market-IV、Hybrid、local-vol 或三端接线文件。后续阶段在最新 master 上重新记录 SHA。

| 当前源码 | 已有能力及本功能必须对接之处 |
|---|---|
| `dal-cpp/dal/model/surface/marketivsurface.hpp`、`marketivsurfaceinternal.hpp` | owning snapshot、fit request/settings、certificate、regime、fit warnings；`MarketIvDupireResult_` 持有 snapshot/request/nodes/surface |
| `dal-cpp/dal/model/surface/marketivsurface.cpp`、`marketivdupire.cpp` | `CarryAt` 与精确 grid 轴；失败节点不得转为成功模型 |
| `dal-cpp/dal/model/surface/lvmodel.hpp` | 波动率对 log-spot 与 time 线性插值、边界平坦、`Prepare` 预备左端时间列；不是对 variance 插值 |
| `dal-cpp/dal/model/base.hpp`、`hybriddata.hpp`、`hybrid.hpp`、`factory.hpp` | ModelData 派生/MutantModel、Model Clone/Allocate/Init、LogDf numeraire/DF、TimelineKnots、factor-aware bridge、模型 dispatch |
| `dal-cpp/dal/script/preparation.cpp`、`simulation.hpp`、`replayadmission.hpp` | dated model guard、普通 passive/AAD、weighted/Jacobian 和 scratch 准入；不可把 wrapper 当普通 Hybrid 的 RTTI 分支 |
| `dal-public/src/models.hpp`、`marketivsurface.hpp`、`value.hpp`、`valuevalidation.hpp` | 新薄工厂和 additive model whitelist；普通 Price、risk 入口共用模型能力检查 |
| `dal-python/src/bindings/marketivsurface.cpp`、`models.cpp`、`value.cpp` | owning typed 值、detached getter、GIL、严格类型、既有 MC settings |
| `dal-excel/src/__marketivsurface.hpp`、`marketivsurface.cpp`、`__risk.hpp` | typed risk-value handle、两列表设置、严格转换、原子输出和生成注册 |

方法术语沿用 [market-IV](../../../docs/models/market-iv-surface.md)、[local volatility](../../../docs/models/local-volatility.md)、[Dupire](../../../docs/models/dupire.md)、[MC simulation](../../../docs/methodology/monte-carlo/simulation.md)：
fit acceptance、连续解析 Dupire、离散 grid 与 MC 时间/抽样精度分别报告；fit certificate 不证明离散重定价精度；native AAD 不是市场 quote vega。

已核对 [活跃工件索引](../README.md) 中七项控制的边界：DAL-398 的 immutable input/字段化错误；DAL-497 三份 PDE 风险/数值控制的 BS-only 准入；DAL-410 和 mixed-mapping XCCY 的独立 owning 模型；DAL-526 的 OIS/quote-risk 合同。本功能不扩展上述产品、PDE 或 quote-risk 准入。

## 2. D1–D6 已决策

| 决策 | 冻结值 | 理由及排除方案 |
|---|---|---|
| D1 来源 | 唯一成功 `MarketIvDupireResult_` 输入；其 snapshot 是市场权威；等内容重建身份相同 | 不接受 snapshot+grid 双输入、裸 surface/matrix、外部 r/q/spot/date 或自报 hash；避免配对歧义 |
| D2 表示与日期 | 独立 owning `MarketIvLocalVolModelData_`；薄 `AAD::MarketIvLocalVolModel_` 只实现 double，委托 Hybrid 路径引擎 | 不伪装为普通 HybridModelData，不复制 MC 引擎；wrapper 提供日期，内部 deterministic delegate 保持无日期，免触发 dated-GSR 整日分步 |
| D3 时间与外推 | ACT/365、显式 horizon、maxStep 默认 1/52、并入所有范围内 carry 和 grid time knots；F/D 末段 log-linear；local-vol tails 平坦 | 不自动推断期限，不冻结末端 F/D 水平，不选择 CUBIC/MIXED，不强制整日步长 |
| D4 归档 | 新模型及其 Excel settings/报告 handle 首版均拒绝 archive；模型及 typed settings 拒绝 pickle | detached dict/list 可自行序列化数值，不能恢复模型或来源；网格 v1 无来源。旧 archive/reader 不变 |
| D5 能力 | 单 EQ/声明的 domestic currency/一个 Brownian factor；passive Price、DF/PAYS、compiled/tree、既有 RNG/bridge | 新模型拒绝 AAD、native/quote 风险、weighted/Jacobian/curvature、segmented、EXERCISE；不通过空轴或 fallback 隐藏拒绝 |
| D6 投影 | 新类型、新工厂、新 getter/schema；独立 Excel settings handle；返回真正 ModelData handle | 旧 BS/Hybrid/local-vol builder/default/table 不变；C++ 下游重编译，无 ABI、跨版本 fingerprint 或 pickle 承诺 |

未留阻挡公共接口实施的选择。内部 helper 的组织可由实现者调整，但必须满足本文语义、预算、能力和依赖方向；改变公共合同须更新此控制并说明影响，不能事后放宽数值阈值。

## 3. C++ 类型、必填项与默认

所有新数值/模型类型位于 `Dal`，core 声明落在新 `dal-cpp/dal/model/marketivlocalvol.hpp`，非模板实现落在 `.cpp`；执行检查的 script 函数落在 `dal-cpp/dal/script/marketivlocalvol.hpp/.cpp`，避免 model 数据层依赖 script。public 在 `dal-public/src/marketivlocalvol.hpp/.cpp` 提供薄入口和字段描述表，不定义第二套数据。

```cpp
struct MarketIvLocalVolModelSettings_ {
    String_ index_, currency_, factor_;
    double horizon_ = std::numeric_limits<double>::quiet_NaN();
    double maxStep_ = 1.0 / 52.0;
    size_t maxInternalSteps_ = 100000;
    size_t maxSimulationDimension_ = 100000;
    size_t numericPayloadBudgetBytes_ = 67108864;
    size_t workspaceBudgetBytes_ = 268435456;
};

void ValidateMarketIvLocalVolModelSettings(
    const MarketIvLocalVolModelSettings_& settings);
Handle_<MarketIvLocalVolModelData_> BuildMarketIvLocalVolModelData(
    const String_& name, const MarketIvDupireResult_& grid,
    const MarketIvLocalVolModelSettings_& settings);

// public factory, same types
Handle_<MarketIvLocalVolModelData_> NewMarketIvLocalVolModelData(
    const String_& name, const MarketIvDupireResult_& grid,
    const MarketIvLocalVolModelSettings_& settings);
```

C++ settings 可逐字段组装；前三个空串和 NaN horizon 是尚未填写的无效状态，构建前必须验证。工厂无默认 settings；必要参数不埋在可选长参数列表。字段映射和约束如下，Python/Excel 使用同一 snake_case 名称：

| C++ 字段 | Python/Excel key | 约束/默认 |
|---|---|---|
| `index_` | `index` | 必填；经现有 plain-equity parser 规范化成一个 `EQ[A]`；拒绝 FX、IR、复合/非 plain EQ |
| `currency_` | `currency` | 必填；现有 `Ccy_` 可识别的 canonical currency |
| `factor_` | `factor` | 必填；非空 UTF-8、无 NUL；一个命名因子，不与组件/输出名称混用 |
| `horizon_` | `horizon` | 必填；有限、>0，单位年；不要求为整数日 |
| `maxStep_` | `max_step` | 有限、>0；默认 1/52 年，允许 1/1000 |
| `maxInternalSteps_` | `max_internal_steps` | size_t；默认 100000；内部正时间区间数上限 |
| `maxSimulationDimension_` | `max_simulation_dimension` | size_t；默认 100000；本模型 dimension=steps |
| `numericPayloadBudgetBytes_` | `numeric_payload_budget_bytes` | size_t；默认 64 MiB；模型 owning 数值容量上限 |
| `workspaceBudgetBytes_` | `workspace_budget_bytes` | size_t；默认 256 MiB；一次请求所有 worker 的数值峰值工作区上限 |

所有 count/budget 的 0 表示不准分配/工作，非无限制：整数 0 可表示，但对应建模/Allocate 必须以 budget error 拒绝。checked 加乘/ceil 在转成 size_t/int 及分配前完成；轴/矩阵维数还须适合现有 int 索引。name 必须非空、有效 UTF-8、无 NUL，只是显示标签，不进入经济身份。currency/index 是调用者对无名称市场数据的声明，不能宣称从 snapshot 识别了交易所或计价币种。

`MarketIvLocalVolModelData_` final 派生 `ModelData_`，Type() 为 `MarketIvLocalVolModelData_`，状态 private；工厂是唯一有效创建途径，不能从 Hybrid settings 重建。公共 const getter 精确如下，均返回值或真正 immutable 值类型：

```cpp
String_ SourceIdentity() const;
String_ ModelIdentity() const;
MarketIvLocalVolModelSettings_ Configuration() const;
MarketIvLocalVolModelSummary_ Summary() const;
Vector_<MarketIvLocalVolCarryTerm_> CarryTerms() const;
MarketIvLocalVolCapabilities_ Capabilities() const;
Vector_<MarketIvLocalVolWarning_> Warnings() const;
MarketIvLocalVolConstructionExecution_ ConstructionExecution() const;
void Write(Archive::Store_&) const override; // throws ArchiveUnsupported
```

继承的空 slide rename/copy 保持相同 immutable state 和 ModelIdentity；非空 slides 延续拒绝，不开放 mutating bump。不暴露内部 delegate/component/可变 surface 或可以重新组合任意 carry 的 public 构造器。C++ 原本的 grid/snapshot getter 保持原签名。

## 4. 来源准入、身份和生命周期

构建顺序：settings/name 结构检查 → grid.IsSuccess → snapshot 状态/证书与每节点成功检查 → 来源数值一致性 → checked 预算 → 复制/冻结 → 计算身份 → 返回完整 handle。任何失败都抛出错误，不返回“部分可定价”模型。成功含 `SUCCESS_WITH_WARNINGS`，从 private FitData 转存 status/underidentified/nearTie，不能因为 snapshot 公共 getter 少两项而丢掉警告。

模型只使用 grid 内嵌 snapshot。逐行/列验证 request.spots/times 与 SurfaceData 的轴完全相同，Nodes 长度=rows×cols、spot-major 节点顺序 `i*nt+j`、DupireStatus=SUCCESS、local vol 有值且有限非负，SurfaceData.vols(i,j) 与节点 LocalVol 值相同。snapshot certificate 必须 certified；spot>0、F/D>0 且有限；不允许用复制 v1 surface 的 archive 绕过准入。

不可变来源保留原始请求、fit settings/selected fit 语义、theta/slopes/certificate/regime、grid request/节点结果以及数值 surface；可共享真正 const snapshot/节点块，必须深拷贝所有可能经公开可变成员别名修改的 surface 数组。getter 改动、销毁请求/result/Excel 源 handle 均不影响已建模型；持有的对象数据才是权威，hash 不是权限或可信输入。

身份使用现有 `dal/storage/canonicaljson_internal.hpp` 的 JCS + SHA-256，值形如 `sha256:<64 lower hex>`，schema domain 分别为：

- `dal.market-iv-local-vol-source/1`：原始 request.market（date ISO、spot、carry_mode、rate/dividend 或全部有序 F/D terms）；全部按输入顺序的 quotes（id/expiry/strike/option_type/quote_type/mid/bid/ask/weight，缺省 optional 为 null）；全部 fit settings；fitted rho/eta、times/theta/slopes、certificate 的所有字段、regime_signature、selected_start、status、underidentified、near_tie；grid_request.spots/times/全部 settings（含 evaluation 和预算）；grid local_vol 矩阵按 spot-major；插值/外推版本。
- `dal.market-iv-local-vol-model/1`：source_identity、规范化的全部 model settings（含预算）、`ACT/365`、`LOG_EULER_LEFT_VOL/1`、`VOL_LINEAR_LOG_SPOT_TIME_FLAT/1`、`LOG_F_LOG_D_LINEAR_LAST_SLOPE/1`、`UNION_CARRY_GRID_EVENTS/1`、capabilities schema。

canonical 根 key 分别 `schema,market,quotes,fit_settings,fit,grid_request,local_vol,interpolation` 和 `schema,source_identity,settings,time_basis,step_scheme,local_vol_scheme,carry_scheme,timeline_scheme,capabilities`。对象键依 JCS 排序；数组顺序保留；有限 double 用 JCS 最短表示，-0 与 0 等价，optional 显式 null。日期 ISO；enum 用 canonical token。排除 name、指针、运行时间、机器/线程数、allocator capacity、迭代日志及执行计数，避免同内容因运行轨迹不同而改变身份。原始 quote 排序变化视为新来源。相同完整内容的复制/重建接受且同 identity；refit、任何 F/D/grid 数值或配置变化则重算，不复用旧 preparation。

S1 的内部 canonical 编码将 size_t count/budget 字段保存为十进制字符串，避免经 JCS 的 binary64 数值丢失 2^53 以上的整数位；这不改变 C++/Python/Excel 的整数类型和取值范围。

每次请求拥有其 prepared state；克隆可共享 source，不能共享 mutable columns/path/scratch。Clone 重绑参数指针和 delegate 所需的定义/时间线生命周期；不能继续借用已释放 defLine 指针。准备失败清理局部状态；原模型和并发请求仍可用。运行时 path 的不可表示 spot/numeraire/DF 必须报错，不能把 Inf/0 当成功；构建期不能保证正态随机变量的所有极端样本永不越界。

## 5. 数学、日期、时间网格

`t=(date-evaluationDate)/365.0`，含闰日，无营业日调整，无全局估值日捕获。wrapper.EvaluationDate() 返回 snapshot date；现有 script 日期解析/历史 fixing/today policy 继续工作。valuation 解析出的 date 必须相同，包含“未显式给 valuation”的调用：旧入口仍按原逻辑解析全局日期，然后新模型 guard 检查。推荐 happy path 显式 date。

令 `L=log(F/S0), H=log(D)`，L(0)=H(0)=0。一般市场从原点到首节点逐段线性，末节点沿最后段斜率外推（只有一条 term 时用原点首段）。FLAT_CARRY 保持显式 marker，直接用 `L=(r-q)t, H=-rt`，不把一般结点自动识别成 flat。

```text
r = -H'(t); drift = L'(t); q = r-drift       (knots use right derivative)
R[a,b] = H(a)-H(b)
Q[a,b] = R[a,b]-(L(b)-L(a))
Δlog S = R-Q-σloc(a,S_a)^2*(b-a)/2 + σloc(a,S_a)*sqrt(b-a)*Z
N(t) = exp(-H(t)); DF(t,u)=exp(H(u)-H(t))
```

现有 Hybrid 把 rate 给出的 R 传入 component.Prepare；新内部 term-carry equity component 预备 Q，以 R-Q 演化，不能再加一次 L 增量。也可在同一 helper 内直接预备等价 R-Q，但必须在 N1 对两种表达做一致性检查。新 component 是 dedicated model 的内部构件，首版不开放任意 Hybrid 组合/AAD。LogDf rate 强制 LOG_LINEAR 和同源 H。不要用右端 r/q 乘 dt 近似积分。

一步条件期望为 `S_a*F(b)/F(a)`，因此 E[S_t]=F(t)。q 非零时 `D(t)S_t` 不是鞅；应检验 `S_t/F(t)` 或 `exp(∫q)D(t)S_t/S0` 的均值为 1。DF 延迟支付满足 `DF(t,u)/N(t)=D(u)`。

wrapper 在 Allocate 前校验所有 event/observation/payment 与 discount maturity 均≤horizon，DF maturity≥其 sample time。实际积分到本请求所需最大时间 Treq，不无条件模拟整个 horizon；Treq 是事件/支付/DF maturity 最大值。内部 anchors 为：
`{0,Treq} ∪ 产品事件/未来观察/支付/DF maturities ∪ (0,Treq] 的全部 market terms ∪ (0,Treq] 的全部 grid.times`。
精确排序去重，不用 tolerance 合并不同合法 knot；产品事件顺序及同日合约次序不变。每相邻区间 [a,b] 用 n=ceil((b-a)/maxStep)，均匀细分，端点精确取原 anchor，n≥1；checked 总和 m≤step/dimension 限额。必要时增加私有 terminal sample/definition，使延迟 DF maturity 的 grid anchor 不会被 Hybrid.GridAnchors 的 productTimeLine.back() 截掉；该 sample 不成为用户产品事件/输出，映射和 scratch 由 wrapper 持有。今天唯一事件且无未来 maturity 可 m=0，仍需合法正 horizon；SimDim=0 不抽取 normals。

冻结 D2 实现路线：wrapper 提供外部日期与 horizon/capability guard，delegate 的 rate/component.EvaluationDate 返回 nullopt；delegate 以连续 double 时间网格执行。这样不修改旧 GSR 的 >=1日/整数日保护。Clone 仍是 wrapper 类型，不能克隆后丢 date/horizon。转发 SupportsIndex、NumAssets/AssetNames、NumFactors、MaxObservedIndices/MaxOutputSlotsPerSample、SupportsBrownianBridge、NumeraireIsDeterministic、SupportsDiscountFactors、Allocate/Init/SimDim/GeneratePath。Parameters/ParameterLabels 返回空且不暴露可变源；空轴不是风险成功的依据。

保持现有 Brownian bridge 的坐标/时间处理约定，不宣称新增精确日历方差 bridge；普通 Price 的 use_bb=true/false 都可用，分布一致，等 draws 比较只在相同 grid/bridge 配置内成立。来源、grid、网格/设置任变即重新 Allocate/Prepare。

local vol 是 σ 而不是 σ²，空间对 log(S)、时间对 t 线性；t=0/首列前取首列，末列后取末列，spot 越界取端点。build 固定暴露 `LOCAL_VOL_FIRST_COLUMN_AT_ZERO`，horizon>末 grid time 加 `LOCAL_VOL_TIME_TAIL`，horizon>最后 market term 加 `MARKET_CARRY_TAIL`；路径 spot 越界按已声明 flat policy，无声称市场证书覆盖该路径区。不夹断路径到 spot grid。log 范围检查覆盖 S0、F、D、1/D、DF 比值、逐步 R/Q、σ²dt；失败指出 time/term/step，允许负 r/q 和 D>1。

## 6. 能力检查覆盖

新 `MarketIvLocalVolCapabilities_` 是只读值，字段顺序：
`schema_="dal.market-iv-local-vol-capabilities/1", passivePrice_=true, discountFactors_=true, brownianBridge_=true, compiled_=true, tree_=true, aad_=false, nativeRisk_=false, quoteRisk_=false, curvature_=false, segmented_=false, exercise_=false, archive_=false`。Python/Excel 用 snake_case。不引入全库新的 capability 框架；增加针对新类型的集中 admission helper，旧模型规则原样生效。

| 通路 | 新模型的检查点/结论 |
|---|---|
| core CreateModel<double>、普通 Script MCSimulation/Price、public Value、Python/Excel 两种 MC Value | 唯一合法 passive adapter；更新 public SCRIPT_MODEL_STORE，保留旧 Type 字符串 |
| CreateModel<AAD::Number_>、enableAad=true、WithRisk | 在 tape/recording、worker dispatch、RNG 前报 DerivativeUnavailable |
| Weighted/Jacobian（含多输出/空请求）、portfolio risk 中含该模型 | 无论轴是否空均先检查模型；有新模型则拒绝该请求并定位 trade/model，不返回零/空风险成功 |
| Dupire parameter pullback、quote-refit/风险重建入口 | 不从新模型提取旧 Hybrid 风险 snapshot；拒绝 DerivativeUnavailable；已有 MarketIvRefitRisk 仍只对曲面输出 full-refit |
| curvature/segmented/固定 BS 或 GSR plan 的 typed API | 本来不接受本 ModelData，保持类型拒绝；若存在通用 ModelData bridge，先集中检查并拒绝，不能经 downcast/序列化绕入 |
| product EXERCISE、LSMC 自动选择、ExplainScriptSimulation | 有效请求在任何训练/抽样前报 UnsupportedMarketIvLocalVolExercise；diagnostics 不能绕过 |
| ExplainScriptValuation、新执行 inspection | 允许 passive prepare；仍检查日期/期限/能力，不抽样 |
| archive/JSON/Excel 库存保存、pickle | ArchiveUnsupported/PickleUnsupported；包括从 ModelData/Storable 基类转入的路径 |
| 未知 EQ/foreign currency/FX/IR 观察 | 保持 provider 拒绝；新 guard 补 field/expected。无 foreign numeraire/quanto |

复用普通线程/编译器/RNG；no hidden fallback。请求还受所选 RNG 的现有 dimension/path 寻址上限约束；模型的100000上限不是 Sobol 方向数保证，preflight 须在准备大列/RNG分配前检查较小的实际上限，超限字段为 /simulation/rsg，拒绝且不换算法。拒绝位置允许发生在已有更早的参数类型/结构检查之后，但所有有效 unsupported 请求均须在 tape/大 workspace/抽样前失败。新增 guard 不能修改旧 flat local-vol/Hybrid 的 native AAD 或 EXERCISE 能力。

## 7. 字段化错误与跨端转换

新增 `MarketIvLocalVolError_` 派生 `ScriptError_`，保存只读 `code_,stage_,field_,expected_,actual_`（全部 String_）；stage token 为 Input/Build/Prepare/Evaluate/Archive。field 是零基 JSON-pointer 风格路径；actual 为简短安全值，不输出整份市场/handle。what() 的业务消息以 `code: field=...; expected=...; actual=...` 开始（保留既有 Exception 的位置栈）。不返回 NaN/空 handle 表示失败。

| code | field 示例/约束 |
|---|---|
| InvalidMarketIvLocalVolRequest | `/settings/horizon` 有限>0；`/settings/index` 一个 plain EQ；`/grid/nodes/12/local_vol` 有限非负；unknown/duplicate key 指原 key |
| MarketIvDupireUnavailable | `/grid` 成功的 DupireResult；携带原 Failure 摘要 |
| MarketIvSourceMismatch | `/grid/surface/vols/2/3` 与 authoritative node 同值；主要针对内部完整性/别名污染，非新增双输入 |
| InvalidModelEvaluationDate | `/valuation/evaluation_date` 必须等于 snapshot date；沿用旧稳定 code |
| MarketIvLocalVolHorizonExceeded | `/product/discount_maturities/2`≤horizon，actual 含请求时间 |
| MarketIvLocalVolBudgetExceeded | 对应设置/ledger 字段，actual=planned bytes/count，expected=limit；0 仍是 limit |
| MarketIvLocalVolNumericRange | `/carry/terms/3`、`/execution/steps/8` 或 `/path/spot`；指数/比率/σ²dt 可表示 |
| DerivativeUnavailable | `/simulation/enable_aad`、`/risk/model`；expected=passive Price |
| UnsupportedMarketIvLocalVolExercise | `/product/exercise/0`；expected=product without EXERCISE |
| ArchiveUnsupported / PickleUnsupported | `/model` 或新 settings/report handle；expected=in-process handle |

Python 原生错误翻译为 `MarketIvLocalVolError(RuntimeError)` 并带上述五个 snake_case 属性。绑定阶段错误类型保持习惯：wrong DAL type/bool/numeric-text/enum→TypeError，有限值/范围/缺必填/未知 key→ValueError；消息也含 code/field/constraint。不把 bool 当 count/real，不静默转换 Fraction/Decimal/ndarray scalar，除非现有共享严格 converter 明确接受且三端文档一致。count 为非 bool Python int、0≤x≤size_t。

Excel typed converter 只接收正确类型且未过期 handle；空 required 值拒绝，空 optional value 使用默认，尾部双空行可忽略，中间/半空行拒绝；重复 key 即使值相同也拒绝，unknown key、字符串数值、bool、Excel error、非整数 count 拒绝。counts≤min(size_t_max,2^53-1)，whole finite number；表 row/col 附在原字段位置之后。字节 ledger 转为 Excel 数字前检查可精确表示；溢出拒绝，不舍入。先完成 native 构造、spill 校验与分配，再原子发布输出；失败保留旧 repository handle。

所有新 getter spill 检查 Excel 行≤1,048,576、列≤16,384（含 header），不截断。编写接口 markup 后用 dal_generate/dal_check_generated，不手改 auto。新 schema 不静默给旧 market-IV/MC 表加列。

## 8. 诊断类型与 getter schema

以下字段名/顺序为 v1 schema；以后新增列另升版本/新 getter。C++ 新 record 字段用 camelCase+下划线，Python/Excel 用表中 snake_case。数值字段：时间/价格/log 为 double，count/bytes/ordinal 为 size_t，date 为 `std::optional<Date_>`，标志为 bool，其余为 String_。getter 全部 detached；C++ const 方法可返回 struct 值，Python dict/list 及 Excel spill 均为副本。无 mutable “last valuation” 写回 ModelData。

- `MarketIvLocalVolModelSummary_` / Summary / Python `summary` / Excel `...GET.SUMMARY(model)`：固定 field/value 两列，按 `schema="dal.market-iv-local-vol-model-summary/1", source_identity, model_identity, evaluation_date, spot, carry_mode, index, currency, factor, horizon, max_step, grid_rows, grid_columns, grid_first_time, grid_last_time, fit_status, underidentified, near_tie, time_basis, step_scheme, local_vol_scheme, carry_scheme, timeline_scheme` 顺序。enum/policy 字符串与身份中的 token 相同。evaluation_date 是 Date_ / Python Date_ / integral Excel serial。
- Configuration / Python `configuration` / Excel `MarketIvLocalVolModelSettings_Get_Configuration(settings)` 和 `MarketIvLocalVolModelData_Get_Configuration(model)`：field/value；schema `dal.market-iv-local-vol-settings/1` 后接第 3 节九个 key，精确输出 resolved 值，无隐含 horizon。Python 返回独立 settings 值。
- `MarketIvLocalVolCarryTerm_` / CarryTerms / Python `carry_terms` / Excel `...GET.CARRY(model)`：header 严格为 `ordinal,date,time,forward,discount,log_forward_over_spot,log_discount,origin`。含原点、全部原始 market terms（包括 horizon 外的原始来源行）及 horizon 插值/外推行，按 time 排序去重；origin=ANCHOR/MARKET/HORIZON/MARKET_AND_HORIZON。ordinal 从 0 起。非整数日 horizon 行 date 缺省（Python None、Excel 空白），不四舍五入；其他行是真实市场 date。该表是来源/配置观察，不是内部 step 表。
- Capabilities / Python `capabilities` / Excel `...GET.CAPABILITIES(model)`：field/value，字段顺序见第 6 节。
- `MarketIvLocalVolWarning_` / 新 `Warnings()` / Python `warnings` / Excel `...GET.WARNINGS(model)`：`code,field,message` 三列，按 code、field 排序。包括 FIT_UNDERIDENTIFIED、FIT_NEAR_TIE 及第 5 节覆盖警告；无警告是仅 header 表，非错误。
- `MarketIvLocalVolConstructionExecution_` / ConstructionExecution / Python `construction_execution` / Excel `...GET.CONSTRUCTIONEXECUTION(model)`：field/value，schema=`dal.market-iv-local-vol-construction/1`；`planned_numeric_payload_bytes,numeric_payload_bytes,planned_workspace_bytes,peak_workspace_bytes` 四个 size_t。
- SourceIdentity/ModelIdentity Python 为 `source_identity/model_identity` 只读 str，Excel 从 Summary 取得；不需要可变 source_handle getter。

请求相关执行 inspection 是单独的只读准备操作，绝不“查看上次定价”：

```cpp
namespace Script {
MarketIvLocalVolExecution_ InspectMarketIvLocalVolExecution(
    const ScriptProductData_& product,
    const MarketIvLocalVolModelData_& model,
    const ScriptValuationSettings_& valuation = {},
    const MonteCarloSettings_& simulation = {});
}
// public same name/arguments in Dal; forwards to Script
```

它调用与 Price 完全相同的 capability/date/horizon/预算/Allocate/Init preparation，释放局部准备区后返回 owning 报告；不运行路径/RNG、不改变全局线程或模型。费用包含 preparation；“inspection 成功”不代表后续所有随机路径成功。新 `MarketIvLocalVolExecution_` 的 v1 field/value 顺序为：
`schema="dal.market-iv-local-vol-execution/1",source_identity,model_identity,request_identity,evaluation_date,requested_max_step,resolved_max_step,required_time,carry_knots,grid_time_knots,anchor_count,internal_steps,simulation_dimension,workers,shared_numeric_bytes,per_worker_numeric_bytes,planned_numeric_payload_bytes,numeric_payload_bytes,planned_workspace_bytes,peak_workspace_bytes,prepared_columns,prepared_market_queries,per_path_market_queries`。
request_identity 域 `dal.market-iv-local-vol-request/1`，包含 model_identity、完整产品源/准备后的定义及时间、resolved valuation/today/fixing 内容、resolved simulation（含 normal/bridge/compiled）、worker 数、ledger/layout version；同一来源的并发产品不可串表。
resolved_max_step 是实际最大区间长度（无步为 0），不是随意回填默认；count knots 只计 (0,Treq] 内精确不同时间。prepared_columns=m（每个左端）；per_path_market_queries=0。

Python `MarketIvLocalVolModelData_Get_Execution(product, model, *, valuation=None, simulation=None)`，None 沿用现有默认 settings。Excel `MarketIvLocalVolModelData_Get_Execution(product, model, valuation_optional, simulation_optional)`；worksheet 名为 `MARKETIVLOCALVOLMODELDATA.GET.EXECUTION`。返回字段与上述类型一致，不能原地向旧 `dal.script-simulation/1` JSON 加字段。

## 9. 数值容量与时间预算

预算为 numeric ledger，非 RSS。同时报告 owning retained capacity、局部 construction/preparation 峰值、共享一次与 worker 私有之和。排除字符串/UTF-8 canonical 临时文本、语言 wrapper、Excel spill、allocator header/fragmentation、普通 MC 的 payoff evaluator/RNG 独立内存；这些不能被宣传成总进程内存受 256 MiB 限制。canonicalization 必须流式或有 checked 文本大小估计，不能借“排除文本”无限复制；grid 原有 maxGridPoints/预算继续独立准入。

ledger 按 capacity（非 size）计每份实际 owning 数值数组，checked 加乘。snapshot 数值块可共享只计一次，但该模型可达块的完整费用仍计入；不能减掉 caller 已有对象。surface 复制费用和原始 grid immutable nodes 若同时持有两份则双计。构造期先从既有 grid.Execution 的保守 payload 包络加新增 carry/模型数组计算 planned，再大复制；实际 retained≤planned≤model payload budget，construction 数值 workspace≤planned≤workspace budget。实现应有单一 allocation plan 和统一 ledger function，不用依平台会变的 sizeof 字符串作经济身份。

对 m 步、ns 个 spot、nt 个 grid 时间、W 个实际 worker，显式下界/主要项：

```text
vol matrix       = 8*ns*nt, plus immutable source numeric arrays
prepared columns = 8*ns*m
per-step arrays  = 8*(m+1) timeline + 8*m*(R,Q,rootDt)
path normal/state/mapping/batch/numeraire/DF arrays = each actual capacity * element width
workspace peak   = max(construction_peak,
                       planning_scratch + shared_prepared + W*private_prepared_and_scratch)
```

不能将上式当完整字节公式漏掉 vector capacities、索引、复制/并存峰值；每个新增数组均登记 owner/element width/capacity/共享次数。允许共享只读 prepared columns；若 delegate clone 实际深拷贝 columns/vol matrix，必须将 W 份计入且在 clone 前预算拒绝。初始化/准备次数与 path 数无关；每路径市场曲线查询=0，不保存全部 paths。预算快测使用 ledger 返回的准确 planned 值，正好限额接受、少 1 byte 拒绝；以动态报告为准，不硬编码 STL 容量。

时间/工作量由 step/dimension 上限以及 existing path-count/worker 控制约束，不承诺 wall-clock timeout。horizon/maxStep 导致 ceil 或并集计数溢出、极大/次正常 dt、零 dt 等在大 allocation 前失败。请求首次 Price 和 inspection 使用同一 preflight；不得 inspection 说可用而实际 path allocator绕过预算。长期运行另受数值鉴证 fixture/sample cap 和 benchmark calibrated sampling 约束。

## 10. Python / Excel 调用与 happy path

Python 注册新派生类（匹配当前 shared_ptr<ModelData_> holder）且继承 ModelData_，无用户可调用的裸构造；返回值可直接传普通 MC。Python `MarketIvLocalVolModelSettings_(index, currency, factor, horizon, *, max_step=1/52, max_internal_steps=100000, max_simulation_dimension=100000, numeric_payload_budget_bytes=67108864, workspace_budget_bytes=268435456)`：必填在前，其余 keyword-only，属性只读。工厂 `MarketIvLocalVolModelData_New(name, grid_result, settings)` 无可选默认；输入 typed 值复制/持有期间释放 GIL，只运行 C++，返回前重获 GIL；native 不回调 Python。copy/deepcopy 保持 immutable owning 语义及身份，pickle 明确拒绝。

Excel handwritten `__marketivlocalvol.hpp/.cpp` 使用独立 `StorableRiskValue_<MarketIvLocalVolModelSettings_>`。构造签名：
`MarketIvLocalVolModelSettings_New(name, key_value_table, out_settings)`、
`MarketIvLocalVolModelData_New(name, dupire_result_handle, settings_handle, out_model)`。
out_model 是真正 `Handle_<MarketIvLocalVolModelData_>` 可向 `Handle_<ModelData_>` 转换，不是 risk-value 包装的模型。worksheet 名依生成规则为 `MARKETIVLOCALVOLMODELSETTINGS.NEW` 和 `MARKETIVLOCALVOLMODELDATA.NEW`；GET 名依第 8 节相同映射。settings 表必含 index/currency/factor/horizon，只有 optional key 的空值才取默认。不接受整张 settings 表为空。

以下片段只展示新增接线；quotes、产品和 valuation 来自相同显式日期的现有 recipe，不伪造可成功的 fit；完整 runnable 数据由 S5 提供。C++：

```cpp
const auto fit = FitMarketIvSurface(request);
REQUIRE(fit.IsSuccess(), fit.Failure());
const auto grid = CalibrateMarketIvDupire(fit.Snapshot(), gridRequest);
MarketIvLocalVolModelSettings_ settings;
settings.index_ = "EQ[A]";
settings.currency_ = "USD";
settings.factor_ = "W_EQ";
settings.horizon_ = 2.25;
const auto model = NewMarketIvLocalVolModelData("market-lv", grid, settings);
const auto pv = ValueByMonteCarlo(product, model, 65536, valuation);
```

Python（fit/grid 继续走现有 API）：

```python
grid = dal.MarketIvDupireResult_New(fit.snapshot, grid_request)
settings = dal.MarketIvLocalVolModelSettings_("EQ[A]", "USD", "W_EQ", 2.25)
model = dal.MarketIvLocalVolModelData_New("market-lv", grid, settings)
pv = dal.MonteCarlo_ValueWithSettings(product, model, 65536, valuation=valuation)
summary = model.summary
```

Excel 最短工作流：成功 fit 的 snapshot → `MARKETIVDUPIRERESULT.NEW` → 上述 settings/model NEW → `MONTECARLO.VALUEWITHSETTINGS(product,model,65536,valuation,simulation)`，其中 simulation.enableAad=false、valuation date=snapshot date；用 SUMMARY/CARRY/EXECUTION 检查来源和期限。不以界面默认误开风险；用户显式设 true 即稳定拒绝。

## 11. N 门槛可执行性与独立参考

计划的 N1–N7 数值门槛保留，单位、SE 和独立 oracle 不放宽。S0 仅静态核验，不提供 PASS。F0/F1/F2、负 r/q、未报价 carry knot、t=0/首段/尾段、T={91/365,1,2,2.25} 和 K/F={0.7,0.85,1,1.15,1.3} 固定；call/put 不作为重复拟合经济报价。Black 生成 F1 使用 erfc，不调用生产 Black。

S3唯一现行增量控制为 [DAL-542 方案 A/3](../specs/dal-542-n5-resolution-a.md)。本次批准 `01a1e58d-1d44-78ac-9b5e-089ac7de74f0`、范围确认 `01a1e590-7664-777a-82c6-7fe8329b5776`，对应附件 `01a1e58a-e622-7842-828a-61f3a2d48278`（SHA-256 `6c8b2c4a2db8244fb01dcaa3ac48043b72706260200ae288148b2530a1d4fbb2`）。仅修订R11唯一段、R12/R14固定标量槽额度和真实FAIL/16条未执行预留的显式恢复；A/1、A/2批准及来源/hash/失败保留为历史，A/2主控制hash为 `84a6de0aa9c0773e74022a5d21ab865291c15fed37fe913f5b53f496d75e485a`。D1–D6、N1–N7两份门槛表、生产/API/default/PDE准入不变，R10诊断地位和必需门禁继续生效。用户追加 `01a1e58d-bf5d-7616-84e8-eb2f47673218` 授权计划内纠偏/控制/实施/review/修复/合并自主接续，不再逐次请求同类批准。旧320行184 PASS/76 FAIL/60 REFERENCE_UNAVAILABLE及Request Changes保留；当次480行仍160复用PASS/320新NOT_RUN，冻结不构成S3 PASS。

F1 fit 初始精确冻结：initialVolatility=.20、normalizedPriceTolerance=1e-8、decimalIvTolerance=1e-8、projectedGradientTolerance=1e-10、parameterStepTolerance=1e-12、objectiveTieTolerance=1e-14、feasibilityTolerance=1e-12、maxIterationsPerStart=400、maxModelRowEvaluations=5000000；其余 existing 默认。报价无 bid/ask（由上述容差构造窄带）；生成行情使用 PRICE/DECIMAL_IV 两种单位、call/put 交错，每 K/T 一条。当前 fit 有 eta=0 seed/bound 及线性 theta 的精确退化路径，附近 flat-fit 测试支持该方案的源码可行性。N3 必须先满足 fit价格/S0≤1e-6 且实际 local vol 与 .20 差≤1e-7；否则报告 fit/fixture FAIL，不拿 MC 抽样带掩盖。F2 默认 fit 设置，连续参考使用实际成功 snapshot，不假定恢复生成参数；另外报告拟合相对生成价格误差，未规定的 F2 fit 精度不能冒称通过 N3。

参考 A 独立分段 logF/logD/Black；B 用 mpmath>=1.3、80 位，独立重建 theta/PCHIP 及 SSVI 导数，固定实际 snapshot 和校验和；禁止缺库 importorskip 后仍算正式 N4 PASS。C 测试专用 log-spot PDE，选 backward Crank–Nicolson + payoff 初始两段隐式 Euler 阻尼、carry knot 精确时间分段、独立边界公式；不调用生产 Hybrid/LocalVolSurface/MC/PDE kernel。连续系数和“σ 在 logspot/time 线性、平坦边界”的离散系数分别独立实现。spot PDE 包含 `r-q=L'`、`-rV` 符号，付款延迟单独检查 D(u)。

C 先过以下自证才作为 oracle：独立 Black 的绝对价格误差/S0≤2.5e-5；概率质量/归一化 forward 的误差≤2.5e-5；最后空间/时间/扩域每个独立差/S0≤2.5e-5。既有连续 oracle 保持原均匀空间 {801,1601,3201}、时间 {512,1024,2048}、[-3,3]→[-4,4] 和唯一 {6401,4096} fallback 的已自证证据。A/1冻结的离散reference算法由A/3继承：knot/strike-fitted非均匀中央 {601,1201,2401,4801}、匹配中央mesh的扩域 {801,1601,3201,6401}、外段从两侧各100初始区间同步二分、CN midpoint/两半IE、原时间阶梯及唯一四求解fallback，精确三点式/层级见A/3 R7–R10。上限仍6401节点/4096 requested steps，actual保守≤4342，两半IE另多一次线性求解。仍未自证为REFERENCE_UNAVAILABLE；仅R10旧8项数值未自证为非阻塞诊断，不可作oracle或认证归因，Black/P7/P8必需自证仍阻断。全部层级/残差留证，不能以生产MC反证参考。

F2 grid 采用 A/3承继的 R4–R6 的 P0–P8：宽 logspot 轴并中心 [-.02,.02] 轴、规则时间列并首段几何列及全部 carry knots；90位求值后一次舍入 binary64，仅按相同值去重。最大401×246=98646点，maxGridPoints=100000不变，禁止删列/临时加点。P0–P6仅诊断；正式预定 P7/P8 都用扩spot域[.1S0,6S0]、τ=1/146000，末列分别2.25/2年，完整参数表见 A/3。h固定为 {1/384,1/768,1/1536,1/3072}，最终只接受最后一档总价及最后两档配对。保留全部 canonical anchors 和严格 n/2n/4n/8n 嵌套，报告实际 max dt；间距不能三次严格二分或模型增非嵌套步时停止 INCOMPLETE，不按价格调轴。F1配置及已有有效证据不变。

| 门禁 | 冻结判定 |
|---|---|
| N1 | logF/logD/R/Q 绝对误差≤2e-12；F/S0、D、N*D 归一化误差≤2e-12；日期错配准备前失败 |
| N2 | 内部 zero-vol 确定性 S/S0、N 误差≤2e-12；同网格同 normals flat 与旧模型比较，grid 改变不要求逐位一致 |
| N3 | 前述 fit/local-vol 预检；MC价格/均值差/S0≤5e-5+4.5ε 且 ε≤1e-4；确定性 DF 支付归一化误差≤2e-12 |
| N4 | 非病态节点 local variance 误差≤2e-10*max(1,abs(ref))；参考 C 每项最后加密/扩域差≤2.5e-5*S0 |
| N5 | 离散-grid PDE vs 连续 PDE≤2.5e-4*S0；MC h/h2 配对差/S0≤2.5e-4+4.5ε差 且 ε差≤5e-5；MC vs 连续 PDE≤6e-4+4.5ε 且 ε≤1e-4 |
| N6 | owning/复制/别名/销毁输入/clone/并发1和4线程；相同环境/配置/draws 遵守现有 reduction；无不合法可用模型 |
| N7 | 预算精确边界/0/overflow、range、坏grid/horizon、所有能力拒绝在大分配/抽样/tape 前；无 NaN 成功 |

τ/尾段连续参考按解析 Dupire，离散参考按已声明 local-vol flat policy；T=2.25 超过 quote/carry 最后2年。A/3承继的P7铺到horizon=2.25，P8最后时间=2年，二者首列均为τ/4=1/146000并独立报告 N5 差及警告；若无法满足阈值为 FAIL，不把“外推有警告”当豁免。PDE 极远 logspot 位置的 oracle 本身不可表示/病态必须明确列 reference 不可用，不删失败点美化汇总。

### 统计协议与原计划的必要修正

普通 `MonteCarloSettings_` 没有通用 passive seed/offset 参数。重复调用 public Price 16 次默认 RNG 会复用同一序列，不能充当独立批次。MRG32 的 `Branch(iChild)` 当前返回相同默认引擎，亦不能用它制造独立流。S3 在测试 harness 中驱动生产 CreateModel/Allocate/Init/GeneratePath 和同一 passive payoff evaluator，使用已有标量 `PseudoRandom::New(MRG32,1024,1,true)`、`SkipUniformDraws` 精确寻址；不新增用户 seed API/第二套路径算法。

固定 batch b=0..15、每批最大 Pmax=2^20，某比较族最大细网格需要 Dmax 个正态坐标；固定标量位置 `((b*Pmax+p)*Dmax+j)`，p=0..Pmax-1，j=0..Dmax-1。同族不同 h/grid/tier 保留完整 fine 坐标槽，粗步通过同一细 Brownian 增量求和，除以 sqrt(dt粗)；不按粗 SimDim 重新解释同 ordinal RNG。checked 64-bit offsets，Precise inverse normal 一 draw 对应一 coordinate；跨批槽不重叠，不把 PRNG 数学独立当严格定理。测试先与 production 默认 stream 的小规模首批同配置逐路径/价格比较，并经 public 三端小集合证明接线一致。

上述族内原则保留；A/3 F2跨族地址为 `(((f*16+b)*2^20+p)*8736+j)`，Dmax=8736、j=0..8735，f=0/1分别为linear/nonlinear，每族P7/P8×四档h共8模型。438 master anchors/437段全部端点保留，仅从0计数段60同时匹配 `0x1.0006f5248e2e8p-4 → 0x1.2006f5248e2e8p-4`（精确1/128）时粗n=3→4，其余436段保持ceil(384*Δt)。四层仍n/2n/4n/8n，例外段步长1/512、1/1024、1/2048、1/4096；固定维数1092/2184/4368/8736、checked上界10408，h/生产严格maxStep不变。每族146565758976槽、两族fine-draw硬上限293131517952、最后地址293131517951，增加268435456约0.09166%；全部乘加/族预留checked uint64。八模型32730→32760步，+30/字段；prepared columns增96240 bytes/batch、16批1539840 bytes，仅结构计数，不能称实际峰值/时耗保证。后续实际核验全部间距/严格2x嵌套/权重、八模型Allocate/Init及完整R13对齐，不能接受旧输入1093/2184/4372/8736或删非嵌套断言。旧Dmax1584/8728 raw不可复用；完整映射和停止规则见A/3 R11–R16，F1原适用性不变。

每批路径阶梯 {2^16,2^17,2^18,2^19,2^20}，只因任一预登记输出 SE 超限才升级整个比较族；已有样本采用相同前缀，未因价格好坏挑 seed。16 独立分块均值估 SE，自由度15，ε=SE/S0、ε差=paired-SE/S0；4.5SE 是固定统计带，非无条件置信概率。达到上限仍 SE 不足为 INCOMPLETE；不能把 Sobol 当 IID，Sobol 仅作辅助重复性/bridge 证据。

A/3继承R10必需顺序及旧8项必做非阻塞诊断地位。已完成10个P3重验、48中央层、8Black、8旧REFERENCE_UNAVAILABLE、16新reference/grid、56单层诊断INCOMPLETE、393782个N4节点及1984探针，在reference/grid/carry/payoff/mesh/kernel及相关依赖未变时按逐证据字段级映射复用；request总hash因MC protocol改变不等于这些输入改变。保留旧执行身份/hash及supersedes，不把旧层重标为新request执行，F1/24连续研究/57生产回归仅保持原适用性。身份/输入/实现/资源异常及所有必需门禁仍阻断；完整F2 alignment、正式MC/paired/SE仍缺失。

累计最多376次含全部历史用量/重验/诊断/必需/fallback；354条件路径/386未截断需求是A/2历史预算分析，不能突破376。继承当前290/290、剩86；PDE221.18207862298004秒、other94.90929656999651秒、MC0 1.5628903489996446秒、MC1 0秒、总317.6542655419762秒，active_phase=null。PDE6小时、MC每族6小时、总18小时、RSS6GiB/证据1GiB、每族16×2^20/两族33554432路径不变；不足真实INCOMPLETE，不能追加额度或回退已耗成本。

真实FAIL恢复先封存A/2执行source `fa0740a86b934c7b8cb21fde480e4ffec58e87a7`、identity `1099c383560b6d252b0dd3a6f7d1ec27649e7bbe8637dee64c3cd9de067094f8`；发布 `9b0de583…` 仅README，不是新执行。失败/截断JSON/manifest、原180字节前缀/181迁移及完整2284事件/hash链保留；之后追加disposition/transition绑定旧/新控制、实际source/binary/config/request/family/slots及逐证据映射。tiny fine在首个Level构造、RNG创建/FillNormal前失败，保留paths=[16,0]、paths_completed=[0,0]及全部成本；每个b=0..15仅将旧(f=0,b,p=0,Dmax8728)显式退役并一对一替换到新同(f=0,b,p=0,Dmax8736)，逐条记录old/new坐标身份、地址及未执行依据，不新增/清零已计预留，完成数保持0直到新tiny真实成功。两个coarse64-path及截断结果不构成完整alignment，不填新统计；禁止旧freeze重跑、manifest重绑或空预算。后续原tester先隔离合成账RED/GREEN及实际协议/完整R13验收，再正式MC；本轮不改账/程序。完整身份、恢复、停止细则见A/3 R10–R18，S3/父保持in_progress、PR保持draft，全部资格及合并门禁满足前不合并或启动S4。

扩充上限的可核验理由：F0 外推到2.25年时 F/S0≈1.04894，Black σ=.20，normalized SD=(F/S0)*sqrt(exp(.04*2.25)-1)≈.322；旧总 2^22 路径的理论 SE≈1.57e-4，超过 1e-4，即使正确实现也可能被样本上限卡住。新总2^24理论 SE≈7.86e-5，保留余量但不保证 smile 全部足够。仅增加运行成本，不修改 N 的精度/统计阈值。该长跑不放每次单测/每阶段矩阵。

快测：N1/N2/N6/N7 的小规模确定性、异常、明确 reference 常量；长数值：opt-in S3 harness 产出 N3–N5 每 K/T/carry raw estimate、SE、fit误差、grid误差、step差、oracle层次与 PASS/FAIL/INCOMPLETE/REFERENCE_UNAVAILABLE。core zero-vol component 可用内部合成网格，不新增公共裸 grid 入口。S3 报告及参考源码由独立数值 reviewer 审查；fit、grid、MC 三张接受表必须分开。

## 12. P/B 门槛、平台资源与证据复用

P1–P3 固定与计划相同的三档 (carry terms,grid,steps)：(8,21×13,52)、(64,81×49,208)、(256,161×97,832)。steps 为基础目标；实际并入 knots/细化后列出真实 m/SimDim 和容量，不能伪造刚好命中目标。相同 horizon/产品/worker。P1 记录 constructor、first preparation、repeated prepare、W=1/4 的容量及市场查询计数；不随 path 数准备。P2 vanilla、月度算术亚洲式；cold=构造+首次 valuation，steady=已建 owning 模型后重复 valuation（当前 API 每次仍 preparation，不谎称 prepared handle 跨调用缓存）。构造市场 fit/grid 的费用另列，不与 hot loop 混用。

S5 在现有 core `script_mc_perf` 增加 `--market-iv-local-vol CASE MODE PATHS` 小入口，CASE 为 small/medium/large/flat-pair/monthly-asian，MODE 为 cold/steady/prepare。测试数据固定 F0 扩展的正 F/D/smile；每行输出 identities/实际 size/counters/时间/ledger。实现将 CLI/CTest/README 一并固定，不默认扩全 benchmark 矩阵；该入口尚未存在，以下为 S5 出口命令形式：

```bash
cmake --build build/dal-538-perf --target script_mc_perf --parallel 4
build/dal-538-perf/dal-cpp/benchmarks/script_mc_perf/script_mc_perf --market-iv-local-vol medium prepare 65536
build/dal-538-perf/dal-cpp/benchmarks/script_mc_perf/script_mc_perf --market-iv-local-vol flat-pair steady 65536
build/dal-538-perf/dal-cpp/benchmarks/script_mc_perf/script_mc_perf --market-iv-local-vol monthly-asian cold 65536
```

每个选定 case 两轮各≥10 interleaved process samples/side、best-of-N、校准 workload 保持不变；flat 新/旧同 timeline/normals/normal precision/compiled/worker，steady path增幅≤10%。flat-pair 在同一个 head binary 保留两个独立计时 case，由采样器交替先后顺序；它是新旧入口的补充 paired 成本，不假设历史 binary 已有新 CLI。专项10%不放宽现有 allowlist 4%。新非平坦没有同功能历史 baseline，只报绝对成本；旧 Hybrid/scalar-dividend/local-vol 的真实受影响 caller 用基线/新头独立 binary比较。共享 headers 不自动触发九项目全套；若实际触及 tape/RNG/interp gated 核，按 [性能流程](../../references/benchmark-workflow.md) 定向加入。噪声或缺样本为 inconclusive，不算 PASS；单次诊断不能发 verdict。

### Windows 路线与真实缺口

S0 已静态读取 `.github/workflows/cmake-windows.yml`、`dal-excel/CMakeLists.txt`、`tests/optional-excel/windows-installed.cmake` 和 `dal-excel/tests/windows/run-compounded-ois.ps1/.py`：

- 真实 native 路线已有：windows-latest/MSVC，aadet 及三个 diagnostics/profiling variants，开启 Python+Excel、Machinist、Release-windows build/install、`dal_excel_tests` 的 OPER/XLL CTest、standalone installed Excel tests OFF/ON 及 XLL runtime contract；这些不是 Linux portable。
- 只读 GitHub 查询：规划基线 Windows run [43734637649](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/runs/43734637649) completed/success；当前 master [43746974703](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/actions/runs/43746974703) 查询时 in_progress，未等待，不宣称当前 head PASS。仅是既有资源/路径证据，不验证本功能。
- 本地主机 `uname -s=Linux`；仓库 `actions/runners` 查询 `total_count=0`（只证明无可见 repo self-hosted runner，不证明无人持有 Excel）。现有 CI 未启动 Microsoft Excel/COM worksheet，不能从 XLL 单测推导活 workbook PASS。
- 已有 live Excel runner 能创建 Excel、RegisterXLL、运行公式、检查独立预期并记录实际加载模块/XLL hash、Excel version、source/dependencies/cache。S4 添加新 feature 的 Windows OPER tests；S5 添加 `run-market-iv-local-vol.ps1/.py` 和可复现 recipe，以该模式校验 F0 同源 model/vanilla/DF、日期/horizon/AAD/EXERCISE 拒绝、spill/失败保留旧 handle。
- 实际 worksheet 缺口：尚无本功能 runner，也未确认有可调度的 Windows+installed Excel+pywin32 主机。协调器在 S4/S5 安排执行；若资源仍缺，B4 worksheet=NOT_RUN/BLOCKED，列出所缺 host/software及已有 native 证据，不能宣告全功能验收通过。此缺口不阻挡 S0 合同 PR，也不删除 B4。

S4 可用命令（Windows MSVC shell；对应新 tests/runner 由该阶段交付）：

```powershell
cmake --preset Release-windows -DDAL_BUILD_PUBLIC=ON -DDAL_BUILD_EXCEL=ON
cmake --build build/Release-windows --target dal_excel dal_excel_tests --parallel 4
ctest --test-dir build/Release-windows -C Release --output-on-failure -R "MarketIvLocalVol"
```

S5 live runner 固定参数 `-Xll -PythonEvidence -BuildCache -Output`，与已有 provenance manifest 约定一致；独立 worksheet期望值不得仅复制同进程 Python PV。保留注册、输入、结果/error、XLL实际模块与未保存源 workbook 的清理证据。这组未来命令未在 S0 运行。

证据按不可变 head/依赖 pin/工具链/CMake flags/AAD backend/CPU/worker/数据及 executable hash 绑定。相同 head 或可证明不受后续 docs/binding 影响的相同数值内核可复用 S3；后续阶段重新构建所有受影响 binary，对 core/共享头变化只重验对应 N/P/B。binary 不同不能凭 commit相近跳过 affected case；不把旧 binary 当新 head。保留原始结果、raw命令及选择/排除理由；CI 仍按 exact head gate。S0文档没有新算法时不跑产品测试/benchmarks。

## 13. S1–S5 落点、示例与出口

遵守 [组件边界](../../references/component-boundaries.md)：`dal-cpp <- dal-public <- {dal-python,dal-excel}`，包含测试/helpers/benchmark。当前 core `models/hybrid_script`、所有 products/methods 示例位置、用途、target/CTest/install 名保留；如遇反向依赖，在原地改成 core API。新 facade 示例是另一个程序，不能搬走旧 core 示例。

| 阶段/任务 | 实施落点与独立出口 |
|---|---|
| S1 DAL-540 | 新 owning source/carry/identity/ledger helper，marketivsurface 内部共享数学；N1、N2 component、来源/预算快测。内部设施不提前开放未接好的 public MC |
| S2 DAL-541 | dedicated ModelData/adapter/内部 term-carry component、factory dispatch、script capability/preflight、date/horizon/Clone/inspection；普通 passive Price，N6/N7及旧 Hybrid/GSR 定向回归 |
| S3 DAL-542 | 独立 A/B/C、F1/F2 和测试 harness、冻结数据/checksum/报告，N3–N5 分层 PASS；任何失败反馈受影响 S1/S2，不用新旧自身一致当 oracle |
| S4 DAL-543 | public 新薄入口/白名单、Python新绑定与重建扩展、Excel typed handles/markup/generation/portable+native Windows；B2/B3/B4有关出口，复用合格S3证据 |
| S5 DAL-544 | core/public 分离 runnable 示例、Python编号示例、Excel live recipe/runner、benchmark小入口与P1–P3、安装consumer/文档/CHANGELOG，汇总所有R/N/P/B及真实缺口 |

新 core 示例 `dal-cpp/examples/models/market_iv_local_vol/`，target/CTest/install program=`dal_market_iv_local_vol_example`，只链接 DAL::cpp；新 facade 示例 `dal-public/examples/market_iv_local_vol/`，target/CTest/install program=`market_iv_local_vol_public`，只链接 DAL::public。分别注册 examples 标签，core/public CMake 与目录说明/总 `docs/examples.md` 索引同步；public 当前无独立 README 也不要求为满足空引用创建它。完整 F0 → fit → grid → model → call/put/forward/DF，退出0还须数值断言与身份/误差输出。例子用小规模确定性/固定 draws smoke，正式精度引向 S3。

B1 必须单独 public关闭的 build root，不能拿普通 public-enabled 构建推断独立性：

```bash
cmake --preset=Release-linux -S . -B build/dal-538-core-only \
  -DDAL_BUILD_PUBLIC=OFF -DDAL_BUILD_PYTHON=OFF -DDAL_BUILD_EXCEL=OFF \
  -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DDAL_PUBLIC_BUILD_TESTS=OFF \
  -DDAL_CPP_BUILD_EXAMPLES=ON -DDAL_CPP_BUILD_TESTS=ON \
  -DDAL_CPP_BUILD_BENCHMARKS=OFF
cmake --build build/dal-538-core-only --target dal_cpp_tests dal_market_iv_local_vol_example --parallel 4
ctest --test-dir build/dal-538-core-only --output-on-failure -R 'MarketIvLocalVol|MarketIvSurface|LocalVol|Hybrid.*(LogDf|LocalVol)|dal_market_iv_local_vol_example'
```

检查 includes 和完整 CMake target闭包没有 public；必须实际运行新 core example及受影响旧core示例，不仅 compile。B2另 build root 开 DAL_BUILD_PUBLIC/DAL_PUBLIC_BUILD_EXAMPLES，运行 facade 示例并 stage到隔离前缀，执行 `tests/installed-consumer/run.cmake`、增加新头/工厂消费者；B3重建本次Python扩展并运行新示例/类型/getter/GIL/异常及受影响旧测试；B4 portable+native+worksheet分别记录；B5旧builder/default/archive/legacy general-FD拒绝和native-AAD不变，有生成改动才运行正常generator/check。不可用项保持 NOT_RUN，不转 PASS。

最终 S5 交付依赖：六份PR各自审查/修复/合并；所有适用 R/N/P/B证据含 raw数值/性能、可重现构建和例子运行；缺 Windows worksheet 则仍是明确接受缺口。当前能力说明沉淀至 market-IV/local-vol/Hybrid/public/Python/Excel guides，产品变化写CHANGELOG；完成后按活跃工件规则退役控制，历史证据留Git/PR。

## 14. S0 自检和剩余风险

S0 只允许三个文档路径：本 API note、执行计划、工件 README 索引；无生产/绑定/生成文件改动，无产品构建、测试、generator、数值oracle或性能采样。已执行 `python3 .github/scripts/check_docs.py`（83份 Markdown 通过）、原附件与当前计划 N1–N7 行逐字比较（门槛未改）、样本上限 Black 方差的公式复算及 UTF-8/换行/围栏检查；发布前再执行 staged scope 和 `git diff --cached --check`。这只构成内容/相对引用/patch 静态自检。独立非draft文档PR的CI无需等待来完成交回，不能把文档轻量路径当产品/Windows数值验证。

剩余实际风险是待实现后的精度/容量/成本能否达门槛、独立PDE参考自证和长数值运行、未确认的实际Excel主机；均有明确阶段和失败状态。D1–D6及公共签名、defaults、错误、getter表、示例边界无悬案；不需要再次请求实施授权。本次不合并、不触发S1或其他专家。
