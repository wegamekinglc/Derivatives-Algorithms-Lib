# DAL-436：全量文档与 agent 契约一致性验收证据

## 交付与基线

本轮完成文档维护和获准的配置核对，保留单一交付分支
`agent/dal-doc-writer/b82e0f16995e`，PR base 为 `master`。
DAL-436 保持 `in_progress`，后续独立审查由协调器安排；本轮未触发其他成员或合并。
PR 的实际链接和交付提交见同线程最终评论。

实际执行检出初始干净，`HEAD = origin/master = git ls-remote origin refs/heads/master`：
`dd6ddc3e9dd1637634287bcbe8dd8d5bc7fc49a4`。
2026-10-25 04:12:09 UTC 再次 `git fetch origin master` 与 `git ls-remote`，三者仍相同。
仓库历史前四项为 #645、#644、#643、#642；未使用协调器另一个检出代替本轮复核。

初始 Git 跟踪 Markdown **157** 个，全部完整读取并逐文件作静态检查；
四个已完成且不再控制活动工作的记录退役，新增本报告，最终 **154** 个。
下面逐文件列出全部 157 个基线文件及本轮新增文件，包含基线内容指纹和处置。
非 Markdown 注册另检查 **10 个 agent TOML + 1 个 skill YAML**。

## 已修正差异

| 文件                                                                          | 原因与结果                                                         |
|-----------------------------------------------------------------------------|---------------------------------------------------------------|
| `CLAUDE.md`                                                                 | tree-walk 默认值过期；改为 compiled 默认/显式 false 选择 tree；补两份现有方法学导航。   |
| `docs/README.md`                                                            | 补 scoped passive constants 的入口，指向已有能力和限制。                     |
| `docs/methodology/README.md`                                                | 补结构 Jacobian 与 passive constants；未新增、删除或重命名方法学。               |
| `.codex/artifacts/api-notes/dal-398-script-contract.md`                     | 将 proposed 状态限定为已接受的设计基线；当前实现和后续产品范围指向现状指南。                   |
| `.codex/artifacts/api-notes/dal-410-xccy-mc-model-decision.md`              | 消除整个 XCCY MC 尚不支持的现状误述；专用实现已存在，通用入口仍拒绝，数值验收仍属 DAL-425。        |
| `.codex/artifacts/README.md`                                                | 完整登记 44 个现存 Markdown 产物；区分活动 XCCY、已完成但仍控制 DAL-396 的上游证据和退役记录。 |
| `.codex/artifacts/implementation/dal-379-market-iv-ci-repair.md`            | 退役：DAL-379 已 done，当前方法说明已发表，Git 保留精确原字节。                      |
| `.codex/artifacts/implementation/dal-387-benchmark-repair.md`               | 退役：DAL-387 已 done，当前 benchmark README 已覆盖启动和运行方式。             |
| `.codex/artifacts/reviews/dal-379-market-iv-surface.md`                     | 退役：完成的旧评审不再作为活动 blocker。                                      |
| `.codex/artifacts/validation/dal-379-market-iv-surface.md`                  | 退役：保留历史 RED/GREEN 于基线 Git，而不将旧 blocker 作为当前状态。                |
| `.codex/artifacts/documentation/dal-436-documentation-agent-consistency.md` | 新增本轮完整清单、契约/权限证据、异常分类和可重放检查程序。                                |

四个退役文件均在 `dd6ddc3e` 中可恢复；精确恢复命令登记在产物索引。
原索引称 `d5b4f85b` 为未推送本地快照，实际
`gh api repos/wegamekinglc/Derivatives-Algorithms-Lib/git/commits/d5b4f85b77a85a6189c213c178aaf755da966b9c --jq .sha`
成功返回完整 SHA，已改为远端可恢复引用。该快照中的 Python market-IV 测试与当前文件 diff 为空。

用 `multica issue get DAL-<number> --output json` 逐项核对
379、387、398–414（不含未列号）、418、419、425 的实际状态：
前述完成项均 `done`，425 为 `in_progress`；另核对 396 为 `in_progress`。
上游检查点仍为 396/425 的来源与验收控制，保留原始数值、失败、预算与发布记录；
索引明确那些检查点的 pending/blocked、路径和 CI 文字不是 live 状态。
没有重新宣告 XCCY 全矩阵验收通过，也没有更改任何验收阈值。

源码依据包括 `dal-cpp/dal/script/settings.hpp` 的 `DEFAULT_COMPILED = true`、
`MonteCarloSettings_::compiled_`，以及 `simulation.hpp`/`diagnostics.cpp` 的
`value_or(DEFAULT_COMPILED)`；README、架构、Python 和 script methodology 的说明一致。
XCCY 现状对照 `dal-cpp/dal/model/xccygaussian.hpp`、
`dal-public/src/xccymontecarlo.hpp`、`xccymontecarlobasket.hpp` 和
`docs/methodology/script_calibration_contract.md` 的专用/通用入口边界。

## 覆盖分布

| 目录         | 基线 | 交付 |
|------------|----|----|
| (root)     | 5  | 5  |
| .claude    | 28 | 28 |
| .codex     | 59 | 56 |
| .github    | 3  | 3  |
| dal-cpp    | 4  | 4  |
| dal-excel  | 4  | 4  |
| dal-public | 1  | 1  |
| dal-python | 2  | 2  |
| datasets   | 2  | 2  |
| docs       | 48 | 48 |
| tests      | 1  | 1  |

`.codex` 基线 59 个中 47 个为产物；交付 56 个中 44 个为产物。
`.claude` 的 28 个包括 11 个 agent 页面、3 个规则、4 个 SKILL 和 10 个历史设计/规格页面。
所有嵌套 README、五个 SKILL、数据集 7,000 行分类总表均在清单内。

## Claude、Codex 与 Multica

权威依据为根 `AGENTS.md` 与最新 master 的 `.codex/agents/*.toml`。
十个 TOML 均可解析、name 与文件一致，description 全部少于 255 个 Unicode 字符。
Claude 十个正文在去掉注册说明注释后与对应 `developer_instructions` 逐字相同，
摘要相同；三份 mirrored rules 逐字节相同。

通过 `multica squad member list 2b409a72-57c6-447a-a252-a8cff3208dc0 --output json`
获取实时 DAL roster，随后逐个 `multica agent get <id> --output json`，
十个名字恰好映射十个 TOML，无缺失、重复或额外成员。
**实际同步写入：零。** 每个 description/instructions 已逐字匹配，无需调用 update。

| agent            | Multica ID                           | description 字符数 | instructions SHA-256                                             |
|------------------|--------------------------------------|-----------------|------------------------------------------------------------------|
| dal-api-designer | 62641d17-7114-4726-976c-64ee433065dd | 64              | 1e0afa9d5bd172c26699911e889794a0c23de90981e16043812c27aa6724ad0a |
| dal-critic       | 5d24a66b-d46b-4e2b-ba87-a273fa37a20b | 56              | da14ba8d61214f1c567c0d56d514fd9596a2c6e0fddf3f5627cf106d150fa2b1 |
| dal-doc-writer   | 92053497-eaee-47b5-9dcf-8e6e302e1348 | 76              | e5268889e37abf198cd6aaa29cb6a9bbf32566d9283d2a8dcad1beff9c781b8a |
| dal-implementer  | 2c2adb0a-dba4-4095-844a-d47c9635a518 | 59              | 40a176c669f143fc9f9318916ccc55dcd906154f3f94bcc73e0925e735cc5fc7 |
| dal-orchestrator | 36a667e5-f5b7-460c-83a2-d1575be5f12a | 56              | aad4d429572f882bc9f32a1caa6cc7554dd22c4db7160bb5d2cb6d927609debf |
| dal-performancer | 6feb9efa-bc37-46f8-bf13-6a2d3d1ce454 | 62              | b994293688713edcaa3bde9eeae507555f0ac811235399bf11e64fb05f19e390 |
| dal-reviewer     | a541f9c9-b79c-4b31-8ff2-a1178c57a395 | 70              | 1ab98c978883337bcc84fe56843384b4891bcf4df91d505aea3ce80b61e4526b |
| dal-simplifier   | 9fa8445f-7ff7-456d-945a-017c37747348 | 77              | aee02a96c657678dea20debcf6c93e1a3ebb0c6b6973e16e6835a3fe0799ff65 |
| dal-spec-writer  | 0032710b-1be3-40eb-9118-815edd395b53 | 65              | f230216c45f70950d0daae5e519e1317d8deb3c66dd4463c4b3e269096fe8af6 |
| dal-tester       | 59e8e14e-1f1f-47d7-93a0-e2c2f8f53d0b | 68              | 5d4dad5fdf80bdb8b50261492cdc16a598219e258bbdc5519dd055dd2458448d |

初读与终读比对了 runtime、model、thinking/service tier、skills、环境标记、MCP、
权限、并发、头像等受保护字段，全部未变。下面指纹覆盖检查程序列出的受保护字段；
动态工作状态和更新时间不纳入该身份比较。

| agent            | 受保护字段 SHA-256（前后相同）                                              |
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

保留的平台差异：Claude 的 `model: inherit`、color/YAML 注册；Claude 四个
user-invocable skills 对应 Codex 一个 `dal-git-pr` 与共享引用；Codex TOML 注册；
Multica runtime、skills/权限/角色与调度元数据。以上不改变共同运行契约。

开放的元数据差异：roster 的部分 role 展示文案仍写“不强制逐步 TDD”“按需文档同步”等旧摘要，
与 TOML 的 implementer TDD 契约或本次指定路线不完全同义。
本 issue 明确禁止修改 role，小队 role 也不是 instructions 的注入来源，故保留并记录；
本轮 description/instructions 的同步验收不受阻。是否另改 role 文案由协调器决定。

## 验证结果与界限

- 仓库检查器初始通过 119 个文件；本轮最终通过 116 个文件。
- 扩展至所有 Git 跟踪 Markdown：基线 157 / 交付 154，CommonMark links/anchors、
  表格列数、trailing whitespace、末尾换行均通过。基线解析链接 8,114 个，交付 8,146 个。
- 对全部当前说明和操作指导中的真实 DAL include、明确 GTest suite、665 个 DAL/类型/选项名称
  作来源存在性核对，通过；`EnumName_`、`VALUE_NAME`、`DF_i`、`SuiteName` 和 `<module>`
  是明确的教学占位符。名称检查不替代逐签名编译或数值验收。
- 10 个 TOML、10 个 Claude frontmatter/body、5 个 SKILL frontmatter、1 个 openai.yaml，
  3 对规则镜像、10 个 roster 映射和前后受保护字段比对通过。
- 数据集 qa/validation 均 7,000 条，ID 唯一，70 个 family；索引含 7,000 个 JSONL 行链接，
  summary 的 qa SHA-256 与实际数据相同，历史结果为 7,000 pass / 0 fail。
- `git diff --check` 通过；仅 Markdown 变更。未修改 C++、生成文件、产品脚本、构建、测试或依赖。

检查器原有正则将旧 PDE spec 的 inline lambda 捕获和参数语法误当链接，
扩展正则扫描因此报了一个假 missing target。
CommonMark 解析确认该代码中无链接；未为误报改写有效历史代码或修改行为检查器。

当前源路径、include、命令/构建选项、版本范围和示例所有权还对照了根/组件 CMake、
`CMakePresets.json`、`build_linux.sh`、Python pyproject/workflow/bindings、
core/public example CMake 和现存测试。核心示例保留 core 位置与 APIs；
facade 示例位于 `dal-public/examples/`，本轮无移动或反向依赖。

本轮执行的是完整文件静态维护，不重新执行历史产品、Windows/Office、MC 或性能矩阵，
也不将原记录里的检查点结果冒充本轮运行结果。所有未修改指南均参加完整结构/名称/引用检查；
有实质差异的目标文件全文核对后修正。没有发现需要变更行为代码的事项。
CHANGELOG 不增加条目：没有 API/算法/重大能力/方法学变更或公开移除。

## 引用路径候选的逐项处置

完整 code-region 路径扫描把占位、历史基线、本地生成输出也列为候选。
以下列出每个不在干净检出中的唯一路径；行号可由后附程序重新生成。
历史文档已经标明 implemented history；活动控制记录由索引统一限定为其命名基线。
`.venv` 和子模块内部 include 属于复现前生成/初始化的前提；
Python README 的 `tests/...` 相对该组件目录解释；`.hpp/.cpp` 是旧控制记录的成对文件简写。
这些均未作为新的现状源路径或公开超链接。

- `.claude/api-notes/joint-aad-gradient.md`: `.claude/designs/aad-analytic-jacobian-naming-and-flag.md`; `.claude/designs/aad-analytic-jacobian-selector-api.md`; `dal-cpp/examples/joint_multi_curve_calibration/joint_multi_curve_calibration.cpp`.
- `.claude/critiques/pde-framework-reimplementation.md`: `dal-cpp/dal/math/pde/fd1d.cpp`; `dal-cpp/dal/math/pde/finitedifference.cpp`; `docs/methodology/pde.md`.
- `.claude/designs/joint-aad-gradient.md`: `.claude/designs/aad-analytic-jacobian-naming-and-flag.md`; `.claude/designs/aad-analytic-jacobian-redesign.md`; `.claude/designs/aad-analytic-jacobian-selector-api.md`; `dal-cpp/examples/joint_multi_curve_calibration/joint_multi_curve_calibration.cpp`; `dal-cpp/examples/yield_curve_jacobian`.
- `.claude/specs/2026-08-14-dal-web-extraction-design.md`: `.claude/rules/dal-web-code-style.md`; `.claude/rules/dal-web-design.md`; `.claude/skills/dal-web-setup`; `.claude/specs/dal-web-db-persistence.md`; `.codex/skills/dal-web`; `.github/workflows/ci.yml`; `.github/workflows/web-calibration-performance.yml`.
- `.claude/specs/2026-08-14-dal-web-extraction-plan.md`: `.claude/rules/dal-web-code-style.md`; `.claude/rules/dal-web-design.md`; `.claude/skills/dal-web-setup`; `.claude/specs/2026-08-14-dal-web-extraction`; `.claude/specs/dal-web-db-persistence.md`; `.codex/skills/dal-web`; `.codex/skills/dal-web/SKILL.md`; `.github/scripts/check_consistency.py`; `.github/scripts/check_frontend_quote_bump_boundary.py`; `.github/scripts/check_native_web_gateway.py`; `.github/scripts/generate_web_calibration_perf_reports.py`; `.github/scripts/generate_web_openapi.py`; `.github/scripts/run_web_calibration_perf.py`; `.github/scripts/x.py`; `.github/workflows/ci.yml`; `.github/workflows/web-calibration-performance.yml`; `dal-python/.venv/bin/python`; `docs/curve-lab.md`; `tests/test_native_runtime.py`.
- `.claude/specs/joint-aad-gradient.md`: `.claude/designs/aad-analytic-jacobian-backend-abstraction.md`; `.claude/designs/aad-analytic-jacobian-naming-and-flag.md`; `.claude/designs/aad-analytic-jacobian-redesign.md`; `.claude/designs/aad-analytic-jacobian-selector-api.md`; `dal-cpp/examples/joint_multi_curve_calibration`.
- `.claude/specs/multi-curve-simultaneous-example.md`: `.claude/critiques/multi-curve-simultaneous-example.md`; `dal-cpp/examples/curve_calibration/curve_calibration.cpp`; `dal-cpp/examples/joint_multi_curve_calibration`; `dal-cpp/examples/yield_curve_jacobian/yield_curve_jacobian.cpp`; `docs/methodology/joint_multi_curve.md`; `docs/methodology/xccy_calibration.md`; `docs/methodology/yield_curve.md`.
- `.claude/specs/pde-framework-reimplementation.md`: `dal-cpp/dal/math/pde/meshers`; `dal-cpp/examples/european_fd/european_fd.cpp`; `docs/methodology/pde.md`.
- `.claude/specs/script-compiled-evaluator-alignment.md`: `.claude/specs/perf-enhancement-candidates.md`; `dal-cpp/dal/auto/MG_NodeType_enum.hpp`; `dal-cpp/examples/uoc_compiled/uoc_compiled.cpp`.
- `.codex/artifacts/api-notes/dal-xccy-mixed-mapping-basket.md`: `dal-excel/src/__xccymontecarlo.hpp/.cpp`; `dal-excel/src/__xccymontecarlobasket.hpp/.cpp`; `dal-public/src/xccymontecarlo.hpp/.cpp`; `dal-public/src/xccymontecarlobasket.hpp/.cpp`.
- `.codex/artifacts/implementation/dal-399-script-contract.md`: `dal-cpp/externals/rapidjson/include`; `tests/script/test_instrument.cpp`.
- `.codex/artifacts/implementation/dal-400-script-preparation.md`: `dal-cpp/dal/script/calibrationpreparation.hpp/.cpp`; `dal-cpp/externals/googletest/googletest/include`.
- `.codex/artifacts/implementation/dal-401-script-mc.md`: `dal-cpp/externals/googletest/googletest/include`; `dal-cpp/externals/rapidjson/include`.
- `.codex/artifacts/implementation/dal-402-deposit-prototype.md`: `dal-cpp/externals/googletest/googletest/include`; `dal-cpp/externals/rapidjson/include`.
- `.codex/artifacts/implementation/dal-403-bondoption-prototype.md`: `dal-cpp/externals/googletest/googletest/include`; `dal-cpp/externals/rapidjson/include`.
- `.codex/artifacts/implementation/dal-405-script-curve-calibration.md`: `dal-cpp/externals/googletest/googletest/include`.
- `.codex/artifacts/implementation/dal-407-script-public-bindings.md`: `dal-cpp/externals/googletest/googletest/include`.
- `.codex/artifacts/implementation/dal-418-publication.md`: `.codex/artifacts/implementation/dal-418-evidence`; `.codex/artifacts/validation/dal-418-validation-evidence`.
- `.codex/artifacts/implementation/dal-418-rate-option-literals.md`: `.codex/artifacts/implementation/dal-418-evidence/capture_final.py`; `.codex/artifacts/implementation/dal-418-evidence/compare_snapshots.py`; `.codex/artifacts/implementation/dal-418-evidence/master_red.py`; `.codex/artifacts/implementation/dal-418-evidence/refresh_master_red.py`.
- `.codex/artifacts/implementation/dal-419-publication.md`: `.codex/artifacts/implementation/dal-419-encoding-evidence`; `.codex/artifacts/implementation/dal-419-publication-evidence`; `.codex/artifacts/implementation/dal-419-publication-evidence/audit_source.py`; `.codex/artifacts/implementation/dal-419-publication-evidence/capture_local.py`; `.codex/artifacts/implementation/dal-419-publication-evidence/make_audit.py`; `.codex/artifacts/implementation/dal-419-publication-evidence/prepare.py`; `.codex/artifacts/implementation/dal-419-publication-evidence/run_native.py`; `.codex/artifacts/validation/dal-419-validation-evidence`.
- `.codex/artifacts/implementation/dal-xccy-basket-public-bindings.md`: `dal-excel/src/__xccymontecarlobasket.hpp/.cpp`; `dal-public/src/xccymontecarlobasket.hpp/.cpp`.
- `.codex/artifacts/implementation/dal-xccy-m1-exact-model.md`: `dal-cpp/externals/googletest/googletest/include`; `tests/test_main.cpp`.
- `.codex/artifacts/implementation/dal-xccy-m4-mc-partials.md`: `dal-cpp/externals/googletest/googletest/include`.
- `.codex/artifacts/implementation/dal-xccy-m5-public-bindings.md`: `dal-public/src/xccymontecarlo.hpp/.cpp`.
- `.codex/artifacts/validation/dal-404-p0-same-source-validation.md`: `dal-cpp/externals/googletest/googletest/include`; `dal-cpp/externals/rapidjson/include`.
- `CHANGELOG.md`: `dal-cpp/examples/american_put_mc`; `dal-cpp/examples/script_tree`; `dal-cpp/examples/yield_curve_jacobian`; `docs/curve-lab.md`.
- `dal-python/README.md`: `dal-python/.venv`; `tests/test_curve_pricing.py`.
- `docs/excel/script-settings.md`: `dal-python/.venv/bin/python`.
- `docs/installation.md`: `dal-cpp/externals/rapidjson/include`; `dal-python/.venv`.
- `docs/models/gaussian-short-rate.md`: `dal-python/.venv/bin/python`.

## 逐文件基线清单

下表 SHA 为基线 UTF-8 内容 SHA-256 的前 16 位，完整值可从固定 Git blob 重算。
“保留”均包含全文读取和全量静态检查；历史/控制记录的判断范围按上文解释。

### (root)

| 文件                | 基线行数 | 基线 SHA-256 前缀    | 处置 |
|-------------------|------|------------------|----|
| `AGENTS.md`       | 106  | ae60479ab197497e | 保留 |
| `CHANGELOG.md`    | 1447 | ea049ba2c30b575d | 保留 |
| `CLAUDE.md`       | 203  | 5402f7085c6694e3 | 修正 |
| `CONTRIBUTING.md` | 199  | ac9565a3fb456386 | 保留 |
| `README.md`       | 220  | ad9519056296504f | 保留 |

### .claude

| 文件                                                      | 基线行数 | 基线 SHA-256 前缀    | 处置      |
|---------------------------------------------------------|------|------------------|---------|
| `.claude/agents/README.md`                              | 75   | 393f8f48877cd657 | 保留      |
| `.claude/agents/dal-api-designer.md`                    | 17   | 51379dd89a8fb629 | 保留      |
| `.claude/agents/dal-critic.md`                          | 15   | ef5abfc77268a473 | 保留      |
| `.claude/agents/dal-doc-writer.md`                      | 17   | 0adc325834193619 | 保留      |
| `.claude/agents/dal-implementer.md`                     | 17   | 53687dd8397407f1 | 保留      |
| `.claude/agents/dal-orchestrator.md`                    | 23   | 7763d601878c3b19 | 保留      |
| `.claude/agents/dal-performancer.md`                    | 17   | a58367b0b076a93d | 保留      |
| `.claude/agents/dal-reviewer.md`                        | 17   | 023c59cb98833815 | 保留      |
| `.claude/agents/dal-simplifier.md`                      | 17   | 913221cb83c06c84 | 保留      |
| `.claude/agents/dal-spec-writer.md`                     | 15   | 244d8906116f96e5 | 保留      |
| `.claude/agents/dal-tester.md`                          | 19   | a0809fcb6002e0da | 保留      |
| `.claude/api-notes/joint-aad-gradient.md`               | 365  | bdaaffc10f360190 | 保留已标注历史 |
| `.claude/critiques/pde-framework-reimplementation.md`   | 218  | d1a119cdda581a15 | 保留已标注历史 |
| `.claude/designs/api-shape-dedup.md`                    | 488  | 2162ead169f13d67 | 保留已标注历史 |
| `.claude/designs/joint-aad-gradient.md`                 | 1623 | ca47b8d3b331a50d | 保留已标注历史 |
| `.claude/rules/code-style.md`                           | 284  | 6441d5a32b6770e1 | 保留      |
| `.claude/rules/git-commit-pr.md`                        | 55   | 683f313aec5e137b | 保留      |
| `.claude/rules/unit-test-style.md`                      | 53   | 6b1fe13503006fd6 | 保留      |
| `.claude/skills/dal-code-style-review/SKILL.md`         | 14   | 575bd0323e36a504 | 保留      |
| `.claude/skills/dal-commit-and-pr/SKILL.md`             | 17   | e9e0b0845ae728e0 | 保留      |
| `.claude/skills/dal-unit-test-skill/SKILL.md`           | 14   | 63ef3db3ce2216c2 | 保留      |
| `.claude/skills/dal-unit-test-write/SKILL.md`           | 17   | c3720a6c73482b7d | 保留      |
| `.claude/specs/2026-08-14-dal-web-extraction-design.md` | 137  | 6aa0ceba0f4a4017 | 保留已标注历史 |
| `.claude/specs/2026-08-14-dal-web-extraction-plan.md`   | 1038 | f889b48518d51b0d | 保留已标注历史 |
| `.claude/specs/joint-aad-gradient.md`                   | 873  | b0113c6a1371241c | 保留已标注历史 |
| `.claude/specs/multi-curve-simultaneous-example.md`     | 830  | 96045456caa839f0 | 保留已标注历史 |
| `.claude/specs/pde-framework-reimplementation.md`       | 855  | 1b08a51228867676 | 保留已标注历史 |
| `.claude/specs/script-compiled-evaluator-alignment.md`  | 200  | 43765e0e74e1e3f9 | 保留已标注历史 |

### .codex

| 文件                                                                        | 基线行数 | 基线 SHA-256 前缀    | 处置         |
|---------------------------------------------------------------------------|------|------------------|------------|
| `.codex/README.md`                                                        | 52   | 90fc972b81004bff | 保留         |
| `.codex/artifacts/README.md`                                              | 124  | 202144862c288a0c | 修正         |
| `.codex/artifacts/api-notes/dal-398-script-contract.md`                   | 753  | d82687a4b8e0808e | 修正         |
| `.codex/artifacts/api-notes/dal-410-xccy-mc-model-decision.md`            | 979  | c2ff68e1a696b620 | 修正         |
| `.codex/artifacts/api-notes/dal-xccy-mixed-mapping-basket.md`             | 888  | 3c5ae6eed21ed058 | 保留控制/检查点证据 |
| `.codex/artifacts/documentation/dal-418-rate-option-literals.md`          | 68   | 5864a1a8bbf24204 | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-379-market-iv-ci-repair.md`          | 206  | dd103f5870c0e6bf | 退役，Git 保留  |
| `.codex/artifacts/implementation/dal-387-benchmark-repair.md`             | 176  | 95045ddd34b203d8 | 退役，Git 保留  |
| `.codex/artifacts/implementation/dal-399-script-contract.md`              | 377  | a2403f61c2a228ac | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-400-script-preparation.md`           | 321  | 130f62e5976bcfa0 | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-401-script-mc.md`                    | 278  | fe9fd9ced99adf35 | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-402-deposit-prototype.md`            | 312  | 50728cb53bcaa7e4 | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-403-bondoption-prototype.md`         | 419  | 66e406cea379a1cb | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-405-script-curve-calibration.md`     | 324  | f5b4b534d2c1cac9 | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-406-curve-product-fast-paths.md`     | 329  | a60e15ba511134b5 | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-407-script-public-bindings.md`       | 580  | 362b832aaa3367bd | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-408-xccy-script-cashflows.md`        | 455  | d8cbf1144cd84875 | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-409-script-xccy-calibration.md`      | 315  | 920e9a2c1495007e | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-411-gaussian-fast-paths.md`          | 306  | 2f954a1a350cc234 | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-412-gsr-script-mc-calibration.md`    | 796  | c6f17a5d357387e4 | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-414-gsr-script-public-bindings.md`   | 426  | fcd4e1a02e6deec0 | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-418-approved-publication.md`         | 115  | 5090df4f573b25c8 | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-418-publication.md`                  | 145  | 5d628d82fb0d99ae | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-418-rate-option-literals.md`         | 76   | f36598571c34941a | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-418-test-complexity.md`              | 120  | d3510282d9a89e7f | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-419-installed-windows-encoding.md`   | 197  | 168fcd01860678e3 | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-419-publication.md`                  | 171  | ce990b4e63498862 | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-xccy-basket-public-bindings.md`      | 245  | 2e2650a6d8eac92c | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-xccy-basket-public-example.md`       | 264  | b9e28b10047f08dd | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-xccy-m1-exact-model.md`              | 463  | 53462f2cd4359657 | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-xccy-m2-observation-execution.md`    | 197  | 26f9a1b10557a1d6 | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-xccy-m3-moments-staged.md`           | 439  | 82aa7103d977a812 | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-xccy-m4-mc-partials.md`              | 440  | 55658df306c59bb8 | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-xccy-m5-public-bindings.md`          | 423  | 7701489a9d89c88e | 保留控制/检查点证据 |
| `.codex/artifacts/implementation/dal-xccy-mixed-mapping-basket-core.md`   | 326  | feae2a49d33a955c | 保留控制/检查点证据 |
| `.codex/artifacts/reviews/dal-379-market-iv-surface.md`                   | 213  | 23bfb1cd765d6004 | 退役，Git 保留  |
| `.codex/artifacts/reviews/dal-418-rate-option-literals.md`                | 75   | cfb1e9ffa503c79a | 保留控制/检查点证据 |
| `.codex/artifacts/reviews/dal-418-test-complexity.md`                     | 72   | 995b53049b9a8802 | 保留控制/检查点证据 |
| `.codex/artifacts/reviews/dal-419-installed-windows-encoding.md`          | 76   | f5852d6fc9a23575 | 保留控制/检查点证据 |
| `.codex/artifacts/validation/dal-379-market-iv-surface.md`                | 255  | 3c59f0311c3e1c1d | 退役，Git 保留  |
| `.codex/artifacts/validation/dal-404-p0-same-source-validation.md`        | 389  | ebc675c242524aa6 | 保留控制/检查点证据 |
| `.codex/artifacts/validation/dal-413-gsr-script-numerical-validation.md`  | 254  | fcbe8dc7440b6f3a | 保留控制/检查点证据 |
| `.codex/artifacts/validation/dal-418-rate-option-literals.md`             | 136  | 8887d581e2668056 | 保留控制/检查点证据 |
| `.codex/artifacts/validation/dal-419-installed-windows-encoding.md`       | 114  | fb89c37a72a562de | 保留控制/检查点证据 |
| `.codex/artifacts/validation/dal-xccy-gaussian-numerical-validation.md`   | 1916 | c788971185a33067 | 保留控制/检查点证据 |
| `.codex/artifacts/validation/dal425-financial-bias-bound-reference.md`    | 300  | 2f15f0bcafdf1e4a | 保留控制/检查点证据 |
| `.codex/artifacts/validation/dal425-finite-stream-bias-reference.md`      | 285  | 31a6054fab531b89 | 保留控制/检查点证据 |
| `.codex/artifacts/validation/dal425-joint-law-applicability-reference.md` | 239  | 10deda438510ee35 | 保留控制/检查点证据 |
| `.codex/references/benchmark-workflow.md`                                 | 347  | 7d2b21fad89709c3 | 保留         |
| `.codex/references/code-style.md`                                         | 284  | 6441d5a32b6770e1 | 保留         |
| `.codex/references/component-boundaries.md`                               | 34   | 6fdcfcfee3e8fea8 | 保留         |
| `.codex/references/git-commit-pr.md`                                      | 55   | 683f313aec5e137b | 保留         |
| `.codex/references/run-tests.md`                                          | 60   | d13ff1a69f2d1710 | 保留         |
| `.codex/references/style-review.md`                                       | 86   | 9f89018398a43770 | 保留         |
| `.codex/references/unit-test-style.md`                                    | 53   | 6b1fe13503006fd6 | 保留         |
| `.codex/references/write-tests.md`                                        | 202  | 875c0e96c0933e69 | 保留         |
| `.codex/skills/dal-agent-team/references/shared-rules.md`                 | 10   | 04913d1c3482e0a0 | 保留         |
| `.codex/skills/dal-git-pr/SKILL.md`                                       | 77   | 1510c9af7a473eae | 保留         |
| `.codex/skills/dal-git-pr/references/publish-workflow.md`                 | 282  | 11337d4e14df68d9 | 保留         |

### .github

| 文件                                 | 基线行数 | 基线 SHA-256 前缀    | 处置 |
|------------------------------------|------|------------------|----|
| `.github/copilot-instructions.md`  | 92   | 6e152193d35a3e5e | 保留 |
| `.github/pull_request_template.md` | 16   | 69353a9445b6976d | 保留 |
| `.github/scripts/tsan/README.md`   | 35   | d04c240de6923e74 | 保留 |

### dal-cpp

| 文件                                            | 基线行数 | 基线 SHA-256 前缀    | 处置 |
|-----------------------------------------------|------|------------------|----|
| `dal-cpp/README.md`                           | 74   | 8da68473774fd8d8 | 保留 |
| `dal-cpp/benchmarks/script_mc_perf/README.md` | 182  | d4b2196b4f67cfe8 | 保留 |
| `dal-cpp/examples/README.md`                  | 97   | e152112cf35938b1 | 保留 |
| `dal-cpp/experimental/passivesink/README.md`  | 123  | cd4e7144796a14fd | 保留 |

### dal-excel

| 文件                                                   | 基线行数 | 基线 SHA-256 前缀    | 处置 |
|------------------------------------------------------|------|------------------|----|
| `dal-excel/README.md`                                | 193  | b4bb068b2a3fa942 | 保留 |
| `dal-excel/examples/008.quote_risk.md`               | 42   | d3d4fe7e0e9fb01c | 保留 |
| `dal-excel/examples/009.generic_joint_quote_risk.md` | 63   | ae2cb7842236a153 | 保留 |
| `dal-excel/examples/011.native_aad_risk.md`          | 91   | 3be82ed5dffd7351 | 保留 |

### dal-public

| 文件                     | 基线行数 | 基线 SHA-256 前缀    | 处置 |
|------------------------|------|------------------|----|
| `dal-public/README.md` | 133  | 0cfac9e193ab622f | 保留 |

### dal-python

| 文件                                | 基线行数 | 基线 SHA-256 前缀    | 处置 |
|-----------------------------------|------|------------------|----|
| `dal-python/README.md`            | 1145 | a2f6eec79e8b24f0 | 保留 |
| `dal-python/benchmarks/README.md` | 678  | 68ff5418ede67730 | 保留 |

### datasets

| 文件                                      | 基线行数 | 基线 SHA-256 前缀    | 处置 |
|-----------------------------------------|------|------------------|----|
| `datasets/dal_script_qa_7000/README.md` | 99   | 816de15a8e76e2a5 | 保留 |
| `datasets/dal_script_qa_7000/index.md`  | 7170 | 54c1737c117b9ac9 | 保留 |

### docs

| 文件                                                   | 基线行数 | 基线 SHA-256 前缀    | 处置 |
|------------------------------------------------------|------|------------------|----|
| `docs/README.md`                                     | 70   | bb324385e98f89c7 | 修正 |
| `docs/architecture.md`                               | 257  | 43a592eca5db9be8 | 保留 |
| `docs/ccy-curves/README.md`                          | 10   | 1859d9cb961a04e9 | 保留 |
| `docs/ccy-curves/pricing-calibration.md`             | 425  | 52e1e6965aa0fac1 | 保留 |
| `docs/examples.md`                                   | 127  | e4ad9d22935949ab | 保留 |
| `docs/excel/README.md`                               | 989  | cc6af41d470be6f7 | 保留 |
| `docs/excel/script-settings.md`                      | 512  | 0670ac0bf4ea37f3 | 保留 |
| `docs/installation.md`                               | 476  | d6ecbc56f587d648 | 保留 |
| `docs/methodology/README.md`                         | 36   | 2e57a5480f5fd7ac | 修正 |
| `docs/methodology/_cpp-example-style.md`             | 223  | 90e562a6c49375b4 | 保留 |
| `docs/methodology/aad-sparsity.md`                   | 324  | 65ed86a55db4c53f | 保留 |
| `docs/methodology/aad.md`                            | 2690 | f84f5535b40b37a1 | 保留 |
| `docs/methodology/dates.md`                          | 178  | 5b83997e16bbfc9c | 保留 |
| `docs/methodology/index_parsing.md`                  | 195  | fcedab268a33fb2c | 保留 |
| `docs/methodology/interpolation.md`                  | 233  | 7e7aea7d2987bace | 保留 |
| `docs/methodology/matrix.md`                         | 619  | 91901b449278e1b5 | 保留 |
| `docs/methodology/monte-carlo/README.md`             | 27   | 111c35a3d5db486f | 保留 |
| `docs/methodology/monte-carlo/lsm.md`                | 512  | 500b5bbae8e421b4 | 保留 |
| `docs/methodology/monte-carlo/sampling.md`           | 488  | b2ef3591ce37fc33 | 保留 |
| `docs/methodology/monte-carlo/simulation.md`         | 154  | be9d23412b31c70c | 保留 |
| `docs/methodology/monte-carlo/special-techniques.md` | 98   | 90bba06dcc3bceba | 保留 |
| `docs/methodology/passive-constants.md`              | 82   | 3238a1148d293e18 | 保留 |
| `docs/methodology/pde/README.md`                     | 13   | fe04362d7da00f7c | 保留 |
| `docs/methodology/pde/aad.md`                        | 148  | 597f77804731d1e8 | 保留 |
| `docs/methodology/pde/early-exercise.md`             | 218  | 72bc38df3bc0f8cf | 保留 |
| `docs/methodology/pde/framework.md`                  | 367  | 30c51c458c3cfec0 | 保留 |
| `docs/methodology/pde/option-pricing.md`             | 74   | e6964a98afa964c5 | 保留 |
| `docs/methodology/quadrature.md`                     | 257  | d6071443ea905d54 | 保留 |
| `docs/methodology/script_calibration_contract.md`    | 1412 | 4d4f4c1d8cb6db28 | 保留 |
| `docs/methodology/script_engine.md`                  | 1823 | bf6a52508ce686f5 | 保留 |
| `docs/methodology/underdetermined_search.md`         | 465  | 1b9e4e423bc95c2b | 保留 |
| `docs/models/README.md`                              | 22   | e0e370ac3ce01778 | 保留 |
| `docs/models/black-scholes.md`                       | 315  | 27b6f1e9b8d81803 | 保留 |
| `docs/models/correlated-bs.md`                       | 115  | 836b5d5a03302d49 | 保留 |
| `docs/models/dupire.md`                              | 206  | b0d08f2b589bee5b | 保留 |
| `docs/models/gaussian-short-rate.md`                 | 550  | 1dff0251d981a6e4 | 保留 |
| `docs/models/hybrid-model.md`                        | 198  | 429fcc4fdd103d15 | 保留 |
| `docs/models/local-volatility.md`                    | 116  | 4fa5a9c3f06a86e4 | 保留 |
| `docs/models/market-iv-surface.md`                   | 320  | 6340fc476b09b824 | 保留 |
| `docs/public-api.md`                                 | 754  | 33434e608b6052d1 | 保留 |
| `docs/python/README.md`                              | 978  | 4f1f10292492a599 | 保留 |
| `docs/yield-curves/README.md`                        | 22   | b249fdbeafc00144 | 保留 |
| `docs/yield-curves/construction.md`                  | 844  | 6daeb5e68379ae6d | 保留 |
| `docs/yield-curves/jacobian-risk.md`                 | 815  | 62f6165c725bf2d2 | 保留 |
| `docs/yield-curves/joint-quote-risk.md`              | 216  | aae501ee285a12ed | 保留 |
| `docs/yield-curves/log-discount.md`                  | 247  | d159638183b56dfb | 保留 |
| `docs/yield-curves/node-risk.md`                     | 189  | 3dfabd9b13596003 | 保留 |
| `docs/yield-curves/ptirds-replication.md`            | 413  | 251132eae9447f23 | 保留 |

### tests

| 文件                                             | 基线行数 | 基线 SHA-256 前缀    | 处置 |
|------------------------------------------------|------|------------------|----|
| `tests/lsmc-training-study/fixtures/README.md` | 23   | cef26de5e09b2d82 | 保留 |

新增：`.codex/artifacts/documentation/dal-436-documentation-agent-consistency.md`；本报告不保存自身递归内容哈希。

## 非 Markdown 注册清单

| TOML                                | SHA-256                                                          | Claude 注册                          |
|-------------------------------------|------------------------------------------------------------------|------------------------------------|
| .codex/agents/dal-api-designer.toml | bb7ab424fd8cd5cb5176cf883ede7842311199e0e012d0faa7217832d35e4d78 | .claude/agents/dal-api-designer.md |
| .codex/agents/dal-critic.toml       | e30a2a9dbb4d8ef481adbe7eaadaf4941ebd7e6a1a09f193043453f95b70cdef | .claude/agents/dal-critic.md       |
| .codex/agents/dal-doc-writer.toml   | 4358c1aefbe092f15067768fe568039831cd1480aa776778ec92bc75784f11d1 | .claude/agents/dal-doc-writer.md   |
| .codex/agents/dal-implementer.toml  | c76cd703eb7a40ff82632642a0bd2ffd430b0c117b3c7a7103c42374ec0804ac | .claude/agents/dal-implementer.md  |
| .codex/agents/dal-orchestrator.toml | 9234da6826142e46bdb937d0422cb4a6511b6d5b478164d498eadc1342eac540 | .claude/agents/dal-orchestrator.md |
| .codex/agents/dal-performancer.toml | fa0400f244d8893316d8c96cf59865622af97089fcffbe70d3fdb048d8b818f9 | .claude/agents/dal-performancer.md |
| .codex/agents/dal-reviewer.toml     | 5a4b44b83770d3286afa659f2e804c89a3641f5b5eee1b769e8ebc1f55248564 | .claude/agents/dal-reviewer.md     |
| .codex/agents/dal-simplifier.toml   | 91cf52a282979fdc05952e92417ed60b33e9157fb75f9a092292e5f65e1319be | .claude/agents/dal-simplifier.md   |
| .codex/agents/dal-spec-writer.toml  | ed0159f7896140b3d506331ab56e4406d7d33f6a734c02ae023f2adfe2844064 | .claude/agents/dal-spec-writer.md  |
| .codex/agents/dal-tester.toml       | 68c65d423a7a1d79db2beae3ab4cd20d227947277594ef86f4997426833c78f1 | .claude/agents/dal-tester.md       |

另检 `.codex/skills/dal-git-pr/agents/openai.yaml`（YAML interface 三个字段非空）；五个 SKILL 的实际文件见上表 Markdown 清单。

## 可重放命令

工具环境：Python 3.13、markdown-it-py 4.0.0、PyYAML 6.0.3。
本地已有依赖，未为本任务安装或更新依赖。

```sh
python3 .github/scripts/check_docs.py
git diff --check
git ls-files '*.md'
git ls-files '.codex/agents/*.toml' '.codex/skills/**/agents/openai.yaml'
git fetch origin master
git ls-remote origin refs/heads/master
```

将下一段 Python 保存到仓库外 `../dal436_check.py`，从仓库根运行
`python3 ../dal436_check.py`。它只读仓库和 Multica，写一份仓库外 JSON 证据；
不会同步任何 agent。若存在本轮入口快照 `../dal436-agents-before.json`，还会比较受保护字段；
否则可用上方指纹核对当前状态，不能据此重建过去的可观察状态。
该代码保留逐文件完整 SHA、命令行数和候选引用，便于独立验收。

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
base = 'dd6ddc3e9dd1637634287bcbe8dd8d5bc7fc49a4'
report = '.codex/artifacts/documentation/dal-436-documentation-agent-consistency.md'


def run(*args):
    return subprocess.check_output(args, text=True)


def digest(value):
    return hashlib.sha256(value.encode()).hexdigest()


tracked = run('git', 'ls-files').splitlines()
baseline = run('git', 'ls-tree', '-r', '--name-only', base).splitlines()
paths = sorted({p for p in tracked if p.endswith('.md') and Path(p).is_file()} | {report})
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
before_path = root.parent / 'dal436-agents-before.json'
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
assert not errors, '\n'.join(errors)
result = {'baseline': base, 'baseline_markdown_count': sum(p.endswith('.md') for p in baseline),
          'markdown_count': len(paths), 'parsed_links': link_count,
          'distribution': dict(collections.Counter(p.split('/')[0] if '/' in p else '(root)' for p in paths)),
          'agents': agents, 'registrations': registrations, 'skills': skills,
          'metadata': metadata, 'source_symbols': len(checked_symbols),
          'missing_path_candidates': missing_paths, 'inventory': inventory,
          'retired': sorted(p for p in baseline if p.endswith('.md') and not Path(p).exists()),
          'errors': errors}
(root.parent / 'dal436-audit.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
print(json.dumps({k: result[k] for k in ('baseline_markdown_count', 'markdown_count', 'parsed_links', 'distribution', 'source_symbols', 'errors')}, ensure_ascii=False))
print('TOML/Claude/Multica: 10 exact matches; protected fields unchanged; five skill frontmatters and YAML pass.')
```
