# DAL-379：一维提前行权 PDE 接口与数值诊断契约

状态：active API decision，供数值风险审查和后续实现使用；尚未实现或完成数值准入。

2026-10-18 B1 定向修订：默认 fully implicit，保留全层区域及事前容量上界，
以第 5 节的完整预算公式核对常用请求和加密路径；等待 critic 仅针对 B1 复核。
原始 [numerical critique](../critiques/dal-379-early-exercise-pde.md) 保留，不替换其 verdict。

基线：`f721478ccb8284a48ea144eae828c37e43dc9142`，工作分支
`agent/dal-orchestrator/9f36be3b414c`。需求沿用 DAL-379 原报告及复评中的 P3，
以及 2026-10-18 用户“先完成 P3”和本轮设计委托；不再建立重复 spec。

## 1. 受众、范围与决策摘要

受众是使用公共 C++、Python、Excel 的 vanilla 期权定价用户，以及实现者、
数值审查者和独立验证者。首版对单个 call 或 put 返回一个 `double` 价格，
支持 Bermudan 精确日期投影、American 障碍求解和 owning 诊断。

模型是风险中性 BS：常数、非负的年化波动率，正 spot/strike，以及按日历日期
分段常数的确定性连续复利 rate/dividend yield。单段即现有 flat BS 用例。
没有离散现金股息、结算延迟、随机利率、局部波动率、脚本 payoff、多标的或 Greeks。
American 从估值日立即可行权；延迟开始的 American 留后续。

采用独立 `EarlyExercisePde*` 类型族，不给 `EuropeanPdeRisk*` 添加行权开关。
价格求解不录制或清理 AAD tape。Bermudan 使用事件投影；American 使用隐式
penalty 迭代，并对未加 penalty 的离散 LCP 重新计算残差。
行权边界以网格上的接触区域及相邻节点括区表示，不假定只有一个自由边界。
默认网格和残差阈值是可复现的数值设置，不是连续问题价格误差保证。
默认时间格式为一阶 fully implicit；CN 是调用者显式选择的格式，仍受单调性细分及
完整预算预检。保留全部可行权层的区域输出，不按实际稀疏性绕过容量上界。

## 2. 当前表面与组件归属

已核对的基线证据：

- [`dal-public/src/europeanpderisk.hpp`](../../../dal-public/src/europeanpderisk.hpp)
  提供 owning call/put 价格、三轴 AAD Jacobian、固定网格和线性求解诊断；
  `EuropeanPdeSettings_` 是现有 AAD settings 的别名。
- [`dal-python/src/bindings/europeanpderisk.cpp`](../../../dal-python/src/bindings/europeanpderisk.cpp)
  使用只读配置、拷贝 getter、严格数值输入及释放 GIL 的执行入口。
- [`dal-excel/src/__europeanpderisk.cpp`](../../../dal-excel/src/__europeanpderisk.cpp)
  使用 settings/request/result 句柄、键值配置和带表头的明细 getter。
- [`dal-cpp/dal/math/pde/thetascheme.hpp`](../../../dal-cpp/dal/math/pde/thetascheme.hpp)
  以及 sampled theta step 只有 European 线性 rollback，没有生产障碍求解器。
- [`dal-cpp/test-support/bermudan_pde.hpp`](../../../dal-cpp/test-support/bermudan_pde.hpp)
  是 put 的测试参照；不迁移为生产实现，也不改成调用待验证的新求解器。
- [`docs/methodology/pde/framework.md`](../../../docs/methodology/pde/framework.md)
  和 [`option-pricing.md`](../../../docs/methodology/pde/option-pricing.md)
  是现行 theta、坐标、边界及 test-only 提前行权词汇的来源。

拟新增 core `dal-cpp/dal/math/pde/earlyexercisepde.hpp`，在 `Dal::PDE` 拥有
请求、结果和 `EvaluateEarlyExercisePde`。公共
`dal-public/src/earlyexercisepde.hpp` 在 `Dal` 以 `using` 暴露相同类型和函数；
不复制算法或维护第二份默认值。Python/Excel 依赖这个 facade。
core 仍可在 `DAL_BUILD_PUBLIC=OFF` 下独立使用。

新枚举采用 Machinist markup 和生成文件，Python/Excel 使用同一组规范字符串。
本记录列出的全大写符号是枚举值，不建议手写 `enum class`。

## 3. C++ 请求、单位与 owning 生命周期

公共执行签名仅有一个必需参数：

```cpp
[[nodiscard]] EarlyExercisePdeResult_
EvaluateEarlyExercisePde(const EarlyExercisePdeRequest_& request);
```

输入是可复制的值配置；执行入口先验证并冻结快照。结构如下，所有标为必需的字段
必须由调用者赋值，不能从全局估值日、fixing、curve handle 或默认市场补齐。
未赋值的日期/spot/strike 等保持无效哨兵并在验证中报错。

| 类型                              | 字段及约束                                                                                                      |
|---------------------------------|------------------------------------------------------------------------------------------------------------|
| `EarlyExercisePdeCarrySegment_` | 必需 `endDate_`, `rate_`, `dividendYield_`                                                                   |
| `EarlyExercisePdeMarket_`       | 必需 `spot_ > 0`, `volatility_ >= 0`, 非空 `carry_`                                                            |
| `EarlyExercisePdeContract_`     | 必需 `evaluationDate_`, `maturityDate_`, `strike_ > 0`, `optionType_`, `exerciseStyle_`；`exerciseDates_` 默认空 |
| `EarlyExercisePdeRequest_`      | 必需 `contract_`, `market_`；`settings_` 默认如下节                                                                |

`optionType_` 复用 `OptionType_`，只接受 `CALL`/`PUT`，拒绝该类型的其他取值。
新增 `EarlyExerciseStyle_` 仅有 `BERMUDAN`/`AMERICAN`。每次请求只定价一个 payoff，
不强制另一层网格和另一组非线性迭代。

spot、strike、网格坐标、边界和价格使用同一标的报价单位，价格是每一单位标的的
期权价值；没有隐含 notional、币种转换或 bp 缩放。rate/yield 为连续复利的
年化小数，允许负值；volatility 为年化小数，`0.20` 表示 20%。所有实数必须有限。
`volatility_ == 0` 使用同一单调离散路径，不能临时加一个正波动率。

`EarlyExercisePdeResult_` 是独立值对象，持有已解析请求、空间/时间网格、估值层、
逐步诊断、行权区域、执行计数和失败信息。其成员私有，纯 getter 返回只读引用或值；
没有 setter、借用输入指针、延迟求值或 tape 节点。销毁/修改原请求、释放 Python
对象或 Excel 上游句柄后，结果及其请求 getter 仍有效。普通 C++ 只读引用随所属结果
寿命结束而失效；需要更久时拷贝。Python 的嵌套对象和容器 getter 返回拷贝；
Excel getter 返回 detached handle 或复制单元格。

执行不读取或修改全局日期和 AAD 模式，允许调用者已有非空 tape。
Python 先在持有 GIL 时完成类型检查和输入拷贝，再释放 GIL 计算；执行不回调 Python。
并发请求各自拥有 scratch，不共享可变缓存。Excel 求值不依赖调用先后顺序。

## 4. 日期、carry 和精确事件投影

只接受有效 `Date_`，且 `maturityDate_ > evaluationDate_`。首版不处理已到期合约。
年数一律为实际日历天数除以 365，闰日计入天数；不做营业日调整或时区转换。
时间 `t=0` 对应估值日，`T=(maturity-evaluation)/365`。

carry 段终点严格递增，位于 `(evaluation, maturity]`，最后一个必须等于 maturity；
首段从 evaluation 开始，以后各段从上段终点开始。段内使用该段 rate/yield，
跨段贴现和 forward 分别由积分 `exp(-∫r)`、`S*exp(∫(r-q))` 决定。
不排序、不去重、不外推缺失的尾段。共同端点处系数取哪一侧不改变积分，rollback
每个开区间只用其所属段；事件日期上的 payoff 不取另一套 carry。

对 Bermudan：

- `exerciseDates_` 必须非空、严格递增，全部位于 `[evaluation, maturity]`，且末项
  必须是 maturity。拒绝重复、倒序、越界或省略到期日，不静默修正。
- evaluation 仅在明确列入日程时投影；不得把所有 Bermudan 都当作今日可行权。
- maturity 初始化 payoff。回滚到每个较早的行权日后执行一次 `max(continuation, payoff)`。
  只在这些精确事件做投影，时间细分不能创造额外行权机会。
- 单个 maturity 的日程是 European 退化情形，不代表调用旧 European AAD API。

对 American：`exerciseDates_` 必须为空，整个 `[evaluation, maturity]` 可行权。
终值是 payoff，其余每个实际目标时间层都解离散 LCP，包含 evaluation。
这是对连续行权问题的时间离散，不将输出时间层称为精确的连续行权日期。

时间锚点是 evaluation、maturity、carry 终点及 Bermudan 行权日的并集。
相同日期合并成一个锚点，保留多个事件标签；carry 跳点本身不创造行权机会。
锚点由日期身份映射至时间层，不用浮点近似相等判断命中。
锚点区间按自身端点生成子层并将末端直接赋为锚点，不靠累计 `dt` 漂移到日期。
中间时间层可能在日内，无需也不能转换为四舍五入后的 `Date_`。

结果 `timeGrid_` 按从 evaluation 到 maturity 递增保存实际时间，配套可空
`calendarDate_` 和 `isExerciseDate_`/`isCarryKnot_`/`isMaturity_` 标记。
只有真正的日历锚点有日期；步长为相邻实际时间之差。所有节点索引从 0 开始。

## 5. 默认设置、网格与有界执行

`EarlyExercisePdeSettings_` 是独立配置。下面的 snake_case 同时是 Python 参数和
Excel 配置键，C++ 按 DAL 规则使用相应 camelCase 加尾下划线成员。

| 配置键                            | 默认值         | 约束与语义                                 |
|--------------------------------|-------------|---------------------------------------|
| `grid_points`                  | `401`       | 总节点数，含两个端点；整数 `5..100001`             |
| `upper`                        | 空           | 解析为 `4*max(spot,strike)`；显式值须有限且大于两者  |
| `ordinary_steps`               | `500`       | 正整数，按整个期限分配的基础步数，非实际求解步数              |
| `theta`                        | `1.0`       | 默认 fully implicit；显式选择 `0.5` 为 CN     |
| `rannacher_intervals`          | `2`         | 非负整数，不超过 `ordinary_steps`；见下文         |
| `penalty_factor`               | `1e8`       | 有限的无量纲数，`1..1e12`；American 使用         |
| `absolute_tolerance`           | `1e-8`      | 有限且严格正；价格单位                           |
| `relative_tolerance`           | `1e-10`     | 有限且在 `(0,1]`                          |
| `linear_backward_error_limit`  | `1e-12`     | 有限且在 `(0,1]`，无量纲                      |
| `max_iterations_per_step`      | `100`       | 正整数；每个 American 实际子步的线性化求解上限          |
| `max_actual_steps`             | `100000`    | 正整数，事件拆分、阻尼和稳定性细分均计入                  |
| `max_work_units`               | `100000000` | 非负整数；每次线性求解尝试计 `grid_points-2`，包括失败尝试 |
| `numeric_payload_budget_bytes` | `67108864`  | 非负整数；64 MiB 的原生结果数值 payload 上限        |
| `workspace_budget_bytes`       | `268435456` | 非负整数；256 MiB 的原生临时数值工作区上限             |

节点数、基础/实际步数、阻尼数量和逐步迭代上限须可表示为 `int`；预算与 work units
须可表示为 `size_t`。不得先窄化转换再检查。已解析请求中的 `upper` 必须填入实际数值，
其余请求设置保留；有效步数、theta 和 stencil 的实际变化记录在 execution/逐步诊断。

空间首版固定为 `[0,upper]` 的均匀物理 spot 网格，`S_i=upper*i/(N-1)`。
端点精确赋值，要求所有节点和差分间距可表示、严格递增，spot/strike 严格在内部。
不要求 strike 避开节点；新接口没有 European AAD 的 kink 导数限制。
spot 在节点上直接读取，否则用相邻两点的线性插值；不使用可能越界的 cubic。
不支持用户自定义非均匀网格或静默插入节点。上界的默认倍数只是截断启发式；
高波动或强 carry 请求必须独立扩大上界检验，残差通过不能代替这项检验。

每个锚点区间的基础步数为 `max(1,ceil(ordinary_steps*区间长度/T))`。
`theta=0.5` 时，从 maturity 的非光滑 payoff 开始向过去的前
`rannacher_intervals` 个基础步分别替换为两个半长 fully implicit 子步。
Bermudan 每次投影后，对下一段重新启动相同阻尼；不足该数量就处理现有全部基础步。
carry-only 锚点不中断已有阻尼计数，也不启动新阻尼。American 只从终值启动，
不在每个 LCP 层重复启动。`theta=1` 时不额外拆分，结果保留请求值但报告有效阻尼为 0。

空间生成元先尝试 central 差分；如有负的邻点系数，则对该行 drift 使用满足正邻点
系数的单边格式。报告受影响的行数，不能给近零波动率加 floor，也不能把负 rate 拒绝为
“不支持”。每个时间步验证隐式矩阵的 M-matrix 充分条件、正主元及显式部分非负。
如仅时间步太大，则按二分细分到这些条件满足，受 `max_actual_steps` 约束；
所有拆分都保留原事件锚点，不静默换 theta 或换方法。若空间算子仍不满足条件则失败。
包括负 rate 引起的 `1+theta*dt*r` 条件；“隐式一定稳定”不能替代检查。

写成每行生成元的非负邻系数 `a_minus,a_plus` 后，预检的充分条件是
`1+theta*dt*r>0`，以及 `1-(1-theta)*dt*(a_minus+a_plus+r)>=0`。
实际求解还要验证正主元。默认 `theta=1` 的显式部分为恒等矩阵，没有因 diffusion
引起的 `dt=O(ΔS²)` 限制；负 rate、不可表示的系数/子步或正主元失败仍须处理。
CN 的非负显式部分条件继续强制执行，不能为减少步数放松它。
二分后端点必须可表示且严格分离；如无法生成合法子层或会超过实际步数上限，
在 rollback 前确定性拒绝，不无限细分。

默认 fully implicit 的时间离散在充分正则的线性问题中为一阶；American penalty、
投影和单边空间格式不提升这个阶数。CN 对光滑线性解的形式时间阶数为二阶，
按上述 Rannacher 规则处理 payoff/投影的非光滑性，但不承诺 American 或任意
Bermudan 日程的全局二阶。必须独立测时间收敛；500 步和残差通过都不是价格误差认证。

预算核算规则：

- 在大分配前，使用检查过溢出的整数计算实际时间计划、结果 payload 上界和工作区上界。
  payload 包括拷贝请求中的数值/日期数组、网格、估值层、逐步/行权层诊断及区域记录的
  原生数值存储；工作区包括所有同时活跃的算子、因子、rhs、迭代层和暂存数值数组。
  两项独立计算，不把结果数组再次计入 workspace。
- 所有有容量的容器按 capacity 而非 size 记账，固定数值字段按 `sizeof` 计；
  执行计数用实际结构大小计算字节，允许不同 ABI 的实际字节计数不同。排除 allocator 元数据、
  字符串/标签、语言包装、worksheet 单元格及进程其他内存，不能称为 RSS 限制。
- 行权区域最坏每个非终值可行权层有 `ceil(N/2)` 个节点 run。按下文 `R` 为完整区域
  表预先规划并保留容量，size 按实际区域数填充；不丢层、截断区域或假定只有一个区域。
  容器不得超出计划容量作隐式几何扩容，也不在 rollback 中重分配整张区域表。
  预检用下面的保守字节包络，分配前拒绝不足预算；预算为 0 不表示无限制。
  无隐藏 refinement、重试或倍增 penalty 的预算外执行。
- `max_work_units` 是可移植的线性求解工作计数，不是浮点操作总数或墙钟超时。
  Bermudan 每个子步一次求解，American 每次 penalty 线性化一次求解。
  投影和诊断扫描仍受已限制的节点/步数控制；每次求解前核对剩余 work units。
- 计划步骤、实际步骤、阻尼/稳定性增加的步骤、总求解次数、最大逐步迭代数、
  upwind 行数、实际 work units、payload 字节和峰值 workspace 字节均进入 execution。
  步数指 rollback 子步；不把一次投影另计为时间步。

### 5.1 完整容量与字节预检

设 `N` 为节点数，`M` 为完成所有事件/阻尼/单调性细分后的子步数，`C` 为 carry 段数，
`D` 为复制的 Bermudan 日期数（American 为 0）。可行权层数 `L` 包括终值层：
American 为 `M+1`，Bermudan 为 `D`。非终值可行权层数 `E=L-1`，区域容量
`R=E*ceil(N/2)`；终值的区域数是 0。所有乘加先检查整数溢出。

为避免在尚未实现结构时把四个 int 的下界误称为完整预算，规定以下**存储上限契约**。
它是预检的保守包络，不是测得的 `sizeof`；实现必须用布局/容量断言证明符合包络，
并报告实际字节。结构 padding、可空数值标记及固定数值元数据都包含在对应包络内。
若新增数值字段或存储策略超出包络，须先修订契约，不能在实现中偷偷提高默认上限。

| 保留项                            | 容量    | 每项字节上限 |
|--------------------------------|-------|--------|
| 请求/设置固定数值、执行与失败元数据、对齐余量        | `1`   | `4096` |
| 复制的 carry 段                    | `C`   | `32`   |
| 复制的行权日                         | `D`   | `8`    |
| 空间网格与最终估值层                     | `N`   | `16`   |
| 时间节点及事件标记                      | `M+1` | `64`   |
| 逐步诊断，含所有可空原值/归一化残差             | `M`   | `256`  |
| 可行权层记录，含终值                     | `L`   | `64`   |
| 区域记录，含四个 int、两个 bool 及 padding | `R`   | `32`   |

因此完整 payload 预检为：

```text
P_plan = 4096 + 32*C + 8*D + 16*N + 64*(M+1) + 256*M + 64*L + 32*R
W_plan = 4096 + 256*N + 64*(M+1) + 64*C + 16*D
```

`W_plan` 覆盖同时活跃的最多 24 个 double 和 16 个 int 的 `N` 长度数值数组
（系数、三对角因子、rhs、payoff、迭代层、残差和 active/stencil 标记），
一个每项至多 64 字节的时间计划暂存、carry 预积分、日期扫描索引和固定余量。
结果已经计入 `P_plan`，不重复计入工作区；任何临时额外拷贝都必须占用这个工作区包络。
不保留 `N*M` 的值面、不几何扩容区域表，不能另分配一个未计账的 full-layer buffer。
字符串、语言包装和 allocator 元数据的排除范围沿用上文。

预检比较 `M<=max_actual_steps`、`P_plan<=numeric_payload_budget_bytes`、
`W_plan<=workspace_budget_bytes`；相等允许，任意一项不足即在大分配/rollback 前抛
`EarlyExercisePdeBudgetExceeded`，标出字段、所需上界和现有预算。不因实际区域少而
豁免 `P_plan`。执行记录增加 `planned_region_capacity`、`planned_numeric_payload_bytes`、
`planned_workspace_bytes`，与实际 capacity/payload/峰值 workspace 分开，三绑定一致。

work units 仍按实际求解次数计，不因为上界迭代数很大就提前拒绝。为检验默认值是否
把冲突转移给 work 上限，下文另核算 `U100=M*(N-2)*max_iterations_per_step`，
即每步都做满 100 次求解的保守上界；`U100` 不是预检强制预留，执行前逐次检查预算。
若该步数下不能在所需 work budget 内继续，就按既定 `WORK_LIMIT` 返回失败结果，
没有可用价格；不裁剪诊断或改变格式。布局/容量断言和真实峰值核算留实现阶段验证。

### 5.2 B1 两组反例的修订后核算

两组均取 `2026-10-18` 至 `2027-10-18`，`S=K=100,r=0.05,q=0`、单段 carry、
American、`ordinary_steps=500`、默认 `upper=400`。A 为 `N=401,sigma=0.50`；
B 为 `N=801,sigma=0.20`（从 401 加密一次）。这里只作契约整数核算，不运行求解器。

**修订后的默认 fully implicit：**两组正 rate 且邻系数可单调化，`1+dt*r>0`，
无需 diffusion 驱动的时间细分；有效 Rannacher 为 0。因此均为 `M=500,L=501,C=1,D=0`。

```text
A: R = 500*ceil(401/2) = 100500
   P = 4096+32+0+6416+32064+128000+32064+3216000 = 3418672 bytes
   W = 4096+102656+32064+64+0 = 138880 bytes
   U100 = 500*399*100 = 19950000 work units

B: R = 500*ceil(801/2) = 200500
   P = 4096+32+0+12816+32064+128000+32064+6416000 = 6625072 bytes
   W = 4096+205056+32064+64+0 = 241280 bytes
   U100 = 500*799*100 = 39950000 work units
```

A/B 完整 payload 分别约 3.2603/6.3182 MiB，均低于默认 64 MiB；完整工作区均低于
256 MiB；500 子步低于 100000；即使每步迭代满 100 次，工作量也低于默认一亿。
这证明的是预算默认路径可用，不是 penalty 必定收敛或价格达到验收精度。

**显式选择 CN：**`theta=0.5`、`rannacher_intervals=2`，前两个基础步共四个隐式半步。
最右内部行的 central 对角为 `-(sigma²*(N-2)²+r)`，因此余下 498 个基础步要求
`dt<=2/(sigma²*(N-2)²+r)`；二分因子 A/B 分别为 64/32。区域与时间诊断不删减：

```text
A: M = 4+498*64 = 31876, L = 31877, R = 31876*201 = 6407076
   P = 4096+32+0+6416+2040128+8160256+2040128+205026432 = 217277488 bytes
   W = 4096+102656+2040128+64+0 = 2146944 bytes
   U100 = 31876*399*100 = 1271852400 work units

B: M = 4+498*32 = 15940, L = 15941, R = 15940*401 = 6391940
   P = 4096+32+0+12816+1020224+4080640+1020224+204542080 = 210680112 bytes
   W = 4096+205056+1020224+64+0 = 1229440 bytes
   U100 = 15940*799*100 = 1273606000 work units
```

只改 `theta` 而保留 64 MiB 时，A/B 仍须在预检确定性拒绝，完整所需上界如上，
不能默认退回 fully implicit。可执行的显式 CN 预算配置是：A 设置
`numeric_payload_budget_bytes=217277488,max_work_units=1271852400`，B 设置
`numeric_payload_budget_bytes=210680112,max_work_units=1273606000`；其余预算保持默认。
这些配置允许区域最坏容量和全部 100 次迭代，未触及默认子步/工作区上限；也可使用
256 MiB payload，但仍须显式考虑 work budget，不能只提高一项预算就承诺整个请求成功。
此 CN 配置的成本是调用者显式选择，不是新默认政策。

### 5.3 默认预算内的空间/时间加密路径

同一一年期常 carry 请求，采用默认 fully implicit，仅改表中的 `N`/`ordinary_steps`。
以下正 rate 案例没有额外事件、阻尼或稳定性子步，故 `M=ordinary_steps`；
空间两次加密和时间至少三档均有默认预算内的路径：

| `N`    | `M`    | 区域容量 `R` | 完整 `P_plan` bytes | 完整 `W_plan` bytes | `U100`     |
|--------|--------|----------|-------------------|-------------------|------------|
| `401`  | `500`  | `100500` | `3418672`         | `138880`          | `19950000` |
| `801`  | `500`  | `200500` | `6625072`         | `241280`          | `39950000` |
| `1601` | `500`  | `400500` | `13037872`        | `446080`          | `79950000` |
| `401`  | `1000` | `201000` | `6826672`         | `170880`          | `39900000` |
| `401`  | `2000` | `402000` | `13642672`        | `234880`          | `79800000` |
| `801`  | `750`  | `300750` | `9929072`         | `257280`          | `59925000` |
| `801`  | `1000` | `401000` | `13233072`        | `273280`          | `79900000` |

每行完整数值均代入第 5.1 节同一公式，不是仅区域字段的内存下界。这覆盖 A/B 的
常用参数和独立加密方向，没有把原冲突转移到默认 100000 步、一亿 work units 或
256 MiB 工作区。更密日程、负 rate 强细分、更大网格/期限的请求仍按完整公式显式选择
预算或确定性失败，不承诺所有有限输入都能用默认预算完成。

实现准入须复现上述计划和容量，检查预算恰好等于 `P_plan/W_plan` 可通过预检，
少 1 byte 时在 rollback 前拒绝；`max_actual_steps=M-1` 同理。
另用不足实际所需 work units 的请求验证途中 `WORK_LIMIT`、完整失败诊断和无价格。
空间收敛须固定充分细时间；时间收敛须固定充分细空间/上界，不以本表同时加密两轴
代替独立收敛证据。时间一阶可能需要更多步才能达到原价格目标，不能用降低准入精度换默认可用。

## 6. 边界条件和 American 残差

对 payoff `g`，定义任一未来可行权时点 `e` 的确定性贴现 payoff：

```text
call: max(s*exp(-Q(t,e)) - K*exp(-R(t,e)), 0)
put:  max(K*exp(-R(t,e)) - s*exp(-Q(t,e)), 0)
```

两侧边界取其在未来允许行权集合上的最大值。Bermudan 回滚 continuation 到行权日
`t` 时，只在严格晚于 `t` 的行权日上取最大值；随后对包含两个端点的整层投影，
投影后的层才包含今日行权。不得把投影后的边界预先注入同一步 continuation 方程。
非行权层同样只用其未来日程；maturity 直接初始化 payoff。American 的集合是 `[t,T]`。
分段常数 carry 下，American 的最大值须检查各段端点
以及段内指数差的有效驻点，不能仅按日采样或只选下一个行权日。
在 `s=0` 这给出精确退化边界：put 可能等待负利率下的未来行权；call 为 0。
在有限的 `s=upper`，这是确定性渐近边界近似，必须通过截断检验；不得标为精确边界。
所有积分、指数和边界值都要检查数值范围。未行权日不能直接套用 `K` 的 put 左边界。

American 每个子步在消去边界后的内部节点上构造未加 penalty 的方程 `A*u=b`。
规定 `A_ii>0`，逐行除以 `A_ii` 得 `Â`、`b̂`；它们是固定的原始 LCP 数据，
不能随 penalty active set 改变。记：

```text
gap_i = u_i - g_i
w_i   = (A*u-b)_i / A_ii
LCP: gap_i >= 0, w_i >= 0, min(gap_i,w_i) = 0
penalty equation: Â*u-b̂ - penalty_factor*max(g-u,0) = 0
```

`gap` 和 `w` 都是价格单位；此归一化避免把价格/时间的连续 PDE 残差直接与 payoff gap
比较。每步计算下面的原始 LCP 诊断，不使用 penalized equation 残差冒充互补残差：

```text
obstacleViolation = max_i max(-gap_i,0)
dualViolation     = max_i max(-w_i,0)
complementarityResidual = max_i abs(min(gap_i,w_i))
iterationChange   = max_i abs(u_i(new)-u_i(old))
C = max(request.spot, request.strike)
epsilon = absolute_tolerance + relative_tolerance*C
```

分别报告前三项原值及除以 `C` 的无量纲值，记录 `epsilon`。American 仅当这三项和
`iterationChange` 全部不超过 `epsilon`，且最后的线性系统 backward error 不超过
设置阈值时通过该步。初始猜测取前一已完成时间层与 payoff 的逐点最大值；每次
线性化求解计一次迭代，首步不为“检查一次”另计迭代。若初值已满足全部条件，
可报告 0 次求解，但不能跳过原始 LCP 检查。没有后置 `max(u,g)` 来掩盖 penalty 残差。
0 次求解的步将 `iterationChange` 记为 0、线性 backward error 记为空，其线性检查
不适用；有求解的步必须报告并通过实际测得的线性误差。

每次线性求解的 backward error 使用物理行分母：
`max_i |(M*u-rhs)_i|/(|rhs_i|+Σ_j|M_ij*u_j|)`，分母与分子同为 0 时记为 0，
否则分母为 0 时视为失败。`M` 是本次实际求解的矩阵；penalty 迭代时含 penalty。
报告该子步全部线性求解的最大值，不能只保留最后一次。线性残差小与原始 LCP
残差小是两个条件；二者都不证明空间、时间、边界截断的定价误差足够小。

Bermudan 的非行权层只验证线性方程；投影后报告 `projectionChange`、
`max|max(u,g)-uProjected|` 和投影后 obstacle violation。American 三项 LCP 残差在
Bermudan 层均为缺省值，不填 0；投影后的值不是同一个 European 线性方程的解。
`penalty_factor`/迭代上限在 Bermudan 不起作用，但仍验证其格式并保留输入快照。

penalty 选择沿用 P3 基线。风险依据是
[Forsyth–Vetzal 原论文](https://cs.uwaterloo.ca/~paforsyt/con7.pdf)：
其结果依赖离散单调性条件，固定时间步的 American 障碍处理不能直接取得 European
Crank–Nicolson 的二阶收敛结论。本文的残差门槛、上界和默认参数是 DAL 设计选择，
不是从该论文推导的通用精度承诺。

## 7. 结果、行权区域与失败语义

`EarlyExercisePdeResult_` 的规范 getter：

- `Status()`、`IsSuccess()`、`Price()`、`Request()`。
- `Grid()`、`Values()`：物理节点与最终估值层，后者仅成功时公开。
- `TimeGrid()`、`StepDiagnostics()`、`ExerciseLevels()`、`ExerciseRegions()`。
- `Execution()`、`Failure()`、`Method()`。方法标签分别为
  `BlackScholesBermudanThetaProjection` / `BlackScholesAmericanThetaPenalty`。

返回类型按同一前缀命名：`EarlyExercisePdeStatus_`（含 `SUCCESS` 和下文失败值）、
`EarlyExercisePdeTimeNode_`、`EarlyExercisePdeStepDiagnostic_`、
`EarlyExercisePdeExerciseLevel_`、`EarlyExercisePdeExerciseRegion_`、
`EarlyExercisePdeExecution_`、`EarlyExercisePdeFailure_`。容器为 owning `Vector_`，
可空日期/实数用 `std::optional`，`Failure()` 成功时为空 optional。时间/节点索引用
`int`，容量和累计工作用 `size_t`。本文规定的字段是这些记录在三种语言中的共同含义。

`Price()` 仅成功时返回有限 `double`。失败时抛出含状态和失败上下文的异常；
内部价格是空 optional，不能把最后一次迭代、部分 rollback 或 NaN 当作价格返回。
`Status()` 和 `Failure()` 在任何已返回的结果上可读取。

`StepDiagnostics()` 按目标 `time_index` 递增排列（与执行 rollback 顺序相反）。
每个已尝试子步一行，含 `rollback_step`（从 1 开始）、源/目标时间索引、`dt`、
实际 theta、是否阻尼、projection 标记、求解次数、迭代改变量、线性 backward error、
可空的投影/原始 LCP 残差、`completed`。摘要保留所有已尝试步的最大残差，不能因
最后一步较好覆盖历史超限。失败步一行保留实测值，尚未计算项为空；后续未执行步不造行。

`ExerciseLevels()` 每个可行权输出层一行，含 `time_index`、可空锚点日期、
`kind`（`BERMUDAN_PROJECTION`/`AMERICAN_LCP`/`TERMINAL`）、`region_offset`、
`region_count` 和分类阈值 `epsilon`。终值明确标 `TERMINAL`，不从 payoff 的处处相等
制造自由边界。无正 payoff 接触节点时区域数为 0，这与未到行权日不同。

对非终值层，正 payoff 且 `|u-g|<=epsilon` 的节点视为近似接触；American 还须
满足 `w>=-epsilon`，Bermudan 使用投影前 `continuation<=g+epsilon`。
相邻接触节点合成区域。`ExerciseRegions()` 的每行是：

```text
time_index, region_index, first_node, last_node, left_clipped, right_clipped
```

`region_index` 在各层从 0 开始；`first_node..last_node` 是闭区间。
不贴边时，左边界括在 `grid[first_node-1]..grid[first_node]`，右边界括在
`grid[last_node]..grid[last_node+1]`。接触端点时相应 clipped=true，无外侧括区。
支持空、单段和多段区域；零 payoff 的无差别节点不算经济行权区域。
这是有残差阈值和空间分辨率的接触分类，不是唯一最优策略或光滑自由边界插值。
失败时只保留已完成层的区域，失败层不发布分类。

错误分两类：

1. **没有开始 rollback 的输入/预检错误**抛 DAL 异常，不创建结果句柄。
   包括日期、类型、单位域、非法设置、不可表示网格/积分、整数溢出，以及计划的
   actual steps/payload/workspace 上界超过配置预算。标识
   `InvalidEarlyExercisePdeRequest` 或 `EarlyExercisePdeBudgetExceeded`，给出字段路径、
   实际值、约束，数组还须有索引；内存分配失败可抛系统异常，不能承诺还能分配失败结果。
2. **开始 rollback 后的可诊断数值失败**返回 owning 失败结果，无价格，保留已完成
   部分和失败步。状态为 `MAX_ITERATIONS`、`LCP_RESIDUAL_LIMIT`、
   `LINEAR_SOLVE_FAILURE`、`NON_MONOTONE_OPERATOR`、`NONFINITE` 或 `WORK_LIMIT`。
   记录失败目标时间/锚点日期、节点/行（若有）、迭代号、实测残差、阈值、
   work units、简短原因。线性求解错误区分 pivot 与 backward-error 超限。

达到迭代上限时若最终改变量已达标而原始 LCP 残差未达标，记 `LCP_RESIDUAL_LIMIT`；
否则记 `MAX_ITERATIONS`。任何失败都不自动减容差、增 penalty、回退成逐步投影或
American→Bermudan。用户可用诊断显式提交新请求；失败结果不参与价格比较或汇总。

错误示例：`InvalidEarlyExercisePdeRequest; contract.exercise_dates[2];
2027-04-17; must be strictly later than exercise_dates[1]=2027-04-17`；
`EarlyExercisePdeBudgetExceeded; numeric_payload_budget_bytes=1024;
required_upper_bound=...`。文案可补充上下文，字段路径/标识和约束不可缺失。

## 8. Python 和 Excel 映射

Python 不暴露超过六个位置参数的万能工厂。推荐签名：

```python
EarlyExercisePdeCarrySegment_(end_date, rate, dividend_yield)
EarlyExercisePdeMarket_(spot, volatility, carry)
EarlyExercisePdeContract_(evaluation_date, maturity_date, strike,
                         option_type, exercise_style, *, exercise_dates=())
EarlyExercisePdeSettings_(*, grid_points=401, upper=None, ordinary_steps=500, ...)
EarlyExercisePdeRequest_(contract, market, *, settings=None)
EarlyExercisePdeResult_New(request)
```

省略号仅表示本记录第 5 节逐项列出的其余 keyword-only 设置，不是自由 kwargs。
日期仅接受现有 `dal.Date_`，不隐式解析字符串、datetime、Excel serial 或浮点年份。
整数拒绝 bool/enum/小数和范围溢出；实数接受有限 int/float，拒绝 bool/enum/数字文本。
风格和 option type 接受对应 DAL 枚举或规范字符串，拒绝其他枚举和整数编码。
只读字段使用 snake_case，数组/嵌套结果 getter 返回副本；可空字段用 `None`。
`.status` 和嵌套失败状态返回规范枚举字符串，`.method` 返回上述方法字符串，
`.is_success` 返回 bool；不向调用者暴露整数状态编码。
未知关键字由 binding 报错。数值失败仍返回结果；`.price` 和 `.values` 在失败时抛异常，
`.status`、`.is_success`、`.failure`、诊断均可读取。无 AAD/Jacobian 属性。

Excel 沿用仓库 `_New` / `_Get_*` 命名，Machinist 生成的 worksheet 名将下划线转点
并大写。必需参数在前，可选参数在后：

```text
EarlyExercisePdeSettings_New(name, [settings]) -> settings handle
EarlyExercisePdeSettings_Get_Configuration(settings) -> canonical key/value rows
EarlyExercisePdeRequest_New(name, contract, market, carry,
                           [exercise_dates], [settings]) -> request handle
EarlyExercisePdeResult_New(name, request) -> result handle
```

contract 为两列键值表，五个必需键为 `evaluation_date`、`maturity_date`、`strike`、
`option_type`、`exercise_style`。market 为两列键值表，必需键 `spot`、`volatility`。
carry 为三列矩形表，每行 `EndDate / Rate / DividendYield`，无表头；至少一行。
exercise_dates 是单行或单列日期区域；Bermudan 必需，American 必须空。
settings 为两列键值表；为空使用默认，空的 `upper` 表示自动上界，其他已提供键值不许空。
所有键忽略大小写但不支持别名；拒绝未知键、重复键、缺失键、额外列及混入的半空行。
整个范围空才表示省略可选参数，不丢弃中间空格来掩盖错误。

日期是有效整数 Excel 日期 serial（经 `Date::FromExcel`），拒绝非整数 serial、bool、
日期文本和越界日期。普通实数拒绝 bool/数字文本；整数设置必须整数、非负/正值按表约束，
预算还须在该平台 `size_t` 和 Excel 可精确表示的整数范围内。识别 `xltypeInt` 时读
`val.w`。输入归一化必须在通用 Machinist 转换前完成，不能被默认值吞掉错误类型。

request getter 为 `EarlyExercisePdeRequest_Get_Contract` / `_Get_Market` /
`_Get_Carry` / `_Get_ExerciseDates` / `_Get_Settings`，都返回拷贝，供审计和往返重建。
结果 getter 为 `EarlyExercisePdeResult_Get_Request` / `_Get_Summary` / `_Get_Price` /
`_Get_Grid` / `_Get_TimeGrid` / `_Get_Steps` / `_Get_ExerciseLevels` /
`_Get_ExerciseRegions` / `_Get_Execution` / `_Get_Failure`。

`Summary` 为 key/value 行，至少含 Status、IsSuccess、Method、Price、PriceUnit、
AbsoluteTolerance、RelativeTolerance、MaxLinearBackwardError、
MaxComplementarityResidual；Bermudan 的 American 残差摘要为空，失败 Price 留空。
`Price` getter 在失败时抛 DAL 异常，由现有 `Excel::Error` 转为包含失败标识的
`#Error: ...` 文本；不另改全局 XLL 错误机制，也不能返回空白或 0。
`Grid` 为 NodeIndex/Spot/Value，失败时 Value 空；
`TimeGrid` 为 TimeIndex/TimeYears/CalendarDate/事件标记。
Steps/ExerciseLevels/ExerciseRegions 按第 7 节字段展开，保留 snake_case 字段意义及单位。
`Execution` 包含第 5 节所有计数；`Failure` 成功时只有表头，失败时为字段/值行。
每张表都有表头；缺省诊断/日期写空白，无适用性不能写 0。
返回前检查含表头的 Excel 行列上限（1,048,576 行、16,384 列），超限报 getter 错误，
不截断。原生执行预算不包含这些单元格。

## 9. 典型用法与示例边界

以下是拟议接口示意，不是基线已可运行的 API。C++ 的简洁 happy path：

```cpp
Dal::EarlyExercisePdeRequest_ request;
request.contract_.evaluationDate_ = Dal::Date_(2026, 10, 18);
request.contract_.maturityDate_ = Dal::Date_(2027, 10, 18);
request.contract_.strike_ = 100.0;
request.contract_.optionType_ = Dal::OptionType_("PUT");
request.contract_.exerciseStyle_ = Dal::EarlyExerciseStyle_("AMERICAN");
request.market_.spot_ = 100.0;
request.market_.volatility_ = 0.20;
request.market_.carry_ = {{request.contract_.maturityDate_, 0.05, 0.0}};
const auto result = Dal::EvaluateEarlyExercisePde(request);
if (result.IsSuccess())
    std::cout << result.Price() << '\n';
```

Python 用同一合约改为有两次机会的 Bermudan：

```python
today, mid, maturity = dal.Date_(2026, 10, 18), dal.Date_(2027, 4, 18), dal.Date_(2027, 10, 18)
contract = dal.EarlyExercisePdeContract_(today, maturity, 100.0, "PUT", "BERMUDAN",
                                        exercise_dates=[mid, maturity])
market = dal.EarlyExercisePdeMarket_(100.0, 0.20,
    [dal.EarlyExercisePdeCarrySegment_(maturity, 0.05, 0.0)])
result = dal.EarlyExercisePdeResult_New(dal.EarlyExercisePdeRequest_(contract, market))
if result.is_success:
    print(result.price)
```

Excel 最短路径是准备上述三张输入表，然后调用
`EARLYEXERCISEPDEREQUEST.NEW(name,contract,market,carry,exercise_dates)`，
再用 `EARLYEXERCISEPDERESULT.NEW(name,request)` 和
`EARLYEXERCISEPDERESULT.GET.SUMMARY(result)` 查看状态、价格与残差。
American 的 exercise_dates 留空。函数帮助必须明确价格单位和失败读取方式。

示例遵守 [component boundaries](../../references/component-boundaries.md)：
`dal-cpp/examples/methods/european_fd/`、`european_aad_fd/`、`american_put_mc/`
保留原位置、目的、target/CTest/安装程序名。它们只用 core API，不导入 facade，
也不将 MC 比较示例改成纯 PDE 示例。新 facade 演示如需要，单独放在
`dal-public/examples/early_exercise_pde/`，链接 `DAL::public`。
core 的演示使用 `Dal::PDE` 相同 owning 类型，不经过公共头文件。
本阶段只写设计记录和索引，没有新增或改写这些程序及 CMake。

## 10. 独立验收矩阵

本节是后续数值风险审查、实现和独立验证的准入清单，不声称已执行。
价格误差与原始 LCP/线性残差分别验收。代表性 BS 用例以 `C=max(S,K)` 缩放：
CRR/Black 对照价格差暂以 `1e-4*C` 为目标，空间、时间及上界各自加密的价格差
暂以 `5e-5*C` 为目标；数值审查可在实现前收紧/调整并固定案例和阈值，不能在
观察失败后临时放宽。近零波动率的单边格式不承诺二阶收敛。

- **独立 CRR**：验证者自行构造风险中性树和最优停止，不调用生产 PDE 系数、
  boundary envelope、projection 或 penalty 代码。常 carry 下 call/put 都覆盖；
  Bermudan 选可精确对齐树层的行权日期，至少比较多组更细树及奇偶振荡包络。
  当概率不在 `[0,1]` 时不能 clamp；细化仍无效则该树不作 oracle。
  零波动率用独立确定性最优行权解析参照，非平坦 carry 用独立分段树/算法或解析极限。
- **European 退化**：Bermudan 日程仅 maturity，比较解析 BS（对分段 carry 用积分 rate/yield），
  以及旧 European PDE 在相同物理输入/设置可对齐时的值；旧 AAD 固定网格限制继续有效。
  无股息且非负 rate 的 American call 比较 European。旧 European/Jacobian/默认值回归保持。
- **金融不等式**：相同空间/时间网格上的嵌套 Bermudan 日程，价值不得因增加机会下降；
  相同网格 American 不低于各 Bermudan/European，容差需含两次求解的残差传播。
  如不同日程改变默认时间计划，先统一实际时间层再作单调性断言。
  只在实际可行权层要求内在价值下界；未含估值日的 Bermudan 不能泛用此下界。
- **日期和 carry**：不规则日历日、闰日、今日行权、同日 carry/event、极短到期、
  仅到期、密集行权，以及非法/重复/未排序/漏到期日程。比较单段与等值拆段，
  分段 carry 跳点必须精确入网；测试不允许 event snap 或新增行权机会。
- **负 rate/yield**：包括负 rate put 在 `S=0` 的等待价值、负 carry 组合、带 yield call，
  以及可能多段接触的情形；不得用非负利率 put 的单边界假设判错。
  边界最优时点含 carry 段内驻点的测试须用独立一维最大化/解析导数核验。
- **零/近零波动率**：`sigma=0`、接近 0、drift 两个符号和 `r=q`。
  独立确定性轨迹最优停止，检查 upwind 报告、非负系数、正主元及时间拆分；
  不得以 epsilon 波动率或概率 clamp 令 oracle 看似可用。
- **分离收敛**：固定充分细时间/上界做空间细化；固定充分细空间/上界做时间细化；
  固定局部空间步长扩大上界，分别记录价格、接触括区和残差。不能同时加密两轴
  掩盖抵消误差，不把迭代收敛称为 PDE 收敛。深 ITM/OTM、高 vol、短期限均覆盖。
- **求解/失败**：强制低迭代数、低 penalty、严残差、线性精度超限、窄预算和整数溢出；
  检查 failure 状态、实测残差、失败位置、无可用价格、无预算外 retry。
  验证无正 payoff 接触、贴边和多段区域的编码，投影残差与 LCP 残差的缺省区别。
- **三绑定一致性**：同一已解析请求通过 C++、Python、Excel portable 层及 Windows XLL，
  比较价格、实际事件索引、时间/空间网格、诊断排序、缺省值、失败代码和所有默认值。
  相同二进制路径应得相同数值；跨编译器目标为 `1e-12*C` 或预先记录的浮点界限，
  不能只比较三者彼此相等而不比较独立 oracle。非法类型/bool/日期文本/重复键均覆盖。
- **owning 与隔离**：销毁输入、复制结果、释放原 Python/Excel handle 后读取；
  并发不同日期/市场；非空 AAD tape 前后节点、模式和 adjoint 不受影响。
- **LSMC 交叉检查**：复用现有合法案例，与独立训练/定价样本的 LSMC 价格按其统计和
  训练误差解释；这是补充证据，不用单次 MC 差异或与同源 helper 的一致性替代 CRR/解析准入。

性能验收限于新路径的代表案例：同规模 Bermudan/ American、密集日程，以及近零 vol
需要 upwind/细分的案例，报告时间、步骤、求解次数、work units 和原生预算峰值。
无历史生产早行权 PDE 性能基线，不声称现有 MC 或 European benchmark 的加速百分比。
这只是设计阶段，不新增单元测试、跑全产品测试或泛化 benchmark 矩阵。

## 11. 兼容性、替代方案与真实未决风险

现有 `EvaluateEuropeanPdeRisk`、`EuropeanPdeRiskResult_New`、Excel 注册、settings 默认、
固定网格/spot index 规则、错误及三轴导数含义全部保持。新 API 不使用旧请求的
`point={rate,vol,strike}` positional 数组，不继承其“不允许 strike 在节点上”的 AAD 条件。
当前 PDE/LSMC 文档在产品真正交付后再更新；本阶段不能写成已支持。

拒绝的替代方案：

- 在旧 European risk API 添加 style 和日期，容易让提前行权 Greeks 被误认为已定义。
- American 每步只做 `max` 投影，不能满足本次要求的离散 LCP 契约。
- 暴露用户 callback/carry curve handle，增加跨语言寿命和状态问题；首版复制分段常数数据。
- 只返回一个 exercise spot 或固定 put=左端区域，不覆盖无行权/多区域和负利率。
- 所有失败抛异常，丢失迭代上下文；所有错误返回 handle 又使非法输入不易发现。
  因此保留预检异常与执行失败结果的明确边界。
- 以 PSOR、policy iteration、多方法 fallback 为首版公共选项，扩大未经验证的组合。
  如风险审查证明 penalty 不足，应在实现前修订本记录，不静默更换。
- 自动选最优网格或声称给定残差即可控制价格误差；首版要求显式收敛检验。

已决定范围内没有阻塞 API 设计的缺失输入。仍需后续角色解决的风险是：

1. 验证 row-normalized penalty 的默认因子、残差阈值及 `max_iterations=100` 在
   负 rate/低 vol 和大网格下的可达性与条件数；通过不了必须显式失败。
2. 验证单调 stencil、CN 非负显式部分及时间细分的预算代价；现有 central theta
   不能未经修改直接覆盖这些要求，改动应留在新生产路径或明确审查共享影响。
   默认已选 fully implicit；两组反例及空间/时间加密的完整预算见第 5.1–5.3 节，
   不是继续将默认冲突留作实现风险。可选 CN 仍可能按空间平方增加时间步，
   应按完整公式预检并显式给预算；实现需证明存储布局、容量和工作区满足所列包络，
   不能放松单调性或丢弃诊断换取成功。
3. 固定网格/上界启发式不提供统一定价误差，需独立固定验收案例和收敛阈值；
   接触分类也受 `epsilon` 与 `ΔS` 影响，不能作为 Greeks 或连续边界的精度认证。
4. 分段 carry 的 American 边界包络有段内极值，需独立验证；有限上界仍是近似。
5. 本轮不具备 Windows XLL 运行证据；portable getter 测试不能替代真实 Excel 类型转换、
   注册与句柄寿命验证。未来局部波动率和重新求解 Greeks 必须重新定义准入。

## 12. 本轮检查与交接

本轮只允许修改此记录和 `.codex/artifacts/README.md`，不提交产品代码或 PR，
不派发其他角色。数值风险审查 → 实现 → 独立定向验证 → 代码评审 → 文档由 orchestrator
顺序安排。实施前若变更契约，应在这里明确调整，不增加平行的重复 spec。

以下复用 2026-10-18 首轮 API 设计的 standalone core-build 实测，本轮 B1 修订不重跑
构建、产品测试或 benchmark：Linux、GCC 15.2.0、CMake 4.2.3、
Release、C++17、native AAD，`DAL_USE_EIGEN=OFF`。初始化了仓库锁定的 rapidjson
`ea152ded01708ff1657b275ed0bcbf0681d64519` 和 machinist
`e76b0ef243ca9bd506bb2f3743d8c9b52ee8d01d` 子模块；未修改其内容或 gitlink。

```bash
cmake --preset=Release-linux -S . -B build/dal379-core-boundary \
  -DDAL_BUILD_PUBLIC=OFF -DDAL_BUILD_PYTHON=OFF -DDAL_BUILD_EXCEL=OFF \
  -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DDAL_CPP_BUILD_TESTS=OFF \
  -DDAL_CPP_BUILD_EXAMPLES=ON -DDAL_CPP_BUILD_BENCHMARKS=OFF -DDAL_USE_EIGEN=OFF
cmake --build build/dal379-core-boundary --target european_fd american_put_mc -j 4
cmake --graphviz=build/dal379-core-boundary/targets.dot build/dal379-core-boundary
ctest --test-dir build/dal379-core-boundary -N -R '^(european_fd|american_put_mc)$'
```

配置和两个目标构建均成功。检查缓存的 public=OFF，CMake graphviz 中两目标的依赖闭包
只有对应 executable、`dal_cpp`（`DAL::cpp`/`dal_library` 别名）及 pthread；
实际 linker 命令均链接 `libdal_cpp.a`，不含 `dal_public`。
CTest 列表仍登记原名 `american_put_mc`、`european_fd` 共两项；这是注册检查，
**没有运行这两项测试**。源目录、安装规则和 target 未修改。没有新增 facade 程序，
也未构建/运行公共示例、Python 或 XLL。本证据只验证原基线的组件边界，
不代表拟议新 API 已实现、已编译或已通过数值验收。

本轮 B1 修订只变更本 API 记录和索引，保留既有 critique 和其他工作。
第 5 节的两组反例、CN 显式配置及加密表使用独立整数算式核对；没有运行新求解器。
本地 Markdown 链接、末尾换行、配置清单、增量范围及 `git diff --check` 通过。
完整布局/容量断言、实际峰值、迭代可达性和独立价格收敛仍待实现和验证阶段证明。
