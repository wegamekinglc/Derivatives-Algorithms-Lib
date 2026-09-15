# DAL-203 F6：公共 C++ 设置、archive 与诊断合同

状态：DAL-233 API 核对完成，可进入 F6 实现；以下新增声明和行为尚未实现。
本 note 只控制正在进行的 F6，沿用已批准 DAL-196 spec/API/critique 和 DAL-203 当前固定合同。
面向 C++ 定价调用方、诊断/持久化消费者，以及后续 Python 研究用户和 Excel 工作簿作者。
F7/F8 本轮仅作投影兼容核对，没有实现语言接口。

## 1. 基线与证据边界

- 读源基线及本地 HEAD：`634ee939099ebee862d71b458afcca672d4ad199`。
- Git tree：`709b8cd28d000fb91b69432337cb3527ae67a9ab`；checkout 初始干净。
- 本地审阅分支：`agent/dal-api-designer/aba780b84874`，由 `multica repo checkout --ref <上述 SHA>` 取得；不是待发布实现分支。
- 后继实现分支：`feature/dal-203-public-settings`。由父验收后将本 note 纳入该分支；本轮不提交、不推送、不制造报告 PR。
- F5 PR #372 已合并的身份以父任务当前交接和上述 master 基线为准。仓库内 `reviews/dal-202/{implementation,documentation}.md` 的“PR open / 尚待独立审查”是历史交付时态，不能重新解释为 F5 依赖未满足。
- 原始 spec、API、critique 已经 authenticated CLI 下载并全文读取；其“待授权”由父任务持续授权覆盖。原设计中的类型/函数草图不等于当前源码。
- 本轮唯一仓库改动为本文件；workdir 的声明原型、编译日志、输入哈希及 manifest 另以附件交付。未运行产品 full tests、数值测试、Machinist、Python、Excel、sanitizer、alternate AAD backend 或性能测试。

## 2. 当前表面与必须补齐的差异

以下行号属于上述基线，后续实现可能移动。

| 源码 | 已存在的事实 | F6 决定 |
| --- | --- | --- |
| `dal-cpp/dal/script/settings.hpp:27` | 三个设置结构都在 `Dal::Script`；product 有 `defaultIndex_`，MC 有 rsg/useBb/enableAad/smooth/compiled。valuation 仅有 `todayFixingPolicy_`、`modelBindings_`。 | 扩展现有 valuation；不重定义类型，不改名为原草图的 `todayFixing_`。 |
| `dal-cpp/dal/model/base.hpp:19` | `ModelIndexBinding_` 在 `Dal`，成员 `assetName_`、`indexName_`。 | 复用；不在 `Dal::Script` 定义第二种 binding。 |
| `dal-cpp/dal/auto/MG_TodayFixingPolicy_enum.hpp` | `Dal::TodayFixingPolicy_` 是 Machinist class；值为 `Value_::MODEL` / `REQUIREHISTORICAL`；独立默认构造是 `_NOT_SET`。 | valuation 的字段初始化保持 MODEL；无新枚举及手写 enum class。 |
| `dal-cpp/dal/indice/fixingsnapshot.hpp:29` | `MarketFixingSnapshot_::values_t`，显式构造函数复制 map，内部 `const values_`。 | 复用真实构造；空 handle 与构造出的空对象必须区别。 |
| `dal-public/src/curveprotocol.hpp:86` | `MarketFixingSnapshotNew(values)` 确实存在，返回相同 core snapshot handle。 | 可复用但需 include 该头；最小 script 示例用 core 构造，无隐含 factory/include。 |
| `dal-cpp/dal/platform/platform.hpp:78` | `Handle_<T>` 继承 `std::shared_ptr<const T>`。 | `Handle_<MarketFixingSnapshot_>` 已是 const-pointee handle；不用二次 const 类型或裸指针。 |
| `dal-cpp/dal/script/event.hpp:232` | data 仅复制 dates/events；`Product()` 重新 parse 全事件，数据对象尚未保存 contract settings。 | data 拥有产品设置副本；每次定价仍 parse 新产品。 |
| `dal-cpp/dal/script/preparation.cpp:240` | builder 无条件 `CaptureScriptEvaluationDate()`；snapshot 和 contract 是独立尾参；模型分配/初始化后 resolve history，编译在 worker 前。 | 统一设置解析，D 显式贯穿；保留准备顺序及 native 旧尾参兼容。 |
| `dal-cpp/dal/script/simulation.hpp:554` | data overload 已支持 prepared double/AAD、MC 设置、snapshot、contract。 | 使用它及共同准备边界；不再增添位置参数。 |
| `dal-public/src/value.cpp:20` | public 仍 `Product()->PreProcess()->MCSimulation(raw)`，没有命名设置路径。 | 旧入口显式映射设置后转发新入口，二者都经准备。 |
| `dal-cpp/dal/script/event.cpp:226,297` | debug /1 拒绝 FIX；archive v1 reader/writer。 | 保留 /1 拒绝，补 default；新增 Describe /2、Explain /1、archive v2。 |
| `dal-cpp/dal/script/visitor/debugger.hpp:656` | FIX 的 label 保留原文，但 DebugNode 的结构字段只有 `kind=fix`，没有 index/date/source。 | /2 必须补结构字段，不能从人类 label 反解析。 |
| `dal-cpp/dal/script/preparation.cpp:59` | bound SPOT 临时伪造 NodeFix，仅有 eventDate 的 source；去重 use 没有每次 raw index。 | 诊断需要保留真实 SPOT source 和每次 FIX 原名；不得用首个规范名替代全部 uses。 |

已读相邻示例 `dal-cpp/examples/{script,script_tree}/*.cpp`；术语与 `docs/methodology/script_engine.md` 的观察日期、union timeline、hard history / fuzzy future、prepared lifetime 对齐，并核对 `docs/methodology/aad.md` 的 recording / mark / seed。

## 3. 拟新增声明与命名空间

### 3.1 Core 值类型

定义位置仍为 `dal-cpp/dal/script/settings.hpp`。直接 include `<dal/indice/fixingsnapshot.hpp>`，使消费者获得真实 snapshot 类型；其依赖全部在 core 内。

```cpp
namespace Dal::Script {
struct ScriptValuationSettings_ {
    TodayFixingPolicy_ todayFixingPolicy_ = TodayFixingPolicy_::Value_::MODEL;
    Vector_<ModelIndexBinding_> modelBindings_;
    std::optional<Date_> evaluationDate_;        // 新增；nullopt = 入口捕获一次
    Handle_<MarketFixingSnapshot_> fixings_;     // 新增；null = 本次 global capture
};
}
```

新字段追加在两个原字段之后，保留已有 `{policy, bindings}` aggregate 初始化。不要把 D 放到首字段、添加有副作用的构造器，或为了统一草图重命名旧字段。product/MC 结构沿用现有字段顺序和默认值。

`ScriptProductData_`（`Dal::Script`，`event.hpp`）保留三参构造，新增无默认第四参 `const ScriptProductSettings_&` 构造；data 保存一份设置。增加 const getter `Settings()`，并为 archive/Describe 提供原 dates/events 的 const 访问或等价内部访问。原三参委托空设置。product 创建只校验表长一致并复制，完整 script/index 语法在 parse/Describe/prepare 校验，零市场 I/O。空表可创建/查看，定价时报结构错误。

### 3.2 Public 入口

在 `dal-public/src/script.hpp` 的 `namespace Dal` 中保留 `using Script::ScriptProductData_`，新增 `using Script::ScriptProductSettings_`。在 `value.hpp` 中新增 `using Script::ScriptValuationSettings_` 和 `using Script::MonteCarloSettings_`。这些是现有 core 类型的 using-declaration，不是包装类型；两个 namespace 同时 using 也不产生两种类型。

```cpp
// script.hpp，namespace Dal
Handle_<ScriptProductData_> NewScriptProduct(
    const String_& name, const Vector_<Cell_>& dates,
    const Vector_<String_>& events, const ScriptProductSettings_& settings);

String_ DescribeScriptProduct(const Handle_<ScriptProductData_>& product);

// value.hpp，namespace Dal
std::map<String_, double> ValueByMonteCarlo(
    const Handle_<ScriptProductData_>& product,
    const Handle_<ModelData_>& modelData, int numPath,
    const ScriptValuationSettings_& valuation,
    const MonteCarloSettings_& simulation = MonteCarloSettings_());

String_ ExplainScriptValuation(
    const Handle_<ScriptProductData_>& product,
    const Handle_<ModelData_>& modelData,
    const ScriptValuationSettings_& valuation = ScriptValuationSettings_());
```

Explain 最后一个默认参数允许查看默认估值准备；不添加路径数、AAD 或 compiled 选项。其语义固定为默认价格准备，不是所有执行模式的审计器。Describe 的模型无关性也体现在签名里。实现可将较大函数体放 `.cpp`，不要求把新 JSON 生成内联到 public header。

旧声明逐项保留：

```cpp
NewScriptProduct(name, dates, events);
ValueByMonteCarlo(product, modelData, numPath,
                 rsg = "sobol", useBb = false, enableAad = false,
                 smooth = 0.01, compiled = std::nullopt);
```

| 调用形状 | 解析与含义 |
| --- | --- |
| NewScriptProduct 三参 | 仅旧入口可用；空产品设置。 |
| NewScriptProduct 第四参为明确 `ScriptProductSettings_` | 新入口；第四参没有默认值。 |
| Value 三参 | 仅旧入口可用；旧入口生成默认 valuation 与 MC，再转发。 |
| Value 第四至第八参依次为 String/bool/bool/double/optional-bool | 原 3–8 参解释；字符串字面量仍可转 String。 |
| Value 第四参为明确 `ScriptValuationSettings_` | 新入口；第五参缺省为 MC 默认值。 |
| Value 第四参为 `{}` | 不承诺；两重载均可能接受。要求写出具体设置类型。 |

复制时旧 `compiled=nullopt` 仍按 false；不能误把它解释为自动启用 compiled。AAD 仍沿现有 fuzzy 语义；结果 map 只有 `PV` 和 AAD 时的 `d_<model/script const parameter>`。PV 从 `aggregated_/numPath` 得均值；`risks_` 已归一，不能再除一次。无 fixing 风险键，也无诊断/计数数值键。

源码兼容不等于 ABI 兼容：新增 data/settings 成员改变布局，下游 C++/Python/Excel 要与同一新版库重建。不得移动已有类型或引入可隐式从 String 构造的 valuation 设置。

## 4. 默认、校验、错误字段矩阵

错误保持 DAL/Script exception 路径；不重建异常体系，不捕获后返回零。新增 setting 错误使用 `InvalidSetting`，已有 core 分类可保留并增补字段/约束。保留既有可断言标识（如 `InvalidSmoothing`、`InvalidTodayFixingPolicy`）作 cause/detail，不能仅因美化统一名字破坏 F1–F5 错误回归。测试断言标识、字段、值及约束，不锁整句英文。

| 输入/字段 | 默认、有效范围 | 校验位置与稳定诊断 |
| --- | --- | --- |
| product/modelData | 必须非 null；模型为 BS/Dupire | public 在解引用前失败：`InvalidSetting`，字段 `product`/`modelData`、`null` 与非空约束；不支持模型保留模型错误并列 Type/支持列表。 |
| dates/events | 等长 | data 构造检查 `dates.size` 与 `events.size`；不在构造期定价。 |
| product.defaultIndex_ | 空表示未绑定；非空需 indice 完整解析 | Describe/prepare 检查；`InvalidIndex`/`UnknownIndex`，字段 `product.defaultIndex_`、原值和语法约束。不是未来模型绑定。 |
| valuation.evaluationDate_ | nullopt 捕获；有值必须 `IsValid()` | 在 parse、model setup、I/O 前；`InvalidSetting`、`valuation.evaluationDate_`、无效 Date 的可安全显示值与有效日期约束。无效 Date 不先调用可能失败的日期格式化。 |
| valuation.todayFixingPolicy_ | MODEL；仅 MODEL/REQUIREHISTORICAL | `InvalidSetting` + `InvalidTodayFixingPolicy`、字段及 underlying enum 值/允许值；显式 `_NOT_SET` 不作为默认。需验证公开可写 `val_` 的无效值后才用 `.String()`。 |
| valuation.modelBindings_[i].assetName_ | 只允许 `spot`，DAL 比较规则 | 空/未知：`UnknownModelAsset`，字段/值/允许 spot；重复：`DuplicateModelBinding`，首个与重复位置。不能静默覆盖。 |
| valuation.modelBindings_[i].indexName_ | 非空，完整可解析 | `InvalidIndex`/`UnknownIndex`，数组位置、原值及约束；不从 model 名称猜测身份。 |
| 模型能力/身份 | 单一普通 EQ；无 FX/IR/composite/delivery/multi-asset | `UnsupportedModelObservation`；缺 binding 为 `MissingModelBinding`；不匹配保留 `ConflictingModelBinding` 并列 requested/bound 原名与规范名。model-aware capability/init 在历史 I/O 前。 |
| valuation.fixings_ | null 是 GlobalSnapshot；非 null 是 ExplicitSnapshot | 明确空对象不补全局；缺值 `MissingFixing`，列 source、canonical/original、午夜 fixing、事件和 source、精确历史要求且不回退。无历史请求时不读取/构造 global snapshot。 |
| snapshot 内容 | timestamp 有效；值 finite；普通 EQ 允许 0/负，FX 必须 >0；双向乘积误差 <=1e-10 | 复用真实构造/bridge/Index::Fixing；现有 `InvalidFixingSnapshot`/`InvalidFixing` 保留。外部先构造 snapshot 的错误仍由既有构造器报告，不虚称都发生在 Value。 |
| simulation.rsg_ | sobol；sobol/mrg32/irn，DAL 比较 | `InvalidSetting`，`simulation.rsg_`、原值和允许列表；保留旧 `rng method is not known` 作为兼容 detail。即使过期/零维仍校验。 |
| simulation.useBb_ | false | bool；零维不构造 RNG/BB。 |
| simulation.enableAad_ | false | bool；true 启用 future fuzzy、历史参数风险；不平滑历史决策。 |
| simulation.smooth_ | 0.01；finite 且 >0 | 不论 AAD 开关都验证；`InvalidSetting` + `InvalidSmoothing`，字段/原值/finite positive 约束。NaN、±Inf、0、负数均失败。 |
| simulation.compiled_ | nullopt -> false；false/tree、true/compiled | 只改执行方式；同一准备必须传同一 effective 选择。native mode mismatch 仍 `UnsupportedExecutionMode`。 |
| numPath | `int`，1..INT_MAX | 转 size_t 前 `InvalidPathCount`，字段 `numPath`、原值及正整数约束；所有 public 路径含过期均检查。 |
| dates / FIX 显式日期 | 有效事件日；FIX 严格 ISO YYYY-MM-DD；午夜精确键 | `InvalidFixingDate`、`LookAheadObservation` 等，列 F、E、raw/canonical 与位置；禁止 F>E，不把 11:00 补到 00:00。 |
| 空表/仅定义/无 PAYS | 可做纯语法 Describe，但不能定价 | 保留当前 `InvalidScriptStructure`，说明 `dates/events` 的 no dated events/no PAYS payoff；在 payoff 下标使用前失败。 |
| 旧/混合 SPOT | 纯未来旧模式允许无绑定；历史需 default；FIX 混用需 default | 保留 `UnboundHistoricalSpot` / `MissingDefaultIndex`；具参 SPOT、FIX() 不成为别名。 |
| 旧 debug /1 接受范围 | 无 FIX 且 default 为空 | `DebugSchemaUnsupported`；包含目标 schema /1 及 `DescribeScriptProduct`、`dal.script-product/2` 迁移提示。 |

错误格式例：`InvalidSetting: ValueByMonteCarlo; simulation.smooth_=nan; expected finite and > 0; InvalidSmoothing`。
观测错误额外带 original index、canonical index、fixing timestamp、event date、original table row、expanded offset/line/column、statement/node ID。请求有多个使用位置时错误至少明确实际失败请求的一个原始 use；Explain 保留全部 uses。FX 反向底层源失败须定位到正确请求/日期，不能拿列表第一个请求冒充。

C++ 强类型没有“未知 setting key”，而 `int` 入口无法知道调用方此前把 1.5 截成了 1。F6 不谎称 runtime 能识别这个信息，也不新增抢占旧调用的 double 模板重载。Python 非整路径/未知关键字与 Excel 两列矩阵/serial 校验归 F7/F8；本轮核对并保留现有入口及 Excel `CheckedMonteCarloPathCount`，不得把后继未实现验证报成 F6 已通过。

### 日期与来源固定语义

设 D=估值日、E=事件日、F=fixing 日：F<D 历史；F=D 默认 Model 或显式 RequireHistorical；F>D 模型；F>E 总是错误。历史事件硬回放，过去 PAYS 不重复支付；未来事件继续按所选 exact/fuzzy。未来普通 EQ 需显式 `spot -> index`；历史多 EQ/FX 不需模型 binding。历史未来来源不能按库中是否有值决定。

全过期先做结构、名称、前视、普通设置及合法 model handle/type 检查，然后 PV 与已有风险标签为零；History/最终 Fixing/Allocate/Init/GeneratePath/worker 均零。无需未使用的模型输出能力；不补读已结算历史。历史未绑定 SPOT 的结构/身份拒绝保持。未过期即使所有 FIX 已知也保留付款 numeraire 模型准备。

## 5. 复制、D 的真实传递和兼容尾参

### 5.1 统一 public 准备边界

1. 旧 public Value 只映射 rsg/useBb/enableAad/smooth/compiled 并调用设置重载，不能再跑 `PreProcess` 或独立捕获日期。
2. 设置重载持有一次 `XGLOBAL::ValuationMutationGuard_`；复制 handles、valuation、simulation。校验输入后，在任何 parse/model construction 前条件读取 D：有 `evaluationDate_` 就取它，否则调用一次 `CaptureScriptEvaluationDate()`。将 D 写入本次设置副本，不写调用方或 global。
3. 必须用条件分支实现缺省捕获。不要写 `evaluationDate_.value_or(CaptureScriptEvaluationDate())`，它会在显式 D 时也求值默认表达式。日期捕获不能藏进产品构造或重复执行的 raw `PreProcess`。
4. `PrepareScript` 的直接 core 调用也采用同一“已给 D 则复用，未给才捕获”的解析规则。builder 用解析出的 D 做 Collector 分类、PartitionEvents、ModelPlan/timeline、PreparedScript.EvaluationDate；后续模拟不再取 global。
5. 在 public 创建模型之前固定 D；`MCSimulation(data,...)` 及 Explain 传递同一已解析设置和产品设置。core 值类型/解析 helper 不得依赖 dal-public。可将解析分工放 settings/preparation；不增加公开 prepared handle。
6. 执行顺序保持：parse/收集全部语法、结构/名称/前视/设置验证 → 分区/index → live 模型 binding/capability/Allocate/Init → 历史去重抓取及虚函数 resolve → hard 历史 state/依赖元数据 → IF/constant 元数据和可选编译 → 提交 worker。

prepared 分支保留 F5 已验收 exact/fuzzy 的语法分支与 kernel；不能顺手把旧 domain/condition folding 重新用于模型准备而丢失 eager/fuzzy/参数风险。迁移旧 public 路径必须用已有原始 legacy fixtures 和独立 oracle 核验，不能只比较两个都已迁移的 public overload。

### 5.2 现有 native 尾参不制造两套输入

现有 `PrepareScript(data,settings,snapshot)`、`PrepareScript(data,model,settings,simulation,snapshot,contract)` 及 `MCSimulation(data,modelData,n,settings,simulation,snapshot,contract)` 保持原调用形状。新增字段不要求再添尾参，也不抢占 `PrepareScript(data,{},{})`（已有回归）。

统一归一化规则：

- 显式 legacy snapshot 非 null 而 settings.fixings_ 为 null：复制到本次设置。legacy snapshot 为 null：使用 settings.fixings_，包括显式空对象。
- 两个 snapshot 都非 null：同一对象可接受；指向不同对象报 `InvalidSetting`，字段为 `valuation.fixings_` 与 `snapshot`，约束“one explicit snapshot”。不比较内容以猜测市场等价性，不静默覆盖。
- 空 legacy contract.defaultIndex_ 表示没有旧 override，使用 data.Settings()；非空且 data default 为空时保留旧 native override 能力。两者非空须解析为同一 canonical identity（DAL 比较），否则 `InvalidSetting`，列两个 default 字段及约束。相同 identity 时 data 的原拼写仍是持久合约身份。
- 这些尾参仅为原 core 调用兼容；public/未来语言 API 只呈现一个产品设置和一个 valuation snapshot 来源。归一化发生在任何市场读取之前。

### 5.3 生命周期

product data 深复制 dates/events/settings 的值；valuation 中 bindings 向量、MC 字段进入入口副本。snapshot 构造复制 caller 的 map；复制 handle 共享不可变数据，无逐路径复制。调用期间禁止并发修改被传入的原始设置对象；入口复制并不使无同步读写合法。

显式 snapshot handle 至少保活到准备完成；`PreparedScript_::Settings()` 保留的设置副本可以继续保活 snapshot，但 worker 的观测读取只用 plan 的 sealed doubles/整数 slot，不需要 snapshot/environment。global 抓取临时 snapshot/environment 在解析结束后可释放；prepare 不把它们写回 product。默认来源逐序列捕获，抓取期不得并发写 fixing，不承诺跨序列原子市场时点。

每次 Value/Explain 创建新 plan；prepared/context 保活到所有 accepted tasks 被收齐，包括异常路径。历史 double 共享，参数相关历史 AAD seed 仍在每 worker 的每 recording 本地重建，不能将准备期 double seed 当风险状态、跨 tape 复用 active Number，或把历史 fixing 变成新风险输入。

## 6. 典型 C++ 调用（目标声明，未链接实现）

下例的 includes、类型、真实 snapshot 构造及新旧 overload 解析已经声明原型 `-fsyntax-only` 检查；定价/诊断的新增函数本轮没有定义、链接或运行。

```cpp
#include <dal-public/src/script.hpp>
#include <dal-public/src/value.hpp>
#include <dal/model/blackscholes.hpp>
#include <dal/indice/fixingsnapshot.hpp>
using namespace Dal;

// 应用按现有约定初始化 DAL registry/global data 后执行。
ScriptProductSettings_ contract;
contract.defaultIndex_ = "EQ[DAL196_TEST]";
auto product = NewScriptProduct("historical-observation",
    {Cell_("SCALE"), Cell_(Date_(2026, 9, 11)), Cell_(Date_(2026, 9, 22))},
    {"2.0", "x = SCALE * FIX(EQ[DAL196_TEST])", "pay PAYS x"}, contract);

ScriptValuationSettings_ valuation;
valuation.evaluationDate_ = Date_(2026, 9, 12);
MarketFixingSnapshot_::values_t history{
    {"EQ[DAL196_TEST]", {{DateTime_(Date_(2026, 9, 11), 0.0), 80.0}}}};
valuation.fixings_ = Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_(history));

Handle_<ModelData_> model(new BSModelData_("fixture", 100.0, 0.0, 0.0, 0.0));
MonteCarloSettings_ simulation;
simulation.enableAad_ = true;
auto result = ValueByMonteCarlo(product, model, 4096, valuation, simulation);
auto description = DescribeScriptProduct(product);
auto explanation = ExplainScriptValuation(product, model, valuation);
```

模型构造顺序实际为 name/spot/vol/rate/div。零利率示例独立预期 PV=160、d_SCALE=80，模型 spot/vol 风险为 0；Explain 是 default double/tree，不能宣称它解释了 AAD branch 优化。此 FIX 自带身份，default 仅展示旧 SPOT 迁移；没有未来命名请求所以无需 binding，未来付款仍需模型 numeraire。

若 P 的脚本增加 `FIX(EQ[DAL196_TEST], 2026-09-15)`，另赋 `valuation.modelBindings_ = {{"spot", "EQ[DAL196_TEST]"}}`，不会改模型参数或产品 default。显式空市场用 `Handle_<MarketFixingSnapshot_>(new MarketFixingSnapshot_())`；默认抓全局用空 handle。已有 `MarketFixingSnapshotNew(history)` 也是合法选择，但须 include `dal-public/src/curveprotocol.hpp`。

## 7. Archive v1 reader / v2 writer 与再生

archive type 与 debug schema 独立：archive `ScriptProductData_v1` 不等于 `dal.script-product/1`。

| 格式 | 读写合同 |
| --- | --- |
| v1 | 保留现有 `name`（optional）、`dates`、`events` reader；读成 defaultIndex_ 空的产品。 |
| v2 | 相同字段，新增 `default_index` optional string；缺失/空均表示未绑定。默认 writer 输出 v2，空字段可按 `SetOptional` 省略。 |
| v1 读后写 | 输出 v2；不保持旧写出版本。 |
| 新原文/身份 | 保留 name、原事件 Cell、未展开事件文本、FIX 无引号原字面量、default 的原拼写。不能存 canonical 改写文本代替原文。 |
| 旧消费者 | 不保证旧二进制读 v2，也不因 v1 能装 FIX 文本就宣称旧 parser 可执行 FIX。 |

禁止序列化本次 D、fixing 值/来源、modelBindings、history slots、sample IDs、timeline/plan、prepared AST、bytecode、AAD seed。Describe 和 Explain 文本不是产品 archive 的替代品。

具体实现落点：

1. `event.hpp` 的 storable Machinist 描述变为 `version 2`，增加 `default_index is ?string`；采用 `manual` reader Build 映射设置值，避免给产品构造器加一个孤立 string overload。
2. 在 `event.cpp` 包含 v1 Read 与 v2 Read/Write；v2 的 handwritten `Reader_::Build()` 构造 `ScriptProductSettings_` 再构造 data。v1 Reader 保留原三参 Build，自动得到空 default。
3. `Write` 只调用 v2 XWrite。保留原 v1 Read generated 文件作为冻结的兼容来源，原 v1 Write 可保留但不再 include/use。没有公开 v1 export，也不为了测试新增会静默丢身份的 v1 writer。
4. 这一“冻结旧 reader + 当前 markup 再生新 reader/writer + handwritten Build”与 `dal/curve/yclogdf.cpp`、`MG_DiscountLogDF_v1_Read.inc` / `_v2_Read.inc` 相邻先例一致。不假设 Machinist 能在同名双版本 markup 下自动注册两个类；不手改 MG 内容。
5. `dal-cpp/config/dal.ifc` 将 storable version 映射到 `auto/MG_*_vN_{Read,Write}.inc`。保留 enum 原 markup 与 `MG_TodayFixingPolicy_enum.*`；无语义改动不应产生 enum diff。
6. 后继在配置好、submodules 齐全的树执行 `cmake --build build/Release-linux --target dal_generate`，提交/验收预期 core 生成物，再用 `dal_check_generated` 确认 committed output 不漂移。该 target 同时运行 Excel generator；F6 不增加 Excel markup，Excel 产物应零 diff，出现 drift 先解释来源，不夹带 F8。

v1 reader 是有意冻结的旧 schema 产物，不把它遗漏在新版本代码里，也不将“生成器未重新生成旧文件”冒称旧兼容验证通过。v1 golden 测试需硬编码/fixture 原始 `~type=ScriptProductData_v1` 数据，以 `JSON::ReadString` / `JSON::WriteString` 走真实 registry。v2 roundtrip 不能成为 v1 reader 的唯一证据。未来若另提供 v1 export，对有 FIX 或非空 default 必须 `ArchiveVersionUnsupported`；本版不新增该入口，故没有虚构的导出成功路径。

## 8. DescribeScriptProduct：dal.script-product/2

### 8.1 行为

纯合约 Describe 直接从 data 的原表与 `Product()` 未 partition 的全部 dated events 生成；可以 IndexVariables，但不调用 legacy `ProductForDump`、CaptureScriptEvaluationDate、PreProcess、PrepareScript、CreateModel 或历史读取。无 domain/branch folding，保留 dead branches、过去/当天/未来的所有合约事件，**不输出 phase、evaluation_date 或 source kind**。

检查脚本文法、index 完整解析与 F<=E 这类不依赖 D 的合约约束。不检查历史齐备、当天来源、模型能力/binding；描述成功不意味着可定价。空表/仅定义/assignment-only 可返回其语法描述，payoff_index 不存在时为 null；Value/Explain 仍按定价结构约束报错，防止 IndexVariables 的空容器下溢泄露成假 payoff。

### 8.2 最小稳定 JSON 结构

采用 JSON 标准字符串转义和 finite round-trip number；dates 为 ISO 日，fixing_time 为精确 `YYYY-MM-DD 00:00:00`。数组顺序确定；不承诺 whitespace/object key 顺序。以下为字段合同，示例中的省略号仅用于文档。

| 字段 | 数据/来源 |
| --- | --- |
| `schema` | 固定 `dal.script-product/2`。 |
| `name` | data.Name() 原名。 |
| `default_index` | `{original: string, canonical: string或null}`；空 default 的 original=""、canonical=null；解析身份来自 Index::Parse/Name。 |
| `input_rows` | 原表顺序 `{row, date_or_definition, text}`；date_or_definition 使用已有 Cell JSON 值表示，text 为未展开原脚本，row 一起用于源码追溯。 |
| `variables`, `constants`, `payoff_index` | 现有变量/常量表；无可选 payoff 用 null，不能 size_t(-1)。仅语法索引，不用市场数值初始化常量表。 |
| `events` | 全部展开/同日合并事件，按现有事件顺序；每项 `{event_id,date,origins,statements}`，无 phase。origins 是预处理来源行/展开偏移。 |
| `statements` | 保留现有 JSON AST children、target/value、condition/then/else 结构。节点 id 为 `n0,n1,...`，按全事件原 AST preorder；禁止先优化再编号。 |
| FIX 叶 | 保留 `kind:"fix"` 的现有 snake_case 惯例；新增语义类型 `type:"Fix"` 和下列独立结构字段，不以 SPOT 代替。 |
| SPOT 叶 | `kind:"spot",type:"Spot"`；绑定时列 default 的 raw/canonical；未绑定时两者 null。不得由邻近 FIX/model 猜名。 |
| 观测叶字段 | `index_original`, `index_canonical`, `fixing_date_mode`（Explicit/EventDate）, `fixing_date_literal`（省略参数则 null）, `fixing_date`（F）, `fixing_time`, `source`。 |
| `source` | `{row,offset,line,column,event_date}`；row 是原表一基行号，line/column 一基、offset 零基，属于展开后文本，不能误称全是原文偏移。 |

`NodeFix_::literal_/index_/fixingDate_/source_` 已可供应 FIX 字段。补充 `DebugNode_` 或等价结构视图；raw/date/position 必须来自 AST 元数据，不从 label/JSON 字符串回解析。SPOT 当前缺 source，需最小的 parser/NodeSpot 位置传播；事件 origins 取 `PreprocessedEvents_::sources_` 并保留到 syntax 描述。这个改动是本阶段诊断所需的元数据补齐，不扩展脚本文法。

原表中的 macro/schedule 与展开事件的身份均可追溯。FIX 显式日期缺省时 AST optional 保持空，Describe 的 resolved fixing_date 来自事件 E；不能为了显示写回 AST optional，丢掉“省略日期”身份。

### 8.3 旧 /1 拒绝与文本输出

旧 `DebugScriptProductJson` 保持纯旧合约 /1 表面，包括日期捕获一次及 phase、空表 JSON。任意 FIX（包括历史事件/dead branch/宏展开产生）或任意非空 default（即使未用到）在输出任何 /1 JSON 前 `DebugSchemaUnsupported`，明确提示 Describe /2。检查数据设置及实际解析节点，不能只搜索原文子串。

原 text/tree 可继续表示 FIX，补 default/date/source 文字不要求逐字兼容；旧未来 SPOT 文本/JSON 回归继续保留。Python `Product_DebugJson` 当前转调这一旧入口，因此 F6 重建后自然沿用拒绝语义；这不是 F7 的新 Describe 绑定已经实现。

## 9. ExplainScriptValuation：dal.script-valuation/1

### 9.1 行为和边界

Explain 与 Value 共享 public 输入/设置/D 解析、模型工厂及 model-aware PrepareScript；传入默认 `MonteCarloSettings_`（double exact、tree、smooth .01、sobol、BB/AAD false）。允许为 live 产品 CreateModel/Allocate/Init 和读取历史、执行准备期 hard replay；不调用任何 MC driver、不生成路径、不构造 worker 任务、不建 active tape，不接受/虚构 numPath。

每次独立准备，无跨调用缓存。若 global H 从 80 改 90，先 Explain 后 Value 可以不同；要比较同一次市场值，用相同显式 D 和同一显式 snapshot，并保持 product/model 输入不变。相同 snapshot 只固定历史，不承诺冻结模型对象的并发外部修改。Explain 的 plan 地址可以与随后默认 Value 的寻址对照，但不作为 AAD 分支优化/所有 mode 的 dump。

### 9.2 最小稳定 JSON 结构与真实来源

| 字段 | 数据/来源 |
| --- | --- |
| `schema` | 固定 `dal.script-valuation/1`，与两个产品 schema/两个 archive 版本独立。 |
| `evaluation_date`, `today_fixing` | PreparedScript.EvaluationDate 与 validated Settings；政策展示 Model / RequireHistorical（从两个 enum 明确映射，不依赖 `.String()` 大小写）。 |
| `source_kind` | 归一化入口选择 GlobalSnapshot / ExplicitSnapshot，即使无读取仍说明所选来源；不是每个请求的历史/模型判定。 |
| `simulation` | 固定有效默认字段 rsg/use_bb/enable_aad/smooth/compiled，compiled 展示 false；说明此 dump 的准备语义。 |
| `all_expired` | PreparedScript.AllExpired。过期请求可保留语法记录但标 `resolution:"SkippedExpired"`；值/槽为 null。 |
| `model_bindings` | validated 设置中每项的 asset、原 index、canonical index；不从 model 名称推断。 |
| `requests` | Plan.Requests 原向量顺序；`request_id`=下标；每项 canonical index、精确 fixing_time、source=Historical/Model、resolution、全部 uses。live 成功时 resolution=Resolved。 |
| `uses` | 保留全事件 collection 阶段的 `{event_id,statement_id,node_id,source,index_original,fixing_date_mode,fixing_date_literal,observation_type}`。node_id 对应 /2 的 n 编号，statement_id 零基。 |
| 历史请求字段 | `history_value_id`=request.historyValueId_；`value`=KnownValues[id]；model_slot=null。不得用 snapshot.Find 代替最终 index 虚函数值。 |
| 模型请求字段 | `model_slot:{sample_id,output_id}`=request.modelSlot_；history_value_id/value=null，不能伪造一个模型当前 spot 充当未来 fixing。 |
| `sample_dates`, `timeline` | Plan.SampleDates/TimeLine；time=(date-D)/DAYS_PER_YEAR；无另一个重算出来的采样计划。 |
| `live_events`, `event_to_sample` | live_events 将全部事件 ID 对应到 partition 后 future_event_index；event_to_sample 原样取 Plan.EventToSample。不得将 use.event_id（分区前）当这个数组下标。 |
| `sample_definitions`, `numeraire_requests` | 来自 Plan.DefLine 的 indexNames/numeraire（及实际 forward/discount 请求如有）；按 sample_id 关联，支付事件用自己的 numeraire sample。不是已生成的 numeraire 数值。 |

`source_kind`、每 use 原名/显式日期模式、原 SPOT 位置与 all-event→future-event 关联目前不全在 plan 中，F6 应在准备收集时补齐被动诊断元数据。可以内部保存小型 metadata，不向 public 暴露新的 mutable plan。不要事后重 parse 一份可能编号不同的树去拼 Explain，也不要按 KnownValues 遍历位置重新编号 requests。

纯 legacy SPOT 可没有 named requests/slot，Explain 仍列真实 event_to_sample/numeraire 与 legacy 模式说明；不能伪造 EQ 名称。全过期 dump 保留语法 uses 和 source 分类，但 KnownValues/sample/numeraire 为空、slot/value 为 null，不读取历史补齐。JSON 中不能将“缺值失败”序列化为成功 null；live 历史缺失必须抛同一准备错误。

## 10. F7/F8 投影兼容核对（未实现）

| 现有来源 | 核对结论/后继合同 |
| --- | --- |
| `dal-python/src/dal/api.py:4` | wrapper `Product_New(events_dates,events)`，只给非 Cell 对象包 Cell；保留 events_dates 拼写。F7 添加 keyword-only settings 时不得双重包装。 |
| `dal-python/src/bindings/script.cpp` | 低层日期参数叫 dates；使用 lambda 调三参 C++，新 overload 不引起函数指针歧义。现有 DebugJson/Tree 名称保持。 |
| `dal-python/src/bindings/value.cpp` | product/modelData/num_path/method/use_bb/enable_aad/smooth/compiled 的 3–8 位置/关键字保持；lambda 明确传全部旧参，释放 GIL 前已有 native handles/String 拷贝。 |
| `dal-python/src/bindings/curve.cpp:521` | snapshot 既有只读 Find/Require；`MarketFixingSnapshot_New(values)` 实际存在。后继 settings 复用其 native const handle，不引入 worker Python callback。DateTime 构造 hour 仍显式传 0。 |
| `dal-excel/src/__script.cpp` | `Product_New(name,dates,events)` 保留；double event serial 的现有转换是兼容事实，不能顺手替换为新 valuation 日期策略。 |
| `dal-excel/src/__value.cpp`、`__value.hpp` | 旧注册 **七输入**，没有 compiled；已调用 CheckedMonteCarloPathCount，输出两列键值。新 C++ overload 不改变该 lambda 的解析。 |
| `dal-excel/src/__curveprotocol.cpp:181` | snapshot 由三列构造，检查等长/重复，serial 小数代表精确日内时间，不能 floor 成午夜；输出 StorableMarketFixingSnapshot 包装已有 core handle。 |

后继名保留原 API：Python `MonteCarlo_ValueWithSettings(..., *, valuation=None, simulation=None)` 与 keyword-only settings；`Product_Describe`、`ScriptValuation_Explain`。设置投影 today_fixing → todayFixingPolicy_，model_bindings → 现有 Dal::ModelIndexBinding_，method → rsg_。str 转换/未知关键字/非整路径在 F7 绑定边界处理，dict 本身不能证明输入前未重复。

Excel 后继保留 `ScriptProductSettings_New`、`Product_NewWithSettings`、`ScriptValuationSettings_New`、`MonteCarloSettings_New`、`MonteCarlo_ValueWithSettings` 以及 Describe/Explain。两列设置/绑定拒绝未知/重复/半空行/非两列，整数 evaluation_date serial，nullable valuation/simulation 与显式空 snapshot 分离。复用 storable 包装，只作为计算对象，不归档 runtime plan。F8 更新 source markup 和 XLL 生成注册，portable 与 Windows XLL 各自验收；F6 不改 dal-python/dal-excel。

## 11. 验收落点与独立预期

以下是交给 implementer/tester 的具体测试合同；“已有”表示读到的测试，不表示本轮执行。遵循仓库 `Test...` 名称前缀，spec 的 T 名是矩阵 ID，不要求把现有 suite 全部改名。public 定价证据主要放 `dal-public/tests/test_value.cpp`，产品/诊断放 `test_script.cpp`，archive 可新增 `test_script_archive.cpp`（public 测试 CMake glob 自动收集）。内部计数/原型模型只留 core/test support，不暴露 public callback。

共同 fixture：D=2026-09-12，H=09-11 00:00，F=09-15，P=09-22，EQ[DAL196_TEST]，H=80、model spot=100、SCALE=2。确定性 PV 容差 `1e-12*max(1,abs(expected))`；解析 AAD 风险 `1e-10`；固定路径 MC 比较均值，归一误差 `1e-8`。平滑 FD 同路径/epsilon/中心差分，hard 切换点不作为平滑导数。

| T | 已有真实落点 | F6 必须补的 public/诊断证据与预期 |
| --- | --- | --- |
| T07 TodayPolicy | `tests/script/test_preparation.cpp::TestTodayPolicyAndFutureOnly`；`test_observation_simulation.cpp::TestTodayPolicyAndRngForBothAdapters` | `ScriptApiTest.TestTodayPolicy`：全局 D 故意与 valuation D 不同，Model PV=100、History/最终 Fixing=0；RequireHistorical PV=80、最终 Fixing=1；缺当天值失败且 worker=0。explicit snapshot 模式全局 History=0，不能把最终 Fixing 与全局 I/O 混算。覆盖 BS/Dupire 与 tree/compiled、double/AAD。 |
| T15 Repricing | `test_preparation.cpp::TestRepricing`、`TestEvaluationDateDoesNotChangeDuringCapture`；`test_observation_simulation.cpp::TestAadRepricingRefreshesGlobalHistoryAndModelInputs` | public 同产品 H=80→90→显式旧 snapshot 80；旧 prepared plan 仍80。Describe 不受 global D/H 影响。Explain 与 Value 各自计数/准备，不能第二次被隐式缓存；有 SCALE 时独立 PV=160→180→160、d_SCALE=80→90→80。 |
| T25 ExpiredAndEmpty | `test_preparation.cpp::TestNoLookAheadAndExpiredStructure`、`TestExpiredSimulationAndSubmissionSeam`；`test_observation_simulation.cpp::TestExpiredConfigurationAndNoWork`、`TestInvalidStructureAndTodayRngSubmitNoWorkers` | `ScriptApiTest.TestExpiredAndEmpty`：有效过期 PV/标签风险0、History/Fixing/Allocate/Init/GeneratePath/worker0；null model、坏路径/settings 仍拒绝；空表/定义-only/无PAYS 报结构错误。native counting model 验证 public 不可注入的 Allocate/Generate 计数，public 另验真实 factory 外部行为。Describe 空产品为合法语法视图，不被误当 PV0。 |
| T27 Legacy | `test_observation_simulation.cpp::TestLegacyPrepared{DuplicateDates,SameDate,DistinctDates}`、`TestDefaultSpotSharesFixing`、`TestUnboundAndExpiredSpotGuards`、`TestNamedAndLegacyFixedPathParityAcrossAdapters`；原 `test_compile_parity*.cpp` fuzz；public `ValueTest.*` | 编译旧 product三参及 Value3–8参；旧未来-only脚本保留，含 compiled；raw旧实现/固定样本/解析 oracle 对照，新旧 overload parity 只作补充。历史未绑定SPOT不再30；FIX混用无default失败；有default的相同时间/身份一请求；SPOT(index)/FIX()拒绝。 |
| T28 Settings | public `test_value.cpp::TestRejectsNonPositivePathCounts/TestRejectsUnknownRngMethod`；core `test_observation_simulation.cpp::TestPreparationValidatesConfiguration/TestModelBindingsBeforeHistory/TestOriginalPreparationBraceDefaults` | `ScriptApiTest.TestSettings`：explicit D 不改 global、fallback 捕获一次（parse/I/O 接缝中改变 global 仍用入口D）、复制后的product settings不变、snapshot输入map改动不改snapshot；非null空snapshot面对已有global值仍MissingFixing；全部错误矩阵及 null handle；legacy尾参双源冲突；compiled默认保持。编译探针区分强类型与语言层非整路径约束。 |
| T29 IndexVersioning | 目前无 ScriptArchive suite；public `test_script.cpp::TestPublicDumpsPreserveRawBranchesAndFixRestrictionsWithoutHistory`；相邻 `tests/curve/test_zerorate_archive.cpp` | `ScriptArchiveTest.TestIndexVersioning` 真v1 golden、v2缺省/显式空default及FIX/delivery/原名/宏文本roundtrip；再次写v2、禁止所有runtime字段。`ScriptApiTest.TestDescribeContractOnly` 在history/model/worker抛错接缝下成功且计数0、global D改变结果相同；检查默认/显式日期、同日多行、schedule来源。`TestExplainPreparation` 核对每个真实request/history ID/model slot/uses及event_to_sample，worker/GeneratePath0；全局来源每底层序列最多一次、最终虚函数每唯一历史一次；显式snapshot全局0。旧 /1 对FIX或任意default拒绝并提示/2。 |
| T31 PreparationBarrier | `test_observation_simulation.cpp::TestFinalHistoryFailureAfterModelSetupSubmitsNoWorkers/TestModelSetupFailuresPrecedeHistoryAndWorkers/TestCompilationFailureBeforeWorkers/TestCompiledPathFailureDrainsEveryBatch`；`ScriptFixingPreparationTest.TestAadPreparationFailureAndPathDrain`；`test_simulation_batch.cpp` drain tests | public 旧/新 overload 分别故障注入最后一个历史、model setup、BeforeCompilation，worker=0；Explain 同历史/model错误、worker=0（默认不编译，不虚构 compiled 注入覆盖）。路径NaN/异常在所有 accepted tasks收齐后才传播，后续调用可恢复。 |

真实计数接缝：`Dal::Detail::ScopedFixingReadObserver_` 的 BeforeHistory/BeforeFixing，`Dal::Script::Detail::ScopedSimulationObserver_` 的 BeforeCompilation/AfterSubmission；Allocate/Init/GeneratePath 使用 core 测试中已有 counting model。保留正向计数对照，不能用从未会触发的空 mock 证明零 I/O。跨日期独立数值例 F=120、P spot=999，P用F且付款numeraire独立；历史单笔付款给 `80*exp(-r*10/DAYS_PER_YEAR)`，SCALE例给 PV=160折现、d_SCALE=80折现、d_rate=-T*PV，避免共享寻址错误被 parity 掩盖。

未来实现至少跑相应 core/public focused、旧签名 consumer、F1–F5 legacy parity/fuzz 与必要 build/sanitizer；完整专家链按父任务的 fresh 验证要求执行。父 F5 测试计数仅继承背景，不是本 note 的 F6 通过证据。性能报告仅 advisory，保留数值验证/原数据/阈值，不因性能报告重启旧实验或额外 gate。

## 12. 本轮实际检查、取舍与开放项

实际检查：

- 读本 issue/父 issue 全文与各自初始 roots-only comment scan（当时均空），父 children 为串行 S1 active、S2–S5 backlog。交付前补扫父评论并展开 `01a0a2dc-110f-71b3-a062-e2e48240fd12`；派发记录确认相同基线/角色链，无新增语义约束。
- 完整读取三份批准附件；对上述 core/public/binding/generated/source/example/methodology 做静态核对。输入路径与 SHA256 见 evidence manifest；仅报告实际读源与原型，不转记 F5 测试为本轮实跑。
- GCC 15.2.0，C++17 `-fsyntax-only`：基线真实头的旧 product/Value 调用全部通过；prototype overlay 中追加设置字段与声明-only新重载后旧调用、typed设置示例、const snapshot类型、既有factory及原aggregate初始化通过。
- `{}` 第四参 negative prototype 预期 exit1，诊断明确列出旧 String 重载与新 valuation 重载；正向两次exit0。没有链接新增符号或运行定价，不是可执行功能证明。
- 工具：multica 0.4.43（2ae2dbbb8）、Git 2.53.0、GCC 15.2.0、CMake 4.2.3。准确命令、exit code、原型和 fresh logs 随证据附件。

拒绝的替代方案：重命名今天政策字段/把core类型搬public；继续给旧Value添位置参数；从模型名或唯一FIX推断default；把显式空snapshot当null；临时SetEvaluationDate再恢复；Describe调用Prepare以省一个collector；从debug label拆回身份；Explain缓存下一次Value；以v1文本容器可装FIX为由静默降版；改F7/F8目录提前补语言功能。

**结论：无需要实质 spec/critic 修订的阻断，可进入 F6 implementer。** 本 note 将草图与当前类型对齐，并明确必要的 D/设置归一化与诊断元数据补齐。原设计未来单EQ、日频午夜、hard历史/fuzzy未来及严格历史屏障均保持。

开放事项均为后继验证工作，不是待用户决策：新符号的真实编译/链接/consumer；v1 golden与v2 registry及Machinist再生；Describe/Explain完整schema和计数；public迁移后独立oracle/legacy parity；测试、文档/CHANGELOG与 mandatory reviewer。Windows XLL、Python新入口和跨语言新设置验证仍归F7/F8。DAL-233只交付API阶段，不能将本结论写成整个F6已实现或可合并。
