# DAL-379 P2：市场隐含波动率曲面 API 与数值契约

状态：active API decision，待约束／导数数值风险审查；未实现或完成数值准入。
基线 `4d4760d1b33c77a2d00fd30a2fa6e0299d24fc5c`，分支
`agent/dal-orchestrator/7bc6ada900c9`。沿用 DAL-379 已采纳的 P2 方案及
2026-10-19 用户实施授权，不建立重复 spec。本阶段仅写此记录与索引。

## 1. 受众、范围与已有表面

面向公共 C++、Python、Excel 的市场报价拟合用户、Dupire 用户及独立数值验证者。
首版是正 spot/forward、单标的、确定性比例 carry、欧洲式 call/put；接受价格或
decimal IV，交付受约束 SSVI、owning 快照、解析导数、独立 Dupire 网格入口和有限
完整 refit bump。没有现金股息、负 forward、normal vol、随机利率、自动报价清洗、
动态无套利承诺或可微优化器 AAD。

已读取的基线约束：

- [`dal-cpp/dal/model/ivs.hpp`](../../../dal-cpp/dal/model/ivs.hpp)：`AAD::IVS_`
  的 `Call` 从 spot 和固定 r/q 构造 F/D；`LocalVol` 使用价格中心差分，非虚函数。
  只覆写 `ImpliedVol` 不能改变这两项语义。
- [`dal-cpp/dal/model/dupire.hpp`](../../../dal-cpp/dal/model/dupire.hpp)：既有
  calibration 会填密网格、使用稳定带和尾部 flat fill，不能称为新解析曲面的精确采样。
- [`dal-cpp/dal/model/dupirerisk.hpp`](../../../dal-cpp/dal/model/dupirerisk.hpp)、
  [`dal-public/src/dupirerisk.hpp`](../../../dal-public/src/dupirerisk.hpp) 和
  [`dupireriskrequest.hpp`](../../../dal-public/src/dupireriskrequest.hpp)：现有风险边界是
  `FixedBaseIVSDeterministicCarryFixedGrids`，原始轴是固定 base 上的 decimal-vol spread。
- [`dal-python/src/bindings/dupirerisk.cpp`](../../../dal-python/src/bindings/dupirerisk.cpp)、
  [`dal-excel/src/__dupirerisk.cpp`](../../../dal-excel/src/__dupirerisk.cpp)：已有只读值、
  typed handle、copied getter；Python 的 IVS subclass 可回调 Python，Excel 旧 base
  分派有自己的 BS/Merton 类型约束。
- [`dal-cpp/dal/model/surface/lvmodel.hpp`](../../../dal-cpp/dal/model/surface/lvmodel.hpp)：
  `LocalVolSurfaceData` archive v1 保存 spot/time/vol 网格，允许零 vol，边界 flat extrapolation。
- [`docs/models/dupire.md`](../../../docs/models/dupire.md) 是现行价格、坐标和差分词汇来源。
  已核对 core 的 UOC/Hybrid 示例和 public owning-risk 示例的组件用法。

新 core 归属 `dal-cpp/dal/model/surface/marketivsurface.hpp`，使用 `Dal` 的
`MarketIv*` 值类型；公共 `dal-public/src/marketivsurface.hpp` 暴露 core 类型及便利入口，
不复制求解器。绑定只依赖 facade，core 不反向依赖 public。

## 2. 输入、日期、forward/discount/carry 与报价单位

`MarketIvMarket_` 是冻结值：必需 `evaluationDate_`、`spot_>0`、`terms_`。
每个 `MarketIvTerm_` 含必需 `expiry_`、`forward_>0`、`discount_>0`；日期有效且严格
递增、晚于估值日。D 可以大于 1；不拒绝负 rate/yield。报价到期必须对应一个 term，
允许另有无报价的 carry knot；不为这种 knot 新增 ATM 方差自由参数。

年数是实际日历天数除以 365，包含闰日，不做营业日调整、时区处理或全局估值日捕获。
query 使用从同一估值日起的年数；日期到 time 的转换结果在快照保留。
通用市场在 `(0,log(D(0))=0,log(F(0)/S)=0)` 和 term 间线性插值 `log D`、
`log(F/S)`；最后一段斜率延伸。得到分段常数
`r=-d(log D)/dt`、`q=r-d(log(F/S))/dt`。term 上精确使用输入 F/D，
不再接受一套可能矛盾的独立 r/q。所有积分、指数和中间值检查有限性/可表示性。

另有显式 flat 工厂 `NewFlatMarketIvMarket(evaluationDate,spot,rate,dividendYield,expiries)`，
保留原始 flat r/q 和 `FLAT_CARRY` 标记，直接按指数公式求 F/D。普通 term 输入标记为
`FORWARD_DISCOUNT_KNOTS`，不通过近似直线检测偷偷转换为 flat。

每个 `MarketIvQuote_` 的字段：

| 字段                   | 约束                                            |
|----------------------|-----------------------------------------------|
| `id_`                | 非空、唯一、无 NUL；结果保持输入顺序                          |
| `expiry_`, `strike_` | 到期须在 market terms；strike 有限且正                 |
| `optionType_`        | 复用 `OptionType_`，只接受 CALL/PUT                 |
| `quoteType_`         | 新 Machinist 枚举 PRICE/DECIMAL_IV               |
| `mid_`               | 必需有限实数；PRICE 为每单位标的的贴现 premium；IV 为正年化小数      |
| `bid_`, `ask_`       | 同时提供或同时为空；与 mid 同单位，bid<=mid<=ask；IV bid 可为 0 |
| `weight_`            | 默认 1，有限且严格正；不是样本数，不以 0 隐藏坏点                   |

PRICE 在 put/call 各自的无套利闭区间内：call `[D*max(F-K,0),D*F]`，
put `[D*max(K-F,0),D*K]`；bid/ask 也须在区间内。IV mid>0、ask>0，0.20 表示 20%，
不是 20 或 0.20%。不静默修剪、改类型、丢点或容许价格单位混用。
同 expiry/strike/optionType 的重复经济报价拒绝；同 K 的 call 和 put 可同时存在，
各保留 ID、权重与 parity 诊断。报价输入次序不用于决定期限次序，规范期限轴独立排序。

统一拟合到 `k=log(K/F)` 和 normalized call：

```text
PRICE call: c=C/(D*F)
PRICE put:  c=P/(D*F)+1-K/F
IV:         c=BlackCall(F,K,sigma*sqrt(T))/F
```

IV 的 bid/mid/ask 各自转换；PRICE 不先做 IV inversion，避免把低 vega 的价格误差
放大成不稳定 IV。原单位、转换后的价格、F/D、尺度和原始行号全部保留。
所有报价转换是确定性数据准备；没有拟合或报错后换单位的路径。
IV bid=0 的 Black 转换显式取 intrinsic，不除以零。weight 先按最大权重缩放再归一化，
避免有限大权重求和溢出；k 可用 log(K)-log(F) 计算，避免先求不可表示的 K/F。

拟合可接受带：有 bid/ask 则使用转换后的整段区间；无 bid/ask 的 PRICE 使用
`c_mid ± normalized_price_tolerance` 与合法价格区间的交集，IV 使用
`[max(0,mid-decimal_iv_tolerance),mid+decimal_iv_tolerance]` 转换后的价格区间。
这只是显式容差带，不修改原报价。权重尺度固定为
`s_i=max((c_ask-c_bid)/2,normalized_price_scale_floor)`；无 bid/ask 使用可接受带半宽。
目标 `J=0.5*sum((weight_i/sum(weight))*((c_model-c_mid)/s_i)^2)`，不另加隐藏 vega 权重。
单个合法价格可在边界，不能据此强制有限 IV 反解或制造一个观测 IV。

已知必要套利矛盾（含 call/put 区间不相交）写输入诊断；mid 的违例不代表整个
bid/ask 区间不可行。只有可证明整段无解的矛盾才可提前报 `INCONSISTENT_QUOTES`。
其他坏点由约束拟合质量判定，不能用稀疏 strike 检查宣布报价集全域无套利。

## 3. SSVI 族、约束与保证域

首版固定公共 rho 和 gamma=1/2 的修正 power-law SSVI，不拟合每期限独立 rho/gamma：

```text
w(k,t) = theta(t)/2 * [1+rho*phi(theta)*k
                         +sqrt((phi(theta)*k+rho)^2+1-rho^2)]
phi(theta) = eta / sqrt(theta*(1+theta))
sigma_imp(K,t) = sqrt(w(log(K/F(t)),t)/t)
```

依据 [Gatheral–Jacquier, SSVI 定义及 Theorems 4.1/4.2](https://arxiv.org/pdf/1204.0646)，
使用的充分条件为：

```text
theta'(t)>=0
0 <= d(theta*phi)/dtheta <= [(1+sqrt(1-rho^2))/rho^2]*phi
theta*phi*(1+abs(rho)) < 4
theta*phi^2*(1+abs(rho)) <= 4
```

rho=0 时第二项上界取无穷。以下是本设计对所选族的代数推导与保守参数盒：

```text
d(theta*phi)/dtheta / phi = 1/(2*(1+theta)) in (0,1/2]
abs(rho)<=rho_limit<1
0<=eta<=eta_max = 2*sqrt((1-constraint_margin)/(1+rho_limit))
theta*phi^2*(1+abs(rho)) <= eta^2*(1+rho_limit) <= 4*(1-margin)
theta*phi*(1+abs(rho)) <= eta_max*(1+rho_limit) < 2*sqrt(2) < 4
```

margin=constraint_margin。这是整个 admissible SSVI 集合的保守子集；eta 上限使用固定
rho_limit 而不是随 rho 改变的绝对值边界，使校准约束保持为可核验的盒约束。
拒绝拟合不证明市场自身套利，也可能是模型族过窄或局部优化失败。eta=0 显式定义
为平坦 smile 的闭包：w=theta；不执行除以 eta 的表达式，canonical rho=0。

每个有报价的不同期限有一个 theta_j，以非负增量参数化：第一项至少 theta_floor，
其余增量>=0；各增量有显式上限。theta 轴包含 `(0,0)`，按第 4 节单调 C1 插值。
成功证书记录参数盒边距、theta 增量、插值规则/系数及逐段导数下界、翼部斜率界，
方法名 `SSVI_GammaHalf_MonotoneTheta_v1`。

数学保证域是所有 `k∈R,t>0`（含短期/长期及两翼），在确定性比例 carry、
连续欧洲式 normalized call 的模型假设下保证 butterfly 与 fixed-k calendar 条件。
日历条件针对固定 K/F 的 normalized call，不要求有 carry 时固定 K 的原始贴现价格
逐期限递增。保证不是动态套利保证、真实市场完美拟合或局部波动率网格重定价保证。
double 实现仍有范围/舍入边界；参数和系数须按充分条件保守核验，采样 g(k,t) 只作
附加诊断，不能补救证书失败或代替全域证明。

## 4. 期限插值、外推与稳定导数

theta 使用单调 PCHIP：节点为 origin 和拟合 theta_j。`h_j=t_(j+1)-t_j`，
`d_j=(theta_(j+1)-theta_j)/h_j`。相邻 secant 有一个为 0 时内部节点斜率为 0；
否则 `m_j=(w1+w2)/(w1/d_(j-1)+w2/d_j)`，其中
`w1=2*h_j+h_(j-1),w2=h_j+2*h_(j-1)`。端点使用三点单边估计并截至
`[0,3*d_endpoint]`；仅一个正期限时两端斜率均为 secant。
相邻节点用 cubic Hermite 基函数及这些斜率组合。逐段 theta' 是二次式，检查端点
及位于段内的极值点下界，不靠采样检查单调性。规则参照
[PCHIP 官方定义](https://docs.scipy.org/doc/scipy/reference/generated/scipy.interpolate.PchipInterpolator.html)，
不引入 SciPy 运行依赖或任意 cubic spline 替换。

origin 到第一期限使用同一 Hermite 段；`t>t_last` 使用
`theta_last+m_last*(t-t_last)`，m_last 非负，可为 0。theta 为 C1，theta'' 可在 knot
跳变；零增量/斜率限制器切换会影响参数风险，不宣称 C2 或拟合处处可微。
wing 永远使用 SSVI 本身，不线性插值 IV、不 clamp 到最近 strike：
斜率为 `beta_±(t)=theta*phi*(1±rho)/2`，上述参数盒保证其严格低于 2。

`MarketIvPointResult_` 持有 w、w_k、w_kk、固定 k 的 w_t、IV、贴现 call/put、
g、可空 local variance、数值状态及单位。导数直接解析计算（或等价 double jet），
禁止在新路径中复用旧价格中心差分。theta knot 的一阶导数连续；carry knot 的
固定 K 时间导数取右侧，结果标出 `RIGHT_CARRY_DERIVATIVE`；跨 knot 的中心差分
不能当作这个单边导数的 oracle。

由 k=log(K/F(t)) 进行 chain rule，必须区别固定 k 的 w_t 与固定 K 的时间导数。
若 u=w(k(K,t),t)，`u_T=w_t-(r-q)*w_k`，`u_K=w_k/K`，
`u_KK=(w_kk-w_k)/K²`；价格导数再包含 D/F 的变化。
所有 sqrt 使用稳定 hypot/尺度化形式，Black tail 采用稳定差值/log 形式；不通过
负方差 floor、有限差分步长或尾部复制值掩盖数值失败。

由 normalized Black 价格和 chain rule 得到新 Dupire 路径：

```text
g = (1-k*w_k/(2*w))^2 - (w_k^2/4)*(1/w+1/4) + w_kk/2
local_variance(K,t) = w_t(k,t)/g(k,t)
```

这里 w_t 固定 k；与现行贴现价格公式
`2*(C_T+q*C+(r-q)*K*C_K)/(K²*C_KK)` 对照，两式相等而非不同 carry 约定。
暴露两种导数坐标及 analytic price derivative getter，标明价格/年、价格/strike、
价格/strike²。超出 double 范围/导数下溢时字段为空并报告具体失败，不能写 NaN 或
伪造为 0；价格/IV 可用性与 local-vol 可用性分开。w_t=0 且 g 合格可得零 local vol；
g<density_factor_floor、w 过小或数值取消使条件不可核验时，local-vol 字段不可用。
这不推翻解析静态条件；它说明当前查询的 double Dupire 计算不可认证。
逐类状态为 `value_status`、`derivative_status`、`dupire_status`，取 SUCCESS /
NUMERIC_RANGE / SINGULAR_DENSITY 等明确值；各类失败只使对应数值为空。
请求了某类 output 的 refit/导出入口必须检查该类状态，不能拿仍可用的价格当成 local vol。

## 5. 拟合、快照、失败与 C++ 表面

核心入口（均为拟议 API）：

```cpp
MarketIvMarket_ NewFlatMarketIvMarket(const Date_& evaluationDate, double spot,
    double rate, double dividendYield, const Vector_<Date_>& expiries);
MarketIvFitResult_ FitMarketIvSurface(const MarketIvFitRequest_& request);
MarketIvPointResult_ EvaluateMarketIvSurface(
    const MarketIvCalibrationSnapshot_& snapshot, const MarketIvQuery_& query,
    const MarketIvEvaluationSettings_& settings = {});
MarketIvFitResult_ RefitMarketIvSurface(
    const MarketIvCalibrationSnapshot_& base, const Vector_<MarketIvQuote_>& quotes);
MarketIvDupireResult_ CalibrateMarketIvDupire(
    const MarketIvCalibrationSnapshot_& snapshot, const MarketIvDupireRequest_& request);
MarketIvRefitRiskResult_ EvaluateMarketIvRefitRisk(
    const MarketIvCalibrationSnapshot_& snapshot, const MarketIvRefitRiskRequest_& request);
Handle_<AAD::IVS_> MarketIvSurfaceAsLegacyIVS(const MarketIvCalibrationSnapshot_& snapshot);
```

`MarketIvFitRequest_` 必需 market/quotes，settings 使用下节默认值。
`MarketIvQuery_` 的两个必需字段是正 strike 和估值日起的正 time（年）；
`MarketIvEvaluationSettings_` 统一持有第 7 节三项查询条件阈值。
Dupire/refit request 的 settings 复用同一 evaluation 配置，不维护另一组默认值。
Quote/market/query 是有校验的复制值，必需字段没有有效市场默认值。
结果/快照成员私有，getter 为纯只读；快照共享 `shared_ptr<const Data_>` 或等价 owning
存储，持有完整原请求、F/D/carry 模式、规范轴、参数/插值系数、证书、每行残差、原单位
和校准 provenance。释放输入、原结果或 Excel 上游 handle 后仍可用，无借用 IVS 指针。
Python 容器/嵌套可变值返回副本；C++ 引用遵循所属对象寿命，Excel getter 返回副本。

固定四个顺序启动：先 eta=0/rho=0 的 flat-smile 子模型，再 rho=-0.5/0/+0.5、eta=0.5
的完整模型；各 theta 初值为 initial_volatility²*t，按增量盒投影并记录投影。
采用带阻尼的 box-constrained Gauss–Newton，解析模型 Jacobian，原参数坐标的 projected
gradient 为收敛检查。已有 `BoundedGaussNewton` 可复用思路，但必须为此入口补预算计数、
证书与质量准入；其任意 `converged_` 标记不能替代这些条件。

具体检查使用 residual `r_i=sqrt(weight_i/sum(weight))*(c_i-c_mid)/s_i`、
模型 Jacobian A、`g=A^T*r`。在下界且 g_j>0、或上界且 g_j<0 的 blocked 分量置 0，
其余保留；要求 `max|g_free| <= projected_gradient_tolerance *
max(1,max_j||A_col_j||2) * max(1,||r||2)`。flat 启动只检查其 m 个自由参数。
参与预算的最终重算复用各候选已缓存的价格残差，证书或 band 检查不得额外漏计模型评价。

全部启动在预算内依次尝试；在已收敛、证书通过、所有拟合值处于可接受带加
feasibility tolerance 的候选中取最小 J。目标差<=objective_tie_tolerance 时按固定启动
次序选，记录 near-tie；flat 子模型若胜出则 rho=0/eta=0，按有效参数维数计算 rank。
不保证全局最优或参数唯一；只达到 step tolerance、NoDescent 或 MaxIterations 不算收敛。
低 rank/稀疏数据可产生有效有证书的曲面，但必须标 `UNDERIDENTIFIED`；它不是数据外
smile 已可靠识别。没有候选满足质量时返回 `FIT_QUALITY_FAILURE`，不是宣称数学全局不可行。

结构/类型/日期/预算预检错误抛 `InvalidMarketIvRequest` / `MarketIvBudgetExceeded`，
包含字段路径、quote ID/行号、实测值及约束，不创建虚假成功快照。
开始拟合后的数值失败返回 owning result，状态为 `NO_CONVERGENCE`、
`FIT_QUALITY_FAILURE`、`CONSTRAINT_FAILURE`、`NUMERIC_RANGE` 或 `WORK_LIMIT`；
有最后候选及失败诊断，但 `Snapshot()`/可用于定价的 surface getter 抛错。
成功是 `SUCCESS` 或 `SUCCESS_WITH_WARNINGS`，只有成功才提供快照。
预算耗尽即使已有候选也不得绕过固定搜索政策返回成功。

诊断包含四次启动的终止原因/迭代/目标/梯度、约束余量、rank 与 condition estimate、
原生单位和 normalized residual、是否在 bid/ask、max band violation、证书域及误差状态；
rank/condition 不冒充统计置信区间。getter 及 Methods 明确 `ConstrainedSSVIFit`，
不是旧 `NativeAADCalibrationVJP`。不读写 global date/fixing/tape；native 执行不回调 Python。

## 6. 默认值、预算与可用规模

所有设置在 C++ 默认值、Python keyword-only 和 Excel 规范键中一致。新枚举经 Machinist
生成，绑定使用规范字符串；拒绝未知键、枚举整数、bool 充当数值及窄化溢出。

| Fit 配置键                                            | 默认                 | 约束/语义                             |
|----------------------------------------------------|--------------------|-----------------------------------|
| `rho_limit`                                        | `0.999`            | `(0,1)`；eta_max 由此及 margin 推导     |
| `constraint_margin`                                | `1e-8`             | `(0,1)`；是解析参数余量，不是拟合误差            |
| `theta_floor`                                      | `1e-10`            | 第一 theta 增量严格正的下界                 |
| `max_variance_increment`                           | `100`              | 有限，>=theta_floor；其余增量范围 `[0,max]` |
| `initial_volatility`                               | `0.20`             | 有限正年化小数；初值不是固定市场参数                |
| `projected_gradient_tolerance`                     | `1e-8`             | 正数；按已归一化目标/Jacobian 尺度判断          |
| `parameter_step_tolerance`                         | `1e-10`            | 触发停滞诊断，不单独准入成功                    |
| `objective_tie_tolerance`                          | `1e-12`            | 非负；确定性候选选择                        |
| `normalized_price_tolerance`                       | `1e-4`             | 无 bid/ask 价格报价的拟合半带宽              |
| `decimal_iv_tolerance`                             | `5e-4`             | 无 bid/ask IV 报价的拟合半带宽             |
| `normalized_price_scale_floor`                     | `1e-4`             | 正数，避免零 spread 的无限权重               |
| `feasibility_tolerance`                            | `1e-10`            | normalized price 的带宽检查余量，不放松解析参数盒 |
| `max_iterations_per_start`                         | `200`              | 正整数；四次启动各自上限                      |
| `max_line_search_trials`                           | `16`               | 正整数；拒绝步亦计预算                       |
| `max_model_row_evaluations`                        | `5000000`          | 非负整数；每个预测报价行/模型 Jacobian 行评价计一次   |
| `max_quotes` / `max_expiries` / `max_market_terms` | `10000 / 64 / 256` | 输入规模上限，非必须填满的数组                   |
| `numeric_payload_budget_bytes`                     | `67108864`         | 64 MiB；保留的原生数值 payload            |
| `workspace_budget_bytes`                           | `268435456`        | 256 MiB；同时活跃的原生数值工作区              |

补充规则：theta_floor 和所有数值容差须有限，比例/精度容差在 `(0,1]`，tie 可为 0；
theta_floor<=max_variance_increment。所有计数先检查 `int`/`size_t` 可表示性及乘加溢出，
预算 0 不等于无限。每个模型行在计算前扣预算，Jacobian、失败试探及最终重算都计入；
原报价最多三次 Black 转换的准备计数另报。没有无限重启、自动提高容差或隐式加参数。

容量预检的保守数值包络（包括布局 padding，需实现证明）：Q=报价数、m=拟合期限数、
C=market term 数、p=m+2、H=4*(max_iterations_per_start+1)。

```text
P_fit = 8192 + 512*Q + 256*C + 256*(m+1) + 128*H + 64*p
W_fit = 8192 + 32*Q*p + 128*Q + 128*p*p + 256*C + 128*H
```

P 包括原报价、规范报价/尺度/每行诊断、market/theta/插值/证书、启动历史和固定元数据；
W 允许四个 Q×p double 矩阵、16 个 Q 长度 double 向量、16 个 p×p 数值矩阵及辅助存储。
不得隐含保留每次迭代的完整 Jacobian。预检使用包络，实际计数按 `sizeof*capacity`，
禁止超出计划容量的几何增长；不足在分配前拒绝，相等允许。字符串、allocator 元数据、
语言包装与 Excel 单元格不计入，两项不是 RSS 限制；分配失败仍可能抛系统异常。

常见 Q=300,m=C=12 的每次拟合，即使四次启动每次 200 轮、每轮一个 Jacobian 和
16 个试探，模型行上界 `4*(1+200*17)*300+2*300=4081800 < 5000000`。
这包括最终 residual/Jacobian 重算；内存按上述完整公式核对，非只计算参数向量。
该案例 `p=14,H=804,P_fit=272000,W_fit=312064` bytes，均低于默认预算。
输入规模上限 Q=10000,m=64,C=256 时，完整 `P_fit=5317504,W_fit=23134208` bytes
仍可预检通过；非线性工作量另受 work budget 约束，不能把 memory 合格当作准入完成。
最大允许输入不保证默认 work budget 足够；必须记录确定性 `WORK_LIMIT`，不能据
输入数量合格承诺每次非线性拟合成功。

## 7. Dupire 接入与旧系统兼容

`MarketIvDupireRequest_` 必需严格递增、正的 `spots_`、`times_`；time 为年。
直接采样显式网格，不调用旧 FillData、不加入未请求的时间层或 ATM 稳定带，也不做
tail flat fill。默认 `density_factor_floor=1e-10`、`min_query_time=1e-8` 年、
`min_total_variance=1e-12`、`max_grid_points=100000`；阈值有限且正。
每点记录 w、w_t、g、local variance/vol、范围/条件状态；任一点失败则 result 不提供
完整 SurfaceData，不用最后成功值填空。零 w_t 的零 vol 合法。

成功 `MarketIvDupireResult_` 持有原 calibration snapshot、市场/provenance、全部节点诊断，
并可导出原 `LocalVolSurfaceData_`，矩阵仍是 `[spot_index,time_index]`。
原 archive v1、log-spot/时间插值和 flat 边界外推含义不变；离散网格和 Hybrid 模型
不是连续 SSVI 曲面本身，不继承其静态套利证书或精确重定价承诺。

设 G=Ns*Nt，导出包络为
`P_dupire=P_fit+8192+512*G+16*(Ns+Nt)`，
`W_dupire=16384+2048*m+64*(Ns+Nt)`；按逐点流式计算，不复制完整历史面。
默认仍使用 64/256 MiB，G 上限和完整字节预算同时检查，不能仅检查点数。
即使以上最大 fit 规模加 G=100000、Ns+Nt=100001，完整
`P_dupire=58125712,W_dupire=6547520` bytes 仍在默认预算内；不是逐点残差的内存下界。

`MarketIvSurfaceAsLegacyIVS` 只接受显式 FLAT_CARRY 快照，返回 owning 的 IVS 子类，
其 spot/r/q 和 ImpliedVol 与新曲面一致；旧 Call/LocalVol 仍走原方法。
通用 F/D 曲线适配请求明确拒绝，不能用平均 rate/q 冒充同一市场。
Excel 可新增 **独立** `MarketIvLegacyDupireCalibration_New(name,snapshot,inputs)`，
内部使用 flat adapter 调旧 calibration，仍返回旧 spread-risk calibration 类型；
不要求旧 Excel BS/Merton 分派自动接受一个未知 handle，不改变已有函数契约。
该桥的风险轴依然是固定拟合快照上的 spreads，不能标成 market refit risk。

通用 F/D 的解析 Dupire 网格可导出，但当前 flat-rate Hybrid 便利构造不能自动表示
非平坦 r/q；首版便利模型接入仅允许 FLAT_CARRY。nonflat 用户须在外部配置一致的
确定性 drift 模型，不能把网格附到不匹配 carry 后宣称市场价格已复现。
新快照是内存 owning 值，首版不承诺可归档整个校准结果；如后续增加 SSVI archive，
使用独立类型/版本，绝不借旧 LocalVolSurfaceData v1 塞入报价或 SSVI 参数。

## 8. 完整 refit 报价风险与不可微边界

`RefitMarketIvSurface` 冻结原 market、quote ID/expiry/K/type/weight 和所有设置，
替换同轴原始报价数值（mid/bid/ask），重新转换尺度、重做四次启动、重算约束/插值/证书。
不是在最终参数上加 spread，也不复用旧最优解当隐含 warm start。

`MarketIvRefitRiskRequest_` 必需非空 quoteIds 和 outputs；每个 output 是
query(K,time) 加 quantity（TOTAL_VARIANCE/IMPLIED_VOL/CALL_PRICE/PUT_PRICE/LOCAL_VOL）。
默认最多 32 个报价、128 个 output；按原 ID 顺序输出，未知/重复 ID 拒绝。
可选 steps 必须与报价数相同且正：默认 IV 为 `1e-4`，PRICE 为该 expiry 的
`1e-4*D*F` 原始价格单位。它们是有限 bump 大小，不是百分比或参数导数。

每个方向将选中报价的 mid/bid/ask 一起加减 h，保留原始单位的 spread 宽度；无 band
则重新生成容差带，IV 转价格的尺度也重新计算。F/D/carry、其他报价、权重保持。
两边从原始基线独立完整 refit，再重估所有 output；LOCAL_VOL 重新计算导数/Dupire。
非法 bump、不合格拟合或输出失败均记录到该行，不自动转单边差分。
每个 h 必须有限且使 mid 及所有非空 bid/ask 可表示地发生变化，否则为 BUMP_INVALID。

返回 f0、f+、f-、`(f+-f-)/(2h)`、两侧斜率、原始 bump/输入与输出单位、两边终止原因、
证书/边界变化、启动选择和 rank 信息。方法 `FullRefitSymmetricBump`，列名为
`symmetric_bump_slope`，不能冒充 analytic gradient。任何一侧失败时该行坡度为空，
其他已完成行可读，但整体 `complete=false`，不能汇总成完整风险。

单调增量活跃集、eta/rho 边界、PCHIP limiter、选中的启动/flat 子模型、报价可接受带、
rank 或 near-tie 状态改变时，标 `REGIME_CHANGED`；即使未改变也不认证可微。
记录两侧斜率差及 h/2 对照计划，不用一个任意阈值把不可微“修正”为光滑 Greek。
rho 在本矩形参数盒中无需 abs(rho) 的优化器链式求导，避免人为引入 rho=0 参数化折点。

aggregate work budget 默认是检查溢出后的 `2*A*base.max_model_row_evaluations`，可显式降低；
每个 refit 仍受原 per-fit 预算。顺序执行，提取输出后释放 bump 快照，不保留 2A 份全量
校准。保留预算 `P_risk=P_fit+8192+512*A+128*A*O+256*O`，
工作区 `W_risk=W_fit+2*P_fit+1024*O`，覆盖一个活动 bump 和输入/输出暂存；
默认仍 64/256 MiB，执行前按 A/O/Q/m/C 核对。每个 output 评价和两个拟合的计数另报。
没有 UDF/Python callback 或无限 refit；预算耗尽的未执行行明确标出且 complete=false。

Risk settings 的规范键为 `max_selected_quotes=32`、`max_outputs=128`、
`max_total_model_row_evaluations=空`（按上式解析）和同名 payload/workspace 预算，
并复用 evaluation 配置。Q=300,m=C=12,A=8,O=25 时完整
`P_risk=316288,W_risk=881664` bytes，aggregate row budget=80000000；即使各 refit
达到上述代表性最坏行数，16 次拟合也不触及这项上限。
Q=10000,m=64,C=256,A=32,O=128 时完整
`P_risk=5899136,W_risk=33900288` bytes，内存仍在默认内，实际 work cap 仍单独强制执行。

本原生入口首版覆盖曲面/价格/局部波动率节点响应。交易/组合 quote bump 的完整路线是
同一 Refit→同一显式 Dupire 网格→重建模型→同配置全重估；MC 对照使用相同随机数并
控制采样误差，直接报价依赖单独重估。不新增通用交易回调，也不把现有
`DupireQuoteRisk_`、script curvature/VJP 改标签成这条风险链。

## 9. Python／Excel 表面、错误和典型用法

Python 的 quote/settings 全部 keyword-only，market/query 的必需参数在前：

```python
MarketIvTerm_(expiry, forward, discount)
MarketIvMarket_(evaluation_date, spot, terms)
MarketIvFlatMarket_New(evaluation_date, spot, rate, dividend_yield, expiries)
MarketIvQuote_(*, id, expiry, strike, option_type, quote_type, mid,
               bid=None, ask=None, weight=1.0)
MarketIvQuery_(strike, time)
MarketIvFitRequest_(market, quotes, *, settings=None)
MarketIvFitResult_New(request)
MarketIvPointResult_New(snapshot, query, *, settings=None)
MarketIvDupireResult_New(snapshot, request)
MarketIvRefitRiskResult_New(snapshot, request)
```

日期只接受 `Date_`，数值拒绝 bool/enum/数字文本，计数须整数且先检查范围。
enum 接受对应枚举或规范字符串，不接受其他枚举。getter 为 snake_case，只读与复制；
status/method 为规范字符串，缺省数值为 None。输入先在 GIL 下拷贝/校验，native
拟合/批量查询/导出/refit 时释放 GIL，不调用 Python 的旧 IVS override。
失败 result 可读诊断，snapshot getter 明确抛异常；不返回空句柄冒充成功。

Excel 新入口使用 `_New` / `_Get_*`，Machinist 生成点分大写名：

```text
MarketIvMarket_New(name,evaluation_date,spot,terms)
MarketIvFlatMarket_New(name,evaluation_date,spot,rate,dividend_yield,expiries)
MarketIvFitSettings_New(name,[settings])
MarketIvEvaluationSettings_New(name,[settings])
MarketIvDupireSettings_New(name,[settings])
MarketIvRefitRiskSettings_New(name,[settings])
MarketIvFitRequest_New(name,market,quotes,[settings])
MarketIvFitResult_New(name,request)
MarketIvQuery_New(name,strike,time)
MarketIvPointResult_New(name,snapshot,query,[settings])
MarketIvDupireRequest_New(name,spots,times,[settings])
MarketIvDupireResult_New(name,snapshot,request)
MarketIvRefitRiskRequest_New(name,quote_ids,outputs,[settings],[steps])
MarketIvRefitRiskResult_New(name,snapshot,request)
```

terms 为三列无表头 Expiry/Forward/Discount；quotes 为九列无表头
ID/Expiry/Strike/OptionType/QuoteType/Mid/Bid/Ask/Weight，最后三列可按规则留空，
weight 空为 1，bid/ask 必须同为空。outputs 为 Strike/Time/Quantity 三列。
settings 两列 key/value，整块空（含 2×3、多行单列空区域）为默认，半空/未知/重复键
或非空坏形状拒绝，不能在 XLL 前置 validator 提前拒绝合法的全空默认。
日期须整数有效 Excel serial，普通数值拒绝日期/bool/文本，xltypeInt 正确读取 val.w。
quote ID 的 NUL、Excel 错误格和越界整数给出表格/行/键定位。

结果 getter 提供 Request/Snapshot/Summary/Quotes/Parameters/Constraints/Iterations/
Execution/Failure；snapshot getter 提供 Market/Terms/Theta/Certificate；Dupire getter
提供 Surface/Nodes/Execution/Failure；refit getter 提供 Axis/Responses/Execution/Failures。
均按原输入 ID 稳定排序、有表头及单位，未计算项留空而非 0。失败的价格/snapshot
读取使用既有 `Excel::Error`（`#Error: ...` 文本），不修改全局错误机制。
超过 Excel 行列上限时 getter 报错，不截断；原生数值预算不包含 worksheet 单元格。
句柄操作名只用于 repository，不影响数学结果。

拟议 Python happy path（非基线已可运行 API）：

```python
d0, d1, d2 = dal.Date_(2026, 10, 19), dal.Date_(2027, 4, 19), dal.Date_(2027, 10, 19)
market = dal.MarketIvFlatMarket_New(d0, 100.0, 0.03, 0.01, [d1, d2])
quotes = [dal.MarketIvQuote_(id=f"{j}-{k}", expiry=d, strike=k,
          option_type="CALL", quote_type="DECIMAL_IV", mid=0.20)
          for j, d in enumerate([d1, d2]) for k in [90.0, 100.0, 110.0]]
fit = dal.MarketIvFitResult_New(dal.MarketIvFitRequest_(market, quotes))
if fit.is_success:
    point = dal.MarketIvPointResult_New(fit.snapshot, dal.MarketIvQuery_(100.0, 1.0))
    print(point.implied_vol)
```

C++ 同样构建 market、quote vector 和单个 request，调用 `FitMarketIvSurface`，检查
`IsSuccess()` 后取 `Snapshot()`；不设计七八个位置参数的万能拟合函数。
示例归属遵守 [component boundaries](../../references/component-boundaries.md)：core 演示
使用 core 头/target，现有 `dal-cpp/examples/products/uoc/` 和 models/methods 程序位置、
目的和 target 不迁移；如后续增加 facade 演示，另建 `dal-public/examples/market_iv_surface/`。
本阶段不新增程序/CMake、不作新 standalone build 声称；若实施涉及示例/构建，须在
实施 head 下补 public=OFF core 构建及相关 facade 独立验证，不能借 P3 旧基线证据代替。

## 10. 独立验收与预算核对计划

- **平坦 BS**：flat carry 和非平坦 F/D 两种市场，多个期限/strike，eta=0、
  theta=sigma²*t；验证 w、IV、price、解析导数、g=1、local vol=sigma，非零 r/q 下
  同时对照贴现价格 Dupire 式。报价行恢复不等于拟合参数唯一。
- **合成恢复**：从独立 SSVI 实现生成 rho=-0.4、eta=0.8、theta_j=0.04*T_j 等满足盒约束
  的报价，混合 price/IV、put/call 和非零 spread。比较报价/未参与拟合点的价格与导数；
  只有识别性足够时才比较参数，不要求所有 seed 逐位恢复同一 rho/eta。
- **全域条件及反例**：独立验证参数边距、每段 theta' 的解析最小值、eta=0 闭包、
  rho 两端、短/长期和两翼斜率。修改成普通 IV 插值、逆序 theta、越界 eta 或断掉
  tail 证明必须被证书检查拒绝。密集/随机 g 采样是补充，不能作为证书。
- **导数独立性**：使用独立高精度（如 80 位）价格/总方差实现，对不同步长的高阶差分
  核对 w_k/w_kk/w_t、固定 K 价格导数和 normalized Dupire 对照；覆盖 ATM、远翼、
  极短期、eta≈0、rho 近界及 theta/carry knots 的左右语义。验收前固定绝对/相对误差
  预算，导数强相消区按高精度误差界决定可用范围，不能只比两个同源 jet 路径。
- **坏/稀疏报价**：仅一期限/少数 strike 的低 rank、重复 ID/经济报价、单边 bid/ask、
  crossed band、违反价格界/put-call、相互矛盾 bid/ask、无可接受 SSVI fit、局部停滞、
  max iterations、范围损失和极端权重。确认诊断/状态区分“优化失败/模型不够灵活”与
  “已证明报价矛盾”；无成功快照，禁止丢点后继续。
- **完整 refit bump**：独立把原始 mid/bid/ask 改动后重新拟合/导出并比较全部输出，
  使用 h、h/2、h/4 检查两侧斜率和敏感度稳定性。固定 base+spread 的旧路线在有
  活跃约束/多期限拟合的案例须产生可区分响应，防止误接旧 VJP 后 parity 仍通过。
  主动构造 theta 增量、flat 模型选择和 limiter 切换，验证 REGIME_CHANGED/失败行。
- **Dupire/模型**：精确请求网格、无隐藏补点/平滑/尾部复制，矩阵方向和 archive v1
  round-trip 不变；非平坦市场 legacy adapter 拒绝。flat 情形用独立解析/价格差分和
  网格/时间收敛重定价，报告离散化误差，不把 SSVI 证书当成模拟重定价证明。
- **绑定/生命周期**：同一请求三绑定的默认、解析轴、数值/证书/失败一致；原生 Windows
  OPER/XLL 前置类型检查与注册必须实际验证，portable 不替代。销毁输入/复制结果、
  并发市场/日期和非空 AAD tape 隔离；旧 BS/Merton/custom IVS、spread VJP/curvature
  及 LocalVolSurfaceData archive 回归兼容。
- **预算**：完整 P/W/工作计数、少 1 byte 预检、每启动/试探/失败行计账、aggregate refit
  预算；布局/capacity 和实际峰值需实施证明。典型 300 报价/12 期限、稀疏单期限、
  代表大规模和八轴 refit 是最小性能路径，不运行泛化参数笛卡尔积。

目标报价/导数容差与高精度 oracle 的可达带宽由下一轮数值风险审查固定；上述默认
fit bands 是产品准入政策，不是验收者可以事后放宽的误差阈值。测试须能击穿漏约束、
错单位、错坐标、错误 carry 和误接旧 spread 风险，三绑定相等不能单独证明金融正确。

## 11. 取舍、真实未决风险与本轮交接

选择修正 power-law 的解析充分条件而非 sampled no-arbitrage 检查；公共 rho、固定
gamma 和保守 eta 盒限制拟合灵活性，但使初版边界、外推及失败解释清楚。
拒绝 raw SVI 每期限独立拟合后再做任意 IV 插值、无预算的多启动、隐式报价修复、
给旧 IVS 增加破坏 ABI/行为的纯虚导数方法，以及对受约束 refit 直接贴 AAD gradient 标签。

需下一阶段定向解决的事项：

1. 证明固定族/参数盒与 PCHIP 在全域及舍入余量下的约束认证，审查保守 eta 盒是否满足
   代表市场需求；报价不合格应保留失败，不把放松约束当作自动修复。
2. 独立验证解析导数、k/K/time chain rule、极端 tail/短期数值可用域及 g 阈值；
   C1 theta 不解决 carry knot 的单边时间导数或局部波动率网格误差。
3. 四启动局部拟合的收敛、rank/near-tie 语义及预算布局；box Gauss–Newton 的条件数
   和低 vega 数据需要实测，预算公式通过不代表校准质量通过。
4. 活跃集/limiter/模型选择导致的有限 bump 非光滑性；先交付完整 refit 响应和诊断，
   暂无“处处唯一、可微”的承诺，也不自动交付组合 AAD 风险。
5. nonflat carry 到既有 flat Hybrid 的模型接入范围及真实 Windows 绑定仍须明确验证；
   导出旧格式网格不使其带上新市场/静态证书，snapshot 归档另需独立版本设计。

本轮仅做契约、数学条件、引用、预算算式、链接和改动范围自检；没有运行拟合器、
产品测试或构建，没有修改产品代码、现有方法文档、示例/生成注册或创建 PR。
交付为本记录与 `.codex/artifacts/README.md`；父任务保持 in_progress，后续由
orchestrator 安排约束/导数审查→实现与定向验证→代码评审及必要文档，不在本轮触发角色。
