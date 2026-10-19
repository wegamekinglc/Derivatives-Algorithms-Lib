# DAL-379 P2：市场 IV 曲面数值风险审查

**发现：所选 SSVI 族的连续数学充分条件、固定 k/K 导数及非平 carry 的 Dupire 桥接相容，代表规模预算算式一致；未发现必须先推翻或重写的数值契约。舍入证书、报价相消和完整 refit 的分支诊断仍须按下列具体探针验收。**

**Verdict: Proceed with caveats**

审查对象：[API 记录](../api-notes/dal-379-market-iv-surface.md) 全文 539 行；下文行号均指该版本。
基线 `4d4760d1b33c77a2d00fd30a2fa6e0299d24fc5c`，分支
`agent/dal-orchestrator/7bc6ada900c9`。API SHA-256：
`165b37a27e860d66bac586f8030972e3fdf779df029cf9955bac7b1d04c0a1ac`。
本记录控制接下来的定向实现验收，不代表拟合器、产品价格、实际布局或 Windows 绑定已验证。

## 阻断项与实施前修订

本轮无有证据的设计 blocker，无需先增加模型族、重复 spec 或重开全量设计审查。
原稿已要求保守证书、数值失败隔离、所有报价带准入及实际布局证明；这些要求可以直接实施。
下面的反例针对可能的错误实现，不声称尚不存在的产品代码已经犯错，也不把实现验证尚未完成作为 blocker。
若实现选择放宽解析约束、改变四启动搜索、静默丢点/填值或更改预算定义，则须先修订唯一 API 记录。

## 重要关注项与定向准入

### S1：全域充分条件成立；证书必须覆盖舍入后的实际表示

证据：API `:104-148`、`:152-166`；[Gatheral–Jacquier 原论文](https://arxiv.org/pdf/1204.0646)
Theorems 4.1/4.2、Corollary 4.1、Remark 4.4。修正 power-law 不能换回普通 `eta/sqrt(theta)` 后沿用长期证书。

对 eta>0，直接求导得到

```text
theta*phi = eta*sqrt(theta/(1+theta))
[(theta*phi)']/phi = 1/[2*(1+theta)]
theta*phi^2 = eta^2/(1+theta)
```

calendar 上界系数 `(1+sqrt(1-rho^2))/rho^2 >= 1`，而左侧比值不超过 1/2；rho=0 用无穷上界。
固定 rho_limit 的 eta 盒使 `theta*phi^2*(1+abs(rho)) <= 4*(1-margin)`，
另一项对全部 theta>0 的上界是 `2*sqrt((1-margin)*(1+rho_limit)) < 2*sqrt(2) < 4`。
故任意短期、长期及两翼均满足所引用的充分条件，无须依赖 strike/time 采样。
eta=0 单独定义 w=theta、rho=0，绕过上述除以 phi 的比值即可；不必排除平坦 BS 闭包。

单调 PCHIP、origin=(0,0) 和末端非负线性斜率给出 theta 的 C1、非减延伸；第一正节点为正，
第一 Hermite 段在 t>0 亦须保持正。末端斜率为 0 的长期平台合法，可产生零局部方差。
短端 theta→0 时用极限论证，不能实际在 origin 计算无穷 phi。

**舍入反例：**合法节点 `t=[0,1,2], theta=[0,.04,.04]` 的首段端点斜率是 `.06,0`。
若天真展开 Hermite 为幂系数，double 计算会出现：

```python
a1 = .06
a2 = 3*.04 - 2*.06
a3 = -2*.04 + .06
print(a1 + 2*a2 + 3*a3)  # -1.3877787807814457e-17，规范端点斜率应为 0
```

这说明不能以“PCHIP 理论单调”替代实际系数证书，也不能借 feasibility_tolerance 把负下界认证成非负。
实现可选择保持共享节点值/斜率的 Hermite 表示、形状保持的分解和带误差界的区间核验；
对 eta 上界取保守内侧值，对每段二次导数的端点/内部极值使用外包误差界。
应明确证书对应哪一组存储数据和评价公式；真正不可认证时按原约定失败，而非采样放行或 clip theta'。
上面平台节点、极小正增量、相邻日期、rho 近界及 eta 上界的 ±1 ulp 是必要探针。
连续数学证明与 double evaluator 的误差声明分开，不承诺每个浮点查询价格都严格满足逐位凸性。

### S2：报价归一化正确，但 put parity 的相消不能只检查 finite

证据：API `:70-100`。价格不先反解 IV、IV bid=0 取 intrinsic、每行独立 band 检查合理。
F/D 为唯一 carry 来源，不应另接受不一致的 r/q；call/put 同 strike 共存时比较的是整个 band 的 parity 可行性。

具体反例：`D=F=1,K=P=1e16`，PRICE put 是合法上界 `P=D*K`。
按精确实数，normalized call 为 1；顺序执行 `P/(D*F)+1-K/F` 在 double 得到 0，
所有中间量仍有限。此处 `(P-D*K)/(D*F)+1` 得到正确的 1，但该重排也不是所有 moneyness 的万能算法。
需按区间/尺度选稳定 parity 表达式，必要时用 FMA、补偿差或误差外包；
转换误差不能小于报价带/scale 所需精度时返回 NUMERIC_RANGE，而非制造新的规范报价再成功拟合。
把 bid/mid/ask、合法价格界和 call/put 交集一起验收；不得悄悄合并它们或倒置 band。

极端权重缩放可能使最小正权重下溢成 0；即使目标中其影响不可表示，仍不得跳过该行的独立 band 准入。
重复日期/ID/经济报价、全空 quotes/terms、单边 band、NaN/Inf 和乘积溢出应在数值执行前明确拒绝。
这里需要实现验证和定位明确的错误，不要求扩大首版金融范围。

### S3：固定 k/K 与解析 Dupire 的公式通过独立算式核对

证据：API `:168-196`、`:335-347`。固定 K 的 `u_T=w_t-(r-q)*w_k`，
不能直接放入 `local_variance=w_t/g`；后者必须使用固定 k 的导数。
贴现价格公式中的 q*C 与 (r-q)*K*C_K 不能省略。carry knot 两侧可有不同 C_T，
同一时点选定侧的 r/q 必须与该侧价格导数一起使用，不能用跨 knot 中心差分验证右导数。

本轮用独立 mpmath 1.3.0、80 位十进制精度作最小探针：

- smile 为 rho=-0.4、eta=0.8、theta=.04*t；k=-.5/0/.5；t=.2、.5 左侧、.5 右侧、.8。
- market 在 .5 前 r=.03,q=.01，之后 r=-.02,q=.015，F/D 积分连续；每侧用对应解析段延伸求导。
- 用高精度直接 Black 价格的固定 K/time 导数构造价格 Dupire，比对独立求导的 w_t/g。
  w 导数用 `z=phi*k+rho, H=sqrt(z*z+1-rho*rho)`，
  `w_k=theta*phi*(rho+z/H)/2`、`w_kk=theta*phi^2*(1-rho^2)/(2*H^3)`，
  w_t 再与高精度直接差分求导核对。

12 个点的最大缩放 w 导数差约 `5.28e-82`，两种 Dupire 差约 `4.43e-80`；
eta=0、theta=.04*t 的非平 carry BS 对照，局部方差相对 .04 的绝对差不超过 `1.64e-76`。
反之误把固定 K 的 u_T 用于 w_t/g，在同组点可造成约 **0.0075313** 的局部方差差异，具备区分力。
这是对设计公式的独立高精度算式核对，未调用或验证待实现的生产 evaluator，也不是全域证明。

建议将普通可用域的导数准入固定为：t>=1e-4、w>=1e-6、g>=1e-4、abs(k)<=.5，
检查无量纲向量
`[w,w_k,w_kk,t*w_t,C/(D*F),K*C_K/(D*F),K^2*C_KK/(D*F),t*C_T/(D*F)]`，
逐项误差不超过 `1e-11+1e-9*abs(reference)`；局部方差误差不超过
`1e-10+1e-8*abs(reference)`。oracle 自身的精度/步长误差应至少小两个数量级。
这些是验收目标，不是新产品可用域限制；远翼、极短期、低 density 另按高精度误差界决定返回值或不可用状态，
不得只因为 g 超过默认 floor 就给出无法认证的高精度比值。
解析真零与数值下溢要区分；平台的 w_t=0 不得被改成正 vol floor。

flat legacy adapter 的限制与现有 `IVS_::Call/LocalVol` 相容；后者仍是价格中心差分，
不应以相同精度逐位等同新解析入口。非平 carry 只导出网格，不自动套入 flat Hybrid 的决定应保留。

### S4：拟合质量与完整 refit 的准入独立于优化器的停止标志

证据：API `:228-258,349-395`。projected gradient、证书和逐行 band 三道门同时要求，
并拒绝用 StepStalled/NoDescent/MaxIterations 代替收敛，足以避免“优化器停了即成功”的契约错误。
所有四启动按预算执行完才选胜者，耗尽 work 即使已有候选仍失败，政策一致。
局部解、低 rank、保守 eta 盒无法拟合市场都不能标成已证明市场套利。

固定 seed 还需在整个合法设置域内可行：例如 rho_limit=.25、margin=.99 都合法，
但 eta_max≈.1788854382，原始三个完整模型 seed 的 eta=.5 全部越界，rho=±.5 也越界。
按声明的 box 约束将**全部 seed 分量**投影并记录，eta=0 再 canonical rho=0；
投影后重复的 seed 仍按四启动固定次序处理，不私自去重改变搜索政策或少计工作。
这是可行盒规则的直接实施要求，默认设置下没有该冲突，无需增加新的拟合方法。

refit 必须重建转换尺度、拟合、PCHIP/证书和要求的 output；不能只改最终 theta/rho 或走旧 spread VJP。
`REGIME_CHANGED` 应比较明确的离散分支签名：活跃 bound、limiter 分支、胜出模型/启动、rank、
near-tie 及 band 准入/接触状态；不能比较连续的 band 数值或证书余量是否改变，否则正常每次报价 bump 都报警。
该标记与数值失败不同：两侧有效时可返回有限 bump slope 并警告，不认证 infinitesimal Greek；
任一侧失败则坡度为空、complete=false，不自动单边回退。

必须有一个稳定、可识别、内部解案例，用独立驱动的 2A 次原始报价重拟合比对全部响应，
再加入零 theta 增量、eta=0 与 near-tie/limiter 切换案例，比较 h/h2/h4 和两侧斜率。
稳定分支直接重拟合输出可用 S3 数值门槛；坡度误差门槛由两次输出误差之和除以 2h 推出，
不可在 h 缩小时仍机械要求相同绝对坡度误差。切换案例验收诊断和有限响应，不要求收敛到唯一导数。
拟合质量 bands 不等于合成恢复精度；生成源在同一模型族且识别性充分时，先固定更紧的独立恢复目标，
避免只要求落入默认宽 band 就使参数/导数错误也通过。

### S5：默认 fit／网格／refit 预算算式自洽，真实布局仍待证明

证据：API `:291-312,328-333,377-390`；本轮逐项独立整数重算如下。

| 路径与规模 | Payload bytes | Workspace bytes | 工作核算 |
|---|---:|---:|---|
| fit Q=300,m=C=12 | 272000 | 312064 | 4081800 行 < 默认 5000000 |
| fit Q=10000,m=64,C=256 | 5317504 | 23134208 | 最坏 136060000 行，须受默认 work cap 截断 |
| 最大 fit + Dupire G=100000,Ns+Nt=100001 | 58125712 | 6547520 | 点数上限及逐点流式评价，计数另报 |
| 代表 refit A=8,O=25 | 316288 | 881664 | 16*4081800=65308800 < aggregate 80000000 |
| 最大 refit A=32,O=128 | 5899136 | 33900288 | aggregate 默认 320000000，单 fit 上限继续独立生效 |

全部列出的内存包络在 64/256 MiB 内；最大输入允许内存预检通过，不意味着可在默认 work 中完成四启动。
这一限制已明确为 WORK_LIMIT，没有把最大规模可用误说为默认拟合保证。
拟合的 17 次/轮行计数只有在解析 Jacobian、缓存候选残差和固定重算计划下成立；
不能调用旧 DifferenceJacobian 后仍沿用该行数。最终证书/梯度/rank 重算如触发新的模型行也必须计账。
Dupire/output 评价另计有数量上限，row budget 不是总 FLOPs 或墙钟超时。

实施须把原请求/规范报价、四启动诊断、暂存 Jacobian、实际 capacity、一个活动 bump 与 base 快照、
导出的 SurfaceData 矩阵和所有临时拷贝映射到包络；共享快照不能重复计入，也不能漏计活动 bump。
恰等字节上限、少 1 byte、每启动/拒绝试探/末次重算、per-fit 和 aggregate 交错耗尽都需定向验证。
目前仅证明算式可用，不证明实际 sizeof/capacity、峰值或性能已通过。

## 次要备注

- 在实现入口明确 quotes/terms 至少一个；无报价 carry knots 不得加入 theta 自由参数。
  query 和网格的重复/倒序时间、不可表示间隔与 size_t 窄化应有字段定位。
- UNDERIDENTIFIED 的 rank/condition、near-tie、band 状态阈值应固定并记录；
  不把 rank 低当作静态证书失败，也不把 rank 变化误称模型参数处处可微。
- 保留 Python GIL 前复制、native 不回调、C++/Excel owning 生命周期和非空 tape 隔离。
  Windows OPER/XLL 是新入口后续验证边界；P3 原生通过不认证 P2 新注册和转换。

## 较小可行方案

按现有公共 rho、固定 gamma、保守 eta 盒和单调 theta 实施；保留明确失败，不自动转 eSSVI/raw SVI、
不加动态套利/组合 AAD/非平 Hybrid 承诺。先做解析 evaluator/证书与小型 fit，再接精确 Dupire 网格和有限 refit。
S1–S4 的少量反例及普通域 oracle 已足以控制首批高风险；无需泛化构建或性能参数矩阵。

## 作者问题与交接

无阻断作者问题。建议把 seed 全分量投影、REGIME 的离散签名和验收误差门槛作为实现注记收敛到唯一 API 记录；
不要求再次请求用户批准既定 P2 范围。若实际市场拟合或舍入证书出现新失败，只按该证据定向修订。

已完整读 API，核对 `docs/models/dupire.md`、`dal-cpp/dal/model/ivs.hpp/.cpp`、`dupire.hpp/.cpp`、
`math/optimization/boundedgn.hpp`，以及 `tests/model/test_ivs.cpp`、相关 Dupire 与 GSR bounds/rank 测试。
旧 `test_dupire.cpp:285` 明确记录旧差分对 calendar 套利可能返回 NaN；新结果状态不能照搬该行为。
旧 `test_ivs.cpp` 的 flat/carry 对照是回归参考，不能代替 SSVI 新导数、非平 carry 或完整 refit oracle。
PCHIP 规则另核对 [SciPy 官方定义](https://docs.scipy.org/doc/scipy/reference/generated/scipy.interpolate.PchipInterpolator.html)，不引入其运行依赖。

本轮仅执行独立高精度算式、三个最小数值风险探针、预算重算及文档静态检查；
未运行生产拟合器、构建、产品测试或性能矩阵，未新增 core-build 声称，也未把 P3 旧基线证据用于认证 P2。
仅写本 critique 与索引，API 原稿和其他已有工作保留；父任务继续 in_progress，后续由 orchestrator 安排。
