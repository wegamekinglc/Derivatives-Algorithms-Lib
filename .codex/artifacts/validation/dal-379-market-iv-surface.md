# DAL-379 P2 独立定向验证

2026-10-19，dal-tester。结论：**存在一个生产数值 blocker，当前版本不能准入**。
合法的极端 PUT PRICE 在 `D*F` 为次正规数时，parity 转换丢失所需精度，仍成功发布 snapshot。
已保留最小 RED 测试，交回 orchestrator 安排 implementer；本轮没有修改产品实现。
父任务保持 `in_progress`。Windows P2 原生 OPER/XLL 仍无运行证据。

## 源码、配置和证据身份

- 分支 `agent/dal-orchestrator/7bc6ada900c9`，固定测试 head
  `0068d0526e5e1e13ecae007215bfbdb93b1b50ea`。
- 产品提交 `246ccd4ad5027caa0a3cb3799b73ee276a02d41b`；基线
  `4d4760d1b33c77a2d00fd30a2fa6e0299d24fc5c`。
- Linux x86-64 WSL2 / glibc 2.43，GCC 15.2.0，CMake 4.2.3，C++17，static
  Release `-O3 -DNDEBUG -ffp-contract=fast`，native AAD，Eigen/native arch OFF。
  Python 3.13.9，pybind11 3.0.4；新增运行 `DAL_NUM_THREADS=4`。
- 复用 `build/dal379-market-iv` 中原有库、绑定和测试二进制；没有重建或重跑已有效集合。
  standalone benchmark 的 `build/dal379-market-iv-core` 确为 `DAL_BUILD_PUBLIC=OFF`。
- 先读 active API、critique、implementation。API / critique SHA-256 分别为
  `165b37a27e860d66bac586f8030972e3fdf779df029cf9955bac7b1d04c0a1ac` /
  `68be175833c54a2b76dd4fcadd65c3689a0c0b3501d7786d8bc9be0ff8e7662e`，未修改。
- 从 Multica 附件 `01a151ff-b67e-76e4-ba6b-55caf3b4a1e1` 下载原始包，SHA-256
  `e117d7f52ab6c76eeb86f6f15ab6ba7a068172b7c4d6fe3f831cc5100c1388f4`。
  包内 65 个文件逐字节对应本地原证据；manifest 的 124 个源码哈希全部对应固定 head
  的 Git blob，7 个现存二进制哈希全部匹配。登记索引后复核时，原控制记录与固定 head
  的 blob 比较，构建日志仍与磁盘原件比较。
- Python 实际模块 `_dal.cpython-313-x86_64-linux-gnu.so` SHA-256
  `0ba2c1daa8f5da3ae630bf0d49983c42d61a90bbf76f55e9ab3b84a457303ec7`；
  探针直接链接 `libdal_cpp.a`，SHA-256
  `2e22eaec48e94c42dc65bf23580aa325fde6f0a2736d9fd083ad7ccff849ea63`。
  因而此失败不是安装目录旧模块或跨 DSO 身份混用。

原 manifest、独立 `provenance-audit.json`、日志、探针数据、补丁随验证证据包交付。
原 124 文件中仅 Python 测试和 artifact 索引因本轮新增用例/登记而改变；产品二进制和产品源码均未变。
记录及探针保留在当前 checkout，未创建分支、PR 或触发其他角色。

## 复用已有验证

以下是通过来源核验后复用的作者证据，**不是本轮重新执行**：

| 原始证据 | 已有结果及适用范围 |
|---|---|
| `build/market-iv-final-19-ctest.log` | 19/19，17 core + public owning/legacy + Excel portable；证书、非默认盒 seed 投影、质量/work/byte 边界、Dupire、AAD 隔离 |
| `build/market-iv-final-7-python.log` | 7/7，无 skip；原 80 位导数、h/h2/h4 raw refit、离散 regime 稳定/切换、正 IV `1e-300` 明确失败、owning/并发/None |
| `build/market-iv-complete-compat.log` | 10/10，旧 IVS / spread snapshot / LocalVol archive 的定向兼容 |
| `build/market-iv-final-benchmark-registered.log` | 1/1，standalone core `quote_risk_market_iv_perf` |
| `build/market-iv-samples-final/results.json` 和 20 个原始进程日志 | 两轮各十个进程，四条新 API 路径；仅 informational，没有同 API 基线回归门禁结论 |

原测试的极端整数 PUT `D=F=1,K=P=1e16` 有效，但没有覆盖本次发现的次正规尺度加小数 strike。
原非平 carry 高精度测试的 carry/theta 期限轴重合；原 raw refit 高精度对照没有双侧报价区间。
本轮仅补这两个覆盖缺口及转换边界，没有扩展旧集合或性能矩阵。

## 新增测试与结果

新增至 `dal-python/tests/test_market_iv_surface.py`，只执行以下最窄过滤：

```bash
DAL_NUM_THREADS=4 PYTHONPATH=build/dal379-market-iv/dal-python \
  /home/wegamekinglc/anaconda3/bin/python3 -m pytest -q -s \
  dal-python/tests/test_market_iv_surface.py \
  -k 'independent_nonflat_unquoted or independent_banded_mixed'
# 2 passed, 8 deselected, 0.15s

DAL_NUM_THREADS=4 PYTHONPATH=build/dal379-market-iv/dal-python \
  /home/wegamekinglc/anaconda3/bin/python3 -m pytest -q -s \
  dal-python/tests/test_market_iv_surface.py -k 'subnormal_df_price_conversion'
# 1 failed, 9 deselected, 0.06s — 当前真实生产 RED，未 xfail/skip
```

- `test_independent_nonflat_unquoted_carry_knot_and_extensions`：90 位 mpmath 从保存的
  theta 节点独立构造 PCHIP，再对直接 Black/SSVI 求导。carry 使用不同期限轴，额外节点
  273/365 的 F=104、D=.975 不含报价；覆盖该点左右相邻 double、报价节点、短期和期限尾延伸。
  在 carry 节点固定使用右区间延拓求导，避免中心差分混合左右导数。
  共 25 点，核对 w/wk/wkk、固定 k/K 的 t*w_t、归一化 C/K*C_K/K²*C_KK/t*C_t
  和 Dupire local variance。最大归一化误差 `3.80498e-12`；使用原 critique 的
  `1e-11+1e-9*abs(ref)`、local variance `1e-10+1e-8*abs(ref)` 门槛。
  参照不调用产品 Smile/Carry/导数函数；可揭露 theta/carry 混轴、固定 k/K 混用和节点导数取错侧。
- `test_independent_banded_mixed_refit_axis_and_work_accounting`：nonflat mixed CALL/PUT、
  PRICE/IV 和完整 bid/mid/ask。请求逆序 IDs `2:1,0:3`、不同步长 `1e-6,2e-7`，
  返回原报价索引顺序 `3,19`。独立逐方向重建原始报价，全部移动 bid/mid/ask，重新运行四启动拟合，
  比较 CALL_PRICE / TOTAL_VARIANCE / LOCAL_VOL 的 f0/f+/f- 与三种斜率；输出绝对误差上限
  `1e-12`（关闭 pytest 默认相对容差）。两轴均为稳定离散 regime。
  独立拟合累计 rows=4968、conversions=324 与 risk 一致，outputs=15。
  quote/history/Jacobian capacity 为 27/804/135；risk payload=50356/137792、workspace=65108/380000。
  较大 IV bump `2e-6` 双侧明确 FIT_QUALITY_FAILURE，无输出斜率。
  首次探索误把此不可满足的 band 期望为成功，保留初始日志并改为失败契约断言，未改生产。
- `test_subnormal_df_price_conversion_is_accurate_or_rejected`：下面的最小 blocker。

## 生产 blocker：PRICE parity 残差先舍入为 double

最小合法请求：evaluation=2026-10-19、expiry=2027-10-19，S=F=`1e-8`，D=`1e-308`，
单条 PUT PRICE，K=`100000000.1`，P=`D*K`（输入 double），无 bands、weight=1。
仅设置 `initial_volatility=8.5, normalized_price_tolerance=1e-10`，其他默认。
以 `Fraction(double)` 精确表示所有输入，计算 `1+(P-D*K)/(D*F)`，结果严格处于 (0,1)，
因此原始 premium 在 PUT 无套利区间内。

```text
精确输入参照 normalized call  0.75561081415586906
实际 normalized_mid           0.75561082353714404
实际 fitted_normalized_call   0.75561082353714215
绝对转换误差                  9.3812749612269342e-09
状态                          SUCCESS_WITH_WARNINGS（可取得 snapshot）
```

误差超过该路径请求的转换精度 `1e-12`，拟合价也超过真实报价带 `±1e-10`
及 feasibility allowance `1e-10`。这是成功准入了被错误转换的规范报价；UNDERIDENTIFIED
等 warning 不能豁免报价质量。相邻反例 K=`100000003.14159265` 转换误差约 `1.94854e-8`。

定位 `dal-cpp/dal/model/surface/marketivmath.cpp:144`：`std::fma(-discount,strike,premium)`
的三个参数都是 double，返回的差先被舍入到次正规 double，随后才提升为 long double 并除以
极小 DF。现有 `8*epsilon*(1+abs(P/DF-K/F))` 未外包此绝对误差放大。
`marketivfit.cpp:202` 将此结果用作目标和 band，因此 public fit 也暴露同一问题。
修复应达到要求精度，或明确 NUMERIC_RANGE 且无 snapshot；不可放宽准入容差掩盖。
本轮仅提供失败测试与根因证据，不实施修复。

## 证书与容量的独立核验

新增可复现探针及 exact-rational oracle 位于 `validation/probes/`：

```bash
/usr/bin/c++ -std=c++17 -O3 -DNDEBUG -ffp-contract=fast -I dal-cpp \
  .codex/artifacts/validation/probes/dal-379-market-iv-certificate.cpp \
  build/dal379-market-iv/dal-cpp/libdal_cpp.a -pthread \
  -o build/market-iv-independent/certificate-probe
DAL_NUM_THREADS=4 build/market-iv-independent/certificate-probe \
  > build/market-iv-independent/certificate-data.jsonl
/home/wegamekinglc/anaconda3/bin/python3 \
  .codex/artifacts/validation/probes/dal-379-market-iv-certificate-oracle.py \
  build/market-iv-independent/certificate-data.jsonl
```

原库为 long double 64 significand bits 的 Linux 构建。探针导出实际保存的 double
节点/斜率/证书下界，oracle 用精确有理数幂多项式和解析内部极值求真实最小值，
不复用生产 Bernstein 下界算法、不以采样代替认证。10 条可认证曲线、27 段均满足
`0 <= reported_bound <= exact_min`；覆盖 plateau、内部谷值、相邻 ULP、巨大/次正规 secant、
非均匀时间以及线性尾。两个下降反例均拒绝。节点/内部/尾查询与独立多项式相符。
5 个 rho/margin 参数盒的实际 eta cap、butterfly/wing 向外界满足精确有理数充分不等式；
eta/rho 上一 ULP 均拒绝。该 Linux 运行不证明 Windows 的浮点实现已执行。

同一探针的 6 个 PRICE 转换边界中，普通极端/互逆尺度/极小 CALL 和整数次正规 PUT
符合精度；两个小数 PUT 命中上述同一缺陷。因此 oracle **退出 1**，其中证书部分无失败。
探针 binary SHA-256 `96994f92e22ae994b8ca24b51a91ca87de39de2fbace7728e0013c28566b6c95`。

容量证据来自哈希匹配的原 benchmark 实际输出，结合源代码核对 sizeof*capacity、padding、
两个 Q×p 矩阵、四候选缓存、共享 base 和逐方向释放的记账；不是 RSS/allocator 峰值测量。
原最大 10000/64/256 case 的 P=3008776/5317504、W=11166528/23134208，
rows=10001、conversions=30000；这是显式 10001 row limit 下 WORK_LIMIT、无 snapshot，
不是最大输入默认预算成功的证据。其余原 fit/refit 容量和工作计数继续复用 implementation 表。
本轮新 risk 的工作总数另与独立四次 fit 相加核对，未仅比较两个接口的自报值。

## 交接边界

已分开：既有测试复用、新写三项测试/探针、生产失败归因；未进行失败修复。
未改 API/critique、生成绑定、产品或旧测试；保留本轮测试和验证产物供修复者复用。
修复后最先重跑 subnormal PRICE 用例及六项转换探针，再按实际改动选择直接受影响检查。
原 owning / 旧接口 / 正 IV 下溢 / 离散 regime / 失败无 snapshot 证据可复用，但不覆盖新失败域。
portable Excel 和 declaration shim 不替代原生 OPER 生命周期、XLL ABI 或 Windows 运行证据；
P3 PR #602 的 Windows 结果也不能认证 P2 新入口。后续 reviewer、文档、精确最终 head 门禁由 orchestrator 安排。

## PRICE blocker 定向独立复验（2026-10-19，525b11de）

**结论：原 PRICE 数值 blocker 已解除，可交回 orchestrator 安排聚焦主审。**
以上 `0068d052` 的首轮失败记录及其日志完整保留；本节是修复后的独立结论。
本次范围没有发现新的本地生产 blocker；Windows 原生 OPER/XLL 仍未执行，父任务保持 `in_progress`。

### 精确版本和来源核验

- 固定 head `525b11dea38d0a4e3d4b8147ace272e8b1480265`，原分支不变。
  产品修复 `85fddb691e18a65548af33a54fc92288ea5863b9`，仅修改 `marketivmath.cpp` 和两项 core 回归；
  后续提交仅增加 implementation 记录和转换探针。
- 配置仍为 Linux x86-64、GCC 15.2.0、C++17、static Release、native AAD、Eigen/native arch OFF，
  `-O3 -DNDEBUG -ffp-contract=fast`，Python 3.13.9。继续使用 `build/dal379-market-iv`，
  Python 实际加载该构建目录中的 `_dal`，已核对绝对模块路径和哈希。
- 当前 `marketivmath.cpp` SHA-256：
  `572c8ff88527f1e0ab7bd082436ebf376fdd1973be3bb30a22bf193aafa17a88`。
  `_dal` SHA-256：`b2b6a5e3f245ed8aed8554096344c9cce7e89ab8f9f5536ae1ef0c252160d956`；
  `libdal_cpp.a`：`1a69195c4c020c48048072326672f5a78a5a9a96ec2eaaf87bbb03198f0c1c37`；
  `dal_cpp_tests`：`58c88a6e6c518823986e9bd7d38194865de5694c564e537c665a23c003707b1c`。
- 下载修复证据附件 `01a1521b-7283-7ae3-9cbd-67e7005e86ca`，包 SHA-256
  `a7f8d037e0aa8bdff2c717200933eddda2f8962e70199ad113cfd70b52471350`。
  包内 36 文件在复验入口逐字节匹配；manifest 的 4 个增量源码对应 head blob，7 个二进制匹配。
  5 个 tester 未提交产物在本轮追加前全部保持原哈希；Python 测试和两个原探针均未改动。
  首轮 124 个源码 blob 中仅 implementation 记录、math 源码和 core 测试随提交改变，
  其余源码一致，支持按实际调用范围复用先前证据。
- 原 RED 的 `build/marketivmath-before-price-fix.cpp` 与 `0068d052` 的 Git blob 一致，SHA-256
  `83a8b72d82387fd1f72de4d83025eb19d181a6afc47a50d963cbe5b92d0d7f9e`；
  未以修复后源码或旧安装模块冒充 RED/GREEN。

### 误差认证及 53 位证据

逐分支核对 `NormalizePrice` 的新增误差项：

- 在运算前拒绝不可表示的 `D*F`。令 `quantum=double.denorm_min`、`u=quantum/computed_DF`，
  接受前要求 `u<1/8`；在至少 binary64 精度及渐进下溢的前提下，真实分母至少为所算分母的 3/4，
  避免把接近零的分母误差当作纯相对误差。
- 原有相对误差项保留；`8*u*(1+abs(normalized))` 外包 double FMA 残差和 DF 绝对舍入误差的放大；
  `8*quantum` 覆盖后续运算及最终转换的绝对量子；OTM PUT 另计 `8*quantum/forward`，
  覆盖 `(F-K)/F` 分支的差分尺度。maxError 和 fit 请求容差均未放宽。
- 接受/拒绝不借助额外 long-double 精度才能维持误差要求；良态次正规值仍可成功。
  53 位模式与 Linux 扩展精度模式不被要求逐位返回相同数值。

没有重跑作者的转换二进制，复用了身份匹配的四份 71 项原始 JSONL，并另外用 `Fraction(double)`
逐项精确计算 `P/(DF)` 或 `1+(P-DK)/(DF)`。该参照不调用产品转换，也不复制其误差认证公式：

| 原始模式 | 成功返回 / 明确拒绝 | 超 `1e-12` 接受值 | 最大成功值绝对误差 |
|---|---:|---:|---:|
| 修复前 native，64 significand bits | 71 / 0 | 4 | 1.9485374814e-8 |
| 修复前强制 53 位 | 65 / 6 | 4 | 1.6340289881e-9 |
| 修复后 native | 61 / 10 | 0 | 5.5042229563e-17 |
| 修复后强制 53 位 | 61 / 10 | 0 | 6.4377915862e-16 |

71 项覆盖 CALL、ITM/OTM PUT 的二进制缩放、normal/subnormal 交界相邻 double、零 premium、
互逆 F/D 尺度和两个原小数 strike 反例。所有原始输入的精确 normalized call 均在 [0,1]。
原 5/5 PRICE 定向 CTest 和 2/2 mixed-quote/原 blocker pytest 的 GREEN 可复用，未重复运行。
旧库 native core RED 为 1 failed / 1 passed，53 位旧转换新增暴露 CALL/OTM PUT 的 DF 放大误差，
说明本批参照可以识别原错误，而非只比较新接口自洽或全部拒绝。

额外检查了已核验 53 位二进制中的实际 `NormalizePrice` 反汇编：该函数使用 SSE double
乘/除/加/减，没有 x87 扩展精度运算，也未调用 long-double 数学库函数。
因此 `-mlong-double-64` 的有效性不只依赖 main 中打印的 digits；其他含混合 ABI 的数学函数未执行。
53 位 probe SHA-256：`ad57d1b8c03625cb4af9759952bc1a4f51b4c36fef9f4933a4eebbd5b3349b56`。
这仍不是 MSVC、SDK、真实 OPER 生命周期、XLL ABI 或 Office 的运行证据。

### 本轮新执行的最窄检查

```bash
DAL_NUM_THREADS=4 PYTHONPATH=build/dal379-market-iv/dal-python \
  /home/wegamekinglc/anaconda3/bin/python3 -m pytest -q -s \
  dal-python/tests/test_market_iv_surface.py \
  -k 'subnormal_df_price_conversion or independent_banded_mixed'
# 2 passed, 8 deselected, 0.02s
```

原最小反例另经公开 API 检查：`NUMERIC_RANGE`，`selected_start=None`，snapshot getter
抛 `SnapshotUnavailable`，model rows=0、quote conversions=1。不是失败后发布部分快照。
保留的带完整 bands 的 nonflat mixed PRICE/IV 双轴 refit 与四次独立重建拟合继续一致：
rows=4968、conversions=324、outputs=15，稳定 regime，payload=50356/137792、workspace=65108/380000。
较大 IV bump 的 FIT_QUALITY_FAILURE 仍无单侧或对称斜率。

来源、四份原始数据的独立精确重算和公共失败 metadata 一并捕获为
`build/market-iv-price-revalidation/audit.json` / `audit.log`；可复现脚本、原始日志及反汇编随本轮证据包交付。
审计脚本从源码根运行，最后参数为已通过 CLI 下载的修复证据 ZIP：

```bash
DAL_NUM_THREADS=4 PYTHONPATH=build/dal379-market-iv/dal-python \
  /home/wegamekinglc/anaconda3/bin/python3 build/market-iv-price-revalidation/audit.py \
  /home/wegamekinglc/multica_workspaces/dal-4ee34dfed184/dal-379-00e9a7cceab7/workdir/attachments/dal-379-price-conversion-fix-evidence.zip
objdump -d --disassemble=_ZN3Dal14MarketIvDetail14NormalizePriceEdbdddd \
  build/market-iv-price-fix-probe-53
```

本轮没有发现需要新写回归测试的剩余覆盖缺口，未改原测试、产品实现、API 或 critique。
未重跑导数、证书、容量、旧接口完整集合或任何性能矩阵；特别是原 benchmark 为 DECIMAL_IV，
不经过修改的 PRICE 分支，继续复用原结果，不把它当作 PRICE 耗时证明。
当前没有本地生产 blocker；后续聚焦主审及原生 Windows 门禁由 orchestrator 安排，未触发其他角色。
