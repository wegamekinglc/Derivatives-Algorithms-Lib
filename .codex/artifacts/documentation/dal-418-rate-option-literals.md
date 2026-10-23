# DAL-418 BG-1 文档核对

已完成文档阶段：仅在现有方法学中补充 accrual 事件编码及数值边界说明，不新增 CHANGELOG 条目或教程。本轮以原 DAL-408 checkout、原分支 `agent/dal-implementer/e2075e8549aa` 的受审 SHA `378ca2f52a44964bb4025d6ce90fd4aeadaa48cb` 为唯一产品基线；未改产品、测试、生成文件或 build 配置。

## 真实差距与处理

完整读取 `docs/methodology/script_calibration_contract.md`（修改前 735 行）和 `CHANGELOG.md`（1,401 行）。现有 Gaussian Caplet/Swaption 章节准确说明正有限 accrual、N/K、完整 AST、日期/支付映射、native domain 和 AAD 准入，但没有说明 importer 的 decimal 编码范围，也没有区分 subnormal 解析边界与 normal 极值的定价准入。这是本轮唯一需要补充的文档差距。

在该章节加入两段、共 13 行当前支持范围：

- Caplet coupon、Swaption fixed/floating 三处 accrual 写为单个 plain-decimal 事件常量；`1e-7` 写作 `0.0000001`，保留有效数字及 normal 正 double 的解析值。
- 已为 plain-decimal 的 accrual 文本保留；同输入、绑定和上下文下，既有成功 source rows、身份及 portable JSON 保持。N/K definition rows、通用 `NumberText`/canonical JSON 和原事件 DSL 不扩大或改写。
- subnormal 仍服从现有十进制解析器的 underflow 行为，可能被拒绝；normal 极值的 import/常量保真不等于 native prepare/Price 支持。

没有宣称事件 DSL 新增科学计数语法、subnormal 普遍可导入、极值普遍可定价，或普通日期日计数交易普遍存在缺陷。旧失败 source 改写后按真实文本生成新身份；没有承诺其保留旧失败文本的指纹。原文的完整 AST、金融定义、错误和持久化契约继续适用。

**CHANGELOG：不新增条目。** 本项修正既有 importer 的有限数值编码边界，没有 breaking API、参数/defaults、数值方法、算法、显著能力或方法学转变，也没有公开移除/弃用。不满足 `.codex/agents/dal-doc-writer.toml` 和 CHANGELOG 自身的收录标准。现有 Gaussian Caplet/Swaption 能力条目及 XCCY 条目保留原字节。

没有新增、移除或重命名 methodology 文档，故 `docs/README.md` 和 `CLAUDE.md` 方法学清单无需更改；既有链接继续指向同一章节。没有新增 example 或绑定，故不需要新 core/facade 示例、run path 或组件描述。跨组件 importer caller 搜索结果与受审记录一致。

## 依据与适用范围

按 repository `AGENTS.md`、`.codex/agents/dal-doc-writer.toml`、code-style 和 component-boundaries 执行。完整读取本项 spec、API note、critique、implementation、validation 和 reviewer 报告；spec/API/critique 位于协调器统一 checkout，implementation/validation/review 位于原 #620 checkout，未复制或改写这些记录。

源码核对包括：

- `dal-cpp/dal/model/gsrrateoption.cpp` 与 `.hpp`：完整读取私有 `AccrualLiteral`、三个事件调用点、原校验及 N/K `NumberText` 定义行，确认公开声明不变。
- `dal-cpp/dal/script/parser.cpp` 的 `ParseConst`、`dal-cpp/dal/string/strings.cpp` 的 `ToDouble`、`dal-cpp/dal/script/instrument.cpp` 的 parse error 包装：常量仍经 `std::stod`，保留 underflow/owning error 边界。
- `dal-cpp/tests/model/test_gsr_rateoption_script.cpp`：核对三位置 tiny literal、精确 normal 边界、独立 subnormal decimal 对照、非法输入、archive/rebind/stale plan 和实际 parent 兼容快照断言。
- 搜索 `dal-cpp`、`dal-public`、`dal-python`、`dal-excel` 中 importer/Price 的声明及调用：没有新增 facade/Python/Excel importer；没有把已有 native option 绑定写成新增脚本入口。

主审 **Approve** 仅适用于本地受审 SHA 的 BG-1 增量及适用证据；独立 tester 的 **AC1–AC7 PASS** 已核对，未在本轮重新执行产品矩阵。两项 Windows 默认 installed-consumer / installed-Excel tests-ON 构建仍失败，`CL=/utf-8` 只作诊断，不能替代默认门禁。DAL-419 在独立分支处理该根因；本轮没有纳入或重派其工作。最终推送 head 的完整 CI 尚未执行，**AC8 未通过，不能声称全部门禁成功或可合并**。

## 检查与产品一致性证明

文档检查：

```bash
python3 .github/scripts/check_docs.py
git diff --check
git diff --cached --name-status
git diff --cached --check
git diff --exit-code 378ca2f52a44964bb4025d6ce90fd4aeadaa48cb -- . ':(exclude)*.md'
```

文档 checker 检查本地链接/anchors、Markdown 表格、数学宏及既有文档约束；加入本报告后为 **94 Markdown files PASS**。另检查两个本轮 Markdown 的 UTF-8、无 trailing whitespace、单个最终 newline 和完整 fence；未新增表格。发布文稿新增文字逐项与上述源码和受审断言核对，无交付历史、活动计划或 CI 成功叙事。

非 Markdown 核对覆盖受审 tree 的 **2,797 项**：2,792 个普通文件和五个 submodule gitlinks。2,787 文件原始字节与受审 Git blob 相同；以下五个 checkout 文件仅有既定 CRLF/LF 差异，按 Git clean filter 后的 blob ID 与受审 SHA 相同，未改行尾规则：

- `build_windows.bat`
- `dal-excel/tests/windows/run-script-fix-settings.ps1`
- `dal-excel/tests/windows/test-script-fix-cleanup.ps1`
- `dal-python/build_wheel.ps1`
- `dal-python/run_tests.ps1`

五个 submodule 的 HEAD pins 与受审 tree 一致，跟踪内容无改动。上述只读审计对所有非 Markdown 文件计算 Git blob SHA-1 并逐项比较；行尾差异另以标准化字节及 `git hash-object --path` 验证，没有将所有工作副本文件误称为 raw-byte 相同。`git ls-tree -r -z` 排除 `.md` 后保留原顺序/尾 NUL 的记录集 SHA-256 为：

```text
9f850672c14e0d3c3695f719c72a4ac4e168f0a59addb326bd6e79ee550a02cb
```

原 XCCY 章节及其后所有章节逐字节不变；`CHANGELOG.md`、`docs/README.md`、`CLAUDE.md` 与 #622 已接受的验证记录逐字节不变；#622 acceptance 测试包含在上述非 Markdown 一致性审计中。既有 reviewer 报告保持未跟踪、未交叉暂存，其 SHA-256 保持：

```text
cfb1e9ffa503c79a7f7ac256d0e0ee3bb86be3545cbe7e465e1bf6d00789e556
```

本轮本地提交仅包含方法学补充和本报告；具体 commit hash 随 issue 评论交付。未推送、ready、修改 GitHub review、合并或派专家。只读实时核实 [PR #620](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/620) 仍 OPEN/draft，远端 head 为 `9c547ab64ff2a53ef444ac928440e443fc720c24`、`mergedAt=null`；旧 head CI 不能证明本地 BG-1。DAL-418 / DAL-408 均保持 `in_progress`，由协调器接续剩余安装和最终 CI 门禁。
