# DAL-204 F7：Python FIX 设置与诊断 API 合同

状态：DAL-238 API 核对交付，供父任务验收后进入实现。本文新增 Python 表面尚未实现。
面向 Python 研究/定价用户、底层 binding 调用方、实现者及独立测试者；保持共同 C++ 合同，Excel 仅核对投影兼容。

## 1. 权威输入、版本与交付边界

- 当前合同为 DAL-204 description（2026-09-15 F7 启动交接）和 DAL-238；原 DAL-196 三份批准附件的历史待授权措辞不控制本轮。
- `multica repo checkout git@github.com:wegamekinglc/Derivatives-Algorithms-Lib.git --ref master` 得到独立分支 `agent/dal-api-designer/d8d6a74a0c42`，初始工作树干净。
- 读源 HEAD：`a98bf9b07e9bd0fee23faa4cc9edca737f709654`；tree：`4bc1d06508fef4d918f587ce189b4c7899731176`。必需 F6 squash 的祖先检查 exit 0，当前 HEAD 就是它。
- 完整读取 `.codex/artifacts/api-notes/dal-203-script-public-settings.md`、批准 spec/API/critique；F6 note 的“尚未实现”是其交付时态，当前 core/public 已有实现，Python 新投影尚无。
- 唯一仓库写入是本文。无产品/测试/公开文档修改、无提交/推送/报告 PR。父验收后由 implementer 将准确附件纳入 `feature/dal-204-python-fix-settings`。
- 附件 evidence 提供输入 SHA256/git blob 身份、编译/签名原型、命令与退出码。它不包含 F7 binding 功能通过证明；F6 的 Python 402 或 CTest 数量不计为本轮结果。

## 2. 当前源码事实

以下行号属于上述读源 HEAD。

| 来源 | 实际事实及 F7 影响 |
| --- | --- |
| `dal-cpp/dal/script/settings.hpp:27` | 三个设置位于 `Dal::Script`；valuation 字段为 `todayFixingPolicy_`、`modelBindings_`、`evaluationDate_`、`fixings_`。复用这些类型。 |
| `dal-cpp/dal/model/base.hpp:19` | binding 是 `Dal::ModelIndexBinding_ {assetName_, indexName_}`；不新建同义 C++ 类型。 |
| `dal-cpp/dal/auto/MG_TodayFixingPolicy_enum.hpp` | `Dal::TodayFixingPolicy_` 是已有 Machinist class，值在其 `Value_`；独立默认 class 是 `_NOT_SET`。Python 默认不能映射成该无效值。 |
| `dal-public/src/{script,value}.{hpp,cpp}` | 新 product/settings Value/Describe/Explain 已存在；旧 Value 映射后转发共同准备。Python 不另实现 prepare 或修改全局日期模拟显式 D。 |
| `dal-python/src/dal/api.py:4` | 高层为 `events_dates`，Cell 原对象直接通过；只对非 Cell 包装。 |
| `dal-python/src/bindings/script.cpp:16` | 低层为 `dates`，元素 cast 到 Cell；event 当前从 Python str 复制。新参数须 `py::kw_only()`。 |
| `dal-python/src/bindings/value.cpp:18` | 旧 3–8 参数 lambda；`int num_path` 会在进入 lambda 前转换，F7 必须先保留 Python 整数对象完成范围检查。 |
| `dal-python/src/bindings/module.cpp:14` | import `_dal` 调 `InitGlobalData()`；模型/产品 handle 注册在 `global.cpp`，snapshot 在 curve 注册，script/value 随后注册。 |
| `dal-python/CMakeLists.txt:196`、`src/dal/__init__.py` | `dal.py` 是构建生成的 re-export shim，源码树没有此文件；高层 api 在低层导入后覆盖同名函数。不要手改生成 shim。 |
| `dal-python/src/bindings/curve.cpp:298,521` | snapshot factory 复制 `dict[str, dict[DateTime_, float]]` 到 native map，返回只读 native handle。没有 datetime 自动转换实现；不把原草图中的泛称当作已有能力。 |
| `dal-python/src/bindings/core.cpp:147`、`curve.cpp:423` | Date_ 构造为 yyyy/mm/dd；DateTime_ 的 hour 必填。正确午夜是 `DateTime_(date, 0)`。 |
| `dal-excel/src/{__script,__value}.cpp` | 旧 Excel 产品三输入、价格七输入；没有 compiled 参数。原 date serial 转换与路径检查保留，F7 不改 Excel。 |

同时核对 C++ script/script_tree 示例、public script_settings 示例、Python european 示例及 script_engine/AAD 方法中的 observation、union timeline、hard history、fuzzy future、mark/seed 术语。

## 3. 冻结签名与返回类型

### 3.1 高层 `import dal`

```python
Product_New(events_dates, events, *, settings=None)  # -> ScriptProductData_

MonteCarlo_Value(product, modelData, num_path, method="sobol",
                use_bb=False, enable_aad=False, smooth=0.01, compiled=None)
# -> dict[str, float]；旧签名、位置及关键字含义不变

MonteCarlo_ValueWithSettings(product, modelData, num_path, *,
                            valuation=None, simulation=None)  # -> dict[str, float]

ScriptProductSettings_(*, default_index="")
ScriptValuationSettings_(*, evaluation_date=None, today_fixing="Model",
                         model_bindings=None, fixings=None)
MonteCarloSettings_(*, method="sobol", use_bb=False, enable_aad=False,
                   smooth=0.01, compiled=None)

Product_Describe(product)  # -> dict[str, object]
ScriptValuation_Explain(product, modelData, *, valuation=None)  # -> dict[str, object]
```

必需参数可位置或按所列拼写传 keyword；星号后的参数只能 keyword。不新增 `**kwargs` 容错槽；重复位置/keyword、未知 keyword、额外位置参数须 TypeError 并列实际/允许参数。
旧 Value 不接受 `settings`、`valuation`、`simulation`；新 Value 不接受 `method`、`use_bb` 等扁平选项。Explain 不接受 `num_path`、`simulation`、`compiled` 或 AAD 选项。

### 3.2 低层 `from dal import dal as native` 或 `dal._dal`

- `Product_New(dates, events, *, settings=None)`：低层保留 `dates`，不接受 `events_dates`。高层不添加 `dates` 别名。
- Value、新设置 constructors、新 Value、Explain 参数形状与高层相同；低层必须直接执行严格校验，不能只靠高层 wrapper。
- `native.Product_Describe(product)` 和 `native.ScriptValuation_Explain(product, modelData, *, valuation=None)` 返回 **Python str，内容为共同 public 原始 JSON**。
- 高层仅对每次低层调用的字符串执行 `json.loads`，返回普通 dict。用私有名导入 json，避免 star export 泄露。不再加 `as_json` 选项或第二组公开 JSON 别名。
- `Product_DebugJson` 在两层均保留 JSON str `/1`；不能为统一新 dict 返回而改它。
- 产品/模型继续为现有 opaque native handle。三设置类由 binding 暴露真实 native 值；两个 Python 导入层应使用相同类对象。

## 4. 类型、None 和字段转换矩阵

矩阵适用于 constructor、同名属性 setter 及两层调用。未列出的隐式转换不支持：不调用任意 `str(x)`、不以 truthiness 解释设置、不从 dict 自动构造整个设置。

| 输入 | 接受/None 含义 | 拒绝与返回/复制 |
| --- | --- | --- |
| `settings` | None → 新默认 `ScriptProductSettings_`；或该 native 类实例 | dict/str/其他设置类拒绝；入口复制值。 |
| `valuation` | None → 新默认 `ScriptValuationSettings_`；或该类实例 | dict/str/其他类型拒绝；不在默认构造时捕获 D。 |
| `simulation` | None → 新默认 `MonteCarloSettings_`；或该类实例 | dict/str/其他类型拒绝；不与旧扁平参数合并。 |
| `default_index` → `defaultIndex_` | Python str 或 DAL String_；空串表示无 default | None/enum/dict/bytes 拒绝；getter 返回 Python str，保留原拼写。名称完整解析留 Describe/prepare。 |
| `evaluation_date` → `evaluationDate_` | None → 未指定；有效 DAL Date_ → native 副本 | str/String_/datetime.date/datetime.datetime/DateTime_/Cell_/数字/enum/dict 拒绝；getter 返回 Date_ 副本或 None。显式日期不读写 global。 |
| `today_fixing` → `todayFixingPolicy_` | 本节 enum，或精确 Python str/DAL String_ `Model` / `RequireHistorical` | None、整数、bool、其他 enum、dict、未知/空/大小写不同/前后空白名字拒绝；getter 返回对应 enum 成员。 |
| `model_bindings` → `modelBindings_` | None → 空 vector；或 dict，键和值各为 Python str/DAL String_ | list/tuple/任意 Mapping/str、非字符串键值、None 键值拒绝；getter 返回独立 `dict[str,str]`，未设置也返回 `{}`。 |
| `fixings` → `fixings_` | None → null handle/global 本次捕获；现有 MarketFixingSnapshot_ 实例 → const handle 副本 | 原始 dict/str/callback/其他 handle 拒绝；getter 返回 snapshot 对象或 None；保持同一 native snapshot 身份，Python `is` 不作跨 wrapper 保证。 |
| `method` → `rsg_` | Python str/DAL String_；sobol/mrg32/irn，沿 DAL 大小写比较 | None/enum/dict/bytes 拒绝；getter Python str，保留输入拼写；不 strip。 |
| `use_bb` → `useBb_` | Python bool，默认 False | None/数字/str/enum/dict 拒绝；getter bool。 |
| `enable_aad` → `enableAad_` | Python bool，默认 False | 同上；getter bool。 |
| `smooth` → `smooth_` | Python int/float（排除 bool），转换后的 double 有限且 >0；默认 .01 | None/str/String_/enum/dict/complex/NaN/Inf/0/负值及 double 溢出拒绝；getter float。 |
| `compiled` → `compiled_` | None 保留 nullopt；或 Python bool | 数字/str/enum/dict 拒绝；getter None/bool；有效执行中 None 等于 False。 |

新字段采用明确 Python 标量契约；不为新 API 承诺 Decimal、NumPy 浮点/bool 或自定义 `__float__` 转换。`num_path` 的整数协议另见第 6 节。旧 Value 的既有 flags/double 转换保持原有效输入接受集合；新设置的严格 bool 不应顺手收紧旧参数转换。非法 smoothing 在新旧两条路径均失败。

### 4.1 今天政策的实际 enum

公开 `dal.TodayFixingPolicy_`，通过 `py::enum_<Dal::TodayFixingPolicy_::Value_>` 绑定已有 enum：

- `dal.TodayFixingPolicy_.MODEL` ↔ `Dal::TodayFixingPolicy_::Value_::MODEL`。
- `dal.TodayFixingPolicy_.REQUIREHISTORICAL` ↔ 对应原值。

不重新定义 C++ enum/class，不公开 `_NOT_SET`/`_N_VALUES` 成员、不提供可写 `val_`。字段转换先确认是该 enum 实例并校验值，再构造原 `TodayFixingPolicy_`。不接受任意整数借 enum caster 混入。即使 pybind 版本能构造未命名 enum 值，setter 也必须拒绝。
字符串分支使用长度明确、区分大小写的匹配，不能直接调用原 Machinist 字符串构造器放宽为 case-insensitive。错误包含 `InvalidSetting`、`InvalidTodayFixingPolicy`、`today_fixing`/`valuation.todayFixingPolicy_`、原值和两项允许值。

### 4.2 名称与 model_bindings

所有新字符串转换从 Python str 的完整 UTF-8 字节复制为 `std::string` 再构造 `String_`；已有 String_ 按 native 值复制。返回用 `std::string(data,size)`，不用 `repr` 或 `c_str()` 截断。嵌入 NUL 的事件/设置字符串在转换边界报字段/约束错误，不交给 C 字符串 parser 截断；名称不 strip、不改写原始大小写。

dict 按插入顺序生成 `Vector_<Dal::ModelIndexBinding_>`，先建立临时 vector，全部转换成功才赋值。保留 DAL native 身份比较，不能先装入会合并键的 native map。

- `{"spot": "EQ[A]"}` 与两个 DAL String_ 对应输入都可转换。
- `{"spot": "EQ[A]", "SPOT": "EQ[A]"}` 在 Python 中仍有两项，native prepare 必须报 `DuplicateModelBinding`。
- `{"spot": "EQ[A]", "spot": "EQ[B]"}` 进入函数前已只剩后一项，不能声称检测过原始重复键。
- 未知/空 asset、空/畸形 index、能力限制和冲突在 Value/Explain 的共同 prepare 拒绝；constructor/setter 只负责字典形状/字符串类型与完整复制，不启动 parse/model/I/O。
- 首版仍是一个 `spot -> 普通 EQ`；历史多个 EQ/FX 不等于模型支持多资产。default_index 只给 SPOT 身份，不替代未来 binding。

## 5. 属性、复制与 GIL

### 5.1 可预测的赋值

只公开 constructor 中的 snake_case 属性；不提供可绕过校验的 `def_readwrite` C++ 字段别名，也不启用 dynamic attributes。未知 constructor keyword → TypeError；未知属性赋值 → AttributeError。失败 setter 保留之前有效值，不留下部分更新。

`b = a` 是普通 Python 别名；`copy.copy(a)` 和 `copy.deepcopy(a)` 均提供新的 native settings 值对象。深复制同样共享 immutable snapshot，复制 date/string/vector，不能把 snapshot 变成 Python dict。constructor 不额外接受位置 copy 参数。

bindings getter 每次返回独立 dict；编辑它不改变 settings，需重新赋值：

```python
bindings = valuation.model_bindings
bindings["spot"] = "EQ[OTHER]"
valuation.model_bindings = bindings
```

返回的 Date_ 也为副本。构造 settings 所用 dict 后续修改、settings 在 Product_New 后修改都不会改变已创建产品。Value/Explain 开始时复制设置与 native handles，调用不会把捕获的 D 写回 `evaluation_date`。调用结束后的设置修改只影响后续调用。

### 5.2 Python 到 native 的边界

1. 持 GIL：完成参数分派、Cell/string/整数/设置转换、日期检查；复制 dates/events、所有设置值和 bindings vector；将 product/model/snapshot 复制成 native const handle。
2. 释放 GIL：仅以 C++ 所有数据调用 `NewScriptProduct` / `ValueByMonteCarlo` / `DescribeScriptProduct` / `ExplainScriptValuation`。不能将 `py::dict`、`py::object`、borrowed iterator 或 settings 引用留作 native 工作输入。不要在接收 Python 容器的整个 lambda 上提前使用 call_guard release。
3. native public 完成返回/异常展开后恢复 GIL，再转 Python map/str；高层 json.loads 也在持 GIL 时执行。

Python 对象可在外层作用域保活，但释放区间/worker 不能访问或销毁需 GIL 的对象。释放期间其它 Python 线程改原 settings 不影响已完成复制的本次调用；转换窗口中调用方仍应避免并发/重入修改输入（自定义 iterable 或 `__index__` 可执行 Python）。不为 free-threaded Python 另作同步承诺。

snapshot handle 共享不可变数据并保活；worker 历史值来自 sealed plan，AAD 参数相关历史状态在 worker 自己的 recording 重建。每次 Value/Explain 都独立准备；无 Python callback、每路径 dict 或隐式 prepared 缓存。
全局逐序列捕获仍非联合原子市场快照，捕获期不得并发写 fixing；显式 snapshot 只固定历史，不能当作整个模型市场版本的冻结。

## 6. 拒绝边界与错误合同

Python 形状/类型不符使用 TypeError（未知属性 AttributeError）；类型正确但数值/政策非法采用现有 DAL/Script exception → RuntimeError，不另造异常体系。Python 独有的检查也给出稳定错误标识、函数、Python 字段、原值/类型和约束；native 错误原文保留，可加上下文，不能替换掉 cause。未知 keyword 的 TypeError 应显示 offending keyword 和允许签名；不要求给没有 core 对应物的分派错误伪造 C++ 标识。

| 输入/触发 | 拒绝边界及必要上下文 |
| --- | --- |
| `num_path` | 两个 Value 的低层调用，在 pybind int 截断/溢出之前检查。接受 Python int 或有效 `__index__` 整数协议，排除 bool 和 enum；范围 `1..numeric_limits<int>::max()`。float（含 1.0）、NaN/Inf、str、None → TypeError；0/负值/超上限 → RuntimeError。均含 `InvalidPathCount`、`num_path`/`numPath` 和正整数范围。保留旧非正错误的 `number of Monte Carlo paths must be positive` detail。 |
| 超大整数 | 直接用 Python 整数/checked C API 与 INT_MAX 比较；不要先转 double 或有符号 int。`INT_MAX` 的上界接受用转换 seam 检查，不能真的申请这么多路径。 |
| `evaluation_date` | constructor/setter 先检查 Date_ 类型再 `IsValid()`；无效值为 `InvalidSetting: valuation.evaluationDate_ ... expected valid date`。不要先格式化无效日期。None 不做 CaptureScriptEvaluationDate。 |
| Date_ 构造本身 | `Date_(2026,2,30)` 已由原构造器拒绝；不可声称是 settings 抛错。构造器允许某些无法表示的年月日得到 invalid Date，例如 1900-01-01，settings 必须再检查有效性。F7 不重新设计 Date_。 |
| product 日期表 | 高层保留非 Cell 包装及原 Cell_ 可转换输入，低层仍要求 Cell 元素；日期行用Date_、definition/schedule通常用字符串。不能给 numeric Cell 新增Excel serial解释或默默floor；原core把非Date Cell转为definition文字的兼容行为保留，Python不能判断一个数值原本是否意图为日期。日期型 Cell 的有效性与 schedule 展开日期由 native parse 在 Describe/Value/Explain 检查，保留 `InvalidFixingDate`、dates/events、一基row。无日期事件由结构校验拒绝，错误原始Python对象在Cell转换边界带行上下文失败。字符串日期不新增自动解析入口。 |
| 长度与事件文本 | Product_New 复制后沿 core 检查等长（`InvalidSetting`、dates.size/events.size）；事件接受 str/String_，不接受任意对象/Cell/bytes/None。脚本文法仍延迟 parse；不能构造时触发市场访问。 |
| `today_fixing` | constructor/setter 拒绝非法名字和值；保留 `InvalidTodayFixingPolicy` + `InvalidSetting` 及两个允许值。 |
| method/smooth | constructor/setter 用 native `ValidateRNG`/`ValidateSimulationSettings` 或等价无 I/O 校验；保留 `simulation.rsg_`/`simulation.smooth_`、`InvalidSetting`、`InvalidSmoothing`。整数转 double 溢出同样给字段约束；即使 AAD False 也检查。共同 public 再验一次。 |
| bindings | 非 dict/非字符串项在 constructor/setter 拒绝并列位置/键；语义在 Value/Explain prepare 报 `UnknownModelAsset`、`DuplicateModelBinding`、`InvalidIndex`/`UnknownIndex`、`MissingModelBinding`、`ConflictingModelBinding` 或 `UnsupportedModelObservation`。保留 native `valuation.modelBindings_[i]` 和原/规范身份。 |
| product default | constructor/setter 接受空或字符串；Describe/prepare 对非空值按原 indice 解析，保留 `product.defaultIndex_`；不在 Python 猜测合法 EQ 字符串正则。 |
| None product/model | 转 null native handle，由共同 public 在解引用前报 `InvalidSetting` 和 non-null 约束。错误类型对象在 binding TypeError。 |
| FIX 日/前视 | parse/prepare 保留 `InvalidFixingDate`、`LookAheadObservation`、原 literal、F/E、source row/offset/line/column；不截断日内时间、不接受 runtime 日期表达式。 |
| 缺历史 | Value/Explain 准备中 `MissingFixing`，明确源类型、raw/canonical、午夜键、事件/节点及 exact-history 约束；不得回退模型/global/旧值/PV0。 |
| snapshot 构造 | 继续用原 factory 的时间、finite、FX 正值/互反规则及异常；已造出的 snapshot 只验类型/保活，不再转换其中 map。不声称全部非法 snapshot 都是在 Value 拒绝。 |
| 空/过期合约 | 空表/定义-only/no PAYS 可以 Describe，Value/Explain 报 `InvalidScriptStructure`。真正全过期仍验证普通设置/结构，才可原合同 PV0；不以空或错误合约冒充过期。 |

严格路径规则覆盖新旧入口及过期产品，不因某路径不启动 worker 而放宽。合法旧未来 SPOT、Cell/string/date、关键词与 compiled 行为保持；旧历史占位值 30、错误输入截断与非法 smoothing 不属于兼容承诺。

## 7. 诊断、snapshot 与 archive 边界

- `Product_Describe` 对齐 **dal.script-product/2**：从原产品表解析全部事件/原分支，包含原/规范 index、default、FIX 省略/显式日期、source 和节点身份。没有市场 I/O、global D 读取、model、phase、worker。成功仅表示可描述。
- `ScriptValuation_Explain` 对齐 **dal.script-valuation/1**：每次独立默认价格准备（double/exact、tree、sobol、BB/AAD False、smooth .01），可模型 Allocate/Init、历史解析/hard replay；没有 MC driver、GeneratePath、worker 或 AAD tape。不缓存下一次 Value。
- 直接保留 C++ JSON 内容及字段大小写：`Model`/`RequireHistorical`、`GlobalSnapshot`/`ExplicitSnapshot`、`Historical`/`Model`、`Resolved`/`SkippedExpired`。dict 转换只把 JSON null/bool/number/string/array/object 映射成 Python None/bool/int-or-float/str/list/dict，不改键或将日期变成 Date_。
- 保留 `observation_mode`、requests/uses、`history_value_id`、model_slot、live_events/event_to_sample、sample_dates/timeline/numeraire_requests。request_id 不等于 history_value_id；全事件 ID 不能直接索引 future-event 数组。不能重 parse 拼出另一套 ID。
- 明确 `fixings=None` 与 `MarketFixingSnapshot_New({})`：前者抓本次 global，后者显式空市场，缺值失败，已有 global 80 也不可补。旧 snapshot H=80 与后续 global H=90 可共存。
- snapshot 当前原生 timestamp 为 DateTime_，午夜须显式 hour 0；仅有 11:00 的快照不能满足日频请求。不扩展 factory 接受 Python datetime；不修其范围外的键转换行为。
- 价格结果只有 `PV` 与 AAD 的 `d_...`，值为 float；PV 已归一，风险也已归一，不再次除路径数。没有 fixing risk 或诊断数字键。
- archive 仍由 F6 native v1 reader/v2 writer 控制：保留原表/default 原名，无 D/历史值/binding/plan/bytecode/AAD seed。旧 DebugJson `/1` 对 FIX 或非空 default 报 `DebugSchemaUnsupported`，不得静默降版。
- 当前 Python ScriptProductData_ opaque 注册未声明 Storable_ base，不能把内部 `_StorableToJson` 当作已支持的产品 archive API。F7 不增产品 pickle/serializer，不扩大注册关系来凑覆盖；T29 的真实 archive roundtrip/golden 在 C++ consumer/public tests 验证，Python 验证 Describe/Explain/旧 DebugJson 边界。诊断 dict/JSON 不宣称可载入为产品。

## 8. 完整目标示例

以下是 implementer 应落入可执行例子并由 tester 实际 smoke 的完整内容。本轮仅 Python 语法/AST 检查，未运行未实现的 API。
import 已初始化 DAL registry/global runtime；这里显式 D，因此无需 EvaluationDate_Set。模型工厂顺序为 spot/vol/rate/div。

<!-- executable-example -->
```python
import math
import dal

D = dal.Date_(2026, 9, 12)
H = dal.Date_(2026, 9, 11)
P = dal.Date_(2026, 9, 22)
index = "EQ[DAL196_TEST]"

snapshot = dal.MarketFixingSnapshot_New({
    index: {dal.DateTime_(H, 0): 80.0},
})
model = dal.BSModelData_New(spot=100.0, vol=0.0, rate=0.0, div=0.0)
product = dal.Product_New(
    ["SCALE", H, P],
    ["2.0", "x = SCALE * FIX(EQ[DAL196_TEST])",
     "pay PAYS x + FIX(EQ[DAL196_TEST], 2026-09-15)"],
    settings=dal.ScriptProductSettings_(default_index=index),
)
valuation = dal.ScriptValuationSettings_(
    evaluation_date=D,
    today_fixing=dal.TodayFixingPolicy_.MODEL,
    model_bindings={"spot": index},
    fixings=snapshot,
)
result = dal.MonteCarlo_ValueWithSettings(
    product, model, 4096,
    valuation=valuation,
    simulation=dal.MonteCarloSettings_(enable_aad=True, compiled=True),
)
assert math.isclose(result["PV"], 260.0, rel_tol=0.0, abs_tol=2.6e-10)
assert math.isclose(result["d_SCALE"], 80.0, rel_tol=0.0, abs_tol=1e-10)
assert all(key == "PV" or key.startswith("d_") for key in result)
description = dal.Product_Describe(product)
explanation = dal.ScriptValuation_Explain(product, model, valuation=valuation)
assert description["schema"] == "dal.script-product/2"
assert explanation["schema"] == "dal.script-valuation/1"
print(result)
```

这是混合历史/未来 FIX：历史状态160 + F日模型100 → PV260，d_SCALE80。无引号 index 是脚本文法；宿主 Python 字符串仍加引号。Explain 是独立默认准备，不是上方 compiled/AAD 的执行轨迹。
T18 另用最后一行 `pay PAYS x`、r=.05 得历史纯付款解析 oracle；不要把本例的混合风险误写成 T18 的模型风险全0。

## 9. 后继验收：必须有独立 oracle

共同 fixture D=2026-09-12、H=09-11 00:00、F=09-15、P=09-22、history80、SCALE2、spot100。
确定性 PV 容差 `1e-12*max(1,abs(expected))`；解析 AAD 风险按父合同绝对 `1e-10`；固定路径 MC 按归一均值 `1e-8`。FD 同路径/epsilon、光滑点中心差分；hard 分支切换点不作光滑导数验收。勿从旧 spec 较宽相对容差覆盖当前父合同。

| 映射 | F7 必须给出的验证与独立预期 |
| --- | --- |
| T28 签名/输入 | `test_api.py`/`test_script.py`/`test_value.py` 同时直接调 high/low：events_dates/dates、已有 Cell 不双包装、旧3–8位置及全 keyword、默认 None、所有新星号边界、未知/重复 keyword。三设置 constructor/setter 使用同一输入矩阵；失败赋值原值不变，copy/deepcopy/new product copy/dict getter 不别名。 |
| T28 严格边界 | paths `{1, numpy整型或自定义__index__, 0,-1,1.0,1.5,True,None,"2",NaN,Inf,INT_MAX+1,2**100}`；上限只在转换 seam 验证。date 类型与invalid Date、smooth坏值、enum错类/非法值/精确string、NUL、model_bindings形状/两种重复情况分别记录。错误检查标识/字段/约束，不能仅 raises(Exception)。 |
| T07 today | D日模型100、历史80、D日付款：Model PV100/历史0，RequireHistorical PV80/历史1，缺值失败。三种RNG、BB开关、tree/compiled、double/AAD；显式 D 与 global D 故意不同。分别计全局 History 和最终 Index::Fixing。 |
| T15 repricing | 同产品 global80 → global90 → 显式旧snapshot80；SCALE版 PV160→180→160，d_SCALE80→90→80（零率）。Explain先80后Value90；旧plan保持80由native验证。显式空snapshot在global有80时仍MissingFixing。Describe改D/H不变；输入map改动不污染snapshot。 |
| T18 historical AAD | H事件 `x=SCALE*FIX(...)`，P事件 `pay PAYS x`，r=.05、T=10/DAYS_PER_YEAR：PV160 exp(-rT)、d_SCALE80 exp(-rT)、d_rate=-T*PV、d_spot=d_vol=0、无fixing风险。tree/compiled、路径1/257/8193、多批/重定价；与C++同snapshot parity仅补充。 |
| T09/T10/T12/T21 future EQ | 合法BS与Dupire显式spot绑定；遗漏/冲突/未知asset/FX/delivery/双EQ在历史I/O/worker前失败。BS vol0且非零r/q，F无事件，P付款：PV=`100*exp((r-q)*tF-r*tP)`，区别F与P采样；重复F相减严格0。用native固定scenario F120/P999的期望120作补充，避免共享寻址错误。 |
| T20/T22 exact/fuzzy | H80、K79.95、future `IF FIX(...,2026-09-11)>K:0.2 THEN pay PAYS SCALE*80 ELSE pay PAYS 0 END`、零率：exact PV160；fuzzy权重.75、PV120、d_SCALE60、d_K=-800；各自tree/compiled独立断言，FD每次重建。相同条件放历史事件则硬选支，不能平滑历史。 |
| T27 legacy | 原未来SPOT fixtures不改脚本、旧modelData/num_path/method与compiled/default行为；保留原解析BS或固定路径参考。历史未绑定SPOT报UnboundHistoricalSpot；FIX混用无default报MissingDefaultIndex；有default同identity/time共用一请求；SPOT(index)/FIX()拒绝。 |
| T29 diagnostics/archive | high dict == json.loads(low JSON)，并逐字段对共同schema/native真实request/uses/history/model IDs；Describe市场/model/worker计数0且正向计数seam可触发，Explain历史每唯一1次/每序列至多1次、worker/GeneratePath0、无缓存。原v1 golden、v2原文字面量/default、archive无runtime污染用真实C++ registry；Python旧DebugJson拒绝新身份，不把新dict当archive。 |
| T25/T31 lifecycle | 全过期合法PV0/标签风险0/无work，坏路径/settings仍失败；空/noPAYS报结构错误。准备最后历史失败/模型失败worker0；释放GIL期间Python线程确实推进，settings复制已完成；异常恢复后调用成功。不得向public暴露Python callback来测计数。 |
| 完整例子 | 将第8节落入直接可执行Python example，构建本次binding后运行并保留stdout/exit code，核对合法模型、import初始化、明确午夜和PV/d_SCALE。 |

global StoreFixings、history/worker计数及固定scenario不是现有公开 Python 能力；由直接测试目录下的 native test-support bridge 或共同 public/core tests 负责，以实际新 binding 的调用覆盖为主。若需要额外测试构建接线，向父说明最小范围；不为了方便新增公开市场写入API。

后继必须实建并证明所加载新模块：记录 OS、Python/pybind/CMake/compiler/AAD backend、源码SHA/tree、`dal.__file__`/`dal._dal.__file__` 和动态库路径，再跑实际配置的 `python -m pytest dal-python/tests/test_api.py dal-python/tests/test_script.py dal-python/tests/test_value.py`、snapshot相关 `test_xccy_resettable.py`/`test_curve_pricing.py` 及完整相关Python回归；没有独立的 `test_snapshot.py` 可直接假定。
补受影响core/public/installed consumer/build及sanitizer；受支持backend/platform与未跑范围分开报告。性能只作参考，不增gate；Linux portable Excel不证明Windows XLL。

## 10. 取舍、实际核对与开放项

决定：新增独立 ValueWithSettings；whole-settings dict不隐式接受；snake_case属性校验；today enum映射原Value_且字符串精确；model_bindings只转native vector；高层dict/低层JSON；复用native snapshot；日期不做隐式字符串/Excel serial/datetime转换。

拒绝：继续增加旧Value位置参数；重命名todayFixingPolicy_；由模型名/唯一FIX推断default；把None和显式空snapshot合并；用global日期临时改写实现显式D；properties返回可变vector引用；worker用Python回调；把Explain当Value缓存或AAD/compiled审计；诊断map混进价格；新增产品pickle/扩大C++或Excel写域。

实际结果：

- GCC 15.2.0、C++17 `-fsyntax-only` 现有 public 头 consumer：exit 0，stderr 为空。静态断言原类型/字段/const snapshot，旧3–8参数、新typed Value、product及两个诊断声明均通过；没有链接或运行。
- Python 3.13.9 `evidence/signature-probe.py`：exit 0，32个语言签名绑定正/负例通过；第8节完整例子 AST/compile 通过。原型只使用空函数签名，未导入DAL或模拟定价。
- Git 2.53.0、CMake 4.2.3、multica 0.4.43；HEAD/tree/ancestor/干净初始状态核对通过。交付前父description与初读一致；新派发评论线程 `01a0a396-2ec9-77bd-a8bc-bd03dac1d550` 全文确认相同串行合同，S2–S5仍backlog。
- `git diff --check` 通过；新增note另做直接文本检查（普通git diff不会包含untracked文件）。输入文件identity、命令与日志见附件evidence。

没有构建/运行F7 binding、定价/风险测试、C++ full tests、sanitizer、alternate backend、Excel、Machinist或CI。探索时不存在的 `src/dal/dal.py`、`model/observation.cpp`、`src/global.cpp` 已分别查到生成shim、真实preparation/model代码和 `src/globals.cpp`；失败查找不算验证通过。

**结论：无实质语义偏离或待用户决定，可进入 implementer。** 后续实际binding构建、全部矩阵与例子smoke、文档/CHANGELOG及独立review尚待串行执行；本设计交付不代表F7实现完成。父验收关闭DAL-238后推进S2；本角色不启动S2或F8。
