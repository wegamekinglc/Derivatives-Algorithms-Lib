# DAL-506：全量文档与 agent 契约一致性

## 交付

[PR #686](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/686)，base master。
主文档提交 `d1c53e369f10194abb77c8eac49f775276e8519c`；
后续提交仅登记本发布记录及增加的链接计数。独立验收由协调器安排。

## 基线、范围与结果

复用协调器提供的干净检出与分支
`agent/dal-orchestrator/d85abf27fe39`。开始时重新核对 HEAD、
`origin/master` 和实时 `git ls-remote origin refs/heads/master`，
三者均为 `1b8c92e8f23320b2723628fd37b399455c918e23`；
修改后再次 `git fetch origin master`，远端仍为该 SHA。PR base 为 master。

本轮重新生成全部 Git 跟踪 Markdown 清单：基线 166 文件、60,767 行。
完整解码和解析每个文件，检查结构、链接、锚点、表格、空白和引用路径；
不抽样。当前指南另核对源码中的 DAL 名称、include、GTest suite、
CMake 选项、示例注册及 Python 发布元数据。历史记录依明确的设计/执行
基线解释；计划接口不作为已发布能力。
这不是逐个编译全部示例、重新证明全部数学推导或执行全部产品测试。
实质修正围绕新合入 core PDE risk 的头文件、实现、测试、现有价格
facade 和语言绑定；没有将未执行的数值资格化写成通过。

非 Markdown 检查覆盖十个 `.codex/agents/*.toml`、
一个 skill `agents/openai.yaml`，以及 Claude 十个 agent、
五个 SKILL 的 frontmatter/body、规则镜像和实时 DAL roster。
逐文件清单、分布、指纹与复现检查程序在下方，均为本轮重算。

## 实质差异与修改原因

- `docs/methodology/pde/early-exercise.md`：补充已存在的 core
  `EvaluateEarlyExercisePdeRisk`，说明原生参数轴、单位、固定计划、
  两尺度 central repricing、有限尺度 Gamma、可用性、累计预算和
  owning getters，并给出与既有 core 价格示例相接的调用片段。
  依据 `dal-cpp/dal/math/pde/earlyexercisepderisk.hpp`、
  `dal-cpp/dal/math/pde/earlyexercisepderisk.cpp` 及
  `dal-cpp/tests/math/pde/test_early_exercise_pde_risk.cpp`。
- `docs/public-api.md`：区分现有 price facade 和仅 core C++ 可用的
  risk；检查 public header、Python/Excel bindings 后，保留待实现投影边界。
- `docs/README.md`、`docs/methodology/pde/README.md`、
  `README.md`、`CLAUDE.md`：修正“仅价格/无 Greeks”的总体介绍，
  导航到同一份风险指南；没有新增、删除或重命名方法学页面。
- `CHANGELOG.md`：补记 2026-11-08 已合入的显著 core 风险能力，
  对应 PR #684；该能力符合现有 changelog 准入规则。
  不将本次文档维护本身登记为算法变更。
- `.codex/artifacts/api-notes/dal-497-early-exercise-pde-risk.md`：
  将设计基线与当前实现区分，标明 core 已实现、其余投影待实现。
- `.codex/artifacts/specs/dal-497-early-exercise-pde-risk.md`：
  更新当前实施状态与指南入口，不修改 R1–R11 语义。
- `.codex/artifacts/specs/dal-497-pde-risk-acceptance.md`：
  区分现有 core deterministic 覆盖与完整独立验收矩阵；
  不变更任何阈值、fixture 或原 S0 的未执行声明。
- `.codex/artifacts/README.md`：更新本轮活动入口与 PDE 状态，
  退役已完成 DAL-479 的交付收据，提供固定 master SHA 的恢复链接。
- `.codex/artifacts/documentation/dal-479-documentation-agent-consistency.md`：
  删除已完成的旧检查收据；已实时读取 DAL-479，状态为 done。
  检查程序复用其固定基线字节，本轮结果与实时配置证据全部重新生成。
- 本文件：活动中的 DAL-506 清单、复现程序与验收交接依据。

PDE core 的存在不代表 public/Python/Excel risk 已交付，也不代表
独立金融/refinement 矩阵已完成。DAL-497 实时状态为 in_progress。
XCCY 冻结证据、FAIL、未资格化标志和已停止的采集链保持原字节及原含义。
C++、生成文件、构建、测试、依赖、agent TOML 与 YAML 均未修改。

## Agent 契约与保留差异

根 AGENTS.md 指定最新 master 的十个 Codex TOML 为完整角色契约来源。
Claude 十个 agent 的摘要及去注册注释后的完整正文，与对应 TOML
`description`/`developer_instructions` 逐字相等；三对规则镜像逐字节相同。
五个 SKILL frontmatter 和一个 skill YAML 均可解析。

实时 DAL squad `2b409a72-57c6-447a-a252-a8cff3208dc0` 的十名 agent
按名称与十个 TOML 一一映射，无缺失、重复或额外成员。
逐一读取完整 agent 记录，十组 description/instructions 均已完全一致；
所有摘要均不超过 255 Unicode 字符。
**配置同步写入零次，未调用 agent update。**
前后读取 roster、leader 和受保护字段，指纹证据见下方。
仅比较 agent get 暴露的环境/MCP 标记或脱敏值，不读取秘密。

保留有依据的平台差异：Claude 的 YAML 注册、model: inherit、color
和四个 user-invocable skills；Codex TOML 与 dal-git-pr/shared references；
Multica runtime、model、thinking/service tier、skills、环境/MCP、
visibility/调用权限、并发、头像和 roster 元数据。
roster 展示 role 中“不强制逐步 TDD”等摘要不是运行 instructions；
本轮没有修改 role、leader 或成员，也未以展示摘要替代权威契约。
路由顺序、职责、升级、交接、活动产物、权限边界的共同正文完全一致。

## 验证与交接

仓库检查为 `python3 .github/scripts/check_docs.py`；
检查器自身的现有 34 项单元测试通过。完整扩展检查覆盖默认范围之外的
所有 Claude、嵌套 README、SKILL 和数据集索引；检查本地链接和锚点、
引用路径候选、TOML/YAML/frontmatter、agent 集合/实时映射、
九个 public examples、数据集已保存的 7,000 条记录及摘要指纹。
数据集历史验证通过不等于本轮重新执行 7,000 个产品用例。

仅文档变化，没有生产调用者或算法性能变化，性能 case 集为空。
未执行 C++/Python/Office 产品构建、测试、MC 或性能矩阵；
core 风险示例按源头声明检查，未编译，数值资格化仍明确待完成。
`git diff --check` 和 staged scope 检查作为提交前门禁。
没有访问或权限阻塞；无需要用户澄清的开放问题。
保持本 issue in_progress，由协调器安排 dal-reviewer 独立验收，
未自行调度其他成员或合并 PR。

## 本轮机器证据

本轮重新读取实时配置并与开始快照比较；检查时间（UTC）：2026-11-08T04:12:09.636603+00:00。
文件 SHA 为完整 UTF-8 文本的 SHA-256 前 16 位；本报告自身不递归保存哈希/行数。
基线完整字节数：5146395。

- 仓库默认检查：125 个 Markdown，通过。
- 现有检查器测试：34/34，通过。
- 扩展检查：166 个交付 Markdown、8261 个解析链接、711 个当前 DAL 名称；零错误。
- 十个 TOML、十个 Claude frontmatter/body、五个 SKILL、一个 YAML、三对规则镜像通过。
- 九个 public examples 的目录、target、DAL::public 依赖、CTest/label 和安装项一致。
- 全部现存 Markdown artifact 均在活动索引登记；当前路径没有未处置的缺失引用。
- 本地链接/锚点检查不声明外部网站可达；声明/API 存在性检查不替代编译。

| 目录         | 基线 | 交付 |
|------------|----|----|
| (root)     | 5  | 5  |
| .claude    | 28 | 28 |
| .codex     | 64 | 64 |
| .github    | 3  | 3  |
| dal-cpp    | 5  | 5  |
| dal-excel  | 5  | 5  |
| dal-public | 1  | 1  |
| dal-python | 3  | 3  |
| datasets   | 2  | 2  |
| docs       | 49 | 49 |
| tests      | 1  | 1  |

实时 roster（含全部成员、role）前后相同，SHA-256：
`60416546697289c78eefd27c0ff45a0e18dda60c22b449acd4a791f59927caf7`。
实时 squad（含 leader、instructions）前后相同，SHA-256：
`f0e78c38bd9b3d3963a3f5a10bccc6e87fd139e1c2c83fc672be4d765490919a`。

| Agent            | Multica ID                           | 摘要字符 | instructions SHA-256                                             |
|------------------|--------------------------------------|------|------------------------------------------------------------------|
| dal-api-designer | 62641d17-7114-4726-976c-64ee433065dd | 64   | 1e0afa9d5bd172c26699911e889794a0c23de90981e16043812c27aa6724ad0a |
| dal-critic       | 5d24a66b-d46b-4e2b-ba87-a273fa37a20b | 56   | da14ba8d61214f1c567c0d56d514fd9596a2c6e0fddf3f5627cf106d150fa2b1 |
| dal-doc-writer   | 92053497-eaee-47b5-9dcf-8e6e302e1348 | 76   | e5268889e37abf198cd6aaa29cb6a9bbf32566d9283d2a8dcad1beff9c781b8a |
| dal-implementer  | 2c2adb0a-dba4-4095-844a-d47c9635a518 | 59   | 40a176c669f143fc9f9318916ccc55dcd906154f3f94bcc73e0925e735cc5fc7 |
| dal-orchestrator | 36a667e5-f5b7-460c-83a2-d1575be5f12a | 56   | aad4d429572f882bc9f32a1caa6cc7554dd22c4db7160bb5d2cb6d927609debf |
| dal-performancer | 6feb9efa-bc37-46f8-bf13-6a2d3d1ce454 | 62   | b994293688713edcaa3bde9eeae507555f0ac811235399bf11e64fb05f19e390 |
| dal-reviewer     | a541f9c9-b79c-4b31-8ff2-a1178c57a395 | 70   | 1ab98c978883337bcc84fe56843384b4891bcf4df91d505aea3ce80b61e4526b |
| dal-simplifier   | 9fa8445f-7ff7-456d-945a-017c37747348 | 77   | aee02a96c657678dea20debcf6c93e1a3ebb0c6b6973e16e6835a3fe0799ff65 |
| dal-spec-writer  | 0032710b-1be3-40eb-9118-815edd395b53 | 65   | f230216c45f70950d0daae5e519e1317d8deb3c66dd4463c4b3e269096fe8af6 |
| dal-tester       | 59e8e14e-1f1f-47d7-93a0-e2c2f8f53d0b | 68   | 5d4dad5fdf80bdb8b50261492cdc16a598219e258bbdc5519dd055dd2458448d |

| Agent            | 受保护字段 SHA-256（前后相同）                                              |
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

受保护字段完整名单在后附程序的 protected 中；没有配置更新调用。

## 全量逐文件清单

每个文件均完整读取并参加结构/引用检查。保留项不表示已重新执行其中的历史命令。

| Markdown 路径                                                                 | 基线行  | 交付行  | 交付文本 SHA（退役为基线）  | 处置 |
|-----------------------------------------------------------------------------|------|------|------------------|----|
| `.claude/agents/README.md`                                                  | 75   | 75   | 393f8f48877cd657 | 保留 |
| `.claude/agents/dal-api-designer.md`                                        | 17   | 17   | 51379dd89a8fb629 | 保留 |
| `.claude/agents/dal-critic.md`                                              | 15   | 15   | ef5abfc77268a473 | 保留 |
| `.claude/agents/dal-doc-writer.md`                                          | 17   | 17   | 0adc325834193619 | 保留 |
| `.claude/agents/dal-implementer.md`                                         | 17   | 17   | 53687dd8397407f1 | 保留 |
| `.claude/agents/dal-orchestrator.md`                                        | 23   | 23   | 7763d601878c3b19 | 保留 |
| `.claude/agents/dal-performancer.md`                                        | 17   | 17   | a58367b0b076a93d | 保留 |
| `.claude/agents/dal-reviewer.md`                                            | 17   | 17   | 023c59cb98833815 | 保留 |
| `.claude/agents/dal-simplifier.md`                                          | 17   | 17   | 913221cb83c06c84 | 保留 |
| `.claude/agents/dal-spec-writer.md`                                         | 15   | 15   | 244d8906116f96e5 | 保留 |
| `.claude/agents/dal-tester.md`                                              | 19   | 19   | a0809fcb6002e0da | 保留 |
| `.claude/api-notes/joint-aad-gradient.md`                                   | 365  | 365  | bdaaffc10f360190 | 保留 |
| `.claude/critiques/pde-framework-reimplementation.md`                       | 218  | 218  | d1a119cdda581a15 | 保留 |
| `.claude/designs/api-shape-dedup.md`                                        | 488  | 488  | 2162ead169f13d67 | 保留 |
| `.claude/designs/joint-aad-gradient.md`                                     | 1623 | 1623 | ca47b8d3b331a50d | 保留 |
| `.claude/rules/code-style.md`                                               | 284  | 284  | 6441d5a32b6770e1 | 保留 |
| `.claude/rules/git-commit-pr.md`                                            | 55   | 55   | 683f313aec5e137b | 保留 |
| `.claude/rules/unit-test-style.md`                                          | 53   | 53   | 6b1fe13503006fd6 | 保留 |
| `.claude/skills/dal-code-style-review/SKILL.md`                             | 14   | 14   | 575bd0323e36a504 | 保留 |
| `.claude/skills/dal-commit-and-pr/SKILL.md`                                 | 17   | 17   | e9e0b0845ae728e0 | 保留 |
| `.claude/skills/dal-unit-test-skill/SKILL.md`                               | 14   | 14   | 63ef3db3ce2216c2 | 保留 |
| `.claude/skills/dal-unit-test-write/SKILL.md`                               | 17   | 17   | c3720a6c73482b7d | 保留 |
| `.claude/specs/2026-08-14-dal-web-extraction-design.md`                     | 137  | 137  | 6aa0ceba0f4a4017 | 保留 |
| `.claude/specs/2026-08-14-dal-web-extraction-plan.md`                       | 1038 | 1038 | f889b48518d51b0d | 保留 |
| `.claude/specs/joint-aad-gradient.md`                                       | 873  | 873  | b0113c6a1371241c | 保留 |
| `.claude/specs/multi-curve-simultaneous-example.md`                         | 830  | 830  | 96045456caa839f0 | 保留 |
| `.claude/specs/pde-framework-reimplementation.md`                           | 855  | 855  | 1b08a51228867676 | 保留 |
| `.claude/specs/script-compiled-evaluator-alignment.md`                      | 200  | 200  | 43765e0e74e1e3f9 | 保留 |
| `.codex/README.md`                                                          | 52   | 52   | 90fc972b81004bff | 保留 |
| `.codex/artifacts/README.md`                                                | 166  | 175  | 4b2cab906d542f70 | 修正 |
| `.codex/artifacts/api-notes/dal-398-script-contract.md`                     | 759  | 759  | 96964d0ad502c0f4 | 保留 |
| `.codex/artifacts/api-notes/dal-410-xccy-mc-model-decision.md`              | 983  | 983  | 6f1231c60cd16676 | 保留 |
| `.codex/artifacts/api-notes/dal-497-early-exercise-pde-risk.md`             | 453  | 456  | b72058c7fd5863ef | 修正 |
| `.codex/artifacts/api-notes/dal-xccy-mixed-mapping-basket.md`               | 888  | 888  | 3c5ae6eed21ed058 | 保留 |
| `.codex/artifacts/documentation/dal-418-rate-option-literals.md`            | 68   | 68   | 5864a1a8bbf24204 | 保留 |
| `.codex/artifacts/documentation/dal-479-documentation-agent-consistency.md` | 572  | —    | b33d8366c9830b6e | 退役 |
| `.codex/artifacts/documentation/dal-506-documentation-agent-consistency.md` | —    | 自身   | 自身               | 新增 |
| `.codex/artifacts/implementation/dal-399-script-contract.md`                | 377  | 377  | a2403f61c2a228ac | 保留 |
| `.codex/artifacts/implementation/dal-400-script-preparation.md`             | 321  | 321  | 130f62e5976bcfa0 | 保留 |
| `.codex/artifacts/implementation/dal-401-script-mc.md`                      | 278  | 278  | fe9fd9ced99adf35 | 保留 |
| `.codex/artifacts/implementation/dal-402-deposit-prototype.md`              | 312  | 312  | 50728cb53bcaa7e4 | 保留 |
| `.codex/artifacts/implementation/dal-403-bondoption-prototype.md`           | 419  | 419  | 66e406cea379a1cb | 保留 |
| `.codex/artifacts/implementation/dal-405-script-curve-calibration.md`       | 324  | 324  | f5b4b534d2c1cac9 | 保留 |
| `.codex/artifacts/implementation/dal-406-curve-product-fast-paths.md`       | 329  | 329  | a60e15ba511134b5 | 保留 |
| `.codex/artifacts/implementation/dal-407-script-public-bindings.md`         | 580  | 580  | 362b832aaa3367bd | 保留 |
| `.codex/artifacts/implementation/dal-408-xccy-script-cashflows.md`          | 455  | 455  | d8cbf1144cd84875 | 保留 |
| `.codex/artifacts/implementation/dal-409-script-xccy-calibration.md`        | 315  | 315  | 920e9a2c1495007e | 保留 |
| `.codex/artifacts/implementation/dal-411-gaussian-fast-paths.md`            | 306  | 306  | 2f954a1a350cc234 | 保留 |
| `.codex/artifacts/implementation/dal-412-gsr-script-mc-calibration.md`      | 796  | 796  | c6f17a5d357387e4 | 保留 |
| `.codex/artifacts/implementation/dal-414-gsr-script-public-bindings.md`     | 426  | 426  | fcd4e1a02e6deec0 | 保留 |
| `.codex/artifacts/implementation/dal-418-approved-publication.md`           | 115  | 115  | 5090df4f573b25c8 | 保留 |
| `.codex/artifacts/implementation/dal-418-publication.md`                    | 145  | 145  | 5d628d82fb0d99ae | 保留 |
| `.codex/artifacts/implementation/dal-418-rate-option-literals.md`           | 76   | 76   | f36598571c34941a | 保留 |
| `.codex/artifacts/implementation/dal-418-test-complexity.md`                | 120  | 120  | d3510282d9a89e7f | 保留 |
| `.codex/artifacts/implementation/dal-419-installed-windows-encoding.md`     | 197  | 197  | 168fcd01860678e3 | 保留 |
| `.codex/artifacts/implementation/dal-419-publication.md`                    | 171  | 171  | ce990b4e63498862 | 保留 |
| `.codex/artifacts/implementation/dal-xccy-basket-public-bindings.md`        | 245  | 245  | 2e2650a6d8eac92c | 保留 |
| `.codex/artifacts/implementation/dal-xccy-basket-public-example.md`         | 264  | 264  | b9e28b10047f08dd | 保留 |
| `.codex/artifacts/implementation/dal-xccy-m1-exact-model.md`                | 463  | 463  | 53462f2cd4359657 | 保留 |
| `.codex/artifacts/implementation/dal-xccy-m2-observation-execution.md`      | 197  | 197  | 26f9a1b10557a1d6 | 保留 |
| `.codex/artifacts/implementation/dal-xccy-m3-moments-staged.md`             | 439  | 439  | 82aa7103d977a812 | 保留 |
| `.codex/artifacts/implementation/dal-xccy-m4-mc-partials.md`                | 440  | 440  | 55658df306c59bb8 | 保留 |
| `.codex/artifacts/implementation/dal-xccy-m5-public-bindings.md`            | 423  | 423  | 7701489a9d89c88e | 保留 |
| `.codex/artifacts/implementation/dal-xccy-mixed-mapping-basket-core.md`     | 326  | 326  | feae2a49d33a955c | 保留 |
| `.codex/artifacts/implementation/dal425-codacy-repair.md`                   | 63   | 63   | efac0436a1a780f1 | 保留 |
| `.codex/artifacts/implementation/dal425-controlled-caller.md`               | 13   | 13   | 203e4ce171cae427 | 保留 |
| `.codex/artifacts/implementation/dal425-controlled-profile.md`              | 57   | 57   | 443fd716b5aada51 | 保留 |
| `.codex/artifacts/implementation/dal425-controlled-support.md`              | 65   | 65   | 887fc62f9b8ba1a2 | 保留 |
| `.codex/artifacts/reviews/dal-418-rate-option-literals.md`                  | 75   | 75   | cfb1e9ffa503c79a | 保留 |
| `.codex/artifacts/reviews/dal-418-test-complexity.md`                       | 72   | 72   | 995b53049b9a8802 | 保留 |
| `.codex/artifacts/reviews/dal-419-installed-windows-encoding.md`            | 76   | 76   | f5852d6fc9a23575 | 保留 |
| `.codex/artifacts/specs/dal-497-early-exercise-pde-risk.md`                 | 382  | 386  | c97a680d366457f7 | 修正 |
| `.codex/artifacts/specs/dal-497-pde-risk-acceptance.md`                     | 468  | 471  | 7d0593964a787347 | 修正 |
| `.codex/artifacts/validation/dal-404-p0-same-source-validation.md`          | 389  | 389  | ebc675c242524aa6 | 保留 |
| `.codex/artifacts/validation/dal-413-gsr-script-numerical-validation.md`    | 254  | 254  | fcbe8dc7440b6f3a | 保留 |
| `.codex/artifacts/validation/dal-418-rate-option-literals.md`               | 136  | 136  | 8887d581e2668056 | 保留 |
| `.codex/artifacts/validation/dal-419-installed-windows-encoding.md`         | 114  | 114  | fb89c37a72a562de | 保留 |
| `.codex/artifacts/validation/dal-xccy-gaussian-numerical-validation.md`     | 2126 | 2126 | c665866233e7db7b | 保留 |
| `.codex/artifacts/validation/dal425-financial-bias-bound-reference.md`      | 300  | 300  | 2f15f0bcafdf1e4a | 保留 |
| `.codex/artifacts/validation/dal425-finite-stream-bias-reference.md`        | 285  | 285  | 31a6054fab531b89 | 保留 |
| `.codex/artifacts/validation/dal425-fixed-output-error-reference.md`        | 272  | 272  | 57b771c143cf52c1 | 保留 |
| `.codex/artifacts/validation/dal425-joint-law-applicability-reference.md`   | 239  | 239  | 10deda438510ee35 | 保留 |
| `.codex/references/benchmark-workflow.md`                                   | 407  | 407  | 6864c21902b7897f | 保留 |
| `.codex/references/code-style.md`                                           | 284  | 284  | 6441d5a32b6770e1 | 保留 |
| `.codex/references/component-boundaries.md`                                 | 34   | 34   | 6fdcfcfee3e8fea8 | 保留 |
| `.codex/references/git-commit-pr.md`                                        | 55   | 55   | 683f313aec5e137b | 保留 |
| `.codex/references/run-tests.md`                                            | 60   | 60   | d13ff1a69f2d1710 | 保留 |
| `.codex/references/style-review.md`                                         | 86   | 86   | 9f89018398a43770 | 保留 |
| `.codex/references/unit-test-style.md`                                      | 53   | 53   | 6b1fe13503006fd6 | 保留 |
| `.codex/references/write-tests.md`                                          | 202  | 202  | 875c0e96c0933e69 | 保留 |
| `.codex/skills/dal-agent-team/references/shared-rules.md`                   | 10   | 10   | 04913d1c3482e0a0 | 保留 |
| `.codex/skills/dal-git-pr/SKILL.md`                                         | 77   | 77   | 1510c9af7a473eae | 保留 |
| `.codex/skills/dal-git-pr/references/publish-workflow.md`                   | 282  | 282  | 11337d4e14df68d9 | 保留 |
| `.github/copilot-instructions.md`                                           | 92   | 92   | 6e152193d35a3e5e | 保留 |
| `.github/pull_request_template.md`                                          | 16   | 16   | 69353a9445b6976d | 保留 |
| `.github/scripts/tsan/README.md`                                            | 35   | 35   | d04c240de6923e74 | 保留 |
| `AGENTS.md`                                                                 | 109  | 109  | 1861ea331e69b286 | 保留 |
| `CHANGELOG.md`                                                              | 1528 | 1537 | 24f80a1c42e30d53 | 修正 |
| `CLAUDE.md`                                                                 | 205  | 205  | bef6bfa163731776 | 修正 |
| `CONTRIBUTING.md`                                                           | 206  | 206  | 8d30ee0f1633b4bd | 保留 |
| `README.md`                                                                 | 220  | 220  | 411f4cd849995355 | 修正 |
| `dal-cpp/README.md`                                                         | 74   | 74   | 8da68473774fd8d8 | 保留 |
| `dal-cpp/benchmarks/script_mc_perf/README.md`                               | 182  | 182  | d4b2196b4f67cfe8 | 保留 |
| `dal-cpp/examples/README.md`                                                | 98   | 98   | 607e8b28b2d52ed5 | 保留 |
| `dal-cpp/examples/products/autocallables/README.md`                         | 77   | 77   | ca4df530ef54ae9a | 保留 |
| `dal-cpp/experimental/passivesink/README.md`                                | 123  | 123  | cd4e7144796a14fd | 保留 |
| `dal-excel/README.md`                                                       | 240  | 240  | f0cbd91ce6a6eb53 | 保留 |
| `dal-excel/examples/008.quote_risk.md`                                      | 42   | 42   | d3d4fe7e0e9fb01c | 保留 |
| `dal-excel/examples/009.generic_joint_quote_risk.md`                        | 63   | 63   | ae2cb7842236a153 | 保留 |
| `dal-excel/examples/011.native_aad_risk.md`                                 | 91   | 91   | 3be82ed5dffd7351 | 保留 |
| `dal-excel/examples/autocallables/README.md`                                | 63   | 63   | fd67315a8aa1c1d6 | 保留 |
| `dal-public/README.md`                                                      | 171  | 171  | 2f83b2b909dd6db8 | 保留 |
| `dal-python/README.md`                                                      | 1149 | 1149 | ffb4868d06b02397 | 保留 |
| `dal-python/benchmarks/README.md`                                           | 828  | 828  | d3d426919b1c6552 | 保留 |
| `dal-python/examples/autocallables/README.md`                               | 28   | 28   | b61fceae2c0aa37a | 保留 |
| `datasets/dal_script_qa_7000/README.md`                                     | 99   | 99   | 816de15a8e76e2a5 | 保留 |
| `datasets/dal_script_qa_7000/index.md`                                      | 7170 | 7170 | 54c1737c117b9ac9 | 保留 |
| `docs/README.md`                                                            | 73   | 77   | cec294b008bc5758 | 修正 |
| `docs/architecture.md`                                                      | 257  | 257  | 43a592eca5db9be8 | 保留 |
| `docs/ccy-curves/README.md`                                                 | 13   | 13   | 3a69a216e6072fae | 保留 |
| `docs/ccy-curves/compounded-ois.md`                                         | 176  | 176  | ae7c8e3279cbcc61 | 保留 |
| `docs/ccy-curves/pricing-calibration.md`                                    | 425  | 425  | 52e1e6965aa0fac1 | 保留 |
| `docs/examples.md`                                                          | 144  | 144  | 20cc82664314d1bf | 保留 |
| `docs/excel/README.md`                                                      | 1121 | 1121 | 279a1424950a758b | 保留 |
| `docs/excel/script-settings.md`                                             | 512  | 512  | 0670ac0bf4ea37f3 | 保留 |
| `docs/installation.md`                                                      | 482  | 482  | 2b684877472cc40b | 保留 |
| `docs/methodology/README.md`                                                | 40   | 40   | 62f74389a4eaa4b7 | 保留 |
| `docs/methodology/_cpp-example-style.md`                                    | 223  | 223  | 90e562a6c49375b4 | 保留 |
| `docs/methodology/aad-sparsity.md`                                          | 324  | 324  | 65ed86a55db4c53f | 保留 |
| `docs/methodology/aad.md`                                                   | 2692 | 2692 | a1390bfe58cdc315 | 保留 |
| `docs/methodology/dates.md`                                                 | 178  | 178  | 5b83997e16bbfc9c | 保留 |
| `docs/methodology/index_parsing.md`                                         | 195  | 195  | fcedab268a33fb2c | 保留 |
| `docs/methodology/interpolation.md`                                         | 233  | 233  | 7e7aea7d2987bace | 保留 |
| `docs/methodology/matrix.md`                                                | 619  | 619  | 91901b449278e1b5 | 保留 |
| `docs/methodology/monte-carlo/README.md`                                    | 27   | 27   | 111c35a3d5db486f | 保留 |
| `docs/methodology/monte-carlo/lsm.md`                                       | 512  | 512  | 500b5bbae8e421b4 | 保留 |
| `docs/methodology/monte-carlo/sampling.md`                                  | 488  | 488  | b2ef3591ce37fc33 | 保留 |
| `docs/methodology/monte-carlo/simulation.md`                                | 154  | 154  | be9d23412b31c70c | 保留 |
| `docs/methodology/monte-carlo/special-techniques.md`                        | 98   | 98   | 90bba06dcc3bceba | 保留 |
| `docs/methodology/passive-constants.md`                                     | 82   | 82   | 3238a1148d293e18 | 保留 |
| `docs/methodology/pde/README.md`                                            | 13   | 19   | 4cc5858071518adc | 修正 |
| `docs/methodology/pde/aad.md`                                               | 148  | 148  | 597f77804731d1e8 | 保留 |
| `docs/methodology/pde/early-exercise.md`                                    | 218  | 358  | afc0a0ecfb71942d | 修正 |
| `docs/methodology/pde/framework.md`                                         | 367  | 367  | 30c51c458c3cfec0 | 保留 |
| `docs/methodology/pde/option-pricing.md`                                    | 74   | 74   | e6964a98afa964c5 | 保留 |
| `docs/methodology/quadrature.md`                                            | 257  | 257  | d6071443ea905d54 | 保留 |
| `docs/methodology/script_calibration_contract.md`                           | 1861 | 1861 | 27a525c323414c53 | 保留 |
| `docs/methodology/script_engine.md`                                         | 1833 | 1833 | fa01b8673f3aa640 | 保留 |
| `docs/methodology/underdetermined_search.md`                                | 474  | 474  | f8d68615b9846b0e | 保留 |
| `docs/models/README.md`                                                     | 22   | 22   | e0e370ac3ce01778 | 保留 |
| `docs/models/black-scholes.md`                                              | 315  | 315  | 27b6f1e9b8d81803 | 保留 |
| `docs/models/correlated-bs.md`                                              | 115  | 115  | 836b5d5a03302d49 | 保留 |
| `docs/models/dupire.md`                                                     | 206  | 206  | b0d08f2b589bee5b | 保留 |
| `docs/models/gaussian-short-rate.md`                                        | 550  | 550  | 1dff0251d981a6e4 | 保留 |
| `docs/models/hybrid-model.md`                                               | 198  | 198  | 429fcc4fdd103d15 | 保留 |
| `docs/models/local-volatility.md`                                           | 116  | 116  | 4fa5a9c3f06a86e4 | 保留 |
| `docs/models/market-iv-surface.md`                                          | 320  | 320  | 6340fc476b09b824 | 保留 |
| `docs/public-api.md`                                                        | 808  | 815  | 687c913d7ec464bd | 修正 |
| `docs/python/README.md`                                                     | 1037 | 1037 | 2b3bbeac2147e647 | 保留 |
| `docs/yield-curves/README.md`                                               | 22   | 22   | b249fdbeafc00144 | 保留 |
| `docs/yield-curves/construction.md`                                         | 844  | 844  | 6daeb5e68379ae6d | 保留 |
| `docs/yield-curves/jacobian-risk.md`                                        | 838  | 838  | cec1978480f2dfd5 | 保留 |
| `docs/yield-curves/joint-quote-risk.md`                                     | 216  | 216  | aae501ee285a12ed | 保留 |
| `docs/yield-curves/log-discount.md`                                         | 247  | 247  | d159638183b56dfb | 保留 |
| `docs/yield-curves/node-risk.md`                                            | 189  | 189  | 3dfabd9b13596003 | 保留 |
| `docs/yield-curves/ptirds-replication.md`                                   | 413  | 413  | 251132eae9447f23 | 保留 |
| `tests/lsmc-training-study/fixtures/README.md`                              | 23   | 23   | cef26de5e09b2d82 | 保留 |

## 路径候选处置

所有缺失候选按文件去重列出，不把原始候选等同于错误或隐藏为通过。
Claude 历史设计及 Codex checkpoint 按其命名基线解释；CHANGELOG 是历史。
尚未存在的 PDE public risk 头和 facade example 在活动合同中明确为计划。
成对扩展名是源码简写；externals include 由 submodule 初始化；.venv 是本地构建产物。
Python README 的 tests 相对 dal-python，benchmark README 的 tests 相对 dal-python/benchmarks；均已核对存在。
旧收据和其本地证据脚本按冻结执行记录保留意义，不是当前运行入口；退役审计可从固定远端 SHA 恢复。

- `.claude/api-notes/joint-aad-gradient.md`：`.claude/designs/aad-analytic-jacobian-naming-and-flag.md`；`.claude/designs/aad-analytic-jacobian-selector-api.md`；`dal-cpp/examples/joint_multi_curve_calibration/joint_multi_curve_calibration.cpp`。
- `.claude/critiques/pde-framework-reimplementation.md`：`dal-cpp/dal/math/pde/fd1d.cpp`；`dal-cpp/dal/math/pde/finitedifference.cpp`；`docs/methodology/pde.md`。
- `.claude/designs/joint-aad-gradient.md`：`.claude/designs/aad-analytic-jacobian-naming-and-flag.md`；`.claude/designs/aad-analytic-jacobian-redesign.md`；`.claude/designs/aad-analytic-jacobian-selector-api.md`；`dal-cpp/examples/joint_multi_curve_calibration/joint_multi_curve_calibration.cpp`；`dal-cpp/examples/yield_curve_jacobian`。
- `.claude/specs/2026-08-14-dal-web-extraction-design.md`：`.claude/rules/dal-web-code-style.md`；`.claude/rules/dal-web-design.md`；`.claude/skills/dal-web-setup`；`.claude/specs/dal-web-db-persistence.md`；`.codex/skills/dal-web`；`.github/workflows/ci.yml`；`.github/workflows/web-calibration-performance.yml`。
- `.claude/specs/2026-08-14-dal-web-extraction-plan.md`：`.claude/rules/dal-web-code-style.md`；`.claude/rules/dal-web-design.md`；`.claude/skills/dal-web-setup`；`.claude/specs/2026-08-14-dal-web-extraction`；`.claude/specs/dal-web-db-persistence.md`；`.codex/skills/dal-web`；`.codex/skills/dal-web/SKILL.md`；`.github/scripts/check_consistency.py`；`.github/scripts/check_frontend_quote_bump_boundary.py`；`.github/scripts/check_native_web_gateway.py`；`.github/scripts/generate_web_calibration_perf_reports.py`；`.github/scripts/generate_web_openapi.py`；`.github/scripts/run_web_calibration_perf.py`；`.github/scripts/x.py`；`.github/workflows/ci.yml`；`.github/workflows/web-calibration-performance.yml`；`dal-python/.venv/bin/python`；`docs/curve-lab.md`；`tests/test_native_runtime.py`。
- `.claude/specs/joint-aad-gradient.md`：`.claude/designs/aad-analytic-jacobian-backend-abstraction.md`；`.claude/designs/aad-analytic-jacobian-naming-and-flag.md`；`.claude/designs/aad-analytic-jacobian-redesign.md`；`.claude/designs/aad-analytic-jacobian-selector-api.md`；`dal-cpp/examples/joint_multi_curve_calibration`。
- `.claude/specs/multi-curve-simultaneous-example.md`：`.claude/critiques/multi-curve-simultaneous-example.md`；`dal-cpp/examples/curve_calibration/curve_calibration.cpp`；`dal-cpp/examples/joint_multi_curve_calibration`；`dal-cpp/examples/yield_curve_jacobian/yield_curve_jacobian.cpp`；`docs/methodology/joint_multi_curve.md`；`docs/methodology/xccy_calibration.md`；`docs/methodology/yield_curve.md`。
- `.claude/specs/pde-framework-reimplementation.md`：`dal-cpp/dal/math/pde/meshers`；`dal-cpp/examples/european_fd/european_fd.cpp`；`docs/methodology/pde.md`。
- `.claude/specs/script-compiled-evaluator-alignment.md`：`.claude/specs/perf-enhancement-candidates.md`；`dal-cpp/dal/auto/MG_NodeType_enum.hpp`；`dal-cpp/examples/uoc_compiled/uoc_compiled.cpp`。
- `.codex/artifacts/api-notes/dal-497-early-exercise-pde-risk.md`：`dal-public/src/earlyexercisepderisk.hpp`。
- `.codex/artifacts/api-notes/dal-xccy-mixed-mapping-basket.md`：`dal-excel/src/__xccymontecarlo.hpp/.cpp`；`dal-excel/src/__xccymontecarlobasket.hpp/.cpp`；`dal-public/src/xccymontecarlo.hpp/.cpp`；`dal-public/src/xccymontecarlobasket.hpp/.cpp`。
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
- `.codex/artifacts/implementation/dal-xccy-basket-public-bindings.md`：`dal-excel/src/__xccymontecarlobasket.hpp/.cpp`；`dal-public/src/xccymontecarlobasket.hpp/.cpp`。
- `.codex/artifacts/implementation/dal-xccy-m1-exact-model.md`：`dal-cpp/externals/googletest/googletest/include`；`tests/test_main.cpp`。
- `.codex/artifacts/implementation/dal-xccy-m4-mc-partials.md`：`dal-cpp/externals/googletest/googletest/include`。
- `.codex/artifacts/implementation/dal-xccy-m5-public-bindings.md`：`dal-public/src/xccymontecarlo.hpp/.cpp`。
- `.codex/artifacts/specs/dal-497-early-exercise-pde-risk.md`：`dal-public/examples/early_exercise_pde_risk`。
- `.codex/artifacts/validation/dal-404-p0-same-source-validation.md`：`dal-cpp/externals/googletest/googletest/include`；`dal-cpp/externals/rapidjson/include`。
- `CHANGELOG.md`：`dal-cpp/examples/american_put_mc`；`dal-cpp/examples/script_tree`；`dal-cpp/examples/yield_curve_jacobian`；`docs/curve-lab.md`。
- `dal-python/README.md`：`dal-python/.venv`；`tests/test_curve_pricing.py`。
- `dal-python/benchmarks/README.md`：`tests/test_native_queries.py`。
- `docs/excel/script-settings.md`：`dal-python/.venv/bin/python`。
- `docs/installation.md`：`dal-cpp/externals/rapidjson/include`；`dal-python/.venv`。
- `docs/models/gaussian-short-rate.md`：`dal-python/.venv/bin/python`。

## 可复现命令

在仓库根执行。依赖 Python 3.11+、markdown-it-py、PyYAML 和已认证的任务 CLI；
将下方最后的主程序保存为 ../dal506-audit.py。证据文件只在本地使用，
共享结论和指纹已在本报告给出；不发布原始 agent 配置。

```bash
python3 .github/scripts/check_docs.py
python3 -m unittest discover -s .github/scripts/tests -p test_check_docs.py -v
python3 ../dal506-audit.py
git diff --check
git diff --cached --name-status
git diff --cached --check
```

先建立本轮 before 快照，再运行主程序（首次复核时的基准读取，无写操作）：

```python
import json
import os
from pathlib import Path
import subprocess

out = Path(os.environ.get("DAL_DOC_EVIDENCE_DIR", str(Path.cwd().parent)))
cli = os.environ["MULTICA_CLI_PATH"]
def read(*args):
    return json.loads(subprocess.check_output([cli, *args, "--output", "json"], text=True))

squad_id = "2b409a72-57c6-447a-a252-a8cff3208dc0"
members = read("squad", "member", "list", squad_id)
agents = [read("agent", "get", m["member_id"]) for m in members]
squad = read("squad", "get", squad_id)
for kind, value in (("roster", members), ("agents", agents), ("squad", squad)):
    (out / f"dal506-{kind}-before.json").write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n")
```

主程序（结果 JSON 含完整哈希与全部逐行路径候选；当前 Markdown 清单每次从 Git 重算）：

```python
import collections
import os
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import tomllib
from datetime import datetime, timezone

from markdown_it import MarkdownIt
import yaml

root = Path.cwd()
evidence_dir = Path(os.environ.get('DAL_DOC_EVIDENCE_DIR', str(root.parent)))
base = '1b8c92e8f23320b2723628fd37b399455c918e23'
report = '.codex/artifacts/documentation/dal-506-documentation-agent-consistency.md'


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
before_path = evidence_dir / 'dal506-agents-before.json'
before = {a['id']: a for a in json.loads(before_path.read_text())}
assert len(before) == len(contracts) == 10
members = json.loads(run(os.environ['MULTICA_CLI_PATH'], 'squad', 'member', 'list',
                         '2b409a72-57c6-447a-a252-a8cff3208dc0', '--output', 'json'))
agents = []
after = []
for member in members:
    assert member['member_type'] == 'agent'
    agent = json.loads(run(os.environ['MULTICA_CLI_PATH'], 'agent', 'get', member['member_id'], '--output', 'json'))
    contract = contracts[agent['name']]
    after.append(agent)
    assert agent['description'] == contract['description']
    assert agent['instructions'] == contract['developer_instructions']
    if agent['id'] in before:
        assert all(agent.get(k) == before[agent['id']].get(k) for k in protected)
    agents.append({'name': agent['name'], 'id': agent['id'],
                   'description_characters': len(agent['description']),
                   'protected_sha256': digest(json.dumps({k: agent.get(k) for k in protected}, sort_keys=True, ensure_ascii=False)),
                   'instructions_sha256': digest(agent['instructions'])})
assert {a['name'] for a in agents} == set(contracts)
assert len(agents) == len({a['id'] for a in agents}) == 10
squad = json.loads(run(os.environ['MULTICA_CLI_PATH'], 'squad', 'get',
                       '2b409a72-57c6-447a-a252-a8cff3208dc0', '--output', 'json'))
assert squad == json.loads((evidence_dir / 'dal506-squad-before.json').read_text())
for kind, value in (('agents', after), ('roster', members), ('squad', squad)):
    (evidence_dir / f'dal506-{kind}-after.json').write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n')

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
assert example_names == documented_examples and len(example_names) == 9
for name in example_names:
    cmake = Path('dal-public/examples', name, 'CMakeLists.txt').read_text()
    assert f'add_executable({name} {name}.cpp)' in cmake
    assert f'target_link_libraries({name} PRIVATE DAL::public)' in cmake
    assert f'install(TARGETS {name} RUNTIME DESTINATION ' in cmake
assert 'add_test(NAME ${example_target} COMMAND ${example_target})' in example_cmake
assert 'PROPERTIES LABELS examples' in example_cmake
artifact_index = Path('.codex/artifacts/README.md').read_text()
artifact_paths = [p for p in paths if p.startswith('.codex/artifacts/') and p != '.codex/artifacts/README.md']
assert all('](' + p.removeprefix('.codex/artifacts/') + ')' in artifact_index for p in artifact_paths)
roster_before = json.loads((evidence_dir / 'dal506-roster-before.json').read_text())
assert sorted(members, key=lambda m: m['id']) == sorted(roster_before, key=lambda m: m['id'])
baseline_inventory = []
for name in baseline:
    if name.endswith('.md'):
        content = run('git', 'show', f'{base}:{name}')
        baseline_inventory.append({'path': name, 'sha256': digest(content), 'lines': len(content.splitlines()), 'bytes': len(content.encode())})
result = {'baseline': base, 'baseline_markdown_count': sum(p.endswith('.md') for p in baseline),
          'checked_at': datetime.now(timezone.utc).isoformat(),
          'baseline_inventory': baseline_inventory,
          'roster_sha256': digest(json.dumps(sorted(members, key=lambda m: m['id']), sort_keys=True, ensure_ascii=False)),
          'squad_sha256': digest(json.dumps(squad, sort_keys=True, ensure_ascii=False)),
          'markdown_count': len(paths), 'parsed_links': link_count,
          'distribution': dict(collections.Counter(p.split('/')[0] if '/' in p else '(root)' for p in paths)),
          'agents': agents, 'registrations': registrations, 'skills': skills,
          'metadata': metadata, 'source_symbols': len(checked_symbols),
          'missing_path_candidates': missing_paths, 'inventory': inventory,
          'retired': sorted(p for p in baseline if p.endswith('.md') and not Path(p).exists()),
          'errors': errors}
(evidence_dir / 'dal506-audit.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
print(json.dumps({k: result[k] for k in ('baseline_markdown_count', 'markdown_count', 'parsed_links', 'distribution', 'source_symbols', 'errors')}, ensure_ascii=False))
print('TOML/Claude/Multica: 10 exact matches; protected fields unchanged; five skill frontmatters and YAML pass.')
assert not errors, '\n'.join(errors)

```
