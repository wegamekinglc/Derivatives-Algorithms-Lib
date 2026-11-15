# DAL-538：同源市场 IV 到非平坦 F/D local-vol MC 开发计划

状态：已接受的六阶段执行计划；S0（DAL-539）正在交付合同冻结文档 PR。日期：2026-11-16（Asia/Shanghai）。新增能力尚未实现或验证。用户评论 `01a1e082-a761-770c-8802-5374db6e203c` 已授权逐阶段实施、每阶段独立任务/PR、自主 review/修复/合并及可运行示例和 benchmark。公共签名、默认值、schema 和补充数值运行规则以 [S0 API 决策](../api-notes/dal-538-market-iv-local-vol-mc.md) 为准；本文 R/N/P/B 的其余数值约束继续生效。S0 只改文档并提交/推送/开非 draft PR，不实施产品、不合并、不触发后续专家。

## 1. 来源、基线与控制文件

- 需求：DAL-538（issue ID `01a1e075-2e65-7d4b-9e2d-973053f4d5ce`），根据周报附件 `01a1d91f-9f4a-75e4-ad41-b3accf247c2a` 的第三个建议事项给出完整开发计划。经认证 CLI 下载的附件名为 `dal-526-weekly-2026-11-14.md`，采用其“3. 非平坦 F/D snapshot 到一致 local-vol MC 模型”（第 131–146 行），不包含前两项 OIS 建议。
- 历史规划授权：评论 `01a1e076-c6c9-78da-b060-1cac567fc05d` 当时仅允许规划；该轮没有实施、提交、推送、PR 或产品验证。当前执行授权为上述用户评论及 DAL-539–DAL-544 的六个阶段任务，每阶段审查/修复/合并后才启动下一阶段。
- 历史规划轮的指定及实际源码 HEAD 均为 `47138fb89949029813ccdf0e4bcdf70d298e3e1e`。当时交接源分支为 `agent/dal-orchestrator/e2c6e5770b10`，专属 checkout 为 `agent/dal-spec-writer/536d5de4b896`，开始时工作树干净。周报分析基线 `49783ad7…` 仅为历史来源；S0 实际 HEAD 见下文。
- 仓库约束：[AGENTS.md](../../../AGENTS.md)、[组件边界](../../references/component-boundaries.md)、[代码规范](../../references/code-style.md)、[测试规范](../../references/unit-test-style.md)、[性能流程](../../references/benchmark-workflow.md)、[共享构建命令](../../../CLAUDE.md)。
- 既有活跃控制以 [.codex/artifacts/README.md](../README.md) 的七份其他文档为准：DAL-526 OIS、DAL-497 三份 PDE 控制、DAL-398 script 合同、DAL-410 XCCY 模型决策及 mixed-mapping basket 控制。S0 同时索引本计划和 API note。已核对其作用域、状态与相关约束；历史完成记录不能充当本功能的新验证证据。

S0 重新核对远端及 checkout：实际源码基线为 `c4f65377650c07f29c8b26852aeb5f8a5ef932b9`，PR base `master`，专属分支 `agent/dal-api-designer/d90a1fab312d`。相比历史计划基线只有 OIS 原生求值 PR #697（23 个文件，734 增/88 删），不改 market-IV、local-vol、Hybrid/factory 或本功能三端入口。上述历史 HEAD 不作为当前 HEAD；后续每阶段另记录不可变 SHA。

DAL-538 与 OIS 观察约定/quote-risk、提前行权 PDE 风险、XCCY calibration/basket 无实现依赖，不修改它们的准入规则。尤其不将确定性 local-vol MC 接线解释成 PDE local-vol、GSR smile 联合校准或 XCCY 支持。未来实现阶段应记录新的不可变源码 SHA，并分析与本文基线的差异。

## 2. 问题、已有能力与本次增量

用户已经能拟合原始 PRICE/DECIMAL_IV 行情、取得 SSVI snapshot、导出解析 Dupire 网格，并单独构建 Hybrid local-vol 模型。缺少的是一个直接从同一 snapshot/grid 创建模型的受控入口，使贴现、远期、dividend carry、估值日和模拟时间轴一致，避免“曲面来自市场 A、carry 来自市场 B”的静默错价。

| 已有能力及源码证据                                                                                                                                                  | 本次新增或必须证明的内容                                                      |
|------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------|
| `dal-cpp/dal/model/surface/marketivsurface.hpp`：owning fit/snapshot、显式 F/D market、Dupire/refit 请求和结果                                                       | 将成功且带来源的 grid 接入 MC；不增加第二个 IV fitter                              |
| `dal-cpp/dal/model/surface/marketivsurface.cpp`：`CarryAt`，log(F/S)、log(D) 插值，carry 结点右导数                                                                   | 复用同一数学合同形成精确的逐步 integrated carry，并共享或集中内部实现                       |
| `dal-cpp/dal/model/surface/marketivdupire.cpp`：精确请求轴、节点状态，结果持有 `Snapshot()` 和 `Request()`                                                                  | 构建时使用该来源，不把普通 `LocalVolSurfaceData_` 当作 provenance                |
| `dal-cpp/dal/model/surface/lvmodel.hpp`：log-spot/time 插值、平坦边界、初始化时预备列                                                                                      | 重用插值及 preparation；新增 carry 不能在每条路径重复查市场曲线                         |
| `dal-cpp/dal/model/hybriddata.hpp`、`dal-cpp/dal/model/hybrid.hpp`：scalar dividend 的 local-vol equity、非平坦 `HybridLogDfRateData_`、`TimelineKnots()`、公共细化时间网格 | 补足 equity term carry，向网格提供 carry knots，限定一个确定性 domestic numeraire |
| `dal-cpp/dal/model/factory.hpp`、`dal-public/src/models.hpp`：现有模型 dispatch 和 BS local-vol convenience factory                                               | 增加带来源/日期/能力边界的 additive model factory，不改旧 builder 语义              |
| `dal-cpp/dal/script/preparation.cpp`：模型提供日期时检查 `InvalidModelEvaluationDate`                                                                                | 新模型必须真正携带日期；普通 LogDf rate 当前没有日期，不能只依赖已有 guard                    |
| `dal-public/src/marketivsurface.hpp`、`dal-python/src/bindings/marketivsurface.cpp`、`dal-excel/src/__marketivsurface.hpp`                                   | 三端投影同一个 core 合同，Python/Excel 不重写数值逻辑                              |

当前 [market-IV guide](../../../docs/models/market-iv-surface.md)、[local-vol guide](../../../docs/models/local-volatility.md)、[Dupire guide](../../../docs/models/dupire.md)、[Hybrid MC 方法](../../../docs/methodology/monte-carlo/simulation.md) 已说明这些能力与边界。旧 `MarketIvSurfaceAsLegacyIVS` 只接受显式 `FLAT_CARRY`；普通 local-vol archive v1 只存数值网格；两点均保持。

附近现有测试为设计证据，未在本轮运行：`dal-cpp/tests/model/test_market_iv_surface.cpp` 的非平坦 carry/右导数、精确 grid/归档及 refit；`dal-cpp/tests/model/test_localvol.cpp` 的内部细分、flat BS path、prepared surface AAD；`dal-cpp/tests/model/test_hybrid.cpp` 的非平坦 LogDf、clone、线程和风险；以及 `dal-public/tests/test_market_iv_surface.cpp`、`dal-python/tests/test_market_iv_surface.py`、`dal-python/tests/test_localvol.py`、`dal-excel/tests/test_market_iv_surface.cpp`。

## 3. 目标、范围和非目标

首版交付：一个资产、一个明确 domestic currency、一个 equity Brownian factor、确定性正 F/D、固定比例 term dividend carry，从已成功拟合的市场 snapshot 导出 grid 后直接进行 passive Price。至少完整支持 vanilla call/put 重定价以及现有普通 script 的单资产未来 PAYS/DF 观察；不宣称所有 script 产品都取得新的数值认证。

支持 `FLAT_CARRY` 与 `FORWARD_DISCOUNT_KNOTS`，允许负利率、D 大于 1、负 dividend yield 和不单调 F/D，只要整个请求计算可表示且满足已有正值约束。接受 `SUCCESS_WITH_WARNINGS` snapshot 时必须传递 underidentification/near-tie 警告，不把拟合成功当作稀疏 smile 的可靠外推承诺。

非目标：重做 SSVI、隐式微分优化器、市场 quote AAD、term-carry 新 AAD 轴、Gamma/曲率、随机利率联合校准、随机 dividend、离散现金股息、多资产、foreign/quanto/XCCY、负 spot/forward、normal vol、自动修复/平滑输入、连续 barrier 数值认证、EXERCISE/LSMC 新准入、Web gateway、持久化完整 calibration session。新入口首版对 exercise 和风险请求明确拒绝；现有其他模型保留自己的能力。

## 4. 编号功能合同

### R1：同源输入与失败准入

权威输入优先为一个 `MarketIvDupireResult_`，其内嵌 snapshot 是唯一 market authority。只接受 `IsSuccess()` 为真、所有所需节点可用的 grid；不接受裸 matrix、裸 `LocalVolSurfaceData_`、调用方自报 identity 或外部替换 r/q/spot/date。snapshot + grid-request 的 convenience 入口必须先调用同一个 `CalibrateMarketIvDupire`，再转入此入口。

如果 API 决策保留 `(snapshot, gridResult, settings)` 双输入形式，必须验证完整 canonical snapshot 内容一致，失败用 `MarketIvSourceMismatch`；同形、同 spot/date/F/D 或只有同 quotes 都不足以证明同源。不得用对象地址比较；snapshot 值拷贝与等内容重建是否视为相同按 D1 冻结。首选单 gridResult 形式从类型上消除此错配入口。

模型来源至少覆盖：evaluation date、spot、carry mode 与全部原始 F/D terms、原始 quote 轴/值/weights、fit/evaluation settings、拟合 rho/eta/theta/slopes/certificate 信息、grid axes/vols/请求、插值及外推版本、currency/index/factor、maxStep、horizon 和预算。建立 versioned canonical 内容身份，不把 provenance hash 当成信任边界；运行时以持有的数据为准，getter 只读。重拟合、改 F/D、改 grid 任一值均产生新来源/模型身份；不能复用旧 prepared model。

### R2：时间、资产与币种

统一时间 `t=(date-evaluationDate)/365`，包含闰日、无自动营业日调整、无全局估值日捕获。新模型须向 script preparation 提供 snapshot 日期；外部 valuation date 不同即在抽样前失败。市场数据当前不带资产名称/币种：settings 必须明确提供规范化 `EQ[...]`、currency、factor，作为调用方对市场币种的声明，不能声称从 snapshot 自动核验其经济含义。模型中所有 payment/discount 观察须符合此声明；未知资产/其他币种按现有 provider guard 拒绝。

不暴露可独立覆盖的初始 spot；从 snapshot 取唯一 spot。如未来提供 expected-date/spot/currency 校验字段，它们只能作断言，不能改值。检查 invalid date、空名称、重复/不合法 index/factor、未知 settings、NaN/Inf、非正或不可表示 F/D/spot、时间乱序和重复，保留现有结构错误语义并补充字段位置。

### R3：F/D 与逐步 carry 的数学合同

定义 `L(t)=log(F(t)/S0)`、`H(t)=log(D(t))`，锚点 `L(0)=H(0)=0`。一般 terms 在各结点间线性插值 L/H，末结点后延用最后段斜率；第一段自原点开始。FLAT_CARRY 直接使用原 snapshot 的 r/q 计算，保留 marker，不将一般结点“识别”为 flat。

```text
r(t) = -H'(t),  b(t) = L'(t),  q(t) = r(t)-b(t)
R[a,b] = -(H(b)-H(a))
Q[a,b] = R[a,b]-(L(b)-L(a))
N(t) = exp(-H(t)) = 1/D(t)
Δlog S = R[a,b]-Q[a,b]-0.5*σloc(a,S_a)^2*Δt
         +σloc(a,S_a)*sqrt(Δt)*Z
       = L(b)-L(a)-0.5*σloc(a,S_a)^2*Δt
         +σloc(a,S_a)*sqrt(Δt)*Z
```

此处区间端点 a/b 与 drift 函数 b(t) 语境不同。实现须用端点差获得积分，不用端点瞬时 r/q 乘整步近似，也不能在已有 numeraire carry 之外再加一次 forward drift。每个确定性离散步条件期望满足 `E[S_b|S_a]=S_a*F(b)/F(a)`；故 `E[S_t]=F(t)`，`E[D(t)S_t]=D(t)F(t)`。q 非零时不得错误要求 `D(t)S_t` 自身为鞅。

复用 `HybridLogDfRateData_` 的 `LOG_LINEAR` 数值合同，输入 `{0}+market term times` 与 `{0}+logD`；禁止新入口选择 CUBIC/MIXED，因为会改变市场 carry。逐步 equity carry 来自同源 L/H；与 market evaluator 的结点值、右侧瞬时导数及末段延伸一致。算术路径可能不同，按 N1 的归一化误差比较而非要求所有数值逐位相同。先检查 log 范围、积分和指数可表示性，不静默截断或输出 NaN。

### R4：公共模拟网格和外推

内部时间网格为 `{0}`、产品未来 observation/payment dates、全部范围内 market carry knots 的有序并集，再按 maxStep 细化；不得只用 quote expiry/theta knots 代替 carry terms。若 local-vol 时间列存在不可跨越的 derivative break，也由明确的 `TimelineKnots()` 提供；具体是否插入全部 grid time knots 由 D3 冻结。产品事件到内步的原映射保持，重复时间去重但不改合约事件顺序。

沿用 log-Euler 左端波动率采样及 `LocalVolSurface_::Prepare`。carry knots 处瞬时导数取右侧；积分连续。新增日期元数据不能意外启用 Hybrid 目前为 GSR 服务的“至少一日/maxStep、只允许整日子步”分支：本功能是确定性连续年分时间，须将日期校验与 dated-GSR grid 限制区分，或在专用模型 wrapper 中处理。1/1000 年 maxStep 应可用，script 自身日期仍按 ACT/365。

建议首版强制显式 `horizon>0`，它覆盖所有 observation、延迟支付与 DF maturity；超出 horizon 在抽样前拒绝。市场最后期限之后可在该 horizon 内沿最后 logF/logD 段延伸，并标记 market-tail 使用；不是将最后 F/D 水平冻结。grid 时间在第一列之前和末列之后、spot 在网格外仍按现有平坦 local-vol 边界处理，分别报告或暴露其配置，不能宣传为市场证书保证。grid 首列必须为正，t=0 沿用首列；首列/末列与 horizon 的覆盖警告见 N5。不存在由网格形状自动推导的可靠定价期限。

### R5：owning、生命周期与归档

模型以不可变 owning state 持有必要 snapshot、grid、market/carry、settings 和来源。可共享 `shared_ptr<const ...>` 的真正不可变底层，不能只保存借用引用。现有 `LocalVolSurfaceData_` 的公开成员可变，不能通过别名把它传入不可变模型：复制数值或持有不可变深层数据，任何 getter 的 mutable view 都必须分离。销毁/修改 fit request、fit result、grid result、Excel 源 handle 后，模型仍独立可用；clone/多线程不共享可变准备区或路径状态。

优先推荐首版新模型为进程内 owning handle，显式拒绝 archive/pickle，与 market snapshot 本身当前不能归档的边界一致。普通 `LocalVolSurfaceData_` v1、`HybridLocalVolEquityData_` v1、`HybridLogDfRateData_` v1 和旧 Hybrid archives 不变。复制 grid v1 的 JSON 再读回不恢复 snapshot/provenance，不能经新入口准入。

若 D4 决定首版必须持久化，则增加独立、版本化的 materialized model/carry schema，写全 date/F/D/grid/settings/来源校验及 capability；不得声称原始 optimizer/证书可由 grid v1 恢复。reader 重新校验数据、拒绝缺失/重复/未知字段、篡改 identity 和不可支持版本，不接受来源字符串替代数值验证。需另加 3–5 人日及 archive fixtures；markup 和 `dal_generate`/`dal_check_generated` 是生成文件的唯一正规路径。实现前先完成该决策，不能交付一半可序列化状态。

### R6：资源、执行和诊断

S0 已冻结 maxStep 默认 `1/52` 年（旧 builder 默认不变）、`maxInternalSteps=100000`、`maxSimulationDimension=100000`、numeric payload 64 MiB、workspace 256 MiB；horizon 必填。grid 仍服从原 `MarketIvDupireSettings_` 的 maxGridPoints 和独立预算。精确 ledger 归属和预检见 API note。

建模时预算计入保留的 snapshot/grid/carry 数组与必要拷贝；Allocate 时用 checked arithmetic 预估结点并集、细分步数、维度、prepared volatility columns、carry/rootDt 和模型 clone 的数值容量，越界在大分配或抽样前失败。共享只读容量与各 worker 私有工作区分别报告；常驻和准备峰值分别计算。该 ledger 不等于 RSS，字符串、语言 wrapper、Excel spill 和 allocator 开销明确除外。0 限额表示不允许工作，不表示无限制。

复用普通 script MC 的线程、RNG、normal precision、bridge 和 compiled/tree 请求，无隐藏 fallback、无新增 Python pricing callback。新请求的 diagnostics 至少提供 source/model identity、date、carry mode、horizon、grid axes/shape、插值/外推策略、requested/resolved maxStep、carry knots/内部步数/SimDim、planned/retained/peak numeric bytes、fit warnings、capability 和已知覆盖警告。现有诊断 schema 若固定版本，新增内容用独立 getter 或新版本，不静默改旧表列。

### R7：AAD 和其他能力边界

首版新模型入口仅提供 passive Price；core `CreateModel<AAD::Number_>` 及 public/Python/Excel `enable_aad=true`、weighted/Jacobian/curvature/segmented 风险入口均须在抽样或 tape 记录前明确拒绝 `DerivativeUnavailable` 或冻结的等价稳定错误，不能返回空风险并标成功。其他入口若接受 `ModelData_`，同样必须检查真实模型能力，不能绕过。

已有 local-vol-grid、spot/scalar-dividend 和 LogDf native AAD 在原 Hybrid 模型中仍有效；它们是固定原生参数的偏导，不含 fit、grid 再生及 carry 约束链。后续可独立增加 `FixedGridNativeAAD`，但须先决定 F/D 相关坐标及 held-fixed 语义。尤其 logDF 改变时 q 是否跟随、F 是否固定必须显式定义，不能复用旧风险标签假装完整来源风险。

已有 `EvaluateMarketIvRefitRisk` 只做曲面输出 full-refit finite bumps；它不自动提供交易 vega。后续交易 quote-refit 要逐方向 refit → 新 grid → 新 model → common-random-number repricing，并报告失败/REGIME_CHANGED；不包含在首版工期。关于 [AAD 方法](../../../docs/methodology/aad.md) 的一般 reverse-mode 能力不能覆盖以上准入边界。

## 5. 接口、模块落点与冻结兼容性

以下为原规划轮的接口草案；S0 已冻结同名工厂及完整类型/getter/错误合同，见 API 决策。以其精确声明替代本节示意签名及“待决”状态。推荐 core 新文件 `dal-cpp/dal/model/marketivlocalvol.hpp/.cpp`，新状态类私有化 owning 数据；在 Hybrid 添加必要的 term-carry equity 数据/执行支持，复用 LocalVolSurface、LogDf、网格细化与 MC 驱动，不复制一套路径引擎。共享 header 变更只扩展 additive dispatch/capability。

```cpp
// 示意签名；settings 含 index/currency/factor、horizon、maxStep 及限额。
Handle_<MarketIvLocalVolModelData_> BuildMarketIvLocalVolModelData(
    const String_& name, const MarketIvDupireResult_& grid,
    const MarketIvLocalVolModelSettings_& settings);

// public facade；返回的派生 handle 可传给现有 passive MC 的 ModelData_ 参数。
Handle_<MarketIvLocalVolModelData_> NewMarketIvLocalVolModelData(
    const String_& name, const MarketIvDupireResult_& grid,
    const MarketIvLocalVolModelSettings_& settings);
```

core 新模型需能保留来源、返回正确 evaluation date、限制 horizon 和 AAD；可用薄 model adapter 委托 `HybridModel_<double>`。adapter 必须转发 clone、Prepare/Allocate、DF 观察与 bridge metadata，不能仅包装数据后又失去 guard。普通 LogDf rate 可原样复用；若改为给 rate 新增日期或在 Hybrid 提升日期协调能力，必须证明旧 archive、subday grid 和 GSR 行为不受影响。内部 helper/组件形式由 S0 选择；不要求整个通用 Hybrid 获得新的序列化版本。

public 新文件建议 `dal-public/src/marketivlocalvol.hpp/.cpp`，只负责构建、别名、typed settings/diagnostics；依赖方向严格为 `dal-cpp <- dal-public <- {dal-python, dal-excel}`。不在 core include public headers，即使测试、benchmark 或 helper 也不例外。

Python 建议：`MarketIvLocalVolModelSettings_`，`MarketIvLocalVolModelData_New(name, grid_result, settings)`，返回可传给现有 `MonteCarlo_ValueWithSettings` 的 owning ModelData。getter 提供 detached `source_identity`、`configuration`、`carry_terms`、`capabilities`；绑定文件建议 `dal-python/src/bindings/marketivlocalvol.cpp`。校验并复制 typed 输入后释放 GIL，原生构建不回调 Python；输入/输出数组生命周期、异常以及并发构造与 C++ 一致。不支持的 pickle 显式失败。

Excel 建议：`MarketIvLocalVolModelSettings_New(name, key_value_table)`、`MarketIvLocalVolModelData_New(name, dupire_result_handle, settings_handle)`、`...Get_Summary`、`...Get_Carry`、`...Get_Execution`，接现有 MC passive 定价函数。复用 typed risk-value handle/输入校验规范；空可选值按默认，未知/重复 key、错误类型、非整数 count、半空矩阵和过期 handle 拒绝。先完成构建再原子替换输出 handle；失败保留旧输出。carry spill 列建议 `ordinal,date,time,forward,discount,log_forward_over_spot,log_discount`，summary/execution 用字段名-值表，具体 schema 在 S0 冻结。注册名、参数顺序、Excel 导出和文档需由正规接口声明/generator 流程保持同步。

| 决策           | 已冻结值与兼容影响                                                                                                            | S0 结论                                      |
|--------------|-----------------------------------------------------------------------------------------------------------------------|----------------------------------------------|
| D1 来源/API 输入 | 单 gridResult 权威；不增双输入 overload。版本化完整 canonical 内容，同内容复制可接受，不依赖指针 | 已关闭 |
| D2 表示与日期 | 专用 owning ModelData + 薄 passive adapter，wrapper 日期 guard，delegate 连续时间；term-carry 复用 Hybrid | 已关闭 |
| D3 时间、范围和默认 | horizon 必填；log-linear market tail；local-vol flat tails；subday、maxStep=1/52；插入全部 carry/grid time knots | 已关闭 |
| D4 归档 | 首版新模型拒绝 archive/pickle，旧 v1 不变 | 已关闭 |
| D5 风险与产品能力 | passive-only，拒绝 AAD/风险与 EXERCISE；未来固定参数偏导/quote-refit 另设计 | 已关闭 |
| D6 导出兼容 | 新命名/schema、独立 Excel settings handle，旧默认/table/legacy/archive 不变 | 已关闭 |

D1–D6 已由 S0 API 决策逐项冻结；上表保留推荐方案及兼容理由，不再是实现前悬案。新增 C++ 类型需要下游重编译；不承诺二进制 ABI、旧环境 pickle 或跨版本 identity 稳定。用户最新要求优先：S0–S5 各自独立任务及 PR，依次合并；每阶段有独立出口，不并发拆成无法验收的层次任务。

## 6. 独立数值 oracle、误差预算与测试矩阵

以下均为未来门禁，本轮未执行。数值门槛须先冻结再测量；失败必须定位 carry、fit、grid、时间离散、抽样或 reference，不得事后放宽门槛/挑 seed。单位均按每单位 notional；价格误差除以 `S0`，F/D/路径用下述归一化标准。统一记 `ε=SE/S0`、`ε差=SE配对差/S0`，所有价格/均值统计带均使用该无量纲误差，不能将货币单位 SE 与归一化价格相加。

### 固定 fixtures 与参考实现

- F0：evaluation `2026-11-16`，S0=100，USD/EQ[A]，carry term days `{91,182,273,365,730}`，F `{101,99,102,100.5,104}`，D `{0.995,1.005,0.998,1.01,0.97}`。273 天仅 carry，不是 quote expiry；覆盖负 r/q、carry 反转、首段及尾段。另设显式 flat r=0.03、q=0.01 退化 fixture。
- F1：在 `{91,182,365,730}` 天、K/F=`{0.7,0.85,1,1.15,1.3}` 生成 flat σ=0.20 的 call/put PRICE/IV（经济 quote 不重复）。使用 S0 API 决策冻结的更严 fit 容差使原始节点和连续 theta 与 σ²t 的误差满足预检；如做不到，不能把该结果直接当精确 Black fixture，应修复输入/拟合配置或将 fit 误差单列。
- F2：同 F/D，SSVI rho=-0.35、eta=0.60、theta(t)=0.04t；独立高精度生成 quotes。另设非线性 smile fixture，theta 的 `(t,value)` 为 `{(0,0),(91/365,0.009),(182/365,0.018),(1,0.04),(2,0.09)}`，采用独立实现的单调 PCHIP 生成行情，分离 theta 与 carry 轴。预期值必须使用实际成功 snapshot 的拟合参数，不能假定 optimizer 必然恢复生成参数。
- 参考 A：测试侧独立 piecewise logF/logD 计算与 `erfc` Black F/D 公式，不调用 `CarryAt`、`EvaluateMarketIvSurface`、生产插值/定价 helper。确定性零波动路径可在内部 component 单测使用合成 grid；不为方便该测试在新公共入口开放裸 grid。
- 参考 B：沿用现有 Python 测试可用的 mpmath，高精度（至少 80 位）重新构造实际 fitted SSVI/theta，独立求导 local variance 和 carry；保存 fixture、脚本/依赖版本、结果校验和。普通 C++ 快速测试读取固定参考数值；生成脚本与产品实现代码不共享导数实现。
- 参考 C：测试专用独立 log-spot backward PDE/forward density solver，分别用连续解析 local vol 和离散 grid 插值作系数，验证 vanilla 与 MC。放在测试支持/数值鉴证目录，不给现有生产 BS PDE 增加 local-vol 准入。参考 solver 不调用生产 Hybrid/LocalVolSurface/MC kernel；独立验证 Black 极限、概率/鞅和加密收敛后才作为 oracle。

建议参考 PDE 初始 domain `log(S/S0)∈[-3,3]`，空间 `{801,1601,3201}`、时间 `{512,1024,2048}`，carry knots 强制命中，使用独立边界公式及 payoff 初始阻尼；最后再将 domain 扩为 `[-4,4]`。若自检未达门槛，只能按预定规则增加一个加密级并记录成本，不能带未自证的 reference 宣告 PASS。

### 抽样规则

正式统计门禁使用 MRG32、Precise normal、无 payoff smoothing 的 passive vanilla；16 个预先固定的互不重叠子流批次。冻结 seed/stream ID 和正常标量坐标预留，避免步数变化导致伪 common paths；不把 deterministic Sobol 路径当 IID。每批路径按 `{2^16,2^17,2^18,2^19,2^20}` 的预定阶梯仅因 SE 不足升级，16 批最多 16,777,216 路径/fixture；S0 的 Black 方差预估表明旧 419 万上限对最长 forward-mean 的 SE 门槛不足，因此只扩充预定样本上限，精度门槛不变。测试侧标量 stream/坐标预留与限制详见 API 决策；到上限仍不够记 INCOMPLETE，不据价格是否接近 oracle 决定增样或换 seed。

SE 使用独立批均值估计并报告自由度；4.5×SE 为本计划的固定统计带，不另宣称无条件覆盖概率。细化比较由测试侧同一细 Brownian 增量聚合到粗网格，计算配对差 SE；不能直接复用不同 SimDim 下同 ordinal 的 RNG 作为 common paths。Sobol 可作为辅助收敛/重复性证据，不替代统计门禁。

| 编号                         | 可执行验收及 oracle                                                                       | 拟议门槛/观察                                                                                           |
|----------------------------|-------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------|
| N1 carry 和日期               | 结点、结点两侧、未报价 carry knot、首段、末段外、负 r/q，对独立 A；检查逐步 R/Q 和复合积分                            | logF/logD、积分绝对误差 ≤2e-12；F/S0、D、N*D 相应归一化误差 ≤2e-12；t=0 锚点正确；日期错配抽样前失败                              |
| N2 零 vol 及 flat 退化         | 内部 zero-vol component 路径应为 F(t)，N=1/D；flat 与旧 BS-local-vol 在相同网格/相同 normals 比较      | S/S0 与 N 误差 ≤2e-12；新增 carry knot 后允许 SimDim 改变，不要求跨不同网格逐位相同                                       |
| N3 Black F/D 闭环            | F1 拟合与 local-vol 预检，再以独立 Black oracle 检查 call/put、远期均值、put-call parity、固定/延迟 PAYS   | fit 诱发价格误差/S0 ≤1e-6，local vol 与0.20绝对差 ≤1e-7；MC价格/均值差除以S0 ≤5e-5+4.5ε，ε≤1e-4；确定性 DF支付相对尺度误差 ≤2e-12 |
| N4 解析 smile 和 reference 自检 | B 验证 interior/knots/tails 的 local variance；C 先验证 Black 极限，再各自时间/空间加密和扩域             | 非病态节点 local variance 误差 ≤2e-10*max(1,abs(reference))；PDE 最后加密/扩域差每项价格/S0 ≤2.5e-5，否则 reference 不可用 |
| N5 smile 的三种误差             | F2 用离散-grid PDE 对连续 PDE；MC h 对 h/2；最终 MC 对连续 PDE，分别汇报                               | grid误差/S0≤2.5e-4；时间细化配对差/S0≤2.5e-4+4.5ε差，ε差≤5e-5；总误差/S0≤6e-4+4.5ε，ε≤1e-4                          |
| N6 owning 和来源              | destroy-input、改 detached getter、重复构建/clone/多线程；refit/改carry/改grid的身份；假来源/裸archive拒绝 | 原模型和价格不变；同 build/config/draws 线程1/4结果遵守现有普通MC的确定性 reduction；非法pair或不支持的输入不能生成可用模型                 |
| N7 预算/异常/能力                | 恰好预算/少1字节、0限额、步数上界、overflow、极端F/D、grid失败节点、horizon越界、AAD/EXERCISE、未知币种资产            | 稳定字段化错误在大分配/抽样/tape前出现，无部分成功模型、无NaN成功、无隐式fallback                                                 |

N5 grid 使用正首列时间 `τ=1/36500` 年，再到 horizon 的规则轴并入必要 knots，spot log 网格初设 `[0.2S0,4S0]`。按 `(81×约49,161×约97,321×约193)` 三档，分别加密空间、时间列和 `τ,τ/2,τ/4`；每次先验证实际并集总点数≤100000。spot 另扩域至 `[0.1S0,6S0]` 作边界敏感性检查。h 阶梯 `1/48,1/96,1/192` 年；先隔离 grid，再隔离 stepping，不同时改变所有配置掩盖误差。覆盖 T=91/365、1、2，以及 horizon 允许的 2.25 年尾段；尾段使用另列结果，不由拟合区结果替代。

N3/N5 的完整长路径资格检查不放进每次快速 unit test；保留小规模确定性路径/异常测试供常规 CI，正式数值运行作为独立验收任务，记录每个 K/T/carry fixture 的 raw estimate、SE、参考值、误差分量、配置和 PASS/FAIL/INCOMPLETE。高精度源码、参考 solver 与数据表由 tester 独立审查；产品开发者不能只以新旧自身结果一致替代 oracle。

## 7. 分阶段任务、依赖、负责人和工作量

协调器已建立 DAL-539–DAL-544 六个串行任务，对应下表 S0–S5；S0 已派给 dal-api-designer，后续阶段暂存 backlog，由协调器在前序独立 review/必要修复/合并后启动。每阶段独立分支及 PR，base master，title 含该子 issue key，body 只 Closes 该子 issue，不关闭父 DAL-538。1 人日按专注工程工作估算；包含定向修复与一次 review，环境等待另计。

| 阶段                      | 任务与交付                                                                                                                   | 依赖               | 拟议负责人                                                        | 人日  | 出口门禁                                       |
|-------------------------|-------------------------------------------------------------------------------------------------------------------------|------------------|--------------------------------------------------------------|-----|--------------------------------------------|
| S0 合同冻结                 | 接受本计划；D1–D6 决策；冻结类型/错误/表schema、日期/时间策略、fixtures/阈值/预算；核对最新基线与旧接口清单                                                      | 用户接受及执行授权（已具备）  | dal-api-designer + dal-orchestrator，模型实现/数值角色参与              | 2–3 | API note/接受记录完整、D1–D6 关闭，独立文档 PR     |
| S1 来源与 integrated carry | owning source、identity、grid准入、term-carry数据及组件、LogDf LOG_LINEAR复用、预算预检、确定性快测                                             | S0               | dal-implementer；dal-reviewer检查数值合同                           | 3–4 | R1–R3/R5基本行为、N1/N2、来源与生命周期快测通过             |
| S2 MC 模型接线              | 专用ModelData/adapter、CreateModel dispatch、date/horizon/capability、TimelineKnots/Allocate、prepared columns/carry、clone及线程 | S1               | dal-implementer；dal-tester协作                                 | 3–4 | R4/R6/R7、N6/N7、普通Price接入和旧Hybrid定向回归通过     |
| S3 独立数值鉴证               | B/C参考实现自证；F1/F2数据；N3–N5分项误差/统计报告；失败反馈S1/S2                                                                              | S2 合并后启动 | dal-tester独立产出，dal-reviewer审查                                | 4–6 | 每个case完整PASS，reference自证合格，无未说明的INCOMPLETE |
| S4 三端投影                 | public薄facade、Python绑定/GIL/getter、Excel typed settings/handles/diagnostics、生成注册、跨端测试                                    | S3 合并后启动 | dal-implementer负责，dal-api-designer复核                         | 3–5 | B2/B3/B4通过，跨端使用同一模型来源/设置                   |
| S5 性能/文档/交付             | 定向性能及预算、core/public分离示例、Python/Excelrecipe、安装/关闭public构建、最终review                                                       | S3、S4            | dal-performancer、dal-doc-writer、dal-reviewer，由orchestrator安排 | 3–4 | P1–P3、B1–B5及文档/接受清单完成                      |

总计保留原估算 18–26 人日，推荐另留 20% 数值/集成风险余量，预算约 22–32 人日。实际按六阶段串行交付，不提前并行派工；原 3–5 周日历估计须结合每阶段 review/合并、扩充后的长数值采样成本及 Windows 资源更新，不构成日期承诺。若要求新 archive schema，额外 3–5 人日；若增加新模型 AAD 或交易 quote risk，必须另做估算，不能计入此首版。

执行关键链为 S0 → S1 → S2 → S3 → S4 → S5，均独立 PR、审查并合并；不提前派工。修复后只重复受影响的 oracle/接口/性能项；共享 Hybrid/factory 改动需分析直接调用者，再扩大回归范围。无需等待 OIS、PDE、XCCY 工作完成；S0 不额外创建任务或触发负责人。

## 8. 定向性能与资源验收

按 [benchmark-workflow](../../references/benchmark-workflow.md) 保留 calibrated sampling、独立 build root、不可变 base/head SHA、同依赖/编译器/native AAD/flags/CPU/线程、build-tree 可执行文件 hash。禁止使用安装目录中的陈旧二进制。普通 correctness 合格后才测，单次运行只诊断。主性能证据先固定下列小集合，不跑整个参数矩阵。

| 门禁           | 变更路径 → 选定场景                                                                                                                                                   | 接受/证据口径                                                                                                                                       |
|--------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------|
| P1 准备与容量     | marketivlocalvol/Hybrid Allocate/Prepare → `(carry terms,grid,steps)` 三档 `(8,21×13,52)`、`(64,81×49,208)`、`(256,161×97,832)`，固定horizon/产品和worker；计数先按knots并集解析 | 记录wall time、数值容量、准备次数；每path市场曲线查询次数=0，准备次数与path数无关；prepared列内存随spots×steps线性，实际容量≤planned，不能保留所有path                                          |
| P2 实际调用与既有路径 | 新模型vanilla及月度观察亚洲式，construction+first valuation和steady valuation分开；另比较flat新入口与基线旧flat-local-vol、并复测实际受影响旧Hybrid callers                                       | 新非平坦功能没有同功能历史binary，只报新能力成本；flat等网格/等draws用补充paired检查，两轮各≥10样本、best-of-N，新旧可比steady path耗时增幅预算≤10%，超出需优化或事前API成本决策，不能称为官方九项regression verdict |
| P3 共享头影响     | `hybrid.hpp`/`factory.hpp`或localvol准备变更 → 精选`script_mc_perf`对应hybrid/local_vol情形及旧scalar-dividend普通Price；如果改动影响RNG/tape/interp通用核才补相应allowlist目标              | `script_mc_perf`为informational/专项证据，不加入官方allowlist。若触及已入gate目标，严格两轮×10、4%既定阈值；不因头文件共享而自动跑九项目或完整benchmark矩阵                                    |

P2 的亚洲式仅验证现有普通 script 调用成本与路径稳定性，不构成新增 exotic 数值认证。新增场景10%专项开销预算已由S0冻结，不放宽既有4%门槛。噪声控制、DAL_NUM_THREADS、冷热态和normal precision保持同值，环境不稳定判 inconclusive。记录新增数据准备/来源校验成本，不能只计path hot loop后宣称整个市场到交易流程提速。若共享代码无需改动某核，明确写“未受影响、未重新测量”，不列作新PASS。

## 9. 示例、构建、文档和交付门禁

新增 core 示例建议 `dal-cpp/examples/models/market_iv_local_vol/`，target `dal_market_iv_local_vol_example`；只含 core 头、链接 `DAL::cpp`，展示非平坦F/D → fit → snapshot → grid → model → vanilla/forward/DF检查。新 public 示例单独放 `dal-public/examples/market_iv_local_vol/`，target `market_iv_local_vol_public`，链接 `DAL::public`。不移动既有core示例，不在core示例中条件打开public，不通过helper/alias/传递链接建立反向依赖。更新两端示例目录、CTest注册和安装目标清单。

Python 新示例在 `dal-python/examples/` 选择未占用编号，明确valuation日期及失败状态；Excel recipe用typed handles与可复现输入表展示同一workflow、summary/carry/error及归档拒绝，不依赖人工看价格判断通过。

| 编号           | 未来门禁                                                                                                     | 可执行接受条件                                                                                                  |
|--------------|----------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------|
| B1 core独立构建  | 检查includes、target依赖闭包；关闭public/语言bindings/portable Excel，在独立build root构建受影响core targets及示例               | `DAL_BUILD_PUBLIC=OFF` 下配置/构建/运行core示例和受影响测试通过，无`dal_public`/`DAL::public`直接或传递依赖                        |
| B2 public与安装 | public开启、独立运行public示例；安装到隔离stage，验证core/public消费者                                                        | CTest注册、targets、头文件和导出依赖正确；public示例退出0且输出必需身份/误差字段；不误用core目录                                             |
| B3 Python    | 重建本轮扩展，运行新test_market_iv_local_vol及受影响market_iv/localvol测试，执行示例                                          | 日期/来源/异常/所有权/getter副本/GIL并发/PV parity/AAD拒绝均通过；解释器与扩展SHA一致                                               |
| B4 Excel     | Linux portable contract tests + Windows native XLL/导出和实际worksheet smoke                                  | 类型/空值/重复key/表列/错误/atomic handle/归档拒绝通过；portable成功不能替代Windows；实际MC调用接受新ModelData                          |
| B5 兼容及发布     | 检查旧builder/legacy bridge/archive和newcapability；生成变更时运行dal_generate与dal_check_generated；现行仓库build/CTest门禁 | 旧flat defaults及general-F/D legacy拒绝不变，旧archive fixtures可读，旧native AAD语义不变；无未解释生成diff/失败，必要平台CI结果由实施交付方记录 |

B1 命令（未来运行；新 target 名已在 S0 冻结，实际运行示例也见 API note）：

```bash
cmake --preset=Release-linux -S . -B build/dal-538-core-only \
  -DDAL_BUILD_PUBLIC=OFF -DDAL_BUILD_PYTHON=OFF -DDAL_BUILD_EXCEL=OFF \
  -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DDAL_PUBLIC_BUILD_TESTS=OFF \
  -DDAL_CPP_BUILD_EXAMPLES=ON -DDAL_CPP_BUILD_TESTS=ON \
  -DDAL_CPP_BUILD_BENCHMARKS=OFF
cmake --build build/dal-538-core-only --target dal_cpp_tests dal_market_iv_local_vol_example -j 4
ctest --test-dir build/dal-538-core-only --output-on-failure \
  -R 'MarketIvLocalVol|MarketIvSurface|LocalVol|Hybrid.*(LogDf|LocalVol)'
```

实施阶段补齐CMake对新core示例的CTest注册，再通过CTest或实际build-tree binary运行。B2 public示例使用另一个build root、`DAL_BUILD_PUBLIC=ON`、`DAL_PUBLIC_BUILD_EXAMPLES=ON`，不得用B1构建结果冒充。正常构建/完整接受命令仍以`CLAUDE.md`、测试规范及实施时的平台为准；正式实施保留必要全套build/CTest gate，不把本轮的“只做计划检查”延续成豁免。

文档交付：更新 `docs/models/market-iv-surface.md` 的model boundary、`docs/models/local-volatility.md` 的term carry/owning/档案限制、`docs/models/hybrid-model.md` 的时间轴/能力、`docs/public-api.md`、`docs/python/README.md`、`docs/excel/README.md` 与示例目录；`CHANGELOG.md`记录用户可见新增与限制。解释F/D单位、声明币种、spot/time网格方向、右导数/积分、尾段与局部平坦边界、Date guard、passive-only、fit证书与离散重定价误差区别；提供从失败状态到诊断的可执行recipe。

完成接受时必须同时具备：R1–R7、N1–N7、P1–P3按适用范围的完整结论、B1–B5、独立review、文档/示例、源码与依赖/二进制身份、逐项未运行/不适用理由。失败、不可用reference、Windows缺失和性能inconclusive不能计作PASS。实施、提交、推送、独立 PR 及后续自主 review/修复/合并已获授权。S0 交付设计 PR 后置 DAL-539 为 in_review，交回协调器安排独立 reviewer；本轮不合并或启动 S1。

## 10. 当前阻塞、风险及本轮检查记录

当前没有阻挡编写与交付本计划的源码/资料缺失；附件、指定HEAD、相关源码和活跃控制均可读取。用户已授予实施授权；S0 的角色/写入范围仍限定为文档。D1–D6 已冻结，不再阻挡接口实现；产品验证继续由后续阶段完成。

真实的后续依赖：独立连续/离散local-vol reference solver尚需开发和自证；正式统计与性能证据尚无；S0 已调查 Windows 路线：现有 cmake-windows.yml 覆盖真实 XLL/OPER/安装消费者；本地为 Linux，仓库 self-hosted runner 查询为 0；新功能 worksheet runner 尚需在 S4/S5 添加并在实际 Excel 主机运行。见 API 决策的资源证据与缺口，不能以 portable 代替 Windows PASS。技术风险主要为日期元数据触发GSR整日限制、F/D与dividend符号/重复carry、mutable grid别名、prepared cache跨来源复用、term knots被quote轴遗漏、local-vol边界和首列造成偏差，以及将native AAD误标market vega。均已有对应R/N/B门禁，不以额外功能掩盖。

本轮允许且执行的自检范围：对照周报第三项与当前源文件；检查本文引用的现存本地路径、计划与control边界、编号/依赖/估算一致性、Markdown/结束换行/空白、工作树变化范围。本轮未执行所有产品测试/构建、数值oracle、性能采样或生成器；上述N/P/B项全部是未来验证门禁，本文不提供已通过的产品验证结论。原规划轮及本次 S0 都未运行产品验证；S0 的内容/引用/patch 静态检查和设计 PR 不改变这一事实。计划在本issue接受后控制后续工作，完成后将当前能力沉淀至docs/CHANGELOG，历史证据进入Git/PR，再按活跃工件规则退役本计划。
