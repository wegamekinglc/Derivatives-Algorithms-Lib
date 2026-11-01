# DAL-479：全量文档与 agent 契约一致性证据

## 基线与范围

本轮实际执行 `multica repo checkout` 创建独立干净分支
`agent/dal-doc-writer/4c2bcceaf841`，未使用协调器检出代替本地复核。
`git fetch origin master` 后，`HEAD`、`origin/master` 与
`git ls-remote origin refs/heads/master` 均为
`2ee93eff5026b3d7e9f297017f6d4e352e675888`。PR base 为 `master`。
2026-11-01 04:10 UTC 再次 fetch；远端 master 与本轮固定基线仍一致。

重新读取 Git 清单：基线 **162** 个 Markdown，59,140 行、5,048,365 字节。
每个文件均完整解码、解析和执行结构/引用检查，未抽样；包括全部嵌套 README、
五个 SKILL、`.claude/**`、`.codex/**`、`.github/**` 及 7,000 行数据集索引。
另外解析十个 agent TOML、一个 skill `agents/openai.yaml`，对照十个 Claude agent
frontmatter/body 与实时 DAL roster。基线全量检查为 8,157 个解析链接、673 个
当前说明中的 DAL 符号，零错误。交付清单删除四份完成收据、增加本记录，共 **159** 个 Markdown。

这里的全量读取指静态程序对每个完整文件作检查，不将机械名称存在性检查描述为
逐签名编译或所有数学推导的重新证明。实质修改目标全文阅读，并对照源码、
public headers、绑定、示例、测试和活动控制记录；历史记录按其明确基线解释。

## 修正内容与来源

- `dal-public/README.md`：新增七个现有公共示例的完整目录、构建/CTest/直接运行方式、
  安装位置和统计限制；修正把全部 core 示例概括为 native instruments 的说法。
  来源为 `dal-public/examples/CMakeLists.txt`、各程序的 CMake 与源码、
  `dal-public/CMakeLists.txt` 和 `CMakePresets.json`。
- `dal-cpp/examples/README.md`：公共示例入口改指完整 facade 目录，保持原 core 位置与依赖。
- `docs/README.md`：补公共 C++ 示例导航；无方法学文档增删或重命名，`CLAUDE.md` 的方法学清单无需变更。
- `.codex/artifacts/README.md`：纠正 DAL-425/396 仍在进行中的陈述，登记遗漏的五份冻结
  证明/执行控制，明确停止的采集链、历史状态与当前状态的区别，并更新本轮维护入口。
- 退役 `.codex/artifacts/documentation/dal-436-documentation-agent-consistency.md`、
  `.codex/artifacts/implementation/dal-469-benchmark-repair.md`、
  `.codex/artifacts/implementation/dal-476-basket-covariance-overflow.md` 和
  `.codex/artifacts/reviews/pr651-full-review.md`：完成交付的收据不再控制活动工作。
  固定基线保留全部原字节，恢复命令在产物索引。DAL-436 检查程序已用于本轮重算。
- 本文件：本轮新清单、检查结果、同步证据与复现程序。

任务状态通过 `multica issue get <identifier> --output json` 分别读取
DAL-436、DAL-469、DAL-476、DAL-425、DAL-396，五项均为 `done`。另读取 DAL-425 最后用户线程
`01a17999-5a4e-7e86-8d7a-55c5c7acfbc3` 与 DAL-396 收尾线程
`01a179a1-f62a-7d33-b358-f9394cc85fa7`：收尾按原单元测试与适用性能门禁范围，
停止运行器细节修复和采集链，保留历史金融 FAIL 与未资格化标志。

XCCY 模型决定、数值记录、参考和证书保持原字节；没有放宽阈值、重新执行 MC、
把旧二进制当作当前源码执行，或把任务关闭解释为完整 V1/quote-risk/P4 数值验收。
尤其四个 global qualification flags 仍为 false，保存输出的严格 RED、
原 18+6 FAIL、288/216/214 计数、原精度/combined/FD 限制均不变。
保留的冻结控制仍供精确来源身份和证明重放使用；此处更新索引而不改写其历史正文。

没有 API、算法、重大能力、方法学或公开移除变更，因此不增加 CHANGELOG 条目。
仅 Markdown 变更；C++、生成文件、测试、构建、依赖与 agent TOML 均未修改。

## Agent 同步结论

权威为最新 master 的根 `AGENTS.md` 与 `.codex/agents/*.toml`。
十个 Claude agent 的摘要和去除注册注释后的完整正文分别等于 TOML 的
`description` 与 `developer_instructions`；三对规则镜像逐字节相同。
五个 SKILL 的 YAML frontmatter 及一个 openai.yaml 均可解析，名称、摘要与入口有效。

从 DAL squad `2b409a72-57c6-447a-a252-a8cff3208dc0` 读取实时 roster，
十个名字一一映射十个 TOML，无缺失、重复或额外 agent。
逐个读取 `multica agent get <id> --output json` 后，十组
`description`/`instructions` 均已逐字相同。**实际同步写入为零，未调用 agent update。**
所有 description 少于 255 个 Unicode 字符。前后快照比对受保护字段和完整 roster，
没有 runtime、model、thinking/service tier、skills、环境、MCP、visibility、权限、
并发、头像、成员或角色变更。字段名称和指纹算法见复现程序。
环境和 MCP 仅比较 `agent get` 暴露的标记/脱敏值，未读取秘密；本轮没有任何配置写调用。

保留平台差异：Claude 的 `model: inherit`、color 与 YAML 注册；Claude 四个
user-invocable skill 对应 Codex 一个 `dal-git-pr` 和共享引用；Codex TOML 注册；
Multica 的 runtime、skills、权限、调度与 roster 元数据。这些差异不扩展共同职责或权限。
roster 部分 role 摘要（如“不强制逐步 TDD”）不等同于 TOML 行为契约；
本任务禁止修改 role，继续以 instructions 为运行契约，不将其当同步阻塞或擅自更改。

## 验证与交付边界

仓库检查：`python3 .github/scripts/check_docs.py`。
全量扩展检查：后附程序，基于 CommonMark 解析全部 Git Markdown 的链接和锚点，
复用仓库表格/空白检查；记录所有 code-region 路径候选，区分当前引用、历史引用、
组件相对路径、初始化依赖和本地生成结果。
还核对当前指南中的 DAL include、GTest suite 和类型/选项名称，
数据集 qa/validation 的 7,000 条、70 个 family、唯一 ID 和已保存 summary 摘要。
数据集历史 pass 结果不是本轮重跑产品测试的声明。

七个 facade 目录与 README、可执行目标、链接方向、安装项、CTest 与 label 逐项一致。
新文档命令依据当前 CMake 选项和 target 定义核对；未执行产品构建或示例。
本轮仅文档维护，无生产调用者或算法性能变化，性能 case 集为空；
不重复 C++、Python、Windows/Office、MC、数值证明或性能矩阵。
这不把任何未运行项记为通过。独立审查由协调器安排，未自行调度其他专家或合并。
父 issue 保持 `in_progress`，等待后续独立验收。

最终验证命令、完整分布与逐文件证据列于下方；本记录自引用哈希不递归保存。

## 最终检查结果

- `python3 .github/scripts/check_docs.py`：121 个默认范围文件通过；其范围之外的文件同样参加下面全量检查。
- `python3 -m unittest discover -s .github/scripts/tests -p test_check_docs.py -v`：34/34 通过。
- 扩展程序：159 个交付 Markdown、8,174 个解析链接、673 个当前 DAL 名称；零结构/名称错误。
- 十个 TOML、十个 Claude frontmatter/body、五个 SKILL、一个 skill YAML、三对规则镜像全部通过。
- 十个 Multica agent 契约一致，description/instructions 写入零；前后 roster 与受保护字段相同。
- 七个 public 示例与 CMake 注册逐项一致；全部 49 个现存 Markdown 产物均已登记到索引。
- `git diff --check` 与暂存区检查通过；仅九个 Markdown 路径变更。
- 无阻塞；没有需要用户澄清的开放问题。roster 展示 role 的非同步字段差异按上述权限边界保留。

## 文件分布

| 目录         | 基线 | 交付 |
|------------|----|----|
| (root)     | 5  | 5  |
| .claude    | 28 | 28 |
| .codex     | 64 | 61 |
| .github    | 3  | 3  |
| dal-cpp    | 4  | 4  |
| dal-excel  | 4  | 4  |
| dal-public | 1  | 1  |
| dal-python | 2  | 2  |
| datasets   | 2  | 2  |
| docs       | 48 | 48 |
| tests      | 1  | 1  |

## Agent 逐项证据

instructions 和受保护字段 SHA-256 均为本轮重新读取后生成；前后相同。

| agent            | Multica ID                           | 摘要字符数 | instructions SHA-256                                             |
|------------------|--------------------------------------|-------|------------------------------------------------------------------|
| dal-api-designer | 62641d17-7114-4726-976c-64ee433065dd | 64    | 1e0afa9d5bd172c26699911e889794a0c23de90981e16043812c27aa6724ad0a |
| dal-critic       | 5d24a66b-d46b-4e2b-ba87-a273fa37a20b | 56    | da14ba8d61214f1c567c0d56d514fd9596a2c6e0fddf3f5627cf106d150fa2b1 |
| dal-doc-writer   | 92053497-eaee-47b5-9dcf-8e6e302e1348 | 76    | e5268889e37abf198cd6aaa29cb6a9bbf32566d9283d2a8dcad1beff9c781b8a |
| dal-implementer  | 2c2adb0a-dba4-4095-844a-d47c9635a518 | 59    | 40a176c669f143fc9f9318916ccc55dcd906154f3f94bcc73e0925e735cc5fc7 |
| dal-orchestrator | 36a667e5-f5b7-460c-83a2-d1575be5f12a | 56    | aad4d429572f882bc9f32a1caa6cc7554dd22c4db7160bb5d2cb6d927609debf |
| dal-performancer | 6feb9efa-bc37-46f8-bf13-6a2d3d1ce454 | 62    | b994293688713edcaa3bde9eeae507555f0ac811235399bf11e64fb05f19e390 |
| dal-reviewer     | a541f9c9-b79c-4b31-8ff2-a1178c57a395 | 70    | 1ab98c978883337bcc84fe56843384b4891bcf4df91d505aea3ce80b61e4526b |
| dal-simplifier   | 9fa8445f-7ff7-456d-945a-017c37747348 | 77    | aee02a96c657678dea20debcf6c93e1a3ebb0c6b6973e16e6835a3fe0799ff65 |
| dal-spec-writer  | 0032710b-1be3-40eb-9118-815edd395b53 | 65    | f230216c45f70950d0daae5e519e1317d8deb3c66dd4463c4b3e269096fe8af6 |
| dal-tester       | 59e8e14e-1f1f-47d7-93a0-e2c2f8f53d0b | 68    | 5d4dad5fdf80bdb8b50261492cdc16a598219e258bbdc5519dd055dd2458448d |

| agent            | 受保护字段 SHA-256                                                    |
|------------------|------------------------------------------------------------------|
| dal-api-designer | dcf93880edbdb253f976cd03423bf2c8a2195e5f2c99539c2e53336d8253fd59 |
| dal-critic       | 8187829d7fe8904c11ca4f177ac05b6a261890d4e4ef7a2f052f5b2a0b10a827 |
| dal-doc-writer   | 14f31d991689c66dc279915e455114fd3be11df2df3f2c79ea169c1a30fc1477 |
| dal-implementer  | 19de74b88d34e3c745c9d3e61954b8c0a5e7f2cd20268e3008ea5923c59be284 |
| dal-orchestrator | 60ee489c70f1ab686dd49782f7a1a067259944ed59d87a60e07b40a7047a36d9 |
| dal-performancer | b785239849845de9bb37aa3dee6c39c9cb512dc77d0f36cbdece32e6dc45b039 |
| dal-reviewer     | a93b98a23d1d6464c7c416f231b52c3afee7c210d6204d01715301d88a3acb18 |
| dal-simplifier   | 7b8dce1ee7de54eb22e76efa82122fb25f3b729bd03a273f5924eb9e48eafb00 |
| dal-spec-writer  | 46ca78df174199746e6fdabd28ebc8e3d2d97c4965a3d2977abf6a5fcb5005ab |
| dal-tester       | c705809fdce290dfc1df4490eab0a48af7f55aee885f481c25e7fd351979a4f2 |

## 逐文件基线清单

下列 162 项均参加基线全量检查。指纹为基线内容 SHA-256 前 16 位，完整值可从上述固定 Git SHA 重算。
“保留”表示参加完整静态检查后保留内容；“修正”表示本轮已修正；“退役”表示固定 Git 基线可恢复。

| 文件                                                                          | 基线 SHA-256 前缀    | 行数   | 处置 |
|-----------------------------------------------------------------------------|------------------|------|----|
| `.claude/agents/README.md`                                                  | 393f8f48877cd657 | 75   | 保留 |
| `.claude/agents/dal-api-designer.md`                                        | 51379dd89a8fb629 | 17   | 保留 |
| `.claude/agents/dal-critic.md`                                              | ef5abfc77268a473 | 15   | 保留 |
| `.claude/agents/dal-doc-writer.md`                                          | 0adc325834193619 | 17   | 保留 |
| `.claude/agents/dal-implementer.md`                                         | 53687dd8397407f1 | 17   | 保留 |
| `.claude/agents/dal-orchestrator.md`                                        | 7763d601878c3b19 | 23   | 保留 |
| `.claude/agents/dal-performancer.md`                                        | a58367b0b076a93d | 17   | 保留 |
| `.claude/agents/dal-reviewer.md`                                            | 023c59cb98833815 | 17   | 保留 |
| `.claude/agents/dal-simplifier.md`                                          | 913221cb83c06c84 | 17   | 保留 |
| `.claude/agents/dal-spec-writer.md`                                         | 244d8906116f96e5 | 15   | 保留 |
| `.claude/agents/dal-tester.md`                                              | a0809fcb6002e0da | 19   | 保留 |
| `.claude/api-notes/joint-aad-gradient.md`                                   | bdaaffc10f360190 | 365  | 保留 |
| `.claude/critiques/pde-framework-reimplementation.md`                       | d1a119cdda581a15 | 218  | 保留 |
| `.claude/designs/api-shape-dedup.md`                                        | 2162ead169f13d67 | 488  | 保留 |
| `.claude/designs/joint-aad-gradient.md`                                     | ca47b8d3b331a50d | 1623 | 保留 |
| `.claude/rules/code-style.md`                                               | 6441d5a32b6770e1 | 284  | 保留 |
| `.claude/rules/git-commit-pr.md`                                            | 683f313aec5e137b | 55   | 保留 |
| `.claude/rules/unit-test-style.md`                                          | 6b1fe13503006fd6 | 53   | 保留 |
| `.claude/skills/dal-code-style-review/SKILL.md`                             | 575bd0323e36a504 | 14   | 保留 |
| `.claude/skills/dal-commit-and-pr/SKILL.md`                                 | e9e0b0845ae728e0 | 17   | 保留 |
| `.claude/skills/dal-unit-test-skill/SKILL.md`                               | 63ef3db3ce2216c2 | 14   | 保留 |
| `.claude/skills/dal-unit-test-write/SKILL.md`                               | c3720a6c73482b7d | 17   | 保留 |
| `.claude/specs/2026-08-14-dal-web-extraction-design.md`                     | 6aa0ceba0f4a4017 | 137  | 保留 |
| `.claude/specs/2026-08-14-dal-web-extraction-plan.md`                       | f889b48518d51b0d | 1038 | 保留 |
| `.claude/specs/joint-aad-gradient.md`                                       | b0113c6a1371241c | 873  | 保留 |
| `.claude/specs/multi-curve-simultaneous-example.md`                         | 96045456caa839f0 | 830  | 保留 |
| `.claude/specs/pde-framework-reimplementation.md`                           | 1b08a51228867676 | 855  | 保留 |
| `.claude/specs/script-compiled-evaluator-alignment.md`                      | 43765e0e74e1e3f9 | 200  | 保留 |
| `.codex/README.md`                                                          | 90fc972b81004bff | 52   | 保留 |
| `.codex/artifacts/README.md`                                                | da917971a529c6cd | 118  | 修正 |
| `.codex/artifacts/api-notes/dal-398-script-contract.md`                     | 96964d0ad502c0f4 | 759  | 保留 |
| `.codex/artifacts/api-notes/dal-410-xccy-mc-model-decision.md`              | 6f1231c60cd16676 | 983  | 保留 |
| `.codex/artifacts/api-notes/dal-xccy-mixed-mapping-basket.md`               | 3c5ae6eed21ed058 | 888  | 保留 |
| `.codex/artifacts/documentation/dal-418-rate-option-literals.md`            | 5864a1a8bbf24204 | 68   | 保留 |
| `.codex/artifacts/documentation/dal-436-documentation-agent-consistency.md` | 096a654a0b79b330 | 611  | 退役 |
| `.codex/artifacts/implementation/dal-399-script-contract.md`                | a2403f61c2a228ac | 377  | 保留 |
| `.codex/artifacts/implementation/dal-400-script-preparation.md`             | 130f62e5976bcfa0 | 321  | 保留 |
| `.codex/artifacts/implementation/dal-401-script-mc.md`                      | fe9fd9ced99adf35 | 278  | 保留 |
| `.codex/artifacts/implementation/dal-402-deposit-prototype.md`              | 50728cb53bcaa7e4 | 312  | 保留 |
| `.codex/artifacts/implementation/dal-403-bondoption-prototype.md`           | 66e406cea379a1cb | 419  | 保留 |
| `.codex/artifacts/implementation/dal-405-script-curve-calibration.md`       | f5b4b534d2c1cac9 | 324  | 保留 |
| `.codex/artifacts/implementation/dal-406-curve-product-fast-paths.md`       | a60e15ba511134b5 | 329  | 保留 |
| `.codex/artifacts/implementation/dal-407-script-public-bindings.md`         | 362b832aaa3367bd | 580  | 保留 |
| `.codex/artifacts/implementation/dal-408-xccy-script-cashflows.md`          | d8cbf1144cd84875 | 455  | 保留 |
| `.codex/artifacts/implementation/dal-409-script-xccy-calibration.md`        | 920e9a2c1495007e | 315  | 保留 |
| `.codex/artifacts/implementation/dal-411-gaussian-fast-paths.md`            | 2f954a1a350cc234 | 306  | 保留 |
| `.codex/artifacts/implementation/dal-412-gsr-script-mc-calibration.md`      | c6f17a5d357387e4 | 796  | 保留 |
| `.codex/artifacts/implementation/dal-414-gsr-script-public-bindings.md`     | fcd4e1a02e6deec0 | 426  | 保留 |
| `.codex/artifacts/implementation/dal-418-approved-publication.md`           | 5090df4f573b25c8 | 115  | 保留 |
| `.codex/artifacts/implementation/dal-418-publication.md`                    | 5d628d82fb0d99ae | 145  | 保留 |
| `.codex/artifacts/implementation/dal-418-rate-option-literals.md`           | f36598571c34941a | 76   | 保留 |
| `.codex/artifacts/implementation/dal-418-test-complexity.md`                | d3510282d9a89e7f | 120  | 保留 |
| `.codex/artifacts/implementation/dal-419-installed-windows-encoding.md`     | 168fcd01860678e3 | 197  | 保留 |
| `.codex/artifacts/implementation/dal-419-publication.md`                    | ce990b4e63498862 | 171  | 保留 |
| `.codex/artifacts/implementation/dal-469-benchmark-repair.md`               | 828c445f529023a2 | 588  | 退役 |
| `.codex/artifacts/implementation/dal-476-basket-covariance-overflow.md`     | fc8eb8d2f7d08d47 | 105  | 退役 |
| `.codex/artifacts/implementation/dal-xccy-basket-public-bindings.md`        | 2e2650a6d8eac92c | 245  | 保留 |
| `.codex/artifacts/implementation/dal-xccy-basket-public-example.md`         | b9e28b10047f08dd | 264  | 保留 |
| `.codex/artifacts/implementation/dal-xccy-m1-exact-model.md`                | 53462f2cd4359657 | 463  | 保留 |
| `.codex/artifacts/implementation/dal-xccy-m2-observation-execution.md`      | 26f9a1b10557a1d6 | 197  | 保留 |
| `.codex/artifacts/implementation/dal-xccy-m3-moments-staged.md`             | 82aa7103d977a812 | 439  | 保留 |
| `.codex/artifacts/implementation/dal-xccy-m4-mc-partials.md`                | 55658df306c59bb8 | 440  | 保留 |
| `.codex/artifacts/implementation/dal-xccy-m5-public-bindings.md`            | 7701489a9d89c88e | 423  | 保留 |
| `.codex/artifacts/implementation/dal-xccy-mixed-mapping-basket-core.md`     | feae2a49d33a955c | 326  | 保留 |
| `.codex/artifacts/implementation/dal425-codacy-repair.md`                   | efac0436a1a780f1 | 63   | 保留 |
| `.codex/artifacts/implementation/dal425-controlled-caller.md`               | 203e4ce171cae427 | 13   | 保留 |
| `.codex/artifacts/implementation/dal425-controlled-profile.md`              | 443fd716b5aada51 | 57   | 保留 |
| `.codex/artifacts/implementation/dal425-controlled-support.md`              | 887fc62f9b8ba1a2 | 65   | 保留 |
| `.codex/artifacts/reviews/dal-418-rate-option-literals.md`                  | cfb1e9ffa503c79a | 75   | 保留 |
| `.codex/artifacts/reviews/dal-418-test-complexity.md`                       | 995b53049b9a8802 | 72   | 保留 |
| `.codex/artifacts/reviews/dal-419-installed-windows-encoding.md`            | f5852d6fc9a23575 | 76   | 保留 |
| `.codex/artifacts/reviews/pr651-full-review.md`                             | 06fd4e477c4d9417 | 149  | 退役 |
| `.codex/artifacts/validation/dal-404-p0-same-source-validation.md`          | ebc675c242524aa6 | 389  | 保留 |
| `.codex/artifacts/validation/dal-413-gsr-script-numerical-validation.md`    | fcbe8dc7440b6f3a | 254  | 保留 |
| `.codex/artifacts/validation/dal-418-rate-option-literals.md`               | 8887d581e2668056 | 136  | 保留 |
| `.codex/artifacts/validation/dal-419-installed-windows-encoding.md`         | fb89c37a72a562de | 114  | 保留 |
| `.codex/artifacts/validation/dal-xccy-gaussian-numerical-validation.md`     | c665866233e7db7b | 2126 | 保留 |
| `.codex/artifacts/validation/dal425-financial-bias-bound-reference.md`      | 2f15f0bcafdf1e4a | 300  | 保留 |
| `.codex/artifacts/validation/dal425-finite-stream-bias-reference.md`        | 31a6054fab531b89 | 285  | 保留 |
| `.codex/artifacts/validation/dal425-fixed-output-error-reference.md`        | 57b771c143cf52c1 | 272  | 保留 |
| `.codex/artifacts/validation/dal425-joint-law-applicability-reference.md`   | 10deda438510ee35 | 239  | 保留 |
| `.codex/references/benchmark-workflow.md`                                   | 0a8a54c4c22b78e2 | 395  | 保留 |
| `.codex/references/code-style.md`                                           | 6441d5a32b6770e1 | 284  | 保留 |
| `.codex/references/component-boundaries.md`                                 | 6fdcfcfee3e8fea8 | 34   | 保留 |
| `.codex/references/git-commit-pr.md`                                        | 683f313aec5e137b | 55   | 保留 |
| `.codex/references/run-tests.md`                                            | d13ff1a69f2d1710 | 60   | 保留 |
| `.codex/references/style-review.md`                                         | 9f89018398a43770 | 86   | 保留 |
| `.codex/references/unit-test-style.md`                                      | 6b1fe13503006fd6 | 53   | 保留 |
| `.codex/references/write-tests.md`                                          | 875c0e96c0933e69 | 202  | 保留 |
| `.codex/skills/dal-agent-team/references/shared-rules.md`                   | 04913d1c3482e0a0 | 10   | 保留 |
| `.codex/skills/dal-git-pr/SKILL.md`                                         | 1510c9af7a473eae | 77   | 保留 |
| `.codex/skills/dal-git-pr/references/publish-workflow.md`                   | 11337d4e14df68d9 | 282  | 保留 |
| `.github/copilot-instructions.md`                                           | 6e152193d35a3e5e | 92   | 保留 |
| `.github/pull_request_template.md`                                          | 69353a9445b6976d | 16   | 保留 |
| `.github/scripts/tsan/README.md`                                            | d04c240de6923e74 | 35   | 保留 |
| `AGENTS.md`                                                                 | 1861ea331e69b286 | 109  | 保留 |
| `CHANGELOG.md`                                                              | bd0adcb3d8c92144 | 1472 | 保留 |
| `CLAUDE.md`                                                                 | 7ba28e31da5b8467 | 205  | 保留 |
| `CONTRIBUTING.md`                                                           | 8d30ee0f1633b4bd | 206  | 保留 |
| `README.md`                                                                 | ad9519056296504f | 220  | 保留 |
| `dal-cpp/README.md`                                                         | 8da68473774fd8d8 | 74   | 保留 |
| `dal-cpp/benchmarks/script_mc_perf/README.md`                               | d4b2196b4f67cfe8 | 182  | 保留 |
| `dal-cpp/examples/README.md`                                                | e152112cf35938b1 | 97   | 修正 |
| `dal-cpp/experimental/passivesink/README.md`                                | cd4e7144796a14fd | 123  | 保留 |
| `dal-excel/README.md`                                                       | de67812330b9912a | 233  | 保留 |
| `dal-excel/examples/008.quote_risk.md`                                      | d3d4fe7e0e9fb01c | 42   | 保留 |
| `dal-excel/examples/009.generic_joint_quote_risk.md`                        | ae2cb7842236a153 | 63   | 保留 |
| `dal-excel/examples/011.native_aad_risk.md`                                 | 3be82ed5dffd7351 | 91   | 保留 |
| `dal-public/README.md`                                                      | 08a9a7fb98191b81 | 136  | 修正 |
| `dal-python/README.md`                                                      | bb5528b7d2f50ef0 | 1145 | 保留 |
| `dal-python/benchmarks/README.md`                                           | d3d426919b1c6552 | 828  | 保留 |
| `datasets/dal_script_qa_7000/README.md`                                     | 816de15a8e76e2a5 | 99   | 保留 |
| `datasets/dal_script_qa_7000/index.md`                                      | 54c1737c117b9ac9 | 7170 | 保留 |
| `docs/README.md`                                                            | 560033b155de85eb | 72   | 修正 |
| `docs/architecture.md`                                                      | 43a592eca5db9be8 | 257  | 保留 |
| `docs/ccy-curves/README.md`                                                 | 1859d9cb961a04e9 | 10   | 保留 |
| `docs/ccy-curves/pricing-calibration.md`                                    | 52e1e6965aa0fac1 | 425  | 保留 |
| `docs/examples.md`                                                          | e4ad9d22935949ab | 127  | 保留 |
| `docs/excel/README.md`                                                      | 277b19b91335e807 | 1001 | 保留 |
| `docs/excel/script-settings.md`                                             | 0670ac0bf4ea37f3 | 512  | 保留 |
| `docs/installation.md`                                                      | 2b684877472cc40b | 482  | 保留 |
| `docs/methodology/README.md`                                                | 62f74389a4eaa4b7 | 40   | 保留 |
| `docs/methodology/_cpp-example-style.md`                                    | 90e562a6c49375b4 | 223  | 保留 |
| `docs/methodology/aad-sparsity.md`                                          | 65ed86a55db4c53f | 324  | 保留 |
| `docs/methodology/aad.md`                                                   | a1390bfe58cdc315 | 2692 | 保留 |
| `docs/methodology/dates.md`                                                 | 5b83997e16bbfc9c | 178  | 保留 |
| `docs/methodology/index_parsing.md`                                         | fcedab268a33fb2c | 195  | 保留 |
| `docs/methodology/interpolation.md`                                         | 7e7aea7d2987bace | 233  | 保留 |
| `docs/methodology/matrix.md`                                                | 91901b449278e1b5 | 619  | 保留 |
| `docs/methodology/monte-carlo/README.md`                                    | 111c35a3d5db486f | 27   | 保留 |
| `docs/methodology/monte-carlo/lsm.md`                                       | 500b5bbae8e421b4 | 512  | 保留 |
| `docs/methodology/monte-carlo/sampling.md`                                  | b2ef3591ce37fc33 | 488  | 保留 |
| `docs/methodology/monte-carlo/simulation.md`                                | be9d23412b31c70c | 154  | 保留 |
| `docs/methodology/monte-carlo/special-techniques.md`                        | 90bba06dcc3bceba | 98   | 保留 |
| `docs/methodology/passive-constants.md`                                     | 3238a1148d293e18 | 82   | 保留 |
| `docs/methodology/pde/README.md`                                            | fe04362d7da00f7c | 13   | 保留 |
| `docs/methodology/pde/aad.md`                                               | 597f77804731d1e8 | 148  | 保留 |
| `docs/methodology/pde/early-exercise.md`                                    | 72bc38df3bc0f8cf | 218  | 保留 |
| `docs/methodology/pde/framework.md`                                         | 30c51c458c3cfec0 | 367  | 保留 |
| `docs/methodology/pde/option-pricing.md`                                    | e6964a98afa964c5 | 74   | 保留 |
| `docs/methodology/quadrature.md`                                            | d6071443ea905d54 | 257  | 保留 |
| `docs/methodology/script_calibration_contract.md`                           | 8cbfcb22d28017b4 | 1468 | 保留 |
| `docs/methodology/script_engine.md`                                         | bf6a52508ce686f5 | 1823 | 保留 |
| `docs/methodology/underdetermined_search.md`                                | f8d68615b9846b0e | 474  | 保留 |
| `docs/models/README.md`                                                     | e0e370ac3ce01778 | 22   | 保留 |
| `docs/models/black-scholes.md`                                              | 27b6f1e9b8d81803 | 315  | 保留 |
| `docs/models/correlated-bs.md`                                              | 836b5d5a03302d49 | 115  | 保留 |
| `docs/models/dupire.md`                                                     | b0d08f2b589bee5b | 206  | 保留 |
| `docs/models/gaussian-short-rate.md`                                        | 1dff0251d981a6e4 | 550  | 保留 |
| `docs/models/hybrid-model.md`                                               | 429fcc4fdd103d15 | 198  | 保留 |
| `docs/models/local-volatility.md`                                           | 4fa5a9c3f06a86e4 | 116  | 保留 |
| `docs/models/market-iv-surface.md`                                          | 6340fc476b09b824 | 320  | 保留 |
| `docs/public-api.md`                                                        | 33434e608b6052d1 | 754  | 保留 |
| `docs/python/README.md`                                                     | 4ffa60c303469f52 | 960  | 保留 |
| `docs/yield-curves/README.md`                                               | b249fdbeafc00144 | 22   | 保留 |
| `docs/yield-curves/construction.md`                                         | 6daeb5e68379ae6d | 844  | 保留 |
| `docs/yield-curves/jacobian-risk.md`                                        | 62f6165c725bf2d2 | 815  | 保留 |
| `docs/yield-curves/joint-quote-risk.md`                                     | aae501ee285a12ed | 216  | 保留 |
| `docs/yield-curves/log-discount.md`                                         | d159638183b56dfb | 247  | 保留 |
| `docs/yield-curves/node-risk.md`                                            | 3dfabd9b13596003 | 189  | 保留 |
| `docs/yield-curves/ptirds-replication.md`                                   | 251132eae9447f23 | 413  | 保留 |
| `tests/lsmc-training-study/fixtures/README.md`                              | cef26de5e09b2d82 | 23   | 保留 |

新增本报告；不递归保存自身内容哈希。

## 路径候选的逐项处置

代码区域扫描记录所有不在干净根检出中的路径，下列按文件合并去重，未把候选隐藏为通过。
Claude 历史设计和旧 checkpoint 的路径依其命名基线解释；CHANGELOG 属于历史。
`.hpp/.cpp` 是成对文件简写，`.venv` 与子模块 include 是需初始化或构建的输入/输出。
Python 组件中的 `tests/...` 相对该组件解释，尤其 benchmark 的 `tests/test_native_queries.py`
实际位于 `dal-python/benchmarks/tests/test_native_queries.py`。新索引的四个退役路径仅用于固定 SHA 的恢复命令。
本报告的退役清单同理；这些均不是当前可点击的本地超链接。

- `.claude/api-notes/joint-aad-gradient.md`：`.claude/designs/aad-analytic-jacobian-naming-and-flag.md`；`.claude/designs/aad-analytic-jacobian-selector-api.md`；`dal-cpp/examples/joint_multi_curve_calibration/joint_multi_curve_calibration.cpp`。
- `.claude/critiques/pde-framework-reimplementation.md`：`dal-cpp/dal/math/pde/fd1d.cpp`；`dal-cpp/dal/math/pde/finitedifference.cpp`；`docs/methodology/pde.md`。
- `.claude/designs/joint-aad-gradient.md`：`.claude/designs/aad-analytic-jacobian-naming-and-flag.md`；`.claude/designs/aad-analytic-jacobian-redesign.md`；`.claude/designs/aad-analytic-jacobian-selector-api.md`；`dal-cpp/examples/joint_multi_curve_calibration/joint_multi_curve_calibration.cpp`；`dal-cpp/examples/yield_curve_jacobian`。
- `.claude/specs/2026-08-14-dal-web-extraction-design.md`：`.claude/rules/dal-web-code-style.md`；`.claude/rules/dal-web-design.md`；`.claude/skills/dal-web-setup`；`.claude/specs/dal-web-db-persistence.md`；`.codex/skills/dal-web`；`.github/workflows/ci.yml`；`.github/workflows/web-calibration-performance.yml`。
- `.claude/specs/2026-08-14-dal-web-extraction-plan.md`：`.claude/rules/dal-web-code-style.md`；`.claude/rules/dal-web-design.md`；`.claude/skills/dal-web-setup`；`.claude/specs/2026-08-14-dal-web-extraction`；`.claude/specs/dal-web-db-persistence.md`；`.codex/skills/dal-web`；`.codex/skills/dal-web/SKILL.md`；`.github/scripts/check_consistency.py`；`.github/scripts/check_frontend_quote_bump_boundary.py`；`.github/scripts/check_native_web_gateway.py`；`.github/scripts/generate_web_calibration_perf_reports.py`；`.github/scripts/generate_web_openapi.py`；`.github/scripts/run_web_calibration_perf.py`；`.github/scripts/x.py`；`.github/workflows/ci.yml`；`.github/workflows/web-calibration-performance.yml`；`dal-python/.venv/bin/python`；`docs/curve-lab.md`；`tests/test_native_runtime.py`。
- `.claude/specs/joint-aad-gradient.md`：`.claude/designs/aad-analytic-jacobian-backend-abstraction.md`；`.claude/designs/aad-analytic-jacobian-naming-and-flag.md`；`.claude/designs/aad-analytic-jacobian-redesign.md`；`.claude/designs/aad-analytic-jacobian-selector-api.md`；`dal-cpp/examples/joint_multi_curve_calibration`。
- `.claude/specs/multi-curve-simultaneous-example.md`：`.claude/critiques/multi-curve-simultaneous-example.md`；`dal-cpp/examples/curve_calibration/curve_calibration.cpp`；`dal-cpp/examples/joint_multi_curve_calibration`；`dal-cpp/examples/yield_curve_jacobian/yield_curve_jacobian.cpp`；`docs/methodology/joint_multi_curve.md`；`docs/methodology/xccy_calibration.md`；`docs/methodology/yield_curve.md`。
- `.claude/specs/pde-framework-reimplementation.md`：`dal-cpp/dal/math/pde/meshers`；`dal-cpp/examples/european_fd/european_fd.cpp`；`docs/methodology/pde.md`。
- `.claude/specs/script-compiled-evaluator-alignment.md`：`.claude/specs/perf-enhancement-candidates.md`；`dal-cpp/dal/auto/MG_NodeType_enum.hpp`；`dal-cpp/examples/uoc_compiled/uoc_compiled.cpp`。
- `.codex/artifacts/api-notes/dal-xccy-mixed-mapping-basket.md`：`dal-excel/src/__xccymontecarlo.hpp/.cpp`；`dal-excel/src/__xccymontecarlobasket.hpp/.cpp`；`dal-public/src/xccymontecarlo.hpp/.cpp`；`dal-public/src/xccymontecarlobasket.hpp/.cpp`。
- `.codex/artifacts/documentation/dal-436-documentation-agent-consistency.md`：`.claude/critiques/multi-curve-simultaneous-example.md`；`.claude/designs/aad-analytic-jacobian-backend-abstraction.md`；`.claude/designs/aad-analytic-jacobian-naming-and-flag.md`；`.claude/designs/aad-analytic-jacobian-redesign.md`；`.claude/designs/aad-analytic-jacobian-selector-api.md`；`.claude/rules/dal-web-code-style.md`；`.claude/rules/dal-web-design.md`；`.claude/skills/dal-web-setup`；`.claude/specs/2026-08-14-dal-web-extraction`；`.claude/specs/dal-web-db-persistence.md`；`.claude/specs/perf-enhancement-candidates.md`；`.codex/artifacts/implementation/dal-379-market-iv-ci-repair.md`；`.codex/artifacts/implementation/dal-387-benchmark-repair.md`；`.codex/artifacts/implementation/dal-418-evidence`；`.codex/artifacts/implementation/dal-418-evidence/capture_final.py`；`.codex/artifacts/implementation/dal-418-evidence/compare_snapshots.py`；`.codex/artifacts/implementation/dal-418-evidence/master_red.py`；`.codex/artifacts/implementation/dal-418-evidence/refresh_master_red.py`；`.codex/artifacts/implementation/dal-419-encoding-evidence`；`.codex/artifacts/implementation/dal-419-publication-evidence`；`.codex/artifacts/implementation/dal-419-publication-evidence/audit_source.py`；`.codex/artifacts/implementation/dal-419-publication-evidence/capture_local.py`；`.codex/artifacts/implementation/dal-419-publication-evidence/make_audit.py`；`.codex/artifacts/implementation/dal-419-publication-evidence/prepare.py`；`.codex/artifacts/implementation/dal-419-publication-evidence/run_native.py`；`.codex/artifacts/reviews/dal-379-market-iv-surface.md`；`.codex/artifacts/validation/dal-379-market-iv-surface.md`；`.codex/artifacts/validation/dal-418-validation-evidence`；`.codex/artifacts/validation/dal-419-validation-evidence`；`.codex/skills/dal-web`；`.codex/skills/dal-web/SKILL.md`；`.github/scripts/check_consistency.py`；`.github/scripts/check_frontend_quote_bump_boundary.py`；`.github/scripts/check_native_web_gateway.py`；`.github/scripts/generate_web_calibration_perf_reports.py`；`.github/scripts/generate_web_openapi.py`；`.github/scripts/run_web_calibration_perf.py`；`.github/scripts/x.py`；`.github/workflows/ci.yml`；`.github/workflows/web-calibration-performance.yml`；`dal-cpp/dal/auto/MG_NodeType_enum.hpp`；`dal-cpp/dal/math/pde/fd1d.cpp`；`dal-cpp/dal/math/pde/finitedifference.cpp`；`dal-cpp/dal/math/pde/meshers`；`dal-cpp/dal/script/calibrationpreparation.hpp/.cpp`；`dal-cpp/examples/american_put_mc`；`dal-cpp/examples/curve_calibration/curve_calibration.cpp`；`dal-cpp/examples/european_fd/european_fd.cpp`；`dal-cpp/examples/joint_multi_curve_calibration`；`dal-cpp/examples/joint_multi_curve_calibration/joint_multi_curve_calibration.cpp`；`dal-cpp/examples/script_tree`；`dal-cpp/examples/uoc_compiled/uoc_compiled.cpp`；`dal-cpp/examples/yield_curve_jacobian`；`dal-cpp/examples/yield_curve_jacobian/yield_curve_jacobian.cpp`；`dal-cpp/externals/googletest/googletest/include`；`dal-cpp/externals/rapidjson/include`；`dal-excel/src/__xccymontecarlo.hpp/.cpp`；`dal-excel/src/__xccymontecarlobasket.hpp/.cpp`；`dal-public/src/xccymontecarlo.hpp/.cpp`；`dal-public/src/xccymontecarlobasket.hpp/.cpp`；`dal-python/.venv`；`dal-python/.venv/bin/python`；`docs/curve-lab.md`；`docs/methodology/joint_multi_curve.md`；`docs/methodology/pde.md`；`docs/methodology/xccy_calibration.md`；`docs/methodology/yield_curve.md`；`tests/script/test_instrument.cpp`；`tests/test_curve_pricing.py`；`tests/test_main.cpp`；`tests/test_native_runtime.py`。
- `.codex/artifacts/documentation/dal-479-documentation-agent-consistency.md`：`.codex/artifacts/documentation/dal-436-documentation-agent-consistency.md`；`.codex/artifacts/implementation/dal-469-benchmark-repair.md`；`.codex/artifacts/implementation/dal-476-basket-covariance-overflow.md`；`.codex/artifacts/reviews/pr651-full-review.md`。
- `.codex/artifacts/implementation/dal-399-script-contract.md`：`dal-cpp/externals/rapidjson/include`；`tests/script/test_instrument.cpp`。
- `.codex/artifacts/implementation/dal-400-script-preparation.md`：`dal-cpp/dal/script/calibrationpreparation.hpp/.cpp`；`dal-cpp/externals/googletest/googletest/include`。
- `.codex/artifacts/implementation/dal-401-script-mc.md`：`dal-cpp/externals/googletest/googletest/include`；`dal-cpp/externals/rapidjson/include`。
- `.codex/artifacts/implementation/dal-402-deposit-prototype.md`：`dal-cpp/externals/googletest/googletest/include`；`dal-cpp/externals/rapidjson/include`。
- `.codex/artifacts/implementation/dal-403-bondoption-prototype.md`：`dal-cpp/externals/googletest/googletest/include`；`dal-cpp/externals/rapidjson/include`。
- `.codex/artifacts/implementation/dal-405-script-curve-calibration.md`：`dal-cpp/externals/googletest/googletest/include`。
- `.codex/artifacts/implementation/dal-407-script-public-bindings.md`：`dal-cpp/externals/googletest/googletest/include`。
- `.codex/artifacts/implementation/dal-418-publication.md`：`.codex/artifacts/implementation/dal-418-evidence`；`.codex/artifacts/validation/dal-418-validation-evidence`。
- `.codex/artifacts/implementation/dal-418-rate-option-literals.md`：`.codex/artifacts/implementation/dal-418-evidence/capture_final.py`；`.codex/artifacts/implementation/dal-418-evidence/compare_snapshots.py`；`.codex/artifacts/implementation/dal-418-evidence/master_red.py`；`.codex/artifacts/implementation/dal-418-evidence/refresh_master_red.py`。
- `.codex/artifacts/implementation/dal-419-publication.md`：`.codex/artifacts/implementation/dal-419-encoding-evidence`；`.codex/artifacts/implementation/dal-419-publication-evidence`；`.codex/artifacts/implementation/dal-419-publication-evidence/audit_source.py`；`.codex/artifacts/implementation/dal-419-publication-evidence/capture_local.py`；`.codex/artifacts/implementation/dal-419-publication-evidence/make_audit.py`；`.codex/artifacts/implementation/dal-419-publication-evidence/prepare.py`；`.codex/artifacts/implementation/dal-419-publication-evidence/run_native.py`；`.codex/artifacts/validation/dal-419-validation-evidence`。
- `.codex/artifacts/implementation/dal-469-benchmark-repair.md`：`tests/fixtures/dal469-profiler-404.txt`。
- `.codex/artifacts/implementation/dal-xccy-basket-public-bindings.md`：`dal-excel/src/__xccymontecarlobasket.hpp/.cpp`；`dal-public/src/xccymontecarlobasket.hpp/.cpp`。
- `.codex/artifacts/implementation/dal-xccy-m1-exact-model.md`：`dal-cpp/externals/googletest/googletest/include`；`tests/test_main.cpp`。
- `.codex/artifacts/implementation/dal-xccy-m4-mc-partials.md`：`dal-cpp/externals/googletest/googletest/include`。
- `.codex/artifacts/implementation/dal-xccy-m5-public-bindings.md`：`dal-public/src/xccymontecarlo.hpp/.cpp`。
- `.codex/artifacts/validation/dal-404-p0-same-source-validation.md`：`dal-cpp/externals/googletest/googletest/include`；`dal-cpp/externals/rapidjson/include`。
- `CHANGELOG.md`：`dal-cpp/examples/american_put_mc`；`dal-cpp/examples/script_tree`；`dal-cpp/examples/yield_curve_jacobian`；`docs/curve-lab.md`。
- `dal-python/README.md`：`dal-python/.venv`；`tests/test_curve_pricing.py`。
- `dal-python/benchmarks/README.md`：`tests/test_native_queries.py`。
- `docs/excel/script-settings.md`：`dal-python/.venv/bin/python`。
- `docs/installation.md`：`dal-cpp/externals/rapidjson/include`；`dal-python/.venv`。
- `docs/models/gaussian-short-rate.md`：`dal-python/.venv/bin/python`。

## 重放程序

需要 Python 3.11+、`markdown-it-py` 和 `PyYAML`。从仓库根执行，先把下段存为父目录的
`dal479-snapshot.py` 并执行一次，生成本次重放的只读初始快照。快照包含配置元数据，应留在本地。
然后把后面的主程序存为父目录的 `dal479-audit.py`，执行 `python3 ../dal479-audit.py`。
它只读 Multica，写本地结果 JSON；不更新 agent，也不执行文档中的产品命令。

```python
import json
from pathlib import Path
import subprocess

def read(*args):
    return json.loads(subprocess.check_output(['multica', *args, '--output', 'json'], text=True))

members = read('squad', 'member', 'list', '2b409a72-57c6-447a-a252-a8cff3208dc0')
agents = [read('agent', 'get', m['member_id']) for m in members]
Path('../dal479-roster-before.json').write_text(json.dumps(members, ensure_ascii=False, indent=2) + '\n')
Path('../dal479-agents-before.json').write_text(json.dumps(agents, ensure_ascii=False, indent=2) + '\n')
```

主程序（基线固定为本轮 master；清单每次从 Git 重算，报告也参加结构检查）：

```python
import collections
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import tomllib

from markdown_it import MarkdownIt
import yaml

root = Path.cwd()
base = '2ee93eff5026b3d7e9f297017f6d4e352e675888'
report = '.codex/artifacts/documentation/dal-479-documentation-agent-consistency.md'


def run(*args):
    return subprocess.check_output(args, text=True)


def digest(value):
    return hashlib.sha256(value.encode()).hexdigest()


tracked = run('git', 'ls-files').splitlines()
baseline = run('git', 'ls-tree', '-r', '--name-only', base).splitlines()
paths = sorted({p for p in tracked if p.endswith('.md') and Path(p).is_file()} | ({report} if Path(report).exists() else set()))
spec = importlib.util.spec_from_file_location('docs', '.github/scripts/check_docs.py')
docs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(docs)
errors, cache, inventory, missing_paths = [], {}, [], []
parser = MarkdownIt('commonmark')
link_count = 0
for name in paths:
    path = Path(name)
    content = path.read_text(encoding='utf-8')
    refs = set()
    local_links = 0
    for token in parser.parse(content):
        for child in token.children or []:
            if child.type not in ('link_open', 'image'):
                continue
            target = child.attrGet('href' if child.type == 'link_open' else 'src')
            if target:
                docs.check_link(path.resolve(), (token.map or [0])[0] + 1, target, cache, errors)
                local_links += 1
    for line, target in docs.agent_referenced_paths(content):
        if (root / target).exists():
            refs.add(target)
        elif target not in docs.AGENT_ALLOWED_MISSING:
            missing_paths.append([name, line, target])
    if not content.endswith('\n'):
        errors.append(name + ': missing final newline')
    inventory.append({'path': name, 'lines': len(content.splitlines()),
                      'links': local_links, 'sha256': digest(content),
                      'references': sorted(refs)})
    link_count += local_links
documents = tuple(Path(p).resolve() for p in paths)
docs.check_tables(documents, errors)
docs.check_whitespace(documents, errors)

contracts = {}
registrations = []
for path in sorted(Path('.codex/agents').glob('*.toml')):
    contract = tomllib.loads(path.read_text())
    assert path.stem == contract['name']
    assert len(contract['description']) <= 255
    contracts[contract['name']] = contract
    claude = Path('.claude/agents', path.stem + '.md')
    _, front, body = claude.read_text().split('---', 2)
    meta = yaml.safe_load(front)
    body = re.sub(r'^\s*<!--.*?-->\s*', '', body, flags=re.S)
    assert meta['name'] == contract['name']
    assert meta['description'] == contract['description']
    assert body == contract['developer_instructions']
    registrations.append({'path': str(path), 'sha256': digest(path.read_text()),
                          'claude': str(claude), 'model': meta['model'], 'color': meta['color']})
assert set(contracts) == {p.stem for p in Path('.claude/agents').glob('dal-*.md')}
for name in ('code-style', 'unit-test-style', 'git-commit-pr'):
    assert Path('.claude/rules', name + '.md').read_bytes() == Path('.codex/references', name + '.md').read_bytes()
skills = []
for name in tracked:
    if name.endswith('/SKILL.md'):
        meta = yaml.safe_load(Path(name).read_text().split('---', 2)[1])
        assert meta['name'] == Path(name).parent.name and meta['description']
        skills.append(name)
metadata = '.codex/skills/dal-git-pr/agents/openai.yaml'
assert yaml.safe_load(Path(metadata).read_text())['interface']['default_prompt']

protected = ('runtime_id', 'runtime_config', 'runtime_mode', 'model', 'thinking_level',
             'service_tier', 'skills', 'disabled_runtime_skills', 'has_custom_env',
             'custom_env_key_count', 'custom_args', 'mcp_config', 'mcp_config_redacted',
             'visibility', 'permission_mode', 'invocation_targets', 'max_concurrent_tasks',
             'avatar_url', 'name', 'owner_id', 'archived_at', 'archived_by', 'conversation_starters')
before_path = root.parent / 'dal479-agents-before.json'
before = {a['id']: a for a in json.loads(before_path.read_text())} if before_path.exists() else {}
members = json.loads(run('multica', 'squad', 'member', 'list',
                         '2b409a72-57c6-447a-a252-a8cff3208dc0', '--output', 'json'))
agents = []
for member in members:
    assert member['member_type'] == 'agent'
    agent = json.loads(run('multica', 'agent', 'get', member['member_id'], '--output', 'json'))
    contract = contracts[agent['name']]
    assert agent['description'] == contract['description']
    assert agent['instructions'] == contract['developer_instructions']
    if agent['id'] in before:
        assert all(agent.get(k) == before[agent['id']].get(k) for k in protected)
    agents.append({'name': agent['name'], 'id': agent['id'],
                   'description_characters': len(agent['description']),
                   'protected_sha256': digest(json.dumps({k: agent.get(k) for k in protected}, sort_keys=True, ensure_ascii=False)),
                   'instructions_sha256': digest(agent['instructions'])})
assert {a['name'] for a in agents} == set(contracts)

qa_root = Path('datasets/dal_script_qa_7000')
qa = [json.loads(line) for line in (qa_root / 'qa.jsonl').read_text().splitlines()]
validation = [json.loads(line) for line in (qa_root / 'validation.jsonl').read_text().splitlines()]
summary = json.loads((qa_root / 'validation_summary.json').read_text())
assert len(qa) == len(validation) == len({r['id'] for r in qa}) == 7000
assert len({r['family'] for r in qa}) == 70
assert summary['sha256_qa_jsonl'] == hashlib.sha256((qa_root / 'qa.jsonl').read_bytes()).hexdigest()
assert summary['passed'] == 7000 and summary['failed'] == 0

source_files = [p for p in tracked if Path(p).suffix in ('.hpp', '.cpp', '.h', '.inc', '.py', '.cmake', '.txt', '.sh', '.bat', '.ps1', '.toml', '.json', '.yml')]
source = '\n'.join(Path(p).read_text(errors='replace') for p in source_files)
tests = '\n'.join(Path(p).read_text(errors='replace') for p in source_files if '/tests/' in p)
suites = set(re.findall(r'\bTEST(?:_F|_P)?\s*\(\s*(\w+)\s*,', tests))
symbol_re = re.compile(r'\b(?:[A-Z][a-zA-Z0-9]+_[A-Za-z0-9_]*|[A-Z][a-zA-Z0-9]{2,}_|DAL_[A-Z_]+)\b')
symbol_placeholders = {'EnumName_', 'VALUE_NAME', 'DF_i'}
checked_symbols = set()
for name in paths:
    if name == report or name == 'CHANGELOG.md' or name.startswith(('.codex/artifacts/', '.claude/specs/', '.claude/designs/', '.claude/critiques/', '.claude/api-notes/')):
        continue
    content = Path(name).read_text()
    for segment in re.findall(r'`([^`\n]+)`', content):
        for symbol in symbol_re.findall(segment):
            checked_symbols.add(symbol)
            if symbol not in source and symbol not in symbol_placeholders:
                errors.append(name + ': absent source symbol ' + symbol)
    for value in re.findall(r'--gtest_filter=[\'\"]?([^\s`\'\"]+)', content):
        for suite in re.findall(r'(?:^|:|-)([A-Za-z]\w*)\.', value):
            if suite not in suites and suite != 'SuiteName':
                errors.append(name + ': absent GTest suite ' + suite)
    for header in re.findall(r'(?m)^\s*#include\s*[<\"]((?:dal/|dal-public/)[^>\"\n]+)', content):
        target = Path('dal-cpp', header) if header.startswith('dal/') else Path(header)
        if not target.exists() and 'EnumName' not in header and '<module' not in header:
            errors.append(name + ': absent DAL header ' + header)
example_cmake = Path('dal-public/examples/CMakeLists.txt').read_text()
example_names = set(re.findall(r'^add_subdirectory\((\w+)\)', example_cmake, re.M))
catalog = Path('dal-public/README.md').read_text().split('## Examples', 1)[1].split('## Build and Test', 1)[0]
documented_examples = set(re.findall(r'\[([a-z_]+)\]\(examples/\1/\)', catalog))
assert example_names == documented_examples and len(example_names) == 7
for name in example_names:
    cmake = Path('dal-public/examples', name, 'CMakeLists.txt').read_text()
    assert f'add_executable({name} {name}.cpp)' in cmake
    assert f'target_link_libraries({name} PRIVATE DAL::public)' in cmake
    assert f'install(TARGETS {name} RUNTIME DESTINATION ' in cmake
assert 'add_test(NAME ${example_target} COMMAND ${example_target})' in example_cmake
assert 'PROPERTIES LABELS examples' in example_cmake
roster_before = json.loads((root.parent / 'dal479-roster-before.json').read_text())
assert sorted(members, key=lambda m: m['id']) == sorted(roster_before, key=lambda m: m['id'])
result = {'baseline': base, 'baseline_markdown_count': sum(p.endswith('.md') for p in baseline),
          'markdown_count': len(paths), 'parsed_links': link_count,
          'distribution': dict(collections.Counter(p.split('/')[0] if '/' in p else '(root)' for p in paths)),
          'agents': agents, 'registrations': registrations, 'skills': skills,
          'metadata': metadata, 'source_symbols': len(checked_symbols),
          'missing_path_candidates': missing_paths, 'inventory': inventory,
          'retired': sorted(p for p in baseline if p.endswith('.md') and not Path(p).exists()),
          'errors': errors}
(root.parent / 'dal479-audit.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
print(json.dumps({k: result[k] for k in ('baseline_markdown_count', 'markdown_count', 'parsed_links', 'distribution', 'source_symbols', 'errors')}, ensure_ascii=False))
print('TOML/Claude/Multica: 10 exact matches; protected fields unchanged; five skill frontmatters and YAML pass.')
assert not errors, '\n'.join(errors)
```
