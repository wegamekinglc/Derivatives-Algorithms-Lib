# DAL-418 test complexity limited review

## Findings

无发现。本轮测试复杂度修正与直接整合差异未发现正确性、断言/覆盖弱化、方法学、兼容性或必要验证阻断项。

精确受审本地 HEAD：**`ad604e82955dbf7df4732c4796b54421e588a856`**，原 DAL-408 checkout、分支 `agent/dal-implementer/e2075e8549aa`，开工工作树干净。其 parent 为正常整合 **`c590d15398a5f5c54933f14371628a95f8661eef`**，真实双 parent 为：

- 旧 PR head `1d42e54c1240a8114bcb895d6895870008160b8c`。
- 已接受 master `2baceaf083258a657dc954642e7c279507fb5721`。

仅复核目标 TEST、两个私有测试 helper、必要活动报告及该 merge 的直接交互/身份。原产品主审、独立验证及文档结论保持适用；未重开已接受上游产品全审。

## Open Questions

无代码或接口待决问题。

交付仍待原作者执行新整合源的完整本地门禁、推送同一 PR 和最终 head 完整 CI。旧 head 的 **35 SUCCESS / 1 Codacy ACTION_REQUIRED** 仍是失败事实；本地 Lizard 通过不能提前认定 Codacy 或 CI 通过。

实时 GitHub metadata 与 `git ls-remote` 确认远端 #620 仍为旧 `1d42e54c`，OPEN、已 ready，远端报告 CONFLICTING；本地尚未推送的 merge 不会更新该远端状态。master 仍为 `2baceaf0`。本轮没有改变 PR 状态，也不认定当前具备合并条件。

## Tests

### 本 reviewer 实际执行

```bash
lizard -l cpp -C 8 -w dal-cpp/tests/model/test_gsr_rateoption_script.cpp
DAL_NUM_THREADS=1 build/dal408/dal-cpp/dal_cpp_tests \
  --gtest_filter=GSRRateOptionScriptTest.TestTinyAccrualTreeCompiledAmountsAndNumeraire
clang-format --dry-run --Werror --lines=97:112 --lines=399:426 \
  dal-cpp/tests/model/test_gsr_rateoption_script.cpp
git diff --check 1d42e54c ad604e82
```

全部 exit 0；本人目标回归 **1/1 PASS**，未重建、重复 116 项或重跑全矩阵。复用标准 Release runner 前已重新计算并确认其 binary、core archive、目标 TU object、CMake cache、compile commands 和 link 命令与作者证据完全相同。

另执行只读源码/树/证据审计：

- 下载的证据 archive SHA-256 为 `5a80116c919c70e2a286be4d7a823d2782fcaa62d4c0a7755761e8c1e0abfcc2`，CRC 正常；86 项 inventory 列出的内容逐项大小/哈希相等，加 inventory 本身共 87 个 archive 文件。
- 旧测试证据逐字节等于 `1d42e54c` Git blob；新测试证据逐字节等于受审 Git blob/工作副本，SHA-256 为 `3aa323cf78f006e33f68a658aad11217d2255a49d8e64d5517ae176bddd7a3aa`。
- 用 Lizard 重算新旧源码指标，旧目标 CCN=14，新目标=7，`TinyAccrualExpectedValue`=5、`TinyAccrualPath`=4；全文件 50 个函数最大 CCN=8。与附件同口径 Lizard 1.23.0 指标一致，没有修改规则或阈值。
- 去掉两个新 helper 并还原其调用后，其他源码逐字节等于旧版；helper 算式逐行相等，路径映射除形参替换、格式和 `return path` 外相等。原 TEST 的 fatal assertions、矩阵、两个 instrument 构造和两个 evaluator 调用原样保留。
- 直接解析原始 `matrix.gcov.json.gz` 并对照当前源码行、source hash、诊断 binary/object/core/GTest hashes，确认 **18** 次 oracle、**36** 个 instrument/path、**72** 次付款判断，精确零 ASSERT_EQ 与非零相对误差 ASSERT_LE 各 **36** 次。诊断编译仅添加 `-O0 -g --coverage -fno-inline`，维护源码未插桩；没有重新运行会累计旧 profile 的 coverage binary。
- 重算真实 Git tree、merge parents 和双方输入的三方合并结果，确认两处冲突为准确文本并集、guide 为 Git 自动合并原结果，以及下述所有路径身份。

### 核验并复用作者结果

已核验原始 argv/exit、日志、XML 和产物身份：标准 `dal_cpp_tests` Release 重建成功；目标 **1/1**、相关回归 **116/116**、独立 coverage 诊断 **1/1** 均无失败/错误/disabled/skipped。

116 项包括完整 `GSRRateOptionScriptTest` 22、preparation 42、instrument 42、lexer 8，以及 DAL-414 两项 completed-summary/owned-axis 直接交互回归。未把结构性 CCN RED 描述为功能测试 RED；原构建日志的无关 nodiscard 警告及历史 watch EOF/同 head 传输恢复事实保留。

旧完整 Linux/Windows/Python/安装结果属于其旧整合源。本轮不把它们改标为 `ad604e82` 的新完整验证，也没有再跑完整矩阵。

## Summary

完整读取变更后的测试文件、实施活动报告、原 publication 记录、控制 handoff 和必要的原主审依据，核对实际最小 diff、双方 Markdown 增量及证据审计/coverage 方法。已读取的 reviewer/style/unit-test/component-boundary 指导与当前源码版本相同。

- `dal-cpp/tests/model/test_gsr_rateoption_script.cpp:97` 的 `TinyAccrualExpectedValue` 仍独立计算 Caplet/fixed/floating、CALL/PUT 金额；不读取生产 formatter、执行器或 Price 结果作为期望。N=100、DF=.91、numeraire=1.7、tiny=1e-7、K=.03 及浮动/固定 leg 原 alpha 均未改变。
- `dal-cpp/tests/model/test_gsr_rateoption_script.cpp:104` 的 `TinyAccrualPath` 返回新 scenario；每个 sample 的 numeraire 仍为 1.7，每个 observation 仍按 LIBOR 名称选 libor 或 .91。没有全零替代、共享状态或隐藏分支。
- `dal-cpp/tests/model/test_gsr_rateoption_script.cpp:399` 保留三个 location、两个方向和三种 libor 的 18 组矩阵、tiny importer/独立手写 decimal 的 36 场景及 tree/compiled 的 72 观测。`:419` 的零分支仍精确 `ASSERT_EQ(actual, 0.)`，非零仍为原 `ASSERT_LE(abs(actual/expected-1), 1e-12)`。断言留在 TEST，fatal 失败不会只退出某个新 helper 后继续执行。
- `c590d153` 的 merge tree 为 `7fba43db4b6d063c4db2da63763a2e1040e1e8fd`。真实 overlap 仅 artifact README、CHANGELOG 和 methodology guide；前两项的 conflict 两侧文本全部原样保留并以空行分隔，guide 与 Git 三方自动合并完全相同。重算全文件字节验证，未只凭摘录推断。
- **30 个**上游独立路径的 mode/type/blob 与 `2baceaf0` 相同，包括上游产品、binding、test、example/CMake/生成文件；整合时上游范围外 **3,002 个**第一 parent 条目保持身份。正常整合后只增加测试整理及实施记录，其余 **3,034 个**整合条目不变。
- BG-1 生产仍为已审 blob `f607d520596275f6e0bc2fb49f0c096d47174121`；原 XCCY/preparation/回归受保护 blobs、质量/依赖/构建设置保持。原 review 原文与已交附件逐字节相等，SHA-256 仍为 `cfb1e9ffa503c79a7f7ac256d0e0ee3bb86be3545cbe7e465e1bf6d00789e556`。
- 本地独有修改没有公共 API、算法、数值预算、采样、容差、timeout 或组件依赖变化。上游 example/build 内容仅核对接受版本身份，不重复审查或运行无关矩阵；本次测试职责提取无需新 benchmark、设计链或 CHANGELOG 条目。

残余验证风险是新 DAL-414 整合的完整本地门禁及最终 CI 尚未执行，特别是新的 GSR/binding/build 输入不能由旧完整矩阵或本次定向检查替代。没有发现需在本轮先行修改的真实代码 blocker。

## Verdict

**Approve** — 仅批准本地 **`ad604e82955dbf7df4732c4796b54421e588a856`** 的测试复杂度修正与 **`c590d15398a5f5c54933f14371628a95f8661eef`** 直接整合差异；无未解决审查发现。

本轮只新增本报告，保留原报告，未修改代码/其他产物、commit、push、提交 GitHub review、resolve、ready、merge 或派工。DAL-418/DAL-408 保持 `in_progress`；协调器接原作者完成完整本地门禁与最终 head CI 后再验收合并条件。
