# DAL-418 BG-1 focused review

## Findings

无发现。此次 BG-1 增量未发现正确性、安全、兼容性、方法学或必要测试覆盖的阻断项。

受审对象为尚未推送的本地 **`378ca2f52a44964bb4025d6ce90fd4aeadaa48cb`**，原分支 `agent/dal-implementer/e2075e8549aa`，开始审查时工作树干净。结论仅覆盖以下 BG-1 增量及其证据，不表示完整交付门禁通过。

- 实际增量 parent：`9c547ab64ff2a53ef444ac928440e443fc720c24`。
- 正式测试提交：`0ac14fb74696b1d0e80e2426e6270555028410fe`、`4b5e49e70fd561a2eca363cd0c5047f3d7acfcc6`；产品修复：`a4386247bef912483aa2d736ee1f037caf6527a5`。
- tester 验证源：`6372ede9452b2ec912234a373666dda9b239a1bc`，正常 merge 的 parents 为 `0ba2e15412aa3ca8184d9e704fed84b0f03f0c79`、`bf4cabea8575206375047e51993a5aa5aeb54c0c`。后续 `378ca2f5` 仅增加验证记录与证据。
- 实时只读核实 [PR #620](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/620) 为 OPEN/draft，base=master，远端 head 仍为 `9c547ab64ff2a53ef444ac928440e443fc720c24`；`git ls-remote` 确认 master 仍为 `bf4cabea8575206375047e51993a5aa5aeb54c0c`。PR metadata 和远端 diff 均未含本次 BG-1 修复，旧 head 的 CI 不适用于本地受审版本。

## Open Questions

无待作者或用户回答的 BG-1 代码问题。

交付仍有明确未完成项：doc-writer 的文档判断、DAL-419 对默认 Windows 安装后构建的独立修复与验收、修复合入后的 #620 集成，以及最终推送 head 的完整 CI/保护门禁。CP936 下 `dal-cpp/dal/script/visitor/debugger.hpp:308`、`:309` 的两项默认安装失败仍阻断 AC8；`CL=/utf-8` 的诊断对照没有豁免效力。该独立根因已获派，本轮未重复派工，也未要求把它混入 BG-1。

## Tests

### 本 reviewer 实际执行

在已核验来源的 tester Linux 正式二进制上执行：

```bash
DAL_NUM_THREADS=1 build/Release-linux/dal-cpp/dal_cpp_tests \
  --gtest_filter='GSRRateOptionScriptTest.*:GSRAverageCalibrationTest.TestSingleFixingDegeneracyPreservesAccrualAndDelayedNumeraire'
clang-format --dry-run --Werror \
  dal-cpp/dal/model/gsrrateoption.cpp \
  dal-cpp/tests/model/test_gsr_rateoption_script.cpp
git diff --check 9c547ab6 378ca2f5
```

结果：**23/23 PASS**（完整 rate-option suite 22 项、直接 single-fixing caller 1 项）；格式与增量空白检查通过。没有新增测试、重建产品、重复完整矩阵或运行 benchmark；复用二进制前已核验 source/dependency/configuration/library/executable 身份。

另外以只读 Python 审计实际重算并核对：

- 作者 final manifest 的 **55 项** source/config/binary/evidence SHA-256 及原始 archive 哈希全部相等；tester **78 项**证据哈希与 archive/report 哈希全部相等。
- tester 当前 Linux 的 **10 项** source/cache/compile-command/library/binary/extension 哈希、原生 Windows 的 **17 项** cache/library/binary/XLL/extension 哈希全部与 manifest 相等；五个依赖的 Git pins 与受审 HEAD 一致，跟踪文件干净。
- 作者初始 master、实际 parent、最新 master 的原始 XML 各为三位置 **3/3 预期 parser RED**；tester 独立最新 master XML 亦相同，均为 `InvalidScriptInstrumentDefinition` / `Not a valid number string`，没有把编译失败算作 RED。作者正式 target 为 **121/121 GREEN**，tester 新鲜 AC 回归为 **9/9 GREEN**。
- 实际 parent 的 **504 条** source rows、七类身份和 portable JSON 记录，与作者 target、tester 独立 parent、Linux、Windows XML 全部逐项相等；parent JSON 快照相等。正式断言还固定组合指纹 `sha256:e6449292ee247ec0e27d808d028b63133a23a39d62b761d6126e41b5526ef8b0`。
- 初始 master manifest 修正仅涉及测试源码哈希及解释；隔离 RED TU 逐字节等于 `0ac14fb7` 的 Git blob，原 manifest 和原命令/二进制/XML 保留。修正没有替换 RED 事实。
- #622 传入的 acceptance 测试及验证记录与 `bf4cabea` 的 Git blobs 完全相同；合并仅增加这两项文件，没有生产、依赖或 build 配置差异。按授权仅核对来源，不重审其已接受内容。

### 核验并复用 tester 的完整结果

以下为他人执行、本 reviewer 核验原始证据后的复用结果；没有声称本轮重跑。

- Linux 原完整脚本：3,883/3,883 PASS。
- 原生 Windows 原完整脚本：3,967/3,967 PASS。
- Windows workspace Python：1,461 passed/3 skipped。
- Windows installed Python：1,460 passed/4 skipped。
- Linux 独立示例：37/37 PASS；installed consumer：3/3 PASS。

保留最初 Linux PATH 导致的三个配置 timeout 原日志；修正工具 PATH 后原脚本通过，测试/timeout 未改。核对 Windows 默认 consumer 和 installed Excel tests-ON 的 C2001/C4819 原日志、driver 及 parent 文件身份；诊断控制分别 3/3、259/259 通过，默认门禁仍失败。安装失败发生在未改头文件的编译阶段；本次没有重新构建完整 parent consumer，因此独立根因归属依赖已记录的相同文件/命令及窄对照证据，不把它称为本轮完整 parent 实验。

## Summary

完整读取 `dal-cpp/dal/model/gsrrateoption.cpp` 和 `dal-cpp/tests/model/test_gsr_rateoption_script.cpp`，以及活动 spec、API、critique、implementation、validation 和证据生成/审计脚本。按仓库 reviewer/style/unit-test/component-boundary 契约核对必要的 NumberText/JCS、lexer/ParseConst/ToDouble、owning parse error、完整 AST matcher、native domain、instrument/plan identity 与方法学上下文。

- `dal-cpp/dal/model/gsrrateoption.cpp:11` 的私有 helper 先保留原 plain 文本，只对受控正有限 binary64 的 exponent 文本移动小数点、补零。位置使用有符号整数，指数有界；不经浮点运算，不截断有效数字，输出保持一个常量。正常数与 subnormal 展开长度均小于 512；subnormal 继续服从原 `std::stod` 的成功/underflow 包装。
- `dal-cpp/dal/model/gsrrateoption.cpp:69`、`:80`、`:95` 覆盖 fixed、floating、Caplet 三个生产点。原 guard 及顺序、N/K definition rows、parser、完整匹配、native kernel、公共签名/defaults、identity/schema/version 均未改。
- `dal-cpp/tests/model/test_gsr_rateoption_script.cpp:261` 以精确 `ASSERT_EQ` 检查单 `NodeConst_` 和多有效位 exponent、切换点两侧、normal min/max；`:280` 以独立手写 decimal 对照 subnormal 值/owned error。极值只要求 import/AST，未错误扩大 prepare/Price 承诺。
- `dal-cpp/tests/model/test_gsr_rateoption_script.cpp:327`、`:350`、`:383`、`:422` 覆盖非法 accrual owner、双 archive/rebind/stale plan、CALL/PUT 的非零及零付款/numeraire、真实 parent 兼容快照。旧断言、数值容差、采样和 AAD guards 保留；未用相似实现或同版本往返代替 parent 证据。
- 性能变化限于 import 时的有界线性字符串展开。caller 搜索确认除本 suite 外的现存调用是已执行的 single-fixing 测试，没有 example/facade/Python/Excel/benchmark caller；没有新增 price/MC/AAD 循环工作或可变共享状态，不要求无关完整性能矩阵。
- 本次无 example/build/公共 API/生成文件/依赖变化或跨组件反向依赖。现行方法学的正 accrual、source 权威及身份契约与修复一致；文档和 CHANGELOG 的最终处理留给已规定的 doc-writer 阶段。原 XCCY 和 #622 已接受内容未重新全审。

残余测试风险：本轮聚焦运行复用了已核验的 Release/native-AADET 构建；未执行所有编译器、diagnostic/profiling、sanitizer、wheel/macOS CI legs，也未解决两项默认 Windows 安装门禁。未来集成改动须重新确认来源适用性，最终远端 head 必须与本地受审产品/测试身份核对；当前不能宣称 merge-ready。

## Verdict

**Approve** — 仅批准本地 `378ca2f52a44964bb4025d6ce90fd4aeadaa48cb` 的 BG-1 代码增量及适用证据。AC1–AC7 无未解决审查发现；AC8 仍未通过。

本轮仅写此审查报告，未改产品/测试/其他产物，未提交 GitHub review、resolve threads、commit、push、ready、merge 或派工。DAL-418 / DAL-408 保持 `in_progress`，PR #620 保持 draft；后续门禁交协调器接续。
