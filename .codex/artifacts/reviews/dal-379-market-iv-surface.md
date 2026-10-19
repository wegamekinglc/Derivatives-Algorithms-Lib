# DAL-379 P2：市场 IV 曲面聚焦代码主审

## Findings

### [P2] required OPER 参数的校验晚于自动转换，坏数组会先被读取

位置：`dal-excel/src/__marketivsurface.cpp:219`；同源模式见 `:134`、`:162`、`:186`、
`:256`、`:293`。生成后的直接证据是
`dal-excel/auto/MG_MarketIvQuery_New_public.inc:15`、`:18`，以及
`dal-excel/auto/MG_MarketIvMarket_New_public.inc:19`、`:20`。

`+` hook 放在最后一个 required 参数下面，Machinist 把它生成在该参数的转换之后。
因此 Query 先调用两次 `ToDouble`，之后才执行 `ValidateMarketIvNumber`；Market/FlatMarket/
FitRequest/DupireRequest/RefitRiskRequest 同样先转换其 required scalar/range。
这使专用 validator 的数组尺寸/存储检查无法保护转换器，也使 normalizer 无法影响已创建的值。
四类 Settings 工厂的 hook 在 name 后、settings 转换前，顺序正确，不属于这个问题。

具体后果与证据：

- Query 的 strike 是 `xltypeMulti`、rows=columns=1、lparray=nullptr 时，
  `dal-excel/src/_excel.cpp:730` 的 `ToDouble` 先通过 scalar-size 判断，再递归解引用空存储；
  本应先由 `__marketivinput.hpp:13` 拒绝。required 矩阵也在 `ToCellMatrix` 读取/分配前缺少
  原始数组检查。这里报告的是这六个新增入口的 hook 顺序，未扩大修改共享转换器的范围。
- 合法的 bool 类型拒绝路径也不能到达专用错误：生成 Query 返回
  `Invalid input type -- number expected (at strike (input #2))`。
  新增真实入口回归 `dal-excel/tests/test_script_raw.cpp:1574` 要求 `strike` 和
  `excluding bool`，后者不在该错误中。静态调用顺序和隔离执行均证实此矛盾；
  **未声称已经在 Windows 运行了该测试**。

只为这个新疑点补了一个 Linux 控制流探针：直接纳入当前未改动的 generated Query wrapper，
提取生产 `ToDouble`、validator、normalizer 和仓库 OPER 声明；仅用替身提供 session、
registration、environment、文本/返回封装及未到达的 typed factory。命令使用
`c++ -std=c++17 -O0 -g -x c++ - -o build/market-iv-review-wrapper-order-probe`。
编译输入在 stdin 中组合，不修改产品或测试源码。结果：

```text
bool:
  direct validator: InvalidMarketIvRequest: strike; numeric cell required excluding bool/text
  generated wrapper: Invalid input type -- number expected (at strike (input #2))
null-storage 1x1 multi:
  direct validator: InvalidMarketIvRequest: strike; invalid array dimensions/storage
  generated wrapper: process exit -11 (SIGSEGV)
```

探针 SHA-256：`34c57fd41de87760719a06ba1252f9c8386e185c32ec594bd482ea359ef23418`。
它证明转换/校验顺序及提前空指针读取，不证明 Windows ABI、Office 或真实 registration 已通过。
最小原生复现是在有效 name/time 下，把 Query 的 strike 换成上述 OPER；bool 可直接复用现有
`TestMarketIvFitActualXllOwningAndTypedRejection`。建议将六个入口的 raw 校验和归一化 hook
放到 name 后、受影响参数第一次转换前，由 Machinist 重生成；保留现有 bool 严格断言，
补 scalar/range 坏存储的真实入口拒绝检查。修复后只需复核生成顺序及直接受影响入口，
无需重跑 SSVI 数值、容量或性能矩阵。

本轮只有上述一个新增代码 blocker。原 PRICE 次正规精度 blocker 维持**已解除**结论。

## Open Questions

没有需要重开 API/数值设计的决定。审查对象是指定本地 P2 head 及 tester 未提交测试；
该分支当前没有 open PR，不使用已合并 P3 PR #602 的检查结果认证本 P2。

本地仍未执行两个 P2 原生 Windows 回归：
`ScriptExcelRawTest.TestMarketIvSettingsActualOperBlankAndStrictInputs` 和
`ScriptExcelRawTest.TestMarketIvFitActualXllOwningAndTypedRejection`。
`dal-excel/CMakeLists.txt:201` 的 glob、`:225` 的 XLL 链接和 `:227` 的
`gtest_discover_tests` 静态覆盖它们，但不是 MSVC 的实际编译/发现/执行证据。
portable、声明 shim、强制 53 位转换探针和本轮控制流探针均不替代原生 CI。

## Tests

范围：`4d4760d1b33c77a2d00fd30a2fa6e0299d24fc5c..525b11dea38d0a4e3d4b8147ace272e8b1480265`，
分支 `agent/dal-orchestrator/7bc6ada900c9`；另含 tester 未提交的三项 Python 回归和 validation。
已读 active API/critique/implementation/validation、完整新增核心/绑定/测试、相关原文件上下文、
markup/生成入口与 build target closure。当前 HEAD 与委托一致。

来源核验及证据复用：

- 125 个提交内改动路径逐一对应 HEAD blob，工作副本仅有预期的 index 和 Python test 差异。
  首轮 124 项 source manifest 对照当前 HEAD，只变化 implementation、math 和 core test 三项；
  后续 probe 为新增文件。API/critique 哈希与原记录一致。
  tester Python 文件 SHA-256 为
  `d731246869ca8c3f043be89bbb55e180493f3814be77dd4a39cc1e63c0b332e0`。
- 修复复验所列七个当前 binary、首轮仍适用的 public/portable/public archive/standalone
  benchmark binary，以及复用日志哈希均再次匹配。当前 Python module 为
  `b2b6a5e3f245ed8aed8554096344c9cce7e89ab8f9f5536ae1ef0c252160d956`，
  core archive 为 `1a69195c4c020c48048072326672f5a78a5a9a96ec2eaaf87bbb03198f0c1c37`。
  未把旧二进制未经过的 PRICE 修复分支写成已测试。
- 复用 19 项新 CTest、7 项原 Python、10 项旧 IVS/spread/archive 兼容，以及修复后的
  5 项 PRICE CTest、mixed/blocker Python GREEN。tester 新增 nonflat unquoted-carry oracle
  为 25 点、90 位精度；banded mixed 两轴与四次独立 refit 一致，rows=4968、
  conversions=324、outputs=15。它们补足不同 carry/theta 轴和完整 bands 的覆盖，具有独立保护价值。
- 复用精确有理数证书核验：10 条曲线/27 段、5 个参数盒，实际下界不高于解析真最小值；
  首轮探针整体 RED 的原因仍保留为已修复 PRICE 问题，没有冒称首轮全通过。
  PRICE 修复前普通/53 位各 4 个超限接受值，修复后各 71 项为 61 个准确返回、10 个明确拒绝、
  零超限接受值；最大成功误差 `5.50e-17/6.44e-16`。复验的反汇编确认转换函数使用 SSE double，
  不依赖 x87 扩展精度。原公共反例为 NUMERIC_RANGE、无 snapshot、rows=0、conversions=1。
- 复用容量/信息性性能和真实 `DAL_BUILD_PUBLIC=OFF` core 构建证据。
  300/12 fit 的实际 P/W 为 131864/95844，对应包络 272000/312064；八轴/25 outputs
  为 160216/260076，对应 316288/881664；最大输入明确在 rows=10001 WORK_LIMIT，
  P/W 为 3008776/11166528，均在原包络内。这是 capacity/固定帧账本，不是 RSS。
  两轮各十进程的四条新增性能路径仅为 informational；没有同 API 基线，不作回归门禁结论。
  benchmark 全走 DECIMAL_IV，不把其耗时用于 PRICE 修复分支。

本轮没有重跑上述集合或性能矩阵。唯一新增可执行检查是 Findings 中的 raw wrapper 控制流探针；
另做源码/二进制/日志身份、生成顺序、文档缺口和 whitespace 核对。

## Summary

除所列 Excel blocker 外，未发现新增数值或公共生命周期 blocker：

- `marketivmath.cpp:24` 的导数下界针对保存的 Hermite 节点/斜率表示，不能认证即拒绝；
  `:203` 起的 eta 内侧/参数外侧界配合单调 theta，而非 g 采样承担全域充分条件。
  核对了 [Gatheral–Jacquier 原论文 Theorems 4.1/4.2 与 Corollary 4.1](https://arxiv.org/pdf/1204.0646)；
  所选修正族的代数界与 active critique 相容。该数学证书不等于任意 double 查询逐位凸性承诺。
- `marketivmath.cpp:138` 的 PRICE 认证补齐 FMA/DF 次正规误差放大，原单位和请求精度未放宽；
  `marketivfit.cpp:202` 的 IV 转换、完整 band 与逐行质量准入没有被权重下溢绕过。
- `marketivevaluate.cpp:27` 的固定 k/K、carry 和价格导数链式关系及 `:112` 的 w_t/g 一致；
  右侧 carry knot 与 theta 轴分离，数值范围/密度失败不借旧差分、floor 或填点掩盖。
- `marketivfit.cpp:305` 的全部 seed 投影、四启动/收敛/证书/逐行 band 搜索政策，及
  `marketivrefit.cpp:40` 的逐方向原始报价重建/重转换/完整拟合保持；regime 使用离散签名。
  任一方向/所需输出失败时不转单边坡度，aggregate 与 per-fit work 独立强制执行。
- 快照为共享 const owning 数据；Python 在释放 GIL 前复制输入，getter 返回复制值；
  Excel typed handles/spills 拥有数据。flat-only legacy 桥保留旧 IVS、spread 风险和 archive v1 语义。
  core benchmark 仍链接 core，Python/Excel 依赖 facade，没有反向组件依赖或示例搬迁。

必要文档/CHANGELOG 收尾尚缺，按委托交给下一阶段，不把写法偏好增加为数值 blocker：

- `dal-cpp/dal/model/surface/marketivsurface.hpp:274` 起的新公共入口目前只有 active 设计记录。
  `docs/models/dupire.md:1` 仍只介绍旧 IVS 差分/补密网格路径，需补新 SSVI/解析导数/显式网格
  的当前方法说明及导航，保留旧接口自身的准确描述。
- `docs/python/README.md:16`、`docs/excel/README.md:545` 及 C++ 用法导航需给同一最小 fit→snapshot→
  query/Dupire/refit 示例，说明 PRICE/decimal IV 单位、日期/carry、全域充分保证的范围、局部优化
  质量门槛、默认预算、失败无 snapshot、分组查询状态与 owning 生命周期。
- 必须区分 full-refit finite bump 与 fixed-base spread VJP，说明 REGIME_CHANGED/不认证可微、
  flat-only legacy/Hybrid 边界以及离散导出不继承连续证书；`CHANGELOG.md:1` 尚无本 P2 能力条目，需补齐。

仅新增本 review 并登记索引，保留 tester 测试/validation/探针、API/critique/implementation 原字节；
未修产品、创建分支/环境/PR、提交 GitHub review、合并或触发角色。父任务保持 `in_progress`，
交回 orchestrator 安排 hook 顺序修复，再进入文档和单一 PR 交付。

**Verdict: Request Changes**

## 2026-10-19：Excel raw hook blocker 定向复核

### Findings

本轮无新增 Findings，**首轮 Excel raw hook 顺序代码 blocker 已解除**。
仅审查 `525b11dea38d0a4e3d4b8147ace272e8b1480265..24662d9ac6492296aaff4dac2e4c6d206c2fd447`
及直接影响；产品修复为 `f9b35fddf8f45fac69aebd9ee9a61b7979ee0896`。
当前 HEAD 和原分支已核实。本节取代首轮该项 blocker 的未关闭状态，首轮全部文字、
Request Changes verdict 和原始失败证据保留；PRICE 已解除结论不变。

解除条件逐项核对：

- `dal-excel/src/__marketivsurface.cpp:128`、`:151`、`:182`、`:215`、`:252`、`:289`
  的六个 markup block 现在均位于 name 后。对应六个生成 inc 的 `:14` 在任一 required
  `ToDouble`/`ToCellMatrix`/`ToHandle` 前完成 raw validator→normalizer→指针替换。
  `MG_MarketIvFitRequest_New_public.inc:14` 的 quotes 检查还先于 market handle 读取；
  optional steps 同样先校验/归一化再转换。normalizer 局部对象覆盖后续转换及 typed 调用的寿命。
- 六份 inc 除整条 hook 的位置外，其余字节与基线相同，注册签名、参数顺序、帮助文字和返回
  协议未变。typed 实现正文未改；四类顺序正确的 Settings 工厂和共享 validator、normalizer、
  转换器不在改动范围。Machinist 一致性日志通过，最后重生成写入 0 个文件。
- `dal-excel/tests/test_script_raw.cpp:89` 的新 helper 对每字段传入八种空存储/非法尺寸，
  包括原 1×1 lparray=nullptr；严格要求标识符、字段名与 `array dimensions/storage`。
  `:1592` 覆盖 Query/Market/FlatMarket 的八个 scalar 字段，`:1619` 覆盖六入口的八个 range
  字段，含 optional steps。每项 TEST 有 64 次实际导出调用，沿用 `:50` 的 GetProcAddress
  和 Output_/xlAutoFree12 生命周期，不是直接调用 typed factory 的替代测试。
- 原两项 settings/owning/typed-rejection TEST 的函数体逐字节保持；尤其 `:1585` 的
  `{"strike", "excluding bool"}` 断言未放宽。原 owning/bool/NUL 函数 SHA-256 仍为
  `a5fd67320c4bbabf789facab1379f9b20c7663e1d0bedd327c558efa399c0a62`。

### Open Questions

没有新的实质修复或 API 决策问题。**原两项加新增两项 Windows OPER/XLL 回归均未原生执行**；
现有 Windows CMake glob/链接/discovery 继续覆盖它们，但实际 MSVC 编译、测试发现及导出调用
仍待最终交付 head 的 CI。Linux 控制流探针、portable 和声明 shim 都不能认证这些原生结果，
本节不宣称整体 merge-ready。首轮列出的当前方法说明、语言用法和 CHANGELOG 收尾仍有效，
无需为这次仅移动 hook 重开数值设计或重写全部流程。

### Tests

完整核对十个增量文件、实施记录新增节、可复现 probe、两组结果、相关日志和 baseline headers。
十个文件均匹配精确 HEAD blob，且匹配 `build/market-iv-raw-hook-identities.json` 的源码哈希；
六个保留产物和六个 binary 哈希也全部匹配。证据包中的六份 baseline 生成头逐一匹配
`525b11de` 的 Git blob，未把 GREEN 源码当作 RED。

直接复用适用的 RED/GREEN，不重跑集合、性能矩阵或已通过的 probe：

- 基线六个入口顺序全为 false；Query scalar-null 和 DupireRequest range-null 为 SIGSEGV，
  bool 缺少专用诊断。当前顺序为 6/6，五个隔离模式全部通过；bool/scalar-null/range-null
  在 typed factory 前拒绝，scalar-int/range-int 到达替代 factory 并验证值。
  当前 probe binary SHA-256 为
  `396992085ab41515dd2024dfa72620551e6337c3d8de5936214929996e055031`。
  probe 纳入未改动的生成 wrapper 和提取的生产控制流；Cell/Matrix/环境/返回等部分有替身。
  range-int 证明归一化先于替代 cell converter，不是原生转换数值或 Windows ABI 证据。
- 重建后的 portable 定向 CTest 为 1/1（0.06 s），binary SHA-256
  `7717ea881a0b6b5f2f53eba103a904a3d8d2a775cc695e156eb9df4c9ffa2177`。
  完整 XLL TU 与 raw TEST TU 的声明 shim syntax 检查通过；它们均未运行 Windows TEST。
- core/public 静态库哈希仍为
  `1a69195c4c020c48048072326672f5a78a5a9a96ec2eaaf87bbb03198f0c1c37` /
  `c554fec91709eb90785b7451773404c40ffa92e2476a4d3978274225f6cbb34e`。
  core/public/Python 与共享输入代码的提交间 diff 为空，既有数值、PRICE、证书、refit、
  容量、性能和 standalone core-build 证据继续适用。

本轮新增检查仅为只读源码/生成顺序、函数体、Git blob、证据包和二进制身份核对；
没有需要补执行的新缺口。`git diff --check 525b11de` 通过。

### Summary

六入口 raw 校验现在能够保护首次转换和存储读取，严格 bool 诊断保留，新增真实导出回归覆盖
原空存储反例及直接路径。仅追加本节并更新 review 索引，首轮原文逐字节保留，tester 未提交
测试/validation 和其他控制记录不改。交回 orchestrator 进入文档收尾及单一 PR 交付，
父任务保持 `in_progress`。

**本轮 Verdict: Approve**（仅本次 blocker 修复与直接影响；四项原生 Windows 回归仍待 CI）。
