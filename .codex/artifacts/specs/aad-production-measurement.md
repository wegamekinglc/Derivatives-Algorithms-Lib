# 原生 AAD 生产阶段与资源测量规格

状态：P01 继续实施。D00–D03 已在 `b42f9eb` 完成功能、性能、数值与 CI 验收；
最新证据提交为 `6161e5a`。完整阶段 A 仍未完成，阶段 B/C/D 的范围保持。

本规格落实[完整方案](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/de5dd8b20089223e6938dfd8100d463aa6d4a169/.codex/artifacts/plans/aad-improvement-plan.md)
5.1、附录 C.2 与 D.1 中尚未完成的生产 MC 阶段、资源和线程扩展要求。
已有 tape/真实曲线 Jacobian、九项目性能门禁、25 个曲线与 44 个生产案例证据继续保留。
不会将新增测量列入正式九项门禁或替代其失败判定。

生产测量实现已发布于 `5a6382b1222a84db725b6e86a6d09bc4d1089345`，
其源码／配置／五项依赖／33 个二进制已冻结，身份保留于
`production-profiling-5a6382b-environment-01.json`。与原始 `b5e3caca` 的
九项两轮各十次配对全部通过，65 个可比案例及 Sobol 比率规则通过；
10 项新增案例仅作信息。原始日志、所有样本、结果和表格位于
`/tmp/dal-aad-evidence/production-profiling-paired-01/`。
这只关闭正式九项门禁，不验收生产补充、线程扩展、资源或诊断开销。

首个发布头 Codacy 报告 26 项新问题，主要为 CLI checker 的 assert、
复杂度和显式进程调用，以及消费者 main 的复杂度。保留完整 annotations。
checker 现使用 unittest 常开断言与更小的检查函数；执行前验证 DAL build-tree
位置和固定 executable 名，保持 shell=False；局部进程检查注释采用既有 gate 的
明确、受限测试执行约定，不改变全局检查配置。
旧 checker 在 Python -O 下错误接受失败进程的 RED 已保留；新 checker
在 -O 下拒绝同一失败，并在进程执行前拒绝非 DAL 路径。
ON 六项契约与 OFF 优化模式六项契约通过，消费者三配置各 2/2 再次通过。
Lizard 实测 Python/C++ 最大复杂度 8/4（阈值 8）。生产及 benchmark 源码未因
这次修正改变；新 exact-head CI 仍待验证。GitHub 当前报告 PR mergeable=false，
没有为 `5a6382b` 创建 Actions runs。补齐隔离 checkout 的祖先对象后，
确认 master 新增 `3fe44ecd` 文档提交，唯一冲突位于 Copilot 的 CI 说明；
将保留其 source/docs-only 区分并落实本任务的原生 AAD 范围。

## 问题与实现边界

现有普通 MC 和 LSM 基准主要给出完整执行时间；过程内 RSS 不能解释准备、
活动参数重注册、路径生成/录制、payoff、后缀与前缀 reverse、任务等待和归约各占多少。
多个 worker 的耗时相加也不是用户看到的请求延迟。

新增明确可选择的诊断测量。默认生产构建不在每节点或每路径维护计数，
计时与存储扫描在专用构建／显式诊断请求中执行。未经插桩的 Release 二进制
继续用于吞吐与原始基线配对；诊断成本另行报告。
本次实现不借机改变 batch 分组、RNG、归约顺序、模型缓存、LSM 回归策略或估值数值。

编译开关为默认 OFF 的 `DAL_ENABLE_AAD_PROFILING`，通过库目标和安装包传播，
避免不同 TU 对头文件内诊断路径的定义不一致。它独立于活动数生命周期诊断，
不改变 Number_/node/tape 的布局，不表示支持原生 reverse event 或高阶 AAD。
仅开启构建开关也不会自动把每个生产请求变成测量请求。

## 要求

R01. 默认 OFF 的成功路径条件编译移除新诊断指针、计时、采样和扫描。
不得仅以一个每路径 null 检查或空虚调用声称默认零成本。保留原有 hot loop 和单位语义。

R02. 诊断配置提供明确的请求范围和每 worker 的局部数据收集；所有任务完成／drain 后才归约数据。
数据所有权跟随现有估值请求，不能把活动数或 tape 句柄传给主线程，不能悬挂指针。
失败清理仍保留原业务异常，下一独立请求可恢复；未完成请求不发布成功结果或伪造完整测量。

R03. 记录顶层请求墙钟、准备（含适用的编译／历史状态／LSM plan）、worker 初始化、
路径前向、payoff、后缀 reverse、前缀 reverse、等待／归约，以及 LSM 训练与定价回放边界。
路径生成与活动录制交织时明确为同一窗口，不声称独立测得每条边的录制耗时。
所有窗口明确父子或包含关系，不能重复相加；无法无歧义分离时报告组合窗口及原因。

R04. 请求墙钟与 worker 累计墙钟分别输出。若报告累计 CPU 时间，使用实际线程 CPU 计时，
并明确平台可用性；不得把 steady_clock 的 worker 总和标成 CPU 时间。
同一个字段不能在 Linux/Windows 有不同含义；不支持的观测明确标为不可用。

R05. 从现有 native tape 存储显式读取逻辑节点/边、使用槽、allocated blocks、live/occupied/capacity bytes。
观测高水位需要在有效录制阶段采样，不能在 rewind 后把零节点当峰值。
固定块容量增长与实际新块分配事件区分；若不能获得真实累计分配次数，不能以保留块数代替。
扫描在诊断请求中执行，记录频率与开销；默认节点不新增计数字段。

R06. 内存分开说明 tape 使用量、保留容量、路径/工作区/结果数组，以及 LSM 回归数据。
RSS 为进程层观测；不能通过 RSS 减法声称精确得到 tape 或外部缓存量。
并发 worker 的局部峰值之和只是上界或代理量，不能伪装成同一时刻的总内存峰值。

R07. 输出实际参数数、不同输出数、传播通道宽、路径数、事件/时间步数、线程数、RNG/seed/BB、
树解释/编译、普通/LSM 方法和训练/定价策略。所有计数依据实际执行，不使用产品名推断。

R08. 扩展 `script_mc_perf`，保留原有默认 CLI、案例名、路径数和计时含义。
显式新增模式用于冷请求、稳定重复请求和阶段诊断；模型/产品/cache 准备是否计入必须可见。
同一生产场景先执行独立数学或固定路径数值验证，再接受计时输出。

R09. 普通 MC 至少覆盖短请求、长事件路径、逐步扩大的固定参数 local-vol 表面；
树解释与编译，以及相同路径的 passive/AAD 对照。记录真实构造和运行数字。
大表面使用固定校准输入，不将固定参数风险误标为市场报价风险；F01 的完整报价链另行实现。

R10. LSM 至少覆盖现有 BS tree/compiled 与 daily local-vol compiled 场景。
训练、回归、定价回放和 AAD 风险分别解释，保留 Frozen/RetrainedBump 等现有策略语义。
测量不能偷偷减少训练/定价路径、调整平滑宽或固定不同的拟合策略。

R11. 原生真实不同输出覆盖 1/4/16/64 输出，明确当前执行策略。
可先使用现有逐输出单结果执行作为参考；不能把一个 root 的多个通道宣称为真实组合执行。
F02 的 VJP/预算分块实现完成后再比较等价输出和结果范围；此处不提前宣称其吞吐或内存优势。

R12. 使用固定总路径的 1/2/4 及机器适用的 8 线程测试。
保留路径编号、RNG/seed 和参数/输出，记录随线程变化的 batch 分组及归约顺序。
比较价格和每个风险；契约容差与 bitwise 一致分别表达。
报告延迟、路径吞吐、speedup/efficiency、归约成本、tape 与进程资源。

R13. 诊断与无插桩执行使用相同请求与数值结果；诊断开销单独配对测量。
原生 lifetime OFF/ON 是不同成本配置，不能把其差异计为算法加速。
无插桩 Release 的当前完整请求与原始基线匹配配置进行既有两轮配对。

R14. 为原生配置、诊断作用域/线程/失败边界、实际阶段关系、非有限或缺失测量输出、
容量与完整 final block 等行为建立独立 RED/GREEN 测试。
不通过测时间阈值测试代码正确性，也不写一个与实现同构的镜像计时器测试。

R15. core/public/Python/portable Excel、例子、安装消费者、生成完整性和最新提交 CI 继续通过。
默认构建的 Number_/node/tape/scope 布局、诊断符号排除与无插桩的原始门禁均有新证据。
诊断 Linux/MSVC 的编译运行覆盖必须实际执行；不以默认 OFF 的编译证明 ON 可用。

R16. 性能纯测量在正确性通过后冻结源、配置、剩余五项依赖、二进制及测量工具。
保持正式九项允许集、两轮各十次、4% 阈值和 Sobol 比率政策。
新增生产/诊断/scaling 数据为独立补充，保存全部样本、失败与复核。
若默认路径有稳定回退，先定位修正再重新验收，不能用诊断成本解释默认退化。

R17. 记录源 SHA、库/探针或 benchmark 的真实构建身份、编译器/选项、硬件与 noisy 状态。
明确首次/重复、准备是否排除、采样顺序、线程 affinity、归约及异常条件。
报告包含每个案例的实际数字和证据路径，不能只留下命令或未来执行计划。

## 验收

1. 默认 OFF 无新的 per-path/per-node 仪表成本与布局变化；明确开关及消费者定义一致。
2. 至少一项录制生命周期、任务异常、跨线程或容量边界先失败后修正；独立数值 oracle 全部通过。
3. 普通与 LSM 的生产阶段数据有明确窗口关系、实际路径/参数/输出元数据，以及真实 tape 高水位。
4. 默认、插桩和适用的 lifecycle-diagnostic 配置区分；同请求数值匹配，诊断开销有独立数据。
5. 固定总路径的线程扩展完成，并提供每个线程数的价格/风险、延迟、吞吐、资源、归约和效率。
6. 原有九项目、新增受影响生产场景与短请求均通过既有不回归政策，所有失败与限制保留。
7. 完整功能/安装/绑定/生成及最新适用 CI 成功，说明与实际实现一致。
8. 完整需求审计后才关闭 P01；Stage A 关闭不表示后续市场／组合／算子／高阶风险已经交付。

## 下一步

先核对实际普通/LSM 调用边界与既有 SimulationObserver 的用途，再以失败测试约束默认擦除、
观测作用域及有效 tape 采样。优先完成可解释的普通/LSM 阶段模式与线程扩展，
然后补长路径/大表面/真实输出的代表性覆盖；代码、功能与测量分阶段验收。
在完整证据到齐前 P01 保持未完成。

当前 TDD 证据：`tests/aad-profiling/run.cmake` 的 enabled 场景先因包中没有
`DAL_CPP_AAD_PROFILING` 字段而失败，日志为
`/tmp/dal-aad-evidence/production-profiling-config-red.log`。
加入默认 OFF、PUBLIC 定义及包字段后，默认/开启/与 lifetime 同时开启三个配置通过，
保留 `production-profiling-config-green.log`。这三项仅验收配置边界。
`dal-cpp/tests/math/aad/test_profiling.cpp` 的核心编译先因缺少 `profiling.hpp` 失败；
`production-profiling-core-red.log` 保留该证据。现已加入 `ProfilingScope_`、
`ProfilingSpan_`、任务各自拥有的 collector、线程 CPU 可用性、显式 tape 扫描，
以及成功创建 storage block 后才触发的分配计数。rewind 重用不计为新分配；
`allocatedArrayBytes_` 只计算 block 数组，不表示 allocator/list 元数据或全进程分配量。
未执行的任务曾错误地让父请求显示 complete，
`production-profiling-task-boundary-red.log` 保留失败；父请求现在同时检查任务完整性。

普通 AAD 树/编译调用的 64 路径计数、数学 delta 与完整风险容差比较通过，
连同核心边界共 12 项，日志为 `production-profiling-mc-on-green-02.log`。
LSM 树/编译的训练、回归、回放和 suffix/prefix reverse 调用已接入；
64 定价/128 训练路径的 Frozen 价格与每个风险保持逐位一致，
`production-profiling-lsm-red.log`／`production-profiling-lsm-on-green.log` 保留 RED/GREEN。
新增 passive 逐路径和实际任务异常 drain 测试也通过全新完整构建。
完整 ON 构建目录为 `/tmp/dal-aad-evidence/production-profiling-on-build`。

普通 MC 的新测试一度误要求两次并行风险逐位相同。
未修改的 `6161e5a` simulation 头与原已冻结 archive 的 20 次重复请求中，
38 个风险单元也出现舍入差，最大绝对值 `3.5527136788005009e-15`；
证据为 `profiling-parallel-repeat.log`。因此按 R12 的 rel/abs `1e-10`
检查普通并行结果，保留实际归约行为；LSM 原有 bitwise 契约仍严格检查。

Scope inclusive CPU 是该线程在观测区间内的实际 CPU；`ActiveWait` 同线程执行子任务时，
请求或父任务的区间会包含子任务区间。新增 self CPU 扣除同请求、同线程的直接子任务
inclusive CPU；不同线程的任务不扣除，独立嵌套请求也不扣除。同请求任务树的 self CPU
可用于累计已观测区间的 CPU，不包含区间之外的线程池工作。缺失任何必要 CPU 样本时
self CPU 明确不可用。跨层 inclusive CPU 不能直接相加。
阶段 wall 的 self 仅排除同一个 collector 内的子 span，不能与不同 worker 的 wall 相加为延迟。
高层 `PrepareScript` 入口已加入 PREPARE；CLI 的冷请求另外覆盖模型/产品构造，
并保留窗口包含关系，不将其与 driver PREPARE 重复相加。

CI 新配置覆盖 profiling OFF/ON 与 lifetime OFF/ON 的完整组合，
并保留原有 sanitizer 路径，增加 profiling/combined ASan+UBSan 和 profiling TSan。
172 个 helper 测试通过（6 项既有 skip）；这些新 CI leg 尚未发布执行。
现有发布头 `6161e5a` 的 28 项检查全部成功。P01 的 CLI 已实现并通过下述功能验证；
线程扩展、实际资源数据、诊断开销、无插桩性能与新增 CI 验收仍待完成。

最新未发布增量增加 setup-prefix tape 采样、所选路径/工作区/结果/回归数组的
live/capacity payload、实际线程 self CPU，以及内存采样的延迟回调。
开启 profiling 构建但没有显式 scope 时，回调不执行；关闭构建开关时连活动检查也移除。
核心测试先因缺少 CPU、内存和回调接口失败；prefix 测试先因缺少 setup 样本失败。
对应 RED 日志为 `production-profiling-{cpu,memory,prefix,lazy-memory}-red.log`。
最新完整 ON 构建的 20 项专项测试通过，日志为
`production-profiling-lazy-full-on-focused.log`；此前 direct core 的 16 项通过
仅为局部证据，不代替该完整库运行。

`script_mc_perf --production-profile` 保留原默认模式，新增 cold/warm/phases。
五个实际场景为 short、long、local-vol、lsmc-bs、lsmc-local-vol；长普通路径有
366 个观察事件，表面 grid 为 2/4/8/16 时实际参数为 7/19/67/259。
1/4/16/64 个不同 strike 输出使用共同路径、逐输出单结果执行，AAD 通道宽为 1。
短 BS 的固定路径价格与四个风险使用独立解析 oracle；长 BS/local-vol 使用两档
common-path 有限差分，LSM 使用树/编译固定路径比较。每次完整计时结果再次逐项
对比未计时的完整路径参考。64 输出的首轮差分穿过 MAX kink 失败记录保留，
改用解析 oracle，没有放宽容差；解析 RNG 初始位置不一致的失败也保留，
oracle 现显式 `SkipNormalTo(0)`。

最新 CLI 契约检查与全部 38 个代表性功能场景通过，日志为
`production-profile-lazy-cli-contract-on-02.log` 和
`production-profile-representative-functional-06.log`。这些运行时机器同时构建，
其中时间数值只作功能 smoke，不能作为性能验收。
最新 ON 全部 2,351 项非 benchmark 功能测试通过，其中 Python 793 项通过。
完整 CTest 同时运行额外 21 个 benchmark 时，`rate_risk_perf` 的既有
joint N=5/1000-trade 耗时比例阈值失败；总命令返回 8，不能记为全部成功。
保留 `production-profiling-lazy-full-on-ctest.log`，等待构建结束后串行复核，
并按 R16 做独立正式配对。最新 OFF 完整 2,332 项功能测试全部通过，
其中 Python 793 项通过；ON/OFF/combined 的两个安装消费者均通过。
combined 完整构建、20 项专项测试和全部 2,380 项功能测试成功，
其中 Python 793 项及慢速 `european_mc` 示例均通过。
保留 `production-profiling-lazy-full-{off,combined}-ctest.log`、
`production-profiling-lazy-{off,on,combined}-consumer.log` 和
`production-profiling-lazy-full-combined-focused.log`。

最新核心 profiler/test TU 以 ASan/UBSan 编译并使用仓库的
`RegisterAll_::Init()` 测试入口，16 项全部通过且 leak detection 开启。
保留 `production-profiling-focused-asan-02-build.log` 与
`production-profiling-focused-asan-02.log`。支撑 archive 为当前 ON Release，
未全量插桩，因此此结果只约束新增核心代码与内联边界；不能代替新 CI 的全库
profiling/combined ASan/UBSan 与 TSan。172 个 helper 测试再次成功（6 项既有 skip），
生成再次零 drift，79 个 Markdown 文件检查成功。

最新类型探针验证 Number/node/tape/recording 均为 `16/40/368/72` bytes；
任务组为 OFF `48`、ON `64` bytes，ON 新增请求身份指针用于 CPU 去重。
保留 `production-profiling-types-{off,on}-02.log`。最新 OFF archive 没有
allocation/sampling/span/task/collector/CPU profiling 符号或引用，日志为
`production-profiling-default-off-symbols-02.txt`。另外 `-O0 -g` 默认探针
没有诊断、enum 验证或回调调用；其未解析引用只有标准库初始化和编译器栈保护。
保留 `production-profiling-default-debug-probe-{build,symbols,disassembly}.log`。
这些证据不等同于整函数在 Debug 没有指令，也不替代正式吞吐验收。

构建与功能测试全部结束后，使用 `DAL_NUM_THREADS=4` 和 CPU affinity
`4,6,8,10` 串行复核 ON 的 21 个既有 benchmark，全部成功，
保留 `production-profiling-lazy-quiet-on-benchmark-smoke.log`。
首次并行 CTest 的 timing assertion 失败仍保留；串行成功只验证 smoke，
不替代九项两轮配对性能门禁或默认吞吐验收。

以下为上一增量的完整功能证据，不能验收最新 CPU/数组/延迟回调变更：
默认 OFF 和 profiling ON 全新静态 Release 构建，
各自通过 2,332／2,346 个非 benchmark CTest 项目，各包含 793 个成功的 Python 测试。
ON 的计时 helper 整理和 LSM 严格 bit 比较之后，再次完整执行也成功，
保存 `production-profiling-full-on-ctest-02.log`，以及最终 15 项
`production-profiling-full-on-focused-02.log`。
OFF 日志为 `production-profiling-full-off-ctest.log`。
安装后各自两个消费者均通过，保留 `production-profiling-{off,on}-consumer.log`。
组合 lifetime/profiling 配置的全新完整构建也成功，全部 2,375 项 CTest 通过，
其中 Python 793 项通过；两个安装消费者通过。
保留 `production-profiling-combined-{ctest,consumer}.log`。

生成检查首次按既有规则拒绝三个新但未跟踪的 enum 输出，
`production-profiling-generation-check.log` 保存原拒绝；将这三个输出与 markup 源一同暂存后，
`production-profiling-generation-check-green.log` 验证再生没有变更，没有绕过检查器。
源码／测试输入 1,179 个文件的第二份 hash manifest 在最终构建、测试和再生后保持一致。

默认 OFF Release archive 的 `nm -C` 列表没有 block-allocation、tape-sampling、span、
CPU-clock 和 TLS collector 的诊断符号或引用；记录为 `production-profiling-off-symbols.txt`。
OFF／ON 的实际 Number_/node/tape/recording 大小均为 `16/40/368/72` bytes。
SimulationTaskGroup_ 为 OFF `48`、ON `56` bytes；只有诊断构建增加任务采集成员。
类型输出保留为 `production-profiling-types-{off,on}.log`。
这些是 Release 构建和布局证据，不能替代默认吞吐配对、诊断开销或线程扩展测量。
