# DAL-542：N5 方案 A 增量数值合同

版本：**A/1**。状态：已批准、已冻结；候选方法、配置和资源估算尚未验证，S3 仍为 in_progress，PR #705 保持 draft。日期：2026-11-16。本合同只替换 S3 受影响的 F2 测试轴、h、独立离散 reference 方法和执行协议；未列出的 [API 决策](../api-notes/dal-538-market-iv-local-vol-mc.md) 与 [计划](../plans/dal-538-market-iv-local-vol-mc.md) 条款继续生效。

## 1. 来源、问题、目标与边界

- 来源：DAL-542，issue `01a1e086-d9b5-798f-931b-7ce8196156b7`；方案附件 `01a1e417-cc5c-7a09-bea5-0a0de765df72`，文件名 `n5-resolution-proposal.md`，SHA-256 `2f9aa3e27149031851325c33e759386788ee67910338dceb1676b7a148dd1942`。决定评论 `01a1e41e-3d3b-70a1-8c5d-857632775224`，用户批准 `01a1e461-4d80-794a-9e15-ec4dfd4782ca`；冻结交接 `01a1e466-7bf5-786b-98f6-57cc17db162a`。附件第 1、3–7 节是本版本的批准来源，第 2 节的研究判断不构成已验证事实，第 8 节的决定已完成。
- 接续：原 [PR #705](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/705)，base master，远端分支 `agent/dal-tester/5d01380c3047`；冻结前 head `d40ff873b661cba94da2c43b0fe7675e1d921793`。保留全部原提交与历史证据，正常快进，不另建 PR 或 force-push。
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

**R10 顺序与次数上限。** 先做 focused reference/runner 支持测试，再冻结执行身份；正式 reference 先完成 8 项 Black F/D 极限（knots/flat × 四个 T，使用 P7 空间/时间模板，覆盖负 r/q 与固定 carry）与原 8 个失败 grid 输入（两族旧 tau_quarter × 四个 T）的新方法研究。后者评估原输入内的方法可行性，不能覆盖旧失败记录；仍未自证即停止为 REFERENCE_UNAVAILABLE。通过后才求两族 P7/P8 × 四 T 的 **16 项必需自证**；P0–P6 × 两族 × 四 T 的 **56 次单层诊断**不承担认证归因，采用中央 2401/2048 基础层。上限 `(8+8+16)*10+56=376` 次新求解，包含 fallback，不能重跑旧 344 次相同方法矩阵来替代定向缺口。必需自证与 grid 价格门禁通过后才启动正式 MC。

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

| 资源 | 整个 A/1 接续的停止线 |
|---|---|
| reference | 新求解最多 376 次；PDE 阶段累计 6 小时，并发最多 4 |
| MC | 每族累计 6 小时；一个 fixture 进程、16 并发 batch，两族顺序执行；不与 PDE 或另一族并跑 |
| 总数值计算 | 累计最多 18 小时，含上述阶段，按前台阶段 elapsed 累加，并行子任务不重复计时；分阶段/重启/接续不得清零 |
| 内存/证据 | 单进程 RSS 6 GiB；留存证据≤1 GiB；同时保持生产 payload/workspace/steps/dimension 准入 |

按既有实测比例估计两族约 4.45/4.49 小时，保守维数约 5.30/5.35 小时/族；这不是实测新配置成本或时间保证。单最细 columns 上界 `8*401*10408=33,388,864 bytes`；八模型之和 `125,208,240 bytes/batch`，16 份约 1.87 GiB，尚未含其他状态/复制。PreparationEnvelope 主项约 160 MiB/模型亦仅估算，必须用实际 Allocate/inspection 及 RSS 检查，不能当已通过生产预算。单 max solve 估算 `6401*4342*12≈3.34e8` RHS 节点步，不等同墙钟成本。

**R15 运行前身份。** 正式数值计算前建立 A/1 新 namespace 的 manifest，固定本合同/API note/plan 的 SHA-256、请求字节与轴算法/配置 hash、两个比较族/所有模型/输出/tiers/停止规则、reservations、源码 commit/实际执行源码 hash、依赖/工具链/build flags/解析模式/可执行 binary hash、机器/worker 设置和已有累计预算。先实施及支持测试，再冻结这些执行身份；禁止依据候选试价选择配置再补登记。capture、reference 和 MC 的后续结果追加其自身 hash，不覆盖已冻结输入。hash 缺失或实际身份不符不得启动正式求解/路径。改变执行身份时只重验直接受影响证据并说明，不能把它当作预算重置或另试配置的机会。

**R16 停止/接续。** 资源触线即前台收齐或终止本次子进程，保存可复现 checkpoint、已用累计时间/求解/路径/证据字节与停止原因，标 INCOMPLETE；停止时已有真实 FAIL 或 REFERENCE_UNAVAILABLE 保留。reference 到唯一 fallback 仍未自证则 REFERENCE_UNAVAILABLE；grid/最终统计带真实超限则 FAIL；SE 到路径上限不足则 INCOMPLETE。新方法不稳定/预算准入失败亦停止，不能提高限额、加档、平滑或按价格选择结果。相同冻结身份/坐标前缀的接续只可消耗原累计预算剩余量，耗尽即交协调器，不自动获新额度。长跑由协调器安排能在前台收齐的窗口，不结束 turn 遗留 run-owned 进程。

## 7. 证据复用、验收与附件逐项对照

**R17 历史与当次视图。** 原 320 行、67,108,864 条路径、344 次 PDE、8 个未自证/76 FAIL、README 说明及全部 raw/fixtures 原样保留。复用 F1 的 160 行；追加新 F2 P7/P8 共 320 行；**当次资格视图=160+320=480 行**，历史 320 行单列，不能把两套视图相加冒称 800 行当次资格。相同生产/依赖/配置/二进制适用时复用 F1 fit/.20 预检、连续 90 位数学、四份实际 snapshot、24 组连续 PDE、F2 fit/quote/carry/payoff 数学、57 项生产回归、生产修复与 4032 个旧插值探针的原范围。

新非均匀 reference 要有 focused RED→GREEN 支持测试（非均匀导数多项式、mandatory knots/strikes、matched-domain、fallback 离散系数保真、缺证据准入）及新 Black 自证；旧 15 项测试只支持未变部分。新 P7/P8 要重新 capture、逐节点高精度 N4 和首段/离节点/边界探针；旧坐标数值不能替代新节点。F2 N5 grid、MC/paired/SE、默认流对齐全部按新身份重验，实际 DF/支付仍检查 2e-12。仅对变化和证据缺口补验，不重跑无关产品全套或 benchmark。

**R18 可执行接受条件。** 以下由后续 tester 实施/运行及 reviewer 核验；本轮只做控制文档静态自检。

| 编号 | 测试/观察与可判定结果 |
|---|---|
| AC1 | 请求生成/解析往返检查 R4/R5 的 canonical 规则、全部 P0–P8 计数/端点/knots；最大 98646，超限/近邻不可二分时拒绝，无 epsilon 合并 |
| AC2 | reference 支持测试验证 D1/D2、多项式导数、强制 knots/strikes 和中央扩域节点逐位一致；原离散系数在 fallback 保真，缺证据不能 PASS |
| AC3 | 分阶段账目最多 376 solves；8 Black+8 旧输入先自证，16 必需研究及 P7/P8 grid 门禁全部通过后方出现正式 MC 记录；各层、自证门槛、requested/actual 与停止传播可重算 |
| AC4 | 协议测试重算 438 anchors、1091/2182/4364/8728、10408 上界、checked addresses、族/块不重叠、默认逐坐标/路径/价格对齐、Brownian 精确嵌套及生产无额外步；不符即停 |
| AC5 | 从 16 块 raw 重算各 tier means/SE/paired-SE、自由度15与 SE-only 升级决定，最终指定 pair/价格与 R2 逐项一致；故意缺 reference/SE/真实超限的支持测试保持真实状态 |
| AC6 | manifest 在正式计算前固定全部输入/执行 hash；累计账跨进程/阶段继续增长，触线停止；零/边界/overflow 支持测试不放宽生产预算 |
| AC7 | 历史 320 行及其 hash 不变；当次 480 行明确引用 F1 的适用身份和新 F2 的320行，不用旧Dmax路径；逐项复用理由、未运行/失败/诊断均可追溯 |
| AC8 | 控制冻结 patch≤4 个授权文档，UTF-8/相对链接/文档检查与 patch 检查通过；无候选执行、生产测试或 benchmark；原提交为新 head 祖先，原 PR/base/分支和 draft 不变 |

正式 S3 仍需全部适用资格、reviewer 复核、doc-writer 判断及既有合并门禁；本合同冻结不是 S3 通过。CI/Codacy 按新 head 的既有事实条件接续，本轮仅一次必要快照，不轮询或重复登记。S4 只能在 S3 正式资格完整并通过全部合并门禁后推进；父 DAL-538 保持 in_progress。

| 批准附件 | A/1 落点及一致性 |
|---|---|
| §1 冲突/不变项 | §1/§2；仅替换 F2 grid/h/reference 配置，全部价格/SE/自证和生产/API/default 约束不变 |
| §3 轴/计数/最终配置 | R4–R6；P0–P8 公式/计数、90位→binary64、严格去重、P7/P8 和诊断边界照录；不再沿用旧删列补预算规则 |
| §4 reference | R7–R10；601 中央、matched 扩域到6401、非均匀三点式、6+4层、requested/actual、8+8+16研究及56诊断一致 |
| §5 抽样 | R11–R13；两族×8模型、438 anchors、四档维数、Dmax/地址/SE-only/最终pair/旧raw不可复用一致 |
| §6 资源 | R14–R16；376/33,554,432/6+6+6小时/18小时/RSS6GiB/证据1GiB一致，明确跨接续累计和触线状态保留 |
| §7 证据 | R3/R15/R17/AC；160复用+320新=480当次视图，新namespace与计算前hash，旧失败/连续reference/生产证据边界一致 |

相对附件仅将“未批准/申请变更”转为已批准 A/1 合同，并展开可判定的层级交叉比较、执行前身份与累计停止验收；没有改变参数、方法选择、门槛或增加预算。资源/精度可行性仍未验证。

## 8. Open Questions

无待用户再次决定事项。新方法能否在 cap 内自证、新 P7/P8 是否满足 grid/MC 与资源门禁是待执行验证项；失败按 R16 交协调器。生产算法/公共默认的任何后续更改须另列决定，不能作为 A/1 的默认补救。
