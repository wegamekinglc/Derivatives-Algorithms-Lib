# DAL-542：N5 方案 A 增量数值合同

版本：**A/2**，唯一现行控制。状态：已批准、已冻结；A/1 已执行部分及失败历史保留，A/2 账目接续和定向资格尚未实施/验证，S3 仍为 in_progress，PR #705 保持 draft。日期：2026-11-17。A/2 仅修订旧输入研究的阶段地位及必要身份/累计账接续，继承 A/1 的算法、配置、全部门槛和资源上限；未列出的 [API 决策](../api-notes/dal-538-market-iv-local-vol-mc.md) 与 [计划](../plans/dal-538-market-iv-local-vol-mc.md) 条款继续生效。

## 1. 来源、问题、目标与边界

- A/1 来源：DAL-542，issue `01a1e086-d9b5-798f-931b-7ce8196156b7`；方案附件 `01a1e417-cc5c-7a09-bea5-0a0de765df72`，文件名 `n5-resolution-proposal.md`，SHA-256 `2f9aa3e27149031851325c33e759386788ee67910338dceb1676b7a148dd1942`。决定评论 `01a1e41e-3d3b-70a1-8c5d-857632775224`，用户批准 `01a1e461-4d80-794a-9e15-ec4dfd4782ca`；冻结交接 `01a1e466-7bf5-786b-98f6-57cc17db162a`。A/1 冻结 head `4b20c54bcd0bf4a5e052d8e13fac7f2ce81527f6`，主控制 SHA-256 `c7408b519cca9f1aa044843137d9bb5447eefa5e9de59f6bd965dba5d6ff57b8`。原附件第 1、3–7 节的未修订参数继续生效；A/1 原文仅作历史，R10 前置条件由下述 A/2 替换。
- A/2 来源：提案附件 `01a1e515-b173-7c87-bfaa-dbaffd023176`，`a1-r10-resolution-proposal.md`，SHA-256 `1fa7693cc8e2322d19b721f70fcfa5aad057858ac7c03cd9a0fa55c2b43cf06d`；tester `01a1e515-dac5-73ff-a45c-be0196e53a8e`，reviewer `01a1e500-1a88-7c2b-a21e-a2b2dd6895fc`，决定说明 `01a1e51d-35d1-7bef-8dc8-374707a34870`，用户批准 `01a1e52a-e892-7771-920e-3a5dc20e6384`，批准范围确认/冻结交接 `01a1e52d-ccb8-7044-bae2-eb9f95c99d58`。现行 R10 使用提案的完整替换文字，AC3 及对应先决条件同步。
- 接续：原 [PR #705](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/705)，base master，远端分支 `agent/dal-tester/5d01380c3047`；A/2 冻结前 head `25f4836ab43672bbf2eecf1ca0f20eb9d8429d93`。保留全部原提交与历史证据，正常快进，不另建 PR 或 force-push。
- 问题：reviewed head `5e5f39f838b5f529a95a7c87d50eccb8ece32511` 的原 320 行为 **184 PASS / 76 FAIL / 60 REFERENCE_UNAVAILABLE**，全部 SE 达标。一年 ATM 已自证 grid 差/S0=`0.001111637541571362`，超过 `2.5e-4`；季度最后 paired 差/S0≈`0.000983889919`，超过其统计带。8 个末级 grid reference 空间检查失败，其中两项季度扩域也失败。既有 Request Changes 继续有效；只增加样本或修 reference 不能消除已成立的价格失败。
- 目标：将获准配置、比较族、准入和停止规则固定为可执行合同，使原 tester 只重验受影响的 F2/reference/runner 证据；明确诊断与正式资格的区别。
- 非目标：不改变 D1–D6、公共签名/default、F0/F1/F2 行情和拟合控制、实际 fitted snapshots、K/F、T、carry/支付语义、生产算法或生产 PDE 准入；不引入 cell-average、Richardson oracle、upwind、density floor、价格平滑或用连续系数替换离散 σ。生产 σ 对 logspot/time 线性插值及平坦边界、左端 σ log-Euler、passive-only 和抽样/tape 前 AAD/风险/EXERCISE 拒绝保持。

本轮冻结仅修改本文件、原 API note/plan 的直接相关 S3 条款和工件索引。后续 tester 实施及数值资格由协调器接续；本轮不实施或运行候选配置，不改 raw/fixtures、生产/测试程序、公共文档或 CHANGELOG。

## 2. 输入、输出与不变门槛

**R1 输入身份。** S0=100，五个 carry days `{91,182,273,365,730}`，F=`{101,99,102,100.5,104}`，D=`{0.995,1.005,0.998,1.01,0.97}`；原 flat r=.03/q=.01 与 F1 不变。F2_linear、F2_nonlinear 继续使用已保存的实际成功 snapshot，不能假定拟合参数等于生成参数。固定 T=`{91/365,1,2,2.25}`，K/F=`{.7,.85,1,1.15,1.3}`；首列前/t=0、负 r/q、未报价 carry knot、首段与尾段均保留。

**R2 门槛。** 下表只是重述现行门槛，不替代 N1–N7 的其他接受条件；ε=SE/S0，ε差=paired-SE/S0。

| 项目 | 每 case 必须满足 |
|---|---|
| N1/N2、实际 DF/固定及延迟支付 | 原相应绝对/归一化误差≤2e-12，日期/能力/预算准入不变 |
| F1 N3 前置 | fit 价格误差/S0≤1e-6，local vol 与 .20 绝对差≤1e-7 |
| F1 N3 统计 | 价格/均值差/S0≤5e-5+4.5ε，且 ε≤1e-4 |
| N4 高精度节点 | 非病态 local variance 差≤2e-10*max(1,abs(reference)) |
| 独立 PDE 自证 | Black 价格差/S0、概率质量/归一化 forward 误差、最后空间/时间/扩域各项差/S0均≤2.5e-5 |
| N5 grid | 两个已自证的离散/连续 reference 差/S0≤2.5e-4 |
| N5 最后 time pair | abs(MC(1/1536)−MC(1/3072))/S0≤2.5e-4+4.5ε差，且 ε差≤5e-5 |
| N5 最终总价 | abs(MC(1/3072)−连续 PDE)/S0≤6e-4+4.5ε，且 ε≤1e-4 |

**R3 输出。** 留存请求/比较族清单、canonical 轴及实际矩阵、逐节点 N4、全部 reference 层级与准入、默认流对齐、坐标 ledger、每 tier 的 16 块 means/SE/paired means、fit/grid/time/统计误差接受表、资源累计账和证据 manifest。每行标明 fixture/P/T/K/output、控制版本、源码/依赖/配置/可执行身份、estimate/SE/oracle/归一化差、实际步数及 PASS/FAIL/INCOMPLETE/REFERENCE_UNAVAILABLE；未运行标 NOT_RUN，不算 PASS。未自证 grid 的价格和 grid/stepping 金额分解只作诊断；不能掩盖另有合格 oracle/SE 支持的真实 FAIL。

## 3. 确定性 grid、canonical 值和诊断范围

**R4 canonical 规则。** 所有轴表达式先用 90 位算术求值，再将最终坐标一次舍入到 binary64；carry 时间、maturity 和端点使用相同有理数来源的 canonical 值。按数值排序，仅去掉完全相同的 binary64 值，不作 epsilon 合并。JSON 往返、capture、reference、MC 必须保留相同坐标值并记录解析模式；不能重复计算近似时间造出一 ULP 间隔。生成后固定请求字节、算法版本和 SHA-256。间距不能支持三次严格二分，或 production preparation 引入非嵌套内步，均停止为 INCOMPLETE，不能移动 anchors 后继续试价。

令 k=log(S/S0)，j 均含两端点；内部表达式保留 90 位精度直到最终坐标舍入。

```text
空间 = {100*exp(log(a)+(log(b)−log(a))*j/(B−1)), j=0..B−1}
     ∪ {100*exp(−.02+.04*j/(C−1)), j=0..C−1}
时间 = {τ+(H−τ)*j/(U−1), j=0..U−1}
     ∪ {τ*((1/32)/τ)^(j/J), j=0..J}
     ∪ {day/365: day∈{91,182,273,365,730}, day/365≤H}
```

**R5 固定阶梯。** 下表去重计数来自获准附件的静态枚举，实施必须核对，不代表本轮已生成或验证候选请求。

| ID | B | C | ns | U | J | τ | a,b | H | nt | ns*nt |
|---|---|---|---|---|---|---|---|---|---|---|
| P0 | 161 | 41 | 202 | 49 | 12 | 1/36500 | .2,4 | 2.25 | 66 | 13332 |
| P1 | 241 | 61 | 302 | 49 | 12 | 1/36500 | .2,4 | 2.25 | 66 | 19932 |
| P2 | 320 | 81 | 401 | 49 | 12 | 1/36500 | .2,4 | 2.25 | 66 | 26466 |
| P3 | 320 | 81 | 401 | 97 | 24 | 1/36500 | .2,4 | 2.25 | 126 | 50526 |
| P4 | 320 | 81 | 401 | 193 | 48 | 1/36500 | .2,4 | 2.25 | 246 | 98646 |
| P5 | 320 | 81 | 401 | 193 | 48 | 1/73000 | .2,4 | 2.25 | 246 | 98646 |
| P6 | 320 | 81 | 401 | 193 | 48 | 1/146000 | .2,4 | 2.25 | 246 | 98646 |
| P7 | 320 | 81 | 401 | 193 | 48 | 1/146000 | .1,6 | 2.25 | 246 | 98646 |
| P8 | 320 | 81 | 401 | 193 | 48 | 1/146000 | .1,6 | 2 | 245 | 98245 |

最大 nt 上界=`U+(J+1)−1+5=246`（共同首列 τ）；P8 的 H=730/365 与 carry 重合再少一列。最大 `401*246=98,646<100,000`，余 1,354 点不用于临时加点。生产 maxGridPoints=100000 与模型 payload 64 MiB/workspace 256 MiB、内部步数/维数各 100000 的准入全部保留；计数不符或超限先作为协议/实现问题处理，不能删非-knot 列凑预算。

P0→P2 仅变空间模板，P2→P4 仅变时间模板，P4→P6 变 τ 及其确定性几何列模板，P6→P7 仅变 spot 域，P7→P8 仅变最后时间。τ 敏感性不能称为仅移动单个节点。几何段固定结束 1/32 年，最终中心 log 步距 .0005；扩域基础远翼 log 步距约 .01284，比旧规则轴粗，长到期/远翼仍可能失败。这是预算取舍，不能宣称必然改善价格。

**R6 最终资格固定为 P7/P8。** 两者都采用扩 spot 域与原基准首列 (1/36500) 的四分之一，即 1/146000；P8 明确测试 T=2.25 的 local-vol time-tail，仍须满足完整 N5，警告不豁免失败。P0–P6 是逐项诊断，不可依据价格选为最终配置；未自证的单层比较统一标诊断 INCOMPLETE。P7/P8 必需 reference 与 grid 门禁全部通过才启动 MC；失败即结束本方案并交协调器，不临时追加 P9、h 档或算法。

## 4. 独立 reference 的非均匀模板与分阶段准入

**R7 保持的数学。** 独立 backward log-spot CN midpoint，payoff 最后时间区间用两半 implicit Euler，保留 `r−q=L'` 与 `−rV`、独立 carry 边界、真实离散 σ 线性后平方、Black/质量/forward 检查。不能调用生产 Hybrid/LocalVolSurface/MC/PDE kernel 作为 oracle。连续 90 位数学及原 24 组连续 PDE oracle 保持旧方法与输入，不用新 solver 覆写历史。

**R8 空间。** 对每个 T，在 [-3,3] 内强制包含全部实际 grid logspot knots、五个 log(K/S0)、0、±3，按 canonical 值去重。新 grid 必需点保守上界 `401+5+1+2=409<601`（0 已在中心轴）。若必需点数为 M，将其余 601−M 个内部节点按 anchor 区间长度比例分配，先 floor，再按小数余数降序、左端点升序补足，区间内等距，使中央初始总节点恰为 601。全部区间二分，中央阶梯 `{601,1201,2401,4801}`。扩域 [-4,4] 保留中央节点，在 [-4,-3] 与 [3,4] 各增 100 个初始区间并同步二分，扩域阶梯 `{801,1601,3201,6401}`；扩域只增加外段，中央节点逐一相同。

令 hm=x_i−x_(i−1)>0、hp=x_(i+1)−x_i>0，独立三对角非均匀三点式为：

```text
D1 = [−hp/(hm*(hm+hp)), (hp−hm)/(hm*hp), hm/(hp*(hm+hp))]
D2 = [2/(hm*(hm+hp)), −2/(hm*hp), 2/(hp*(hm+hp))]
L  = .5*σ²*D2 + (L'−.5*σ²)*D1 − r*I
```

不从生产核查取边界外系数。不改实际 σ、平滑价格或静默换 upwind；极端网格比例/算子不稳定报告 reference 不可用。非均匀 D2 在区间骤变处局部可能一阶，不能预称全局二阶，实际嵌套差决定准入。

**R9 自证层级。** 每项 study 的基础六次求解：中央 601/1201/2401（各 2048 requested steps）、中央 2401（512/1024 requested steps），扩域 3201（2048 requested steps）。最后空间用中央 2401 vs 1201 同 2048；最后时间用中央 2401 的 2048 vs 1024；扩域用中央 2401 vs 扩域 3201 同 2048。任一原门禁未过，只允许一次四求解 fallback：中央 4801/4096、中央 2401/4096、中央 4801/2048、扩域 6401/4096。分别作相同时间的空间差、相同空间的时间差和匹配中央 mesh 的扩域差；Black、质量、forward 同时保留原门槛。fallback 始终保持同一实际离散系数，不准回退到连续 σ。

时间模板沿用独立 solver：anchors 为 0、T 和范围内 grid/carry 时间的并集，每段用 ceil(requested_steps*Δt/T) 个子步，端点保留。最大节点 6401，最大 **requested steps=4096**；并入 grid/carry anchors 后保守 **actual steps≤4096+246=4342**，两半 IE 另多一次线性求解。requested/actual、全部节点、每层 residual/raw values、实际线性求解数分开留存；不能把 4096 宣称为实际总步数，也不能用 actual 的增加偷加 requested 档。病态/不可表示 oracle 逐项报不可用，不删点。

**R10 顺序与累计次数上限。** 先完成定向 reference/runner 支持测试，再冻结本修订的控制、输入、执行身份及与原180事件累计账的接续关系。正式 reference 先完成8项 Black F/D极限（knots/flat × 四个T，P7空间/时间模板）的准入：只复用身份和输入一致、未受修改影响的原中央层，P3影响的原扩域层必须以修正后的外段重验；每项须按R9完成全部自检，基础层不通过仅允许R9的一次四求解fallback，最终未自证即停止为REFERENCE_UNAVAILABLE。随后完成原8个失败grid输入（两族旧tau_quarter × 四个T）的新方法**诊断研究**，逐项保留实际PASS/REFERENCE_UNAVAILABLE及原层级、差和输入身份；后者的数值未自证本身不再阻断新P7/P8，亦不覆盖旧历史或认证原输入。每项仍遵守R9的基础六层和唯一四层fallback，已执行层只能按明示适用性复用。身份/输入/实现或资源异常仍按R15/R16停止，不能作为普通诊断失败略过。

完成上述诊断后，执行原定P0–P8 capture及P7/P8逐节点N4、首段/离节点/边界探针；按原准入通过后，执行两族P7/P8 × 四T的16项必需reference研究。每项均须按R9独立完整自证且满足原grid价格门禁，任一不可用或FAIL即停止。P0–P6 × 两族 × 四T的56次单层诊断仍采用中央2401/2048，不承担认证归因，未自证的诊断不标认证PASS。只有Black、P7/P8完整reference、N4和grid门禁全部通过，才可进行原定小规模生产默认流/逐路径价格对齐及正式MC/paired/SE资格；R11–R13不变。

整个接续最多376次新reference求解，**包含原已完成58次、P3受影响重验和所有fallback**，不重新获得预算。每项待执行求解/四求解fallback先核对同一累计账的剩余容量；不足即INCOMPLETE，不删必需验证、不提高cap或预算。旧344次矩阵不能替代定向缺口，全部R14资源上限及R15/R16身份、停止、前台收齐规则继续生效。

冻结后先完成 R17 的10个 P3 受影响扩域层重验，再按 Black_knots 四 T、Black_flat 四 T 重新判断准入，然后完成 F2_linear 旧输入四 T、F2_nonlinear 旧输入四 T 诊断，T 顺序均为 `{91/365,1,2,2.25}`。其后 capture/N4/探针 → 16项必需研究及 grid → 56次诊断 → 默认流/路径/价格与配对嵌套核对 → 两族正式 MC；不得凭剩余预算越过仍必需的门禁。

累计次数的条件路径为 `58+10+70+160+56=354≤376`，仅当修正域后的8项 Black 仍在基础层通过；10层重验见 R17，70为余7项旧输入最多各10层，160为新16项最多各10层。8项 Black 原来均未用 fallback，若全部触发则额外 `8*4=32`，未截断需求为 **386>376**。354不是无条件最坏上界，376仍是硬上限且包含所有旧用量/重验/诊断/必需研究/fallback；不预支未发生的节省。22次条件余量最多容纳5组完整额外 fallback，实际每次/每组执行前按账检查，不足如实 INCOMPLETE，不删验证凑数或追加额度。

## 5. 比较族、Brownian 坐标与 SE-only 抽样

**R11 固定族。** f=0 为 F2_linear，f=1 为 F2_nonlinear；每族同时驱动 P7、P8 各四档 `h={1/384,1/768,1/1536,1/3072}`，共 8 模型。每 T 沿用原 20 个 passive outputs（五 call、五 put、DF、discounted forward、五 parity、mean、fixed、delayed），每族最终 160 行。同族所有模型、h、tier 共用 fine Brownian 字段；不同族不合并估 SE。

master anchors 为 0、P7/P8 两组 grid times、全部 carry 和四个 T 的并集，固定 **438 anchors、437 段**。每段 n=ceil(384*Δt)，四档严格用 n/2n/4n/8n，保留原端点逐位值；粗 normal 是相同 fine 增量的 sqrt(dt_fine/dt_coarse) 加权和。实际粗步总数必须为 **1091**，四档维数 **1091/2182/4364/8728**；checked 保守上界 `8*(384*2.25+437)=10408<100000`。计数、严格嵌套或模型实际 SimDim 不符须停止并修协议，不能改 reservations 抽样；仍适用 R4 的间距检查。

**R12 固定预留。** seed=1024、MRG32 Precise、每 draw 一个 coordinate、无 Brownian bridge/无 payoff smoothing，16 块 b=0..15；Pmax=2^20，Dmax=8728，p=0..Pmax−1，j=0..8727。地址的每次乘加及整个预留均用 checked uint64：

```text
address(f,b,p,j) = (((f*16+b)*Pmax+p)*8728+j)
每族 slots = 146431541248
两族 slots = 292863082496；最后地址 = 292863082495
按10408保守维数算的两族上界 = 349234528256 < 2^64
```

两个族完整预留不重叠；每次升级保留完整 fine 槽与原路径前缀，不按粗 SimDim 重解同 ordinal。旧 **Dmax=1584 raw 不可复用**为新 Dmax=8728 路径。不能用 Branch 或重复 public 默认 seed 充当独立块；只复用已验证的协议逻辑和数学事实。

**R13 默认对齐与抽样决定。** 新配置先按原小规模 64-path 协议，对默认生产流与标量寻址逐坐标、路径、价格对齐，记录实际模型/坐标/解析模式；跨族/块地址另作精确寻址检查。随后每批只用 `{2^16,2^17,2^18,2^19,2^20}`，仅因任一预登记价格输出 SE 或相邻 h 的 paired SE 不足升级整族，不能根据 oracle 误差、价格改善或 seed 好坏决定扩样。所有预登记价格及相邻配对 SE 都达标时停止，不需跑满上限；保留各 tier。

16 块均值估 SE，自由度 15，4.5SE 是固定统计带。最终判定必须使用 h=1/3072 总价及 1/1536 vs 1/3072 配对，不能选较接近 oracle 的粗档。SE 达标不能抵消价格 FAIL，上限仍不足为 INCOMPLETE；Sobol 不能替代统计门禁。

## 6. 累计预算、身份冻结与停止

**R14 计算/容量边界。** 每族最多 `16*2^20=16,777,216` 条 fine 字段路径，两族最多 **33,554,432**；每条驱动 8 个模型，不将其乘 8 重称独立路径。fine draws 上限 **292,863,082,496**；每条八模型总步数 `2*(1091+2182+4364+8728)=32730`，两族约 `1.098e12` 模型步。不得保存全部路径。

| 资源 | A/1→A/2 同一累计接续的停止线 |
|---|---|
| reference | 累计最多 376 次，含原58次、全部重验/诊断/必需研究/fallback；PDE 阶段累计 6 小时，并发最多 4 |
| MC | 每族累计 6 小时；一个 fixture 进程、16 并发 batch，两族顺序执行；不与 PDE 或另一族并跑 |
| 总数值计算 | 累计最多 18 小时，含上述阶段，按前台阶段 elapsed 累加，并行子任务不重复计时；分阶段/重启/接续不得清零 |
| 内存/证据 | 单进程 RSS 6 GiB；留存证据≤1 GiB；同时保持生产 payload/workspace/steps/dimension 准入 |

按既有实测比例估计两族约 4.45/4.49 小时，保守维数约 5.30/5.35 小时/族；这不是实测新配置成本或时间保证。单最细 columns 上界 `8*401*10408=33,388,864 bytes`；八模型之和 `125,208,240 bytes/batch`，16 份约 1.87 GiB，尚未含其他状态/复制。PreparationEnvelope 主项约 160 MiB/模型亦仅估算，必须用实际 Allocate/inspection 及 RSS 检查，不能当已通过生产预算。单 max solve 估算 `6401*4342*12≈3.34e8` RHS 节点步，不等同墙钟成本。

**R15 运行前身份。** 下一实际执行前冻结 A/2 manifest，固定本合同/API note/plan 的 SHA-256、请求字节与轴算法/配置 hash、两个比较族/所有模型/输出/tiers/停止规则、reservations、源码 commit/实际执行源码 hash、依赖/工具链/build flags/解析模式/可执行 binary hash、机器/worker 设置和已有累计预算；引用本次批准评论、原 manifest/ledger/journal 的 SHA-256 以及逐层复用/待重验映射。先实施及支持测试，再冻结这些执行身份；禁止依据候选试价选择配置再补登记。capture、reference 和 MC 的后续结果追加其自身 hash，不覆盖已冻结输入。hash 缺失或实际身份/输入/层映射/账不符不得启动正式求解/路径。

三种已封存身份分开：实际数值执行 `6ae635f2f0e60622adcb3740c15d2dfacb51afc0` / `a23afd16afef0b73374f648264f03fd269dc90820207141b0da9cfe6e27181ba`；此前发布 `97e0c1a0e5400169736e4155e742902a9ac8fbc0` / `86cd2a05…`；P3发布 `25f4836ab43672bbf2eecf1ca0f20eb9d8429d93` / `8402dd04fc7839bfb2d5eddcc4e504aea77b96e2571f1267b0f7d14f4cf4124b`。发布身份不等同新的数值执行，不可重绑旧 manifest 或声称最终发布 head 已运行全部资格；完整身份/hash 从封存证据核验。

同一累计账迁移必须封存原180事件前缀及hash链；首新事件 **181** 为显式 transition，绑定 old/new 执行身份、批准评论和新控制，引用原 manifest/ledger/journal hashes。原 ledger SHA-256=`3bf459b95be10ba8a13a3d4b22c4f424bfc164cfa5aceb0ae31db103c1fb95d2`，journal SHA-256=`651f549f358dd88afc31cbc7b9641348b4864a90c00f593025c62d35f4ccbecb`（tester P3封存报告）。继承 **58次预留/58次完成、PDE与总数值均20.26435111698811秒、两族MC秒数/预留路径/完成路径均0、active_phase=null**。transition只扩展执行身份，不新增额度；原事件不得改写。拒绝 `create=True`、另建 namespace 清零、冻结新预算或将旧 manifest 直接换绑新源码。本轮仅冻结上述接续合同，不迁移账、不改程序。

**R16 停止/接续。** 资源触线即前台收齐或终止本次子进程，保存可复现 checkpoint、已用累计时间/求解/路径/证据字节与停止原因，标 INCOMPLETE；停止时已有真实 FAIL 或 REFERENCE_UNAVAILABLE 保留。reference 到唯一 fallback 仍未自证则 REFERENCE_UNAVAILABLE，其中仅 R10 的8项旧输入数值未自证为非阻塞诊断；Black与P7/P8必需reference不可用仍停止，身份/输入/实现/资源异常不能被诊断地位豁免。grid/最终统计带真实超限则 FAIL；SE 到路径上限不足则 INCOMPLETE。新方法不稳定/预算准入失败亦停止，不能提高限额、加档、平滑或按价格选择结果。经 R15 显式迁移后的新执行身份同样只可消耗原累计预算剩余量，checkpoint/run/source变化不清零，耗尽即交协调器，不自动获新额度。长跑由协调器安排能在前台收齐的窗口，不结束 turn 遗留 run-owned 进程。

## 7. 证据复用、验收与附件逐项对照

**R17 历史与当次视图。** 原 320 行、67,108,864 条路径、344 次 PDE、8 个未自证/76 FAIL、README 说明及全部 raw/fixtures 原样保留。复用 F1 的 160 行；追加新 F2 P7/P8 共 320 行；**当次资格视图=160+320=480 行**，历史 320 行单列，不能把两套视图相加冒称 800 行当次资格。相同生产/依赖/配置/二进制适用时复用 F1 fit/.20 预检、连续 90 位数学、四份实际 snapshot、24 组连续 PDE、F2 fit/quote/carry/payoff 数学、57 项生产回归、生产修复与 4032 个旧插值探针的原范围。

新非均匀 reference 要有 focused RED→GREEN 支持测试（非均匀导数多项式、mandatory knots/strikes、matched-domain、fallback 离散系数保真、缺证据准入）及新 Black 自证；旧 15 项测试只支持未变部分。新 P7/P8 要重新 capture、逐节点高精度 N4 和首段/离节点/边界探针；旧坐标数值不能替代新节点。F2 N5 grid、MC/paired/SE、默认流对齐全部按新身份重验，实际 DF/支付仍检查 2e-12。仅对变化和证据缺口补验，不重跑无关产品全套或 benchmark。

P3修复后原58层中 **48个中央层逐位不变**，可在 model/carry/T/strike、中央mesh、timeline/actual steps、scheme、依赖/工具链身份一致时复用；若另有输入/solver/依赖变化，先重新核定适用性及预算。需重验的 **10个扩域层** 为8项Black各level2/3201点/2048 requested，加首个 `F2_linear_old_T0` 的level2/3201/2048及level3/6401/4096（9个level2、1个level3）。其中央节点匹配不变，外段舍入已变；修正后的10层均 **NOT_RUN**，原8项Black PASS仅属于旧执行。按原输入/中央mesh/timeline/scheme/依赖身份定向重验，完整保留新mesh/raw/residual与扩域差，组合未变中央层重新判断自证；追加 supersedes 关系，不覆盖旧层、旧status或原hash。

首旧季度同4096 requested/4107 actual的中央2401/4801价格仍为 `4.421132674134743 / 4.432596118708112`，差/S0=`1.146344457336923e-4 > 2.5e-5`，P3没有消除此失败。旧未自证价格不能作oracle或认证金额归因，诊断失败不能重标PASS；亦不能推定未运行的P7/P8已失败或通过。A/2冻结时当次480行仍为 **160复用PASS/320 NOT_RUN**、accepted=false；P7/P8/capture/N4/56诊断/MC尚未运行，旧320行 **184 PASS/76 FAIL/60 REFERENCE_UNAVAILABLE** 不变。

**R18 可执行接受条件。** 以下由后续 tester 实施/运行及 reviewer 核验；本轮只做控制文档静态自检。

| 编号 | 测试/观察与可判定结果 |
|---|---|
| AC1 | 请求生成/解析往返检查 R4/R5 的 canonical 规则、全部 P0–P8 计数/端点/knots；最大 98646，超限/近邻不可二分时拒绝，无 epsilon 合并 |
| AC2 | reference 支持测试验证 D1/D2、多项式导数、强制 knots/strikes 和中央扩域节点逐位一致；原离散系数在 fallback 保真，缺证据不能 PASS |
| AC3 | 同一累计账最多376 solves（含原58及重验/fallback）；8 Black先完整自证，8旧输入必做非阻塞诊断且保留真实状态，再capture/N4/探针、16项必需reference及grid、56次诊断、默认流/路径/价格与配对嵌套，全部必需门禁通过后方有正式MC。354仅Black基础复验通过的条件路径，未截断386不得突破376；每次/每组执行前检查容量，不足INCOMPLETE，各层/停止传播可重算 |
| AC4 | 协议测试重算 438 anchors、1091/2182/4364/8728、10408 上界、checked addresses、族/块不重叠、默认逐坐标/路径/价格对齐、Brownian 精确嵌套及生产无额外步；不符即停 |
| AC5 | 从 16 块 raw 重算各 tier means/SE/paired-SE、自由度15与 SE-only 升级决定，最终指定 pair/价格与 R2 逐项一致；故意缺 reference/SE/真实超限的支持测试保持真实状态 |
| AC6 | manifest在正式计算前固定新控制/实际输入/执行及原manifest/ledger/journal hashes；原180事件前缀/hash链不变，181显式transition绑定old/new身份及批准。58/58、PDE与总20.26435111698811秒、两族MC秒/路径0、active_phase=null继承；拒绝create=True/重绑/新namespace清零，跨进程/阶段/source不重置且容量不足停止；零/边界/overflow支持测试不放宽生产预算 |
| AC7 | 历史 320 行及其 hash 不变；当次 480 行明确引用 F1 的适用身份和新 F2 的320行，不用旧Dmax路径；逐项复用理由、未运行/失败/诊断均可追溯 |
| AC8 | 控制冻结 patch≤4 个授权文档，UTF-8/相对链接/文档检查与 patch 检查通过；无候选执行、生产测试或 benchmark；原提交为新 head 祖先，原 PR/base/分支和 draft 不变 |

正式 S3 仍需全部适用资格、reviewer 复核、doc-writer 判断及既有合并门禁；本合同冻结不是 S3 通过。CI/Codacy 按新 head 的既有事实条件接续，本轮仅一次必要快照，不轮询或重复登记。S4 只能在 S3 正式资格完整并通过全部合并门禁后推进；父 DAL-538 保持 in_progress。

| A/1 批准附件 | A/2 承继及显式替换 |
|---|---|
| §1 冲突/不变项 | §1/§2；仅替换 F2 grid/h/reference 配置，全部价格/SE/自证和生产/API/default 约束不变 |
| §3 轴/计数/最终配置 | R4–R6；P0–P8 公式/计数、90位→binary64、严格去重、P7/P8 和诊断边界照录；不再沿用旧删列补预算规则 |
| §4 reference | R7–R9算法/中央与外段模板/6+4层/cap逐字不变；8+8+16研究及56诊断保留，旧8项的自证先决地位由A/2 R10替换为必做非阻塞诊断 |
| §5 抽样 | R11–R13；两族×8模型、438 anchors、四档维数、Dmax/地址/SE-only/最终pair/旧raw不可复用一致 |
| §6 资源 | R14–R16；376/33,554,432/6+6+6小时/18小时/RSS6GiB/证据1GiB一致，明确跨接续累计和触线状态保留 |
| §7 证据 | R3/R15/R17/AC；160复用+320新=480当次视图与旧失败保留；R15明确跨身份同一累计账，不能以新namespace清零 |

| A/2 批准提案 | 现行对应条款 |
|---|---|
| R10完整替换及AC3同步 | R10前三段逐字采用获准替换文字；AC3及API note/plan同步先决条件，只有旧8项数值未自证不阻断 |
| 固定顺序与48/10层映射 | R10/R17；Black、旧诊断、capture/N4、16研究/grid、56诊断、默认对齐/配对、MC顺序一致；10层修正结果NOT_RUN，旧层追加supersedes而非覆写 |
| 354有条件/386未截断/376硬停止 | R10/R14/AC3；包含旧58、全部重验/诊断/必需/fallback，每次/每组先查同一账容量，不省略必需验证或提高额度 |
| 跨执行身份同一累计账 | R15/R16/AC6；执行/发布分开，旧180事件封存、181显式transition，58/20.26435111698811/0继承，无重绑/清零 |
| 退出与证据边界 | R16/R17；必需reference/N4/grid/MC/paired/SE仍阻塞，旧诊断不可当oracle，历史320及当次480状态不变 |

A/2相对批准提案只将“未批准/建议”转为已批准的现行地位，并明确引用身份与验收映射；不改变算法、P0–P8/grid/h、门槛、随机方案、生产/API/default/PDE准入或资源额度。完整资格及资源可行性仍未验证；A/1来源/hash和真实失败历史继续保留，旧R10不再是现行先决条件。

## 8. Open Questions

无待用户再次决定事项。A/2身份迁移/账目接续、10层定向重验和新P7/P8完整资格仍待实施验证；354条件路径不保证376内完成，容量不足按R16保留INCOMPLETE交协调器。生产算法/公共默认的任何后续更改须另列决定，不能作为A/2的默认补救。
