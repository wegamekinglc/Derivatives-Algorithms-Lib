# DAL-418 rate-option accrual 实施记录

BG-1 最小修复已在原 DAL-408 checkout、原分支 `agent/dal-implementer/e2075e8549aa` 本地提交，继续使用 PR #620。三个 accrual 生产点均通过正式 RED → GREEN。未推送、改 PR、ready、合并或触发其他专家；DAL-418 / DAL-408 保持 in_progress，等待协调器安排独立 tester → reviewer → doc-writer。

## 基线与提交

- 开工 master：`aa9ba13a338f65b66f0fc2620562abfc17736666`；开工远端核实一致，且为增量 parent 的祖先。
- 收尾 master 前进至 `bf4cabea8575206375047e51993a5aa5aeb54c0c`（DAL-413 #622），仅新增另一份验证记录和 `test_gsr_script_acceptance.cpp`；生产/依赖树完全未变。隔离 source 补齐这两个 Git blob，重新 configure/build core、编译正式测试并取得最新 master 的同样 3/3 RED。未改原实现分支或自行 merge；独立 tester 应考虑该新增验证文件的后续集成。
- 实际增量 parent / 远端 PR head：`9c547ab64ff2a53ef444ac928440e443fc720c24`；开工工作树干净，PR OPEN、draft、mergedAt=null。
- 初始正式测试提交：`0ac14fb74696b1d0e80e2426e6270555028410fe`。
- 精度、subnormal、错误、持久化与执行边界测试提交：`4b5e49e7`。
- 产品修复与 parent 快照固定断言：`a4386247`。最终交付 HEAD 的后续证据提交仅含本实施记录和文本证据。

控制输入为 DAL-418 的 spec R1–R7 / AC1–AC8、API D1–D3、Proceed with caveats critique、implementation-handoff，以及 script_calibration_contract 方法学。完整读取了 importer、native domain / AST matcher、附近正式测试和 XCCY 私有 Literal analogue；未改变控制文档或旧 DAL-408 artifact。

## 设计与准确范围

`dal-cpp/dal/model/gsrrateoption.cpp` 新增一个匿名 namespace `AccrualLiteral`：先取原 NumberText，plain 文本原样返回；exponent 文本只通过字符串移动小数点和补零展开。仅 Caplet coupon、Swaption fixed、Swaption floating 三处事件调用改用该 helper。正值校验及其顺序、N/K definition rows、完整 AST、native pricing、公共声明和 parser 均保持。

`dal-cpp/tests/model/test_gsr_rateoption_script.cpp` 新增 9 项正式测试及局部 helper，原断言未修改。新增实施记录和 `implementation/dal-418-evidence/` 为唯一其他修改；没有新增实现分支、issue、PR 或依赖。没有设计偏离。

## RED：两个真实基线

首次测试草稿曾因 EuropeanRateOption 含 BondOption、generic visitor 分派过宽而编译失败，已改为按 location 提取对应 variant；该编译错误不计作 RED。以下均由编译成功的正式测试产生预期 parser 失败。

1. 对 parent `9c547ab6` 的未改产品，按 `build/dal408/compile_commands.json` 的原 flags 重新编译测试 TU 和 test_main，链接重建 core archive，生成独立 `build/dal418-parent/rate_tests`。
2. `master_red.py` 用 `git archive aa9ba13a` 建立隔离 source，只覆盖已提交的正式测试 TU。五个依赖只读指向原 checkout，逐一核对 pins 和无跟踪修改；独立 configure、构建 core/gtest、编译并链接测试 runner，不复用调查 archive 或二进制。

```bash
DAL_NUM_THREADS=1 build/dal418-parent/rate_tests \
  --gtest_filter='GSRRateOptionScriptTest.TestTiny*' \
  --gtest_output=xml:.codex/artifacts/implementation/dal-418-evidence/parent-red.xml
DAL_NUM_THREADS=1 python3 .codex/artifacts/implementation/dal-418-evidence/master_red.py
DAL_NUM_THREADS=1 python3 .codex/artifacts/implementation/dal-418-evidence/refresh_master_red.py
```

两个 master runner 及 parent runner 均 exit=1、3/3 预期失败：Caplet、fixed、floating 全部为 `InvalidScriptInstrumentDefinition /source/rows/2/text / Not a valid number string`，无 fixture/build 失败冒充。测试先计算 native finite Price，并构造独立手写 decimal 完整事件，再调用被测 importer。初始测试提交保留；两个 Python orchestration 脚本在核实预期 RED 后自身 exit=0。

加入边界测试后、产品仍未改时，独立 expanded runner 为 9 项中 7 RED、2 PASS；两个 PASS 为原非法输入 owner 和成功输入快照。原始 XML、日志、compile/link argv 与哈希均在证据目录。

## GREEN 与验收

实际重建最终正式 target：

```bash
cmake --build build/dal408 --target dal_cpp_tests -j 4
DAL_NUM_THREADS=1 build/dal408/dal-cpp/dal_cpp_tests \
  --gtest_filter='GSRRateOptionScriptTest.*:CalibrationPreparationTest.*:ScriptInstrumentTest.*:ScriptLexerTest.*:XccyScriptRepairTest.*:GSRAverageCalibrationTest.TestSingleFixingDegeneracyPreservesAccrualAndDelayedNumeraire' \
  --gtest_output=xml:.codex/artifacts/implementation/dal-418-evidence/target-green.xml
python3 .codex/artifacts/implementation/dal-418-evidence/compare_snapshots.py
python3 .codex/artifacts/implementation/dal-418-evidence/capture_final.py
clang-format --dry-run --Werror dal-cpp/dal/model/gsrrateoption.cpp dal-cpp/tests/model/test_gsr_rateoption_script.cpp
git diff --check
```

全部 exit=0；正式 target **121/121 PASS**：GSRRateOptionScriptTest 22、CalibrationPreparationTest 42、ScriptInstrumentTest 42、ScriptLexerTest 8、XccyScriptRepairTest 6、single-fixing caller 1。较早独立 runner 也是 121/121，最终证据以正式 target 为准。

- **AC1–AC2：完成。** 三位置各 CALL/PUT；固定/浮动另一个 leg 固定 .25。事件与独立手写 `0.0000001` 完整 source 一致，单 NodeConst 和 descriptor 用 ASSERT_EQ 精确比较输入。规则与 FastGaussian backend 保持；Price 等于独立 source，并按非零相对误差 ≤1e-12 / 零精确相等对照 `100*native Price`。
- **AC3：完成。** 三位置分别覆盖 .25、1e-6 两侧 nextafter、1e-7、多有效位 `1.2345678901234567e-7`、`1.2345678901234568e22`、1e20、1e21 两侧 nextafter、normal min/max。原始 AST 为单常量、数值精确相等，定位 accrual token 检查无 exponent、仅数字/小数点且长度 ≤512；极值不强求 prepare/Price。
- **subnormal：完成目标运行库对照。** denorm_min 和 nextafter(min,0) 以独立手写 decimal 完整事件对照，三位置均返回 `stod` underflow；精确比较 Input / InvalidScriptInstrumentDefinition、field、actual、ID、可用 row/date/statement 和 owned report。没有接受任意异常或承诺跨运行库成功。
- **AC4：完成。** 产品编辑前由真实 parent 捕获 36 组普通 CALL/PUT、1e-6、N=1e-7/1e22、K=1e-7/0/负值。逐项比较 504 条记录：source rows/key/text、source/definition/binding/instrument/compiler/context/plan 七类身份、portable JSON 全部相等。ID、pillar=null、context、ACT_360 约定及默认执行选择固定。原始 parent 快照保留，正式测试还固定全部记录的摘要 `sha256:e6449292ee247ec0e27d808d028b63133a23a39d62b761d6126e41b5526ef8b0`，非同版本往返自证。旧失败 exponent 文本没有成功 instrument 身份；新 source 按真实内容生成身份。
- **AC5–AC6：完成。** 普通及新修复 source 分别经 portable JSON、DAL archive 往返，保持 source/definition/binding/instrument 和价格；N/K immutable rebind 以 native 缩放复核。旧 plan 对 changed source/binding/context 均拒绝。三个位置的 0、-0、负数、NaN、±Infinity 共 18 组继续拥有原 Input owner、field、instrument ID 和非空 report。既有日期/index/payment-lag、整源 guards、坏 archive、legacy parser 回归通过。
- **AC7：完成本阶段定向验证。** tiny 与独立 decimal 在相同路径的 tree/compiled 执行分别对照独立金额与 numeraire oracle；libor=0/.04/100000 覆盖每个位置和方向的可观测非零及零 payoff，非零用相对误差，零精确相等。既有 AAD/CRN、rebind、convention invalidation、rank/domain、零 exposure/payoff 拒绝及 recording cleanup 测试通过。未改变 sampling、预算或容差。
- **AC8：后续门禁待办。** 本阶段局部验收完成；完整本地矩阵、独立 tester/reviewer、doc-writer 判断、最终发布和完整 CI 尚未执行，不宣称已通过。

## 来源、性能范围与证据修正

`final-manifest.json` 固定 source、最终正式 executable、core archive、compile_commands/cache 的 SHA-256、测试/产品提交、五个依赖 pins、RED/GREEN XML 汇总。原始 log/XML 压缩保存在 `raw-evidence.zip`，保留原字节及行尾空白，两个读取脚本直接支持该 archive；不修改原件来绕过 diff 空白检查。环境为 WSL2 / Intel i9-13900HX、GCC 14.3、CMake 4.2.3、Release O3 C++17、静态 native AAD、Eigen ON、native arch OFF、DAL_NUM_THREADS=1。构建配置未改。

master 构建最初在结束时错误记录了随后已编辑的工作副本测试哈希；核对发现后，改为实际隔离 TU 并逐字节验证等于 `0ac14fb7` 的 Git blob。保留 `master-manifest-original.json`，修正 manifest 明示原因；脚本已改为构建前捕获并验证 immutable test 输入。master RED 二进制、命令、测试提交和 XML 均未变。

性能按实际 caller 排除无关矩阵：本次只增加 import 时的线性字符串展开，不进入 prepare/Price/MC/AAD 循环；全库搜索除本 suite 外唯一现存调用为 single-fixing degeneracy 测试，已纳入定向验证。现有 benchmark、examples、facade/Python/Excel 无该 importer caller，九项性能门禁也不覆盖该路径，因此本阶段没有选中 timing case、未运行或宣称全 benchmark regression pass。新 benchmark 若有需要应单独覆盖 importer 普通/指数/极值批量构造，不在本次扩范围。现有定价/采样数值回归继续通过，输出长度和常量精度由正式边界测试保护。

## 交接限制

未运行 build_linux.sh 完整本地矩阵、Windows、Python、Excel/XLL、所有 frozen 数值矩阵或 CI；后续由独立 tester 按仓库与 issue 门禁执行，reviewer 审本增量，doc-writer 判断文档/CHANGELOG。没有已知实现阻塞。协调器完成各阶段前不推送或 ready；旧 PR head 的 CI/review 不能替代最终 head。本轮无后台工作遗留。
