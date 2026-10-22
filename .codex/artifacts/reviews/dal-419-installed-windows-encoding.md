# DAL-419 installed Windows encoding review

## Findings

无发现。未发现本地表示保持修复的正确性、兼容性、安全、组件边界或必要验证阻断项。

精确受审 HEAD：**`909d59d3c49b3a116e2835511cf6ebaf6fa96840`**，分支 `agent/dal-implementer/3103ab041f74`，开工工作树干净。测试提交为 `a4ab9bef1dff4ac580d6bc9c6680bb7e9caacd1c`，产品修复为 `1bbb195aab407ed6e63f6b9dd5490c28d2f4066d`，实际 base 为 `bf4cabea8575206375047e51993a5aa5aeb54c0c`。受测源 HEAD `696edf742f4c9024e7745f55c65923b56d98abca` 至受审 HEAD 仅增加独立验证记录；两项产品/测试文件与受测修复完全相同。

本轮明确审查尚未发布的本地内容。Multica 的 DAL-419 PR 列表为空，远端没有该实现分支；未将旧 [PR #620](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/620) 的内容或 CI 当作本修复证据。

## Open Questions

无待作者或用户裁定的接口或行为问题。无需追加 spec/API/critic 链。

未完成的是交付门禁：新 master **`acadd8ee44fdb6a0bb2e70de9d559db59de066f8`** 的 132 文件整合、受影响安装门禁、独立 PR 及最终 head 完整 CI。当前结论不批准尚未产生的整合内容，不代表父 DAL-418 AC8 已完成；由原作者后续正常整合，并对冲突解决或新增语义风险做必要有限复核。

## Tests

### 本 reviewer 实际执行

在来源和二进制哈希核验后运行现存 Linux 构建：

```bash
build/dal419-linux-focused/debugger_tests
build/dal419-linux-focused/compat
git diff --check bf4cabea 909d59d3
clang-format --dry-run --Werror --lines=305:346 \
  dal-cpp/dal/script/visitor/debugger.hpp
clang-format --dry-run --Werror --lines=27:58 \
  tests/installed-consumer/main.cpp
```

结果：**42/42 PASS**；compat 的 **3,456 字节**输出与真实旧 master Windows oracle 去除 CRLF 转换后的字节完全一致；改动区域格式和增量空白检查通过。没有新建测试、重建产品或重复全量矩阵。

只读审计实际重算并核验：

- 作者 archive、**125 项** raw evidence、**7 项** metadata、**19 项**现存 binary 哈希；tester archive/report、**122 项** raw evidence、**16 项**现存 binary 和 **6 项**工具哈希，全部相符。
- 当前两项产品/测试源与 tester 原生导出、fix commit 相等；安装后的 header 与受审 header 原始字节相等。五个依赖 pins 与受审源一致，跟踪内容干净。
- 运行已读取并确认的纯词法/转义审计函数：排除注释后 **384 个实际字符串 token** 的 payload 全部不变，**23 个**字符串仅拼写变化，其他代码 token 全部相等。作者的 421 计数包含注释中的引号，不能当作实际代码 token 数量；该差异已在独立验证中正确解释。
- 对两种原生 CMake 下 consumer 各 1 条、Excel tests 各 32 条及 compat 1 条，合计 **67 条实际编译命令**核查，均无 `/utf-8`、source/execution charset 绕过。原始 driver 在 VS setup 前后检查 CL/_CL_，仅查询 `chcp`；运行日志记录系统/控制台 CP936、CL/_CL_ 为空及对应版本。
- 作者实际 master header/consumer/Excel/test-only 的四份 RED 日志，以及 tester 独立旧 master header RED，均含原 C2001；setup harness 错误没有计入有效 RED。

### 核验并复用的独立 Windows 结果

以下由 tester 执行，本轮审计其日志、命令、cache、来源和 executable 身份后复用，未重复 Windows 构建：

- 原生 CMake **4.2.3**：新鲜 core/public 安装；原默认 installed-consumer **3/3 PASS**，原 installed-Excel **259/259 PASS**，tests-OFF/ON XLL 与 package/runtime audit 均通过。
- 原生 CMake **3.27.6**：独立新建消费目录运行同两条原驱动，分别 **3/3、259/259 PASS**，tests-OFF/ON XLL/audit 均通过。
- 原生 debugger/exercise **42/42 PASS**；默认编码编译的 compat 输出 **3,658 字节**与真实旧 master oracle 完全相等，覆盖两模式全部 **66 个符号**、宽度 **1/20/125** 和空产品。输出 SHA-256 为 `96d36cc6aa6479f06f786fa615dd02da86f25a8052226cec38ce22fb8922fd9b`。

旧 master oracle 特意以 `/utf-8` 编译，仅用于取得原 UTF-8 字节期望；既有 Unicode golden 测试 target 的 `/utf-8` 也明确保留。它们没有替代默认 consumer、Excel tests 或新 compat 的编译证据。XLL 自身的既有私有 `/utf-8` 与默认消费目标分开核查。

### 历史 CMake 版本更正

重新核验父 DAL-418 原生 Windows 缓存，SHA-256 仍为原 manifest 的 `2d489b1c77bf2f6175d4b0f9e34ed3b73dd8114082c36c718207ca8b75bfb40e`；内部 MAJOR/MINOR/PATCH 明确为 **3/27/6**。因此父原生失败环境实际为 **CMake 3.27.6**，原报告称 4.2.3 有误，4.2.3 是当时 Linux 版本。作者和父原生环境没有通过切换版本消除失败。

tester 对历史记录的更正有未改原始缓存及哈希支持，且新实验分别覆盖两个原生版本。原始报告/证据未被改写，本轮也未编辑冻结产物。作者实施记录中的父版本描述视为被该有据更正取代。

## Summary

完整读取两项变更源文件、实际 diff、implementation/validation、证据生成与审计脚本，以及必要的 installed-consumer CMake/driver 和渲染方法学。沿用已读取且与本 base 相同的 reviewer、C++/测试 style 和 component-boundary 契约。

- `dal-cpp/dal/script/visitor/debugger.hpp:306` 保持 `const char*`/C++17 表面、静态对象、字段次序和 ASCII 模式。23 个非 ASCII literal 改为明确 UTF-8 字节；每个 `\x` 的最长十六进制消费均在下一个反斜杠、空格或引号处结束，没有后续十六进制字符被意外吞入，也没有使用会改变 C++20 字符类型的 `u8` 前缀。对所测 MSVC/GCC 目标，原 Unicode 输出字节和宽度语义保持。
- `tests/installed-consumer/main.cpp:28` 检查两模式全部 33 字段，独立固定字节期望、保留空格和符号顺序，且在 `main.cpp:256` 必须成功才继续执行。它进入既有 installed consumer，原三个 CTest 项及原断言保留；能够保护实际下游默认编译边界。
- 无 CMake/export、API/default/error、依赖或生成文件变化。新增 core header include 位于原已链接 `DAL::public` 的根级安装消费测试，依赖方向合法；没有 core example 反向依赖或迁移。性能仅涉及相同常量的源码表示，无新增渲染/定价/AAD 计算，不选择无关 benchmark。
- `docs/methodology/script_engine.md` 的 Unicode/ASCII、UTF-8 terminal 和 width 契约仍然适用。修复不改变终端显示要求，无需新增能力说明或 CHANGELOG 条目。父 BG-1 review/docs 及已接受 #621 内容未重开全审。

实时 `git ls-remote` 确认 master 为 `acadd8ee`。核对该上游增量确为 132 文件；debugger header、两安装驱动/consumer、root/Excel CMake 及依赖 pins 没有变化，但 public/Python CMake、新 Excel 源/测试等会影响完整集成安装。其未整合、未验证的事实不能由本地旧 base 的绿色结果替代。

残余风险：本轮只验证表示保持和适用既有证据，未重跑全量产品矩阵、所有编译器/诊断/sanitizer CI，也未测试新 master 整合。既有 C4819 警告仍可见，不承诺默认 `/WX` 无警告。父 BG-1 完整矩阵属于另一源版本，不标为子任务新运行；DAL-419 合入 master、#620 真实集成及父最终验收仍待完成。

## Verdict

**Approve** — 仅适用于本地 **`909d59d3c49b3a116e2835511cf6ebaf6fa96840`** 的 DAL-419 表示保持修复和上述适用证据；没有未解决审查发现。

本轮只新增本审查报告，未改产品/测试/其他产物，未提交 GitHub review、resolve、commit、push、开 PR、ready、merge 或派工。DAL-419 保留 `in_review`，DAL-418/DAL-408 保持 `in_progress`，#620 保持 draft；独立 PR、最终 head CI 和后续集成交协调器接续。
