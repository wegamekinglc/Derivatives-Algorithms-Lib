# DAL-379 P2：市场 IV 曲面实施记录

状态：active implementation，供独立定向验证、review 和文档收尾使用。父任务保持 `in_progress`。
分支 `agent/dal-orchestrator/7bc6ada900c9`；基线 `4d4760d1b33c77a2d00fd30a2fa6e0299d24fc5c`。
产品提交：`246ccd4ad5027caa0a3cb3799b73ee276a02d41b`。本记录后续提交仅登记控制记录和证据，不改变该数值版本。

## 控制记录和交付范围

完整遵循 [API](../api-notes/dal-379-market-iv-surface.md) 与
[critique](../critiques/dal-379-market-iv-surface.md)。两个原稿均未修改：

```text
API SHA-256      165b37a27e860d66bac586f8030972e3fdf779df029cf9955bac7b1d04c0a1ac
critique SHA-256 68be175833c54a2b76dd4fcadd65c3689a0c0b3501d7786d8bc9be0ff8e7662e
```

交付约束 SSVI 四启动拟合、owning snapshot、解析方差/价格导数、精确请求 Dupire 网格、
完整原始报价 refit bump，以及 C++ facade、Python 和 Excel 表面。旧 `IVS_`、spread 风险、
LocalVolSurfaceData v1 及 P3 归档保持。没有新增非平 carry 的 flat Hybrid 便利桥或 snapshot archive。

- `marketivsurface.hpp/.cpp`：复制值、日期/报价/设置校验、F/D 分段 carry、不可变结果 getter。
- `marketivmath.cpp`：稳定 Black、PRICE put 转换、SSVI 解析核、Hermite 表示和证书。
- `marketivfit.cpp`：解析 Jacobian、阻尼 box Gauss–Newton、原坐标 projected gradient、rank、
  四启动诊断、逐行 band 准入和严格预算。先取得所有合格候选的最小 J，再在 tie 带内选最早启动。
- `marketivevaluate.cpp`：固定 k/K 导数、右侧 carry 导数、分组可用性、flat-only owning legacy adapter。
- `marketivdupire.cpp`、`marketivrefit.cpp`：无填点/平滑/尾部复制的导出；顺序独立双侧完整拟合和 output 重估。
- `dal-public/src/marketivsurface.hpp`：复用 core 类型/函数，并集中绑定的规范配置字段；不复制求解器。
- `dal-python/src/bindings/marketivsurface.cpp`：严格类型、keyword-only quote/settings、复制 getter、
  GIL 下复制及 native 阶段释放 GIL。失败 metadata 返回 None，完整 refit 不走旧 spread VJP。
- `dal-excel/src/__marketivsurface*`、`__marketivinput.hpp`：typed owning handles、copied spills、
  真实 OPER 前置校验和 Machinist 注册；所有全空 settings 形状取默认，非空非法形状/半空/坏类型严格拒绝。

## 数值和保证域

所有默认值和 P/W/work 包络保持 API 原值。约束保证仍为公共 rho、gamma=1/2 的修正 power-law
SSVI、所有实 k 和 t>0、确定性比例 carry 的连续欧洲式 normalized call 条件。

证书对应保存的共享节点和 Hermite 斜率，不保存会在平台端点相消的幂展开系数。
对实际表示的导数二次式，非负 Bernstein 控制界可直接证非负，其余检查内部极值并减去舍入外包；
包含 subnormal 的绝对误差项。不能认证时拒绝，不借 feasibility tolerance 或 g 采样放行。
端点估计先归一化权重，内部 harmonic mean 按较小 secant 缩放；无法表示的斜率不伪装成平台。
非平台内部的导数下溢也不输出假零 local vol。

eta 上限以逐操作内侧/外侧舍入得到，参数证书和报告的 butterfly/wing 上界使用外包值；
不依赖 Windows 上不存在的额外 long-double 精度。所有 seed 的 rho、eta、theta 增量均投影。
eta=0 仍 canonical rho=0；rank 按 flat 子模型的 m 维或完整模型的 m+2 维计算。
rank 阈值固定为最大列范数的 `1e-8`；报价数不足以识别 m+2 维 SSVI 时，
即使获胜 flat 子模型自身 rank=m，也标 UNDERIDENTIFIED。不把 rank/condition 当统计置信区间。

PRICE put 使用 FMA 计算 premium 与 D*K 的差，并检查转换误差相对所请求精度是否可接受。
原始 PRICE 的可证明 parity 矛盾才提前返回 INCONSISTENT_QUOTES；其他 normalized parity gap
保留诊断并交质量门判断。正 IV 方差下溢、Black tail 范围损失、查询取消等均有明确失败。
默认 PRICE bump 先形成可表示的 D*F，再乘 `1e-4`，避免中间 D 缩放下溢。

成功同时要求 projected gradient、证书和每一行的 band（包括权重缩放下溢成零的行）。
STEP_STALLED、NO_DESCENT、MAX_ITERATIONS 不能单独准入。WORK_LIMIT 即使已有合格候选也无 snapshot。
保留当前 seed/最后候选、四启动终止原因、可用残差、计数和失败；未计算数值在绑定中为空。

regime 比较活跃 bound、PCHIP limiter、启动/模型、rank、near-tie 和 band 接触的离散签名。
连续 band 数值变化不自动报警。任一 refit 或所需 output 失败时不提供 slope、不转单边差分；
局部波动率不可用时不拿仍可用的价格替代。

## RED / GREEN 证据

以下日志和最小探针随证据包交付。最终 GREEN 以本记录的构建/定向命令和 binary 哈希为准。

| 反例/入口 | RED | 修正后的保护 |
|---|---|---|
| 新 core API | `c++ -std=c++17 -I dal-cpp -I dal-cpp/externals/googletest/googletest/include -c dal-cpp/tests/model/test_market_iv_surface.cpp -o build/market-iv-red.o`：缺失头文件 | core 定向测试和 standalone core 构建 |
| 新 Python 表面 | 旧构建包执行新 pytest：缺少 MarketIvMarket_ | 新模块全部定向 pytest |
| 正 IV `1e-300` | `test_positive_iv_underflow_is_explicit_numeric_failure`：错误 SUCCESS_WITH_WARNINGS | NUMERIC_RANGE，snapshot getter 抛错 |
| Hermite 导数下界 | `build/market-certificate-probe`：下界 .0533333 > 实际 .0333333 | 保守下界 .0133333；独立解析中点及平台回归 |
| XLL 包装器 | Clang 声明 shim 编译：生成 wrapper 在 Dal 命名空间外 | wrapper 和原生测试源码语法检查通过；未替代 Windows 运行 |
| 极端 D/F 的默认 PRICE bump | `market-bump-scale-probe`：合法尺度的默认步长被算成零并拒绝 | 可表示的正步长、完整双侧响应和近 1 的原价响应 slope |
| 失败 metadata | WORK_LIMIT 返回 selected_start=-1、未计算 rank=0、遗漏当前候选 | Python None / Excel 空格；当前候选参数保留 |

另补目标 `1,.91,.89`、tie=.1 的非传递选择反例，要求选择启动 1；
以及 eta/rho 上限 ±1 ulp、巨大/极小 secant、零局部波动率、零有效权重坏 band、末次重算耗尽和精确字节上限。

## 实际构建与定向验证

Linux x86-64，GCC 15.2.0、CMake 4.2.3、C++17 Release、static、native AAD、
Eigen OFF、native architecture OFF；Python 3.13.9、pybind11 3.0.4。
性能进程统一 `DAL_NUM_THREADS=4`；不使用安装目录的旧 binary。

```bash
cmake --preset=Release-linux -S . -B build/dal379-market-iv \
  -DDAL_BUILD_PYTHON=ON -DDAL_BUILD_EXCEL=ON \
  -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_PUBLIC_BUILD_EXAMPLES=OFF -DDAL_USE_EIGEN=OFF
cmake --build build/dal379-market-iv \
  --target dal_cpp_tests dal_public_tests dal_excel_portable_tests _dal -j 6
ctest --test-dir build/dal379-market-iv --output-on-failure \
  -R '^(MarketIvSurfaceTest|PublicMarketIvSurfaceTest|ExcelMarketIvSurfaceTest)\.'
PYTHONPATH=build/dal379-market-iv/dal-python python3 -m pytest -q \
  dal-python/tests/test_market_iv_surface.py
ctest --test-dir build/dal379-market-iv --output-on-failure \
  -R '^(ModelTest.Test(FlatIVS|MertonIVS|LocalVolSurfaceInterpolatesAndRoundTrips)|DupireRiskTest.Test(Snapshot|Invalid|Mismatched))'
```

最终结果：19 项新 CTest、10 项定向兼容 CTest、7 项 Python 测试全部通过。
兼容检查一次与本地链接重叠而未能启动，属于 BAD_COMMAND；已在构建完成后顺序重跑，未把它记为产品通过。
没有重跑全量测试/性能矩阵。

```bash
cmake --preset=Release-linux -S . -B build/dal379-market-iv-core \
  -DDAL_BUILD_PUBLIC=OFF -DDAL_BUILD_PYTHON=OFF -DDAL_BUILD_EXCEL=OFF \
  -DDAL_EXCEL_BUILD_TESTS=OFF -DDAL_PUBLIC_BUILD_TESTS=OFF \
  -DDAL_CPP_BUILD_TESTS=OFF -DDAL_CPP_BUILD_EXAMPLES=OFF \
  -DDAL_CPP_BUILD_BENCHMARKS=ON -DDAL_USE_EIGEN=OFF
cmake --build build/dal379-market-iv-core --target quote_risk_perf -j 4
DAL_NUM_THREADS=4 ctest --test-dir build/dal379-market-iv-core --output-on-failure \
  -R '^quote_risk_market_iv_perf$'
DAL_NUM_THREADS=4 python3 build/market-iv-sample.py
```

实际 standalone core 构建只依赖 DAL::cpp；新测量为现有 `quote_risk_perf --market-iv` 模式，
原目标、默认协议、原 benchmark 参数/阈值不变。CTest 注册 `quote_risk_market_iv_perf`，benchmark 标签。
初始新增独立 target 触发文档 target inventory 检查，已收敛到该现有 quote-risk 模式，未改保护的 Claude 文档。

## 路径到测试/性能映射

| 路径 | 验证 | 性能范围 |
|---|---|---|
| marketivmath / fit / surface | flat BS、mixed price/IV call/put 合成恢复、非平 carry/闰日轴、平台/解析证书、独立 80 位 kernel 和价格导数、坏/稀疏输入、质量/迭代/work/byte 边界 | 稀疏 1/1、300/12、2048/64；10000/64/256 的明确 WORK_LIMIT |
| marketivevaluate / dupire | fixed-k/K、右 carry 导数、g 与价格 Dupire、精确非方网格、零 vol、旧 archive 往返、flat 网格和模型步长对照重定价 | 300/12 snapshot 的 25 个 local-vol outputs |
| marketivrefit | 独立从原始报价驱动 2A 次重拟合；h/h2/h4；稳定及切换 regime；输出/aggregate work/不可表示 bump 失败；旧固定 base+spread 的可区分响应；非空 AAD recording 隔离 | 八轴/25 outputs，顺序 16 次完整 fit |
| dal-public facade | 复制/销毁输入后继续使用；flat legacy 和 nonflat 拒绝 | header re-export，无第二套求解器；复用 native 路径 |
| dal-python binding | 严格类型、默认设置、生命周期/副本、并发、完整 refit、网格/legacy、失败 None | native 调用同一路径；不运行旧全绑定矩阵 |
| dal-excel binding / auto | portable copied spills；新 Windows 原生 OPER/XLL 测试注册、语法探针 | 本地无 Windows 运行，不以 portable 替代 |
| Matrix_::Capacity | 仅新增 const getter，用于实际导出矩阵容量记账；原数值算法不变 | 旧矩阵执行路径无改动，不扩大矩阵 benchmark 集合 |

普通导数域用独立 mpmath 80 位直接 Black/SSVI 加数值求导，固定准入为
无量纲向量 `1e-11+1e-9*abs(reference)`，local variance `1e-10+1e-8*abs(reference)`。
另有六组独立 80 位 kernel 常量，涵盖 eta=0、eta=1e-14、rho=±.998 和 theta=4e-12；
验收 `1e-42+1e-12*abs(reference)`，没有把近零导数一律当零。
flat 网格重定价使用独立 Black 与 96 点 Gaussian quadrature；网格/最大模型步长 1→1/16，
价格误差门槛 .06，两个离散配置价格差 1e-9；该误差不是连续曲面精确重定价承诺。

## 容量、峰值和性能

数值 payload 按实际 `sizeof * capacity`（含对象布局/padding）计入原请求、诊断、
四启动/历史、参数、curve、证书；输入去重和 parity 使用有界索引向量，不保留每轮 Jacobian。
workspace 计共存的两个 Q×p 矩阵、lower、候选/梯度/价格缓存、Hermite 临时容量；
四个候选只缓存价格以执行最终 tie 政策。固定 fit/refit 原生帧按原包络的 8192 bytes 保守计入，
并覆盖 preflight/数值失败和无 work 的路径。这里是容量/固定帧的保守峰值账本，不是 RSS 或 allocator 探针。
Dupire 矩阵通过 `Capacity()` 读取，base snapshot 共享只计一次；refit 逐方向释放 bump snapshot。
字符串动态内容、allocator 元数据、语言包装和 worksheet 单元格按 API 排除。

以下单位为 bytes，分子为实际容量账本、分母为批准的计划包络；计数为单次调用。

| case (Q/m/C 或 A/outputs) | persistent / envelope | peak workspace / envelope | row work | conversions | output work |
|---|---:|---:|---:|---:|---:|
| fit 1/1/1 | 40,668 / 112,576 | 9,176 / 112,736 | 95 | 3 | 0 |
| fit 300/12/12 | 131,864 / 272,000 | 95,844 / 312,064 | 10,105 | 900 | 0 |
| refit 8/25 | 160,216 / 316,288 | 260,076 / 881,664 | 162,880 | 14,400 | 425 |
| fit 2048/64/64 | 663,336 / 1,196,928 | 2,328,532 / 5,272,576 | 83,968 | 6,144 | 0 |
| fit 10000/64/256，WORK_LIMIT | 3,008,776 / 5,317,504 | 11,166,528 / 23,134,208 | 10,001 | 30,000 | 0 |

最后一项明确设置 row limit=10,001，失败且无 snapshot；不是未完成的 benchmark。
四个 fit 的 quote/history/Jacobian capacity 分别为 `1/804/3`、`300/804/4200`、
`2048/804/135168`、`10000/804/660000`。逐方向 refit 的聚合计数不伪装成同时保留的 fit 容量。

两轮、每轮 10 个进程样本，保持每 case 1 次 warmup / 10 次内部重复，保留全部原始输出并按 min 归约。
这是新 API 的新增信息性覆盖，基线没有同功能 API；没有伪造旧版对比或作 4% regression verdict，
也未把新 target 加入原九项 gate 的闭集。

| case | round 1 minimum (ms) | round 2 minimum (ms) |
|---|---:|---:|
| fit 1/1 | 0.045353 | 0.045340 |
| fit 300/12 | 3.327 | 3.300 |
| fit 2048/64 | 123.534 | 123.726 |
| refit 8/25 | 54.336 | 53.891 |

机器 Intel Core i9-13900HX，32 logical CPUs，governor 无可用读取值。
最终独立 benchmark SHA-256 为
`0fafc51c3d65f9a4e05f759cc4da954d9bb1bf2e5cdb62e1100aaf0648580e6a`。
原始 20 个进程输出、归约脚本、两轮 JSON、源码/构建配置和 binary 身份均随证据包提供。

## 生成、自检和真实边界

枚举、Java enum 投影和 44 个 Excel 入口的 inc/htm 均经 `dal_generate` 产生，未直接编辑 auto。
`dal_check_generated`、仓库 Markdown、diff whitespace 和定向复杂度检查的最终结果随证据交付。
生成一致性通过，74 个 Markdown 通过，staged diff whitespace 通过；
`lizard -w` 对新 core、facade、绑定、core/portable 测试和 benchmark 源码无告警。
最终日志为 `market-iv-final-generated.log`、`market-iv-final-docs.log`、
`market-iv-final-complexity.log`、`market-iv-final-19-ctest.log`、
`market-iv-final-7-python.log`、`market-iv-complete-compat.log` 和
`market-iv-final-benchmark-registered.log`；均在证据包 `build/` 下。
定向 core syntax 使用现有 warning-clean CI 的原 flags（含既有 baseline exclusions），没有更改仓库规则。
初始无 exclusions 的额外探针被旧 AAD 头的 unused-parameter 警告拒绝，不把它写成 CI 通过。

真实 Windows MSVC/SDK/Office 的两个新增原生回归尚未运行：

- `ScriptExcelRawTest.TestMarketIvSettingsActualOperBlankAndStrictInputs`
- `ScriptExcelRawTest.TestMarketIvFitActualXllOwningAndTypedRejection`

它们通过 `GetProcAddress` 调实际生成 XLL 导出，覆盖全空 2×3/多行单列、相邻非法值、
xltypeInt、错误格/NUL、数组边界和 fit→snapshot→query→point。Clang 声明 shim 仅证明语法，
不替代 Windows CI、真实 OPER 生命周期、Office ABI 或 registration 运行。

该保守模型族及局部优化没有全局最优/市场可拟合保证；范围不足的点会明确失败。
旧 test-support oracle 未改动；未用三绑定一致性代替独立金融参照。
本轮仅创建本地提交，后续独立验证、review、发布文档和单一 PR 由 orchestrator 顺序协调。

## PRICE 次正规尺度 blocker 修复（2026-10-19）

本节是 tester 在 `0068d0526e5e1e13ecae007215bfbdb93b1b50ea` 复现问题后的生产修复记录；
原 validation 的失败结论、原始日志、三项未提交 Python 测试及索引内容均保持原样。
本地产品修复提交：`85fddb691e18a65548af33a54fc92288ea5863b9`。没有改动 API、critique、生成接口或其他角色的结论。

根因是 double FMA 残差的绝对舍入误差被极小 D*F 放大，原有相对误差项没有覆盖；
在 MSVC 的 double 精度 long double 上，D*F 本身的次正规乘积舍入也需覆盖。
原反例 normalized call 的精确参照为 .75561081415586906，原实际值为 .75561082353714404，
误差约 9.38e-9；二进制放大 2^8 后仍有约 7.55e-11，均超过转换精度 1e-12。

`NormalizePrice` 保留三条原数值表达式及 `maxError`，补充以 double 的
`denorm_min/(D*F)` 度量的绝对误差放大项、最终舍入的绝对下限，以及 OTM put 差分的绝对误差项。
先拒绝无法表示的 D*F；相对量子达到 1/8 时拒绝，避免在可能接近零的真实分母上使用相对扰动论证。
量子小于 1/8 时，真实分母至少为已算分母的 3/4（包括 double epsilon 的余量），
系数 8 外包乘积、FMA、除法、加法及最终 double 舍入；原相对误差项仍保留。
新增认证使用 double 的 epsilon/最小次正规量子，未借更宽 long double 的范围或精度放行。
不是提高允许误差：请求精度保持不变，不能认证的输入按既有 NUMERIC_RANGE 失败，不发布 snapshot。

只改 `marketivmath.cpp` 中的认证，不增加 heap 分配、请求设置、报价单位、诊断字段或 work 操作。
P/W 包络、计数定义及输出／失败接口不变；原容量账本继续复用。
原 benchmark 全部使用 DECIMAL_IV，不经过修改的 PRICE 分支，其性能证据继续复用；未对新 PRICE 失败路径声称原耗时。

新增两个 core 回归：44 项二进制缩放的精确有理数常量参照，覆盖 CALL、ITM/OTM PUT；
另 12 项覆盖 normal/subnormal 交界相邻 double 和零 premium，要求这些良态边界继续可转换。
测试不要求所有次正规输入失败：任何返回数值均须满足原精度，真正可稳定转换的输入仍允许成功。
独立 probe 另覆盖 71 项，使用 tester 原精确有理数 oracle 验证，不复制生产误差认证算法。

复现和定向命令（沿用上文 Release/static/GCC 配置）：

```bash
DAL_NUM_THREADS=4 PYTHONPATH=build/dal379-market-iv/dal-python \
  /home/wegamekinglc/anaconda3/bin/python3 -m pytest -q -s \
  dal-python/tests/test_market_iv_surface.py -k subnormal_df_price_conversion
# RED: 1 failed, 9 deselected；原回归未屏蔽或修改。

cmake --build build/dal379-market-iv --target dal_cpp_tests _dal -j 6
ctest --test-dir build/dal379-market-iv --output-on-failure \
  -R '^MarketIvSurfaceTest.Test(PriceConversion.*|PlatformCertificateAndExtremePut|ExtremePutAndDisjointParityNeverCreateSnapshot|StableDefaultPriceBumpAtExtremeCarryScale)$'
DAL_NUM_THREADS=4 PYTHONPATH=build/dal379-market-iv/dal-python \
  /home/wegamekinglc/anaconda3/bin/python3 -m pytest -q -s \
  dal-python/tests/test_market_iv_surface.py \
  -k 'subnormal_df_price_conversion or synthetic_mixed_quotes_flat_and_owning'
```

独立探针的普通精度编译命令如下；第二次增加 `-mlong-double-64`，输出文件名增加 `-53`：

```bash
/usr/bin/c++ -std=c++17 -O3 -DNDEBUG -ffp-contract=fast -I dal-cpp \
  .codex/artifacts/implementation/probes/dal-379-price-conversion.cpp \
  dal-cpp/dal/model/surface/marketivmath.cpp \
  build/dal379-market-iv/dal-cpp/libdal_cpp.a -pthread \
  -o build/market-iv-price-fix-probe
build/market-iv-price-fix-probe > build/market-iv-price-fix-probe.jsonl
/home/wegamekinglc/anaconda3/bin/python3 \
  .codex/artifacts/validation/probes/dal-379-market-iv-certificate-oracle.py \
  build/market-iv-price-fix-probe.jsonl
```

修复前的 native core 回归为 1 failed / 1 passed；71 项 probe 的普通精度版本存在 4 个超限结果。
原函数的强制 53 位版本也存在 4 个超限结果，包含 CALL 和 OTM PUT，直接证明还需外包 D*F 的舍入。
修复后普通和 53 位版本各 71 项：61 个准确返回值、10 个明确拒绝，没有超限接受值；
probe 标识分别确认 long_double_digits=64 和 53。
强制 53 位运行只执行实际转换函数及 double JSON 输出，不调用混合 long-double ABI 的库数学函数；
它验证该认证不依赖额外精度，**不等于原生 MSVC／SDK／Office 执行**。

最终定向结果：5/5 CTest、2/2 Python（8 deselected），普通/53 位精确有理数 oracle 各 71 项均无超限接受值。
原 public blocker 返回 NUMERIC_RANGE、snapshot 不可取得、model rows=0、quote conversions=1。
上文原 19 项 core/portable 集合、完整 Python 集合、旧接口兼容及性能矩阵未重跑。
修改的 core 源文件按原 warning-clean CI flags 通过 syntax 检查；定向 lizard 无告警，
75 个 Markdown 检查和 diff whitespace 通过。没有枚举或注册 markup 改动，复用既有生成一致性证据。
没有示例或构建依赖变化，复用原 standalone core-build 的组件边界证据。
完整命令、原始 RED/GREEN、构建配置及 source/binary 身份随本次增量证据包交付。
保留原 tester 失败记录；未重启导数/refit/证书的完整验证集合或性能矩阵，
原 PRICE 相关 case（含极端价格 bump）仅作为直接影响验证运行。
Windows 原生 OPER/XLL 仍未执行，独立 blocker 复验及聚焦 review 由 orchestrator 后续安排。
