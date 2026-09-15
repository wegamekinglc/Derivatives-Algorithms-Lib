# DAL-205 F8：Excel FIX 设置、诊断与工作簿 API 合同

状态：DAL-243 S1 API 设计交付，供父任务验收后进入 S2。本文的新 Excel 函数尚未实现。
面向工作簿作者、Excel binding 实现者和独立测试者；C++/Python 调用方继续使用 F6/F7 已实现的合同。

## 1. 基线、权限与结论

- 实际 HEAD：`9e6a55f58228a2c5b8101e08b6ab3551ae99a119`，tree：`ce51205a29fc1a65750b04937675f78feebe2266`。
- `multica repo checkout git@github.com:wegamekinglc/Derivatives-Algorithms-Lib.git --ref master` 得到分支 `agent/dal-api-designer/1fce2ccb5ab0`；初始工作树干净，必需 F7 squash 祖先检查 exit 0。
- 当前 DAL-243 / DAL-205 description 优先；两任务启动时 roots-only comment scan 均空。已读父全文、children、active runs；没有另建专家链。原批准 spec/API/critique 经 authenticated CLI 下载；其待授权文字及 F6/F7 note 的交付时态不是本轮限制。
- 全文核对 F6/F7 API note，核对当前 core/public、Python、Excel、生成器、相关测试、示例及方法说明。实际 native 字段是 `todayFixingPolicy_`；不移动或重新定义设置类型。
- 唯一仓库写入是本文。声明/生成原型、独立算式、日志、manifest 在自身 workdir，以 evidence 附件交付；没有产品实现、提交、推送或报告 PR。父验收后由 implementer 纳入实现分支。

**结论：无实质语义偏离，可进入 S2。** 三个容易漏掉的 Excel 边界已给出可落实决定：设置矩阵使用 `cell[][]+` 保留完整范围；snapshot 三条并行输入允许全部为空以构造显式空对象；诊断返回可无损拼接的 JSON 文本列。后两项均保留既有 native 合同。

## 2. 实际源码对应与兼容边界

行号属于上述 HEAD。

| 来源 | 已有事实 | F8 落点 |
| --- | --- | --- |
| `dal-cpp/dal/script/settings.hpp:27` | `Dal::Script` 已有三个设置值类型；valuation 有 policy/bindings/optional Date/snapshot | Excel 只包装这些值，不增加第二套设置定义或默认值。 |
| `dal-public/src/script.hpp:16`、`script.cpp:16` | 三参/四参 `NewScriptProduct`，`DescribeScriptProduct` | wrapper 分别选择明确重载。创建产品不 prepare、不读市场。 |
| `dal-public/src/value.hpp:18`、`value.cpp:36` | typed Value 与 Explain 已实现；旧 Value 转发 typed Value | 新 wrapper 读取设置副本再调用；不自行创建 prepared plan。 |
| `dal-excel/src/__script.cpp:44` | 旧产品数值日期逐项 `Date::FromExcel(Cell::ToInt(v))` | 新产品完整复用这段规则。`Cell::ToInt` 检查精确整数，不能把旧规则描述成接受小数后 floor。 |
| `dal-excel/src/__value.cpp:15`、`auto/MG_MonteCarlo_Value_public.inc` | 旧七输入、`QQQQQQQQ`、无 compiled；输出 map 顺序的两列表 | 注册/次序/日期转换/二维表保持；不新增旧入口第八个 Excel 参数。 |
| `dal-excel/src/__value.hpp:14` | `CheckedMonteCarloPathCount(double)` 检查 finite/integral/1..INT_MAX | 新旧 Value 继续复用；新 wrapper 可增补 `InvalidPathCount` 和 `n_paths` 上下文。 |
| `dal-excel/src/__curve_storable.hpp:70` | `StorableMarketFixingSnapshot_::val_` 是已有 core snapshot handle | valuation 复制该 handle；不再造 script snapshot 或逐路径 map。 |
| `dal-excel/src/__curveprotocol.cpp:181` | 三条并行 vector 等长、重复检查；serial 的 floor 部分作日，余数保留时间；空 vectors 可构造空 native snapshot | 仅给原 markup 增加三输入 optional 和准确帮助，再生成 inc/htm，数值转换主体保持。 |
| `dal-excel/src/_excel.cpp:934,982,1019` | vector 在首个空项停止；普通 matrix 按 NonblankSize 缩小 | 设置/binding 不用 dictionary/vector，也不用普通非 greedy matrix。snapshot 仍沿已有并行 vector 规则，不能宣称新增了跳过内部空行的 snapshot 语义。 |
| `dal-excel/src/_excel.cpp:852,915` | nullable handle 经 `ToString(...,true)`；递归处理单元素 multi 时 ToString 丢掉 optional | 新入口局部展开单元素 multi 再使用生成的 nullable ToHandle，覆盖实际空单元格引用；不全局改旧 converter。 |
| `dal-excel/src/_excel.cpp:373` | WriteToOper 会截断过长字符串，并逐字节写入宽字符 | 新诊断在 wrapper 中作 JSON 的 ASCII 转义与分块；不依赖底层静默截断。 |
| `dal-cpp/dal/script/diagnostics.cpp:79,143` | 产品 `/2`、估值 `/1` 是当前真实 JSON；Explain 有 `observation_mode` 等字段 | 只投影同一 JSON，不重新收集请求或编号。 |
| `dal-cpp/config/{dal.ifc,dal.mgl}`、`dal-cpp/CMakeLists.txt:108` | 从 Excel 源码 IF markup 生成 `auto/MG_*_public.{inc,htm}`，使用 pinned Machinist | 实现修改来源后跑 dal_generate/dal_check_generated，不手改生成文件。 |

已核对 Python 的 `Product_New(..., *, settings=None)`、`MonteCarlo_ValueWithSettings(..., *, valuation=None, simulation=None)`、三设置类、低层 JSON / 高层 dict。Excel 的工作簿矩阵与 opaque handle 是语言投影，不要求 Python 接受 Excel serial 或数字 bool。

## 3. 冻结注册、输入与输出

当前模板发出 C 导出 `xl_<public-name>`，worksheet 名将下划线变点并大写，category 为 `Base`。**本功能使用下表名字，不凭旧别处的 `DA.` 前缀添加新别名。** Q 为 XLOPER12 类型代码，包含返回值；表内方括号表示 Machinist `&optional`，不是公式里的字符。新增函数均非 volatile，与相邻入口一致；显式重算仍会重新 prepare，不代表 Excel 自动观察 global 历史变动。

| Worksheet 名 / C public 名 | 注册 argNames | argTypes | 输出 |
| --- | --- | --- | --- |
| `SCRIPTPRODUCTSETTINGS.NEW` / `ScriptProductSettings_New` | `name,[settings]` | `QQQ` | 一个 `StorableScriptProductSettings_` handle |
| `PRODUCT.NEWWITHSETTINGS` / `Product_NewWithSettings` | `name,dates,events,settings` | `QQQQQ` | 原 `ScriptProductData_` handle |
| `SCRIPTVALUATIONSETTINGS.NEW` / `ScriptValuationSettings_New` | `name,[settings],[model_bindings],[fixings]` | `QQQQQ` | 一个 `StorableScriptValuationSettings_` handle |
| `MONTECARLOSETTINGS.NEW` / `MonteCarloSettings_New` | `name,[settings]` | `QQQ` | 一个 `StorableMonteCarloSettings_` handle |
| `MONTECARLO.VALUEWITHSETTINGS` / `MonteCarlo_ValueWithSettings` | `product,modelData,n_paths,[valuation],[simulation]` | `QQQQQQ` | `Matrix_<Cell_>`，N×2，无标题 |
| `PRODUCT.DESCRIBE` / `Product_Describe` | `product` | `QQ` | `Vector_<String_>`，N×1 JSON 文本块，无标题 |
| `SCRIPTVALUATION.EXPLAIN` / `ScriptValuation_Explain` | `product,modelData,[valuation]` | `QQQQ` | 同上，估值诊断 |

必须逐项保留旧注册：`PRODUCT.NEW` / `xl_Product_New`，`name,dates,events` / `QQQQ`；`MONTECARLO.VALUE` / `xl_MonteCarlo_Value`，`product,modelData,n_paths,rsg,use_bb,enable_aad,smooth` / `QQQQQQQQ`。旧 `PRODUCT.DEBUG` 不改名、不替换成新 Describe。

### 3.1 typed 内部边界

三 constructors 接收 `const String_& name, const Matrix_<Cell_>& settings`；valuation 再接 `const Matrix_<Cell_>& model_bindings`、`const Handle_<StorableMarketFixingSnapshot_>& fixings`，最后为输出 handle 指针。这是 Excel 本地函数，不向 dal-public 增加这些包装类型。

新产品的第四输入是**必需且有正确类型的产品设置 handle**；默认产品使用旧 `PRODUCT.NEW`，或先 `SCRIPTPRODUCTSETTINGS.NEW("defaults")`。不将此第四输入再放宽为矩阵、JSON 或其他设置 handle。

新 Value 的 valuation/simulation 尾参可分别省略。生成器必须发出 `Excel::ToHandle<StorableScriptValuationSettings_>(...,true)` 与 simulation 的对应调用；不提供 `(...)` native 默认实例来代替 handle 转换。进入内部函数后：null handle → 默认 native 值；非 null → 复制 `val_`；随后明确调用 typed public Value。Explain 只取 valuation 副本，不能接路径数或 MC 设置。

结果与旧 Value 相同：每行第一列为 `PV` 或 `d_<parameter>`，第二列为 finite double；AAD 关闭只有 PV，开启有模型/脚本参数风险。按 public map 的真实顺序写表，**不承诺 PV 在第一行**。不能再次除风险、增加 fixing 风险或把诊断混进价格表。调用失败经 Excel::Error 返回错误文本，不返回零或部分价格。

## 4. 矩阵、字段、默认值及错误

### 4.1 全范围与空值

设置和 binding 的源声明均为 `cell[][]+`，放在 `&optional` 后，生成 `ToCellMatrix(xl_arg,true,true)`。不使用 `Dictionary_`：它会合并/跳过输入，丢失重复项与物理行号。

- 省略参数、missing/nil、空字符串标量或一个空单元格引用表示未提供矩阵；portable 的标准 0×0 空 Matrix 同义。
- 实际范围必须恰好两列；N×2 中整行的 key/value 都空可跳过，但继续读后面的行。空包括 monostate 或空字符串，不包括空格、0、FALSE。
- 全空 N×2 表得到默认值。**非两列范围即使全空也拒绝**；单一空单元格只是上面的缺省表示，不能把任意全空 3 列表都当缺省。
- 一行仅一侧为空即拒绝。`default_index | 空` 也拒绝；要表达无 default，省略这一行。`compiled | 空` 同理，省略键保留 nullopt。
- 数据范围不含标题。key 必须 string；按照 DAL 的大小写不敏感字符串比较检查已知键和重复键，不 trim，不接受别名。`today_fixing` 与 `TODAY_FIXING` 是重复，` today_fixing` 是未知键。
- 从第一个物理行开始逐行校验；忽略空行不重新编号。检测完当前行再写入本地临时 native 值；全部成功后才发布 handle。不能先建 map 再声称检查过重复。
- binding 同样两列 `asset,index`；保留 vector 顺序，重复 asset 按 DAL 比较拒绝，即使 index 相同。空/未知 asset 明确报错，当前有效 asset 为 `spot`。重复诊断同时给首个与再次出现的行。

### 4.2 值与 native 映射

| 表/键 | 未提供时 | 接受类型及约束 | native 字段 |
| --- | --- | --- | --- |
| product `default_index` | 空 | 非空 string，保留原拼写；完整 index 文法归 Describe/prepare | `defaultIndex_` |
| valuation `evaluation_date` | nullopt，Value/Explain 入口捕获一次 | 有效 `Date_` Cell 或 finite、精确整数 Excel serial；见下节 | `evaluationDate_` |
| valuation `today_fixing` | Model | **区分大小写**的 `Model` / `RequireHistorical` string；不接受布尔、数值、空格或 UseIfAvailable | `todayFixingPolicy_` 的 MODEL / REQUIREHISTORICAL |
| MC `method` | sobol | string，sobol/mrg32/irn，沿 DAL 大小写比较；不 trim | `rsg_` |
| MC `use_bb` | false | Excel boolean，或恰好数值 0/1 | `useBb_` |
| MC `enable_aad` | false | 同上 | `enableAad_` |
| MC `smooth` | .01 | numeric Cell，排除 bool，finite 且严格 >0 | `smooth_` |
| MC `compiled` | nullopt → tree/false | Excel boolean，或恰好数值 0/1；键缺省才为 nullopt | `compiled_` |
| binding 列1/列2 | 空 vector | 非空 string/string；asset 只允许 spot；完整 index 解析/模型能力归共同准备 | `ModelIndexBinding_ {assetName_,indexName_}` |

数字 bool 接受集合与现有 Excel::ToBool 一致，确定为 `{0,1}`；`2`、`-1`、NaN/Inf、文本 `"TRUE"`/`"FALSE"`/`"1"` 均拒绝。不用 `Cell::ToDouble` 的 bool 提升放宽 smooth，不用 truthiness。today 字符串必须用长度明确、区分大小写的比较，不能直接交给大小写不敏感的 Machinist enum 字符串构造器；也不映射成 `_NOT_SET`。

constructor 校验矩阵、标量类型、日期、政策、RNG/smooth 与重复 asset；不调用 `ResolveValuationSettings`，它会捕获 global D。完整脚本/index 解析、binding identity/capability、F<=E 和历史齐备留 public Describe/prepare，沿用现有错误与源码位置。构造 settings 成功不意味着产品可定价。

### 4.3 日期与路径

- evaluation_date 的 numeric Cell 必须先 `isfinite`、精确 `trunc(x)==x`、安全范围检查，再转 integer、`Date::FromExcel`，最后 `IsValid()`；不能先 `AsInt`/cast 溢出，再检查。
- 当前 Date::FromExcel 有效 serial 为 **25569..91103**（内部 uint16 范围），不是 Excel 可显示的全部日期。prefer 从 Date 有效性及真实边界定义实现/断言；边界探针包括 25568、25569、91103、91104、1e100、NaN/Inf。不要宣称 Date_ 接受所有 1900..2199 构造年月日都得到有效值。
- 不接受日期文本、DateTime_（午夜 DateTime 也不是 Date）、bool、小数 serial。工作表 `DATE(2026,9,12)` 为 numeric serial，格式不决定类型。缺省通过省略键表达，不用 0 表示今天。
- `Product_NewWithSettings` 的事件日期数组完全复用旧 Product_New 转换；不新增文本日期解析，不把 definition/schedule Cell 误判成估值日期。已有数值日必须精确整数。
- snapshot 的时间是另一合同：`DATE(2026,9,11)+TIME(11,0,0)` 的 fraction 保留为精确日内键；日频 FIX 只请求 `2026-09-11 00:00:00`，11:00 绝不能补午夜。
- `n_paths` 是 Excel numeric，范围 1..INT_MAX、finite、精确整数，使用 `CheckedMonteCarloPathCount`。INT_MAX 只在转换 seam 验证，不真的模拟。非 numeric 在生成转换边界失败；原 helper 的详细文字保留，新 wrapper 可附 `InvalidPathCount: n_paths ...`。

### 4.4 错误定位与 raw Excel 边界

设置/binding 错误固定包含 `InvalidSetting`、函数、参数名、**相对该输入矩阵的一基 row/column**、字段/原值或实际类型、允许值或约束。保留 native cause，如 `InvalidSmoothing`、`InvalidTodayFixingPolicy`、`DuplicateModelBinding`、`UnknownModelAsset`。列数错误以 row=1 为范围锚点：缺列报首个缺失列（1列→column=2），多列报 column=3，并写实际 rows/cols 与 expected 2 columns；它不是伪造一个已有内容的单元格。

示例（断言标识和字段，不锁整句）：

```text
InvalidSetting: ScriptValuationSettings_New; settings row=4 column=1;
duplicate key today_fixing; first row=2; expected each key once

InvalidSetting: MonteCarloSettings_New; settings row=3 column=2;
enable_aad received string TRUE; expected Excel boolean or numeric 0/1

InvalidSetting: ScriptValuationSettings_New; settings row=1 column=2;
evaluation_date=46277.458333...; expected a valid integral Excel date serial

InvalidSetting: ScriptValuationSettings_New; model_bindings row=3 column=1;
DuplicateModelBinding: asset=SPOT; first row=1; expected unique asset
```

key 空而 value 非空定位列1；反之列2。错误类型先查 Cell alternative 再转换，不能把 double 自动字符串化。对于 Excel 的 error Cell、嵌套 multi、reference 等不能形成 Cell_ 的输入，普通 ToCellMatrix 会在 parser 前抛泛化错误，因此新入口必须有局部 raw-range preflight，逐物理单元格检查并给同样的 row/column。其后仍调用现有 greedy converter；不复制整套 `_excel.cpp`。

可落实且已生成验证的来源方案：在 constructor 的 `name` markup 后使用 Machinist 支持的 `+` 插入代码，先设置正确 `argName`，再调用 `_WIN32` 本地 helper：

```text
+argName = "settings (input #2)"; Excel::ValidateScriptSettingsRange(xl_settings, "ScriptProductSettings_New", "settings");
&optional
settings is cell[][]+
    Two columns key/value: default_index. Blank selects no default index.
```

valuation 的 binding 范围同样预检。这个 helper 的职责仅是空/维数/不可转换的 raw cell 类型及坐标，不做 native 准备。不要以修改全部 dictionary/calibration 转换行为作为实现捷径。

nullable handle 另用同样 `+` 插入点将单元素 multi 展开成其元素，随后保留标准 `ToHandle<T>(...,true)`：`xl_valuation = Excel::ScriptScalarInput(xl_valuation);`。simulation/fixings 同理。普通非空标量不变；多元素范围仍由标准转换拒绝。省略、`""`、真空单元格、单元素空 multi 都必须 default；0、FALSE、坏 tag、错误对象类型不能 default。这些 helper 仅在新的 wrappers include 前定义，不改全局 ToString 的旧兼容行为。

## 5. 不可变设置与显式空 snapshot

建议新本地 `__script_storable.hpp` 放三个 `Storable...Settings_`。构造形状为 `(const String_& name, const NativeSettings_& value)`；`Storable_("ScriptProductSettings",name)` / `"ScriptValuationSettings"` / `"MonteCarloSettings"`；`val_` 为 **const native 值**，或等价 private const 成员加 const getter。不提供 setter，不把 Cell 矩阵引用或调用者 vector 引用存进去。

从对象读取 native 副本；bindings/date/string 均为值复制，snapshot handle 共享 const pointee。输入表或设置公式改变会创建新对象，已经创建的 product 不受之后产品设置对象改变影响；worksheet 依赖重算可以创建新 product。这两种行为不能混淆。

设置 wrappers 是本地计算对象：沿相邻 wrapper 的无归档注册做法实现 Storable::Write，不增加 reader/schema/pickle/可复用 prepared handle。不可归档 runtime plan、历史 sealed 值、模型槽或 AAD seed；`ScriptProductData` 的既有 v2 合约 archive 保持不变。

| 输入状态 | 本次 valuation 意义 |
| --- | --- |
| null valuation handle | 默认值；Value/Explain 入口捕获 global D，按需抓 global 历史 |
| 非 null valuation，由空 settings/bindings、null fixings 构造 | 相同默认行为；**构造时不固定 D 或历史** |
| 非 null valuation，显式 D、null fixings | 显式 D；本次 global 历史 |
| 非 null valuation，非 null snapshot，含值或空 | ExplicitSnapshot；只用这个对象，不补全局 |
| 非 null snapshot wrapper 的 native `val_` 却为 null | 非法包装对象，明确拒绝，不将其偷偷当默认全局 |

### 5.1 使“显式空”在真实工作簿可表达

当前 typed `MarketFixingSnapshot_New({}, {}, {}, &out)` 已创建非 null 空对象，但 generated 三个 mandatory vector converter 先拒绝空，**当前 worksheet 没有相同调用能力**。

决定：在 `__curveprotocol.cpp` 的原 `MarketFixingSnapshot_New` markup 中把三个输入放到 `&optional`，更新帮助并再生成。保持 C 导出、worksheet 名、`QQQQ`、输入次序及三条并行数组；注册 argNames 变为 `[indexNames],[fixingTimes],[values]`。现有非空公式不受影响，新增：

```excel
=MARKETFIXINGSNAPSHOT.NEW(,,)
```

它返回**非空 tag**，不是空句柄；随后传给 valuation 就是显式空市场。三个空范围/空字符串参数同义。仅部分数组非空仍经长度检查失败；snapshot 现有首空停止的 vector 规则保持，样例不得在 snapshot 数据列中插空行。只在设置/binding 表允许跳过空行。

帮助修正为 finite observation values，FX positive、普通 EQ 可0/负；不要保留把全部值称为 positive 的旧误导。snapshot 构造本体、原时间 fraction、重复观测验证保持。此项是 F8 必需的注册来源修改，S2 写域应明确包含该 markup 与对应两个生成文件，不需要新增公共 core/Python API。

## 6. 固定 FIX 与准备语义

- 脚本写 `FIX(EQ[AAPL])` 或 `FIX(EQ[AAPL], 2026-09-11)`，index 无引号；不新增 FIX()、具参 SPOT 或日期表达式。
- D=估值日，F=fixing 日，E=事件日：F<D 历史；F=D 由 Model/RequireHistorical 控制；F>D 模型；F>E 总是失败。daily key 精确午夜，不用历史是否存在推断来源。
- default_index 仅给旧 SPOT 合约身份，binding 仅显式声明 model spot 的身份；两者互不推断。纯旧未来 SPOT 无绑定兼容；历史无绑定 SPOT 失败；FIX/未绑定 SPOT 混用失败。
- 未来首版仅 BS/Dupire 的一个普通 EQ，显式 `spot -> EQ[...]`。未来 FX/IR/composite/delivery/多资产拒绝；多个历史 EQ/FX 不代表未来多资产支持。
- 非全过期先检查全部语法/身份/模型能力，历史去重在 worker 前全部读完。过去条件硬回放，未来继续 exact/fuzzy；历史数据为被动 double，但 SCALE 等参数依赖由每个 AAD recording 重建。
- default global 抓取按序列进行，不承诺联合原子 snapshot；抓取时禁止并发 fixing 写入。每次 Value/Explain 独立准备、无跨次缓存。全部过期有效产品的既有零工作规则保持。

## 7. Describe/Explain 与长文本输出

### 7.1 语义与 schema

`PRODUCT.DESCRIBE(product)` 直接调用 `DescribeScriptProduct`。schema 为 `dal.script-product/2`，保留 name/default_index/input_rows/variables/constants/payoff_index/events、AST Fix/Spot、原名/规范名、fixing_date_mode/literal/date/time 与 source/origins。它不捕获 D，不读历史，不建 model，不启动 worker；无 phase/evaluation_date。空/definitions-only/no-PAYS 可描述，不代表可定价。

`SCRIPTVALUATION.EXPLAIN(product,modelData,[valuation])` 直接调用 `ExplainScriptValuation`。每次 prepare，允许 CreateModel/Allocate/Init、最终历史读取和 hard replay；**无 GeneratePath、worker、active AAD recording 或缓存**。固定 default double/tree、sobol、BB/AAD false、smooth .01、compiled false，不添加 simulation/path 参数。

schema 为 `dal.script-valuation/1`；沿 `diagnostics.cpp` 保留：

- evaluation_date、today_fixing、source_kind、simulation、all_expired、observation_mode、model_bindings；
- requests 的原序 request_id、index_canonical、fixing_time、source、resolution、uses；每 use 的 event/statement/node/source/original/date mode/literal/type；
- 历史 `history_value_id/value` 与模型 `model_slot {sample_id,output_id}`，未适用字段为 null；live 缺历史是错误，不输出成功 null；
- sample_dates/timeline、live_events/event_to_sample、sample_definitions（含 index_names、discount/forward maturities、libor definitions）、numeraire_requests。

不得用 request_id 假装 history_value_id；不得把 all-event id 直接当 future-event 下标；历史值来自最终 index 虚函数，付款 numeraire 使用付款 sample。这些字段和语义由既有 core schema 控制，Excel 不重新生成一套诊断 schema。

### 7.2 工作表运输形式

两个函数均返回**一列字符串、无标题、无序号列**。每次先获得一个完整 public JSON，再将其中非 ASCII Unicode 字符转为标准 JSON `\uXXXX`（非 BMP 用 surrogate pair），保留 ASCII 和原有转义；不得逐个 UTF-8 byte 伪造 Unicode。然后按最多 **30,000 ASCII 字符**切成连续块；拼接时**不插任何分隔符或换行**。块可以跨 JSON token/转义序列边界，每块不要求单独是 JSON。

这是 JSON 运输形式的等价转义，不改变 schema、字段值或 numeric 文本，不从 human debug label 反解析。可以用经过测试的通用 JSON 字符串转义工具；不能为了实现运输重新定义 core schema。非法 UTF-8 必须明确报诊断编码错误，不静默丢字节。这个输出方案不宣称修复了现有 Excel 输入端所有 Unicode 转换限制。

Excel 单元格上限为 32,767 字符；使用 30,000 的块为此留余量。[Microsoft Excel limits](https://support.microsoft.com/en-us/excel/excel-specifications-and-limits)
输出行数超过可表示工作表行数则报 `DiagnosticOutputTooLarge`、函数及实际/允许行数，不截断。产品描述通常只有一行；大 JSON 消费者读整个 spill 的 Value2，在宿主内拼接后 JSON parse。不要用受单元格长度限制的 TEXTJOIN 重组大输出，也不要依赖单元格视觉换行判断完整性。

Windows 必验每块实际长度、末尾 sentinel、引号/反斜线/换行、非 ASCII/BMP/非 BMP、>32,767 和 >65,535 字符诊断；拼接 parse 后与同输入 public JSON 深度相等，并额外检查核心字段和独立 oracle。不能仅验证生成字符串里出现 schema 字样。原型仅验证运输算式，真正 wrapper/Excel 还须实现并测试。

## 8. 最小可执行工作簿布局

交给 S2 生成实际 `.xlsx` 与 Windows runner；本 S1 没有生成可运行的新 XLL 或宣称公式已执行。下表是完整 ASCII 公式与单元格合同，可直接据此生成 fixture。建议文件 `dal-excel/examples/010.script_fix_settings.xlsx`，runner `dal-excel/tests/windows/run-script-fix-settings.ps1`；文件名是实施建议，交付时记录实际路径。

工作簿使用 **1900 date system (`Date1904=false`)**。以下公式为 invariant 英文名/逗号，COM 写 Formula2，不写本地语言 FormulaLocal。事件文本的 `2.0` 必须写为 string；不能以 numeric cell 代替 `events is string[]`。所有公式下方留足 spill 空间。

### 8.1 Inputs sheet

| 范围 | 内容 |
| --- | --- |
| B2:B5 | 依次 `=DATE(2026,9,12)`、`=DATE(2026,9,11)`、`=DATE(2026,9,15)`、`=DATE(2026,9,22)`；分别 D/H/F/P，serial 46277/46276/46280/46287 |
| D2:E2 | `default_index` / 文本 `EQ[AAPL]` |
| D5:E6 | `evaluation_date` / `=$B$2`；`today_fixing` / 文本 `Model` |
| G5:H5 | 文本 `spot` / `EQ[AAPL]` |
| D9:E13 | `method`/文本 `sobol`；`use_bb`/`=FALSE()`；`enable_aad`/`=TRUE()`；`smooth`/数值 `.01`；`compiled`/`=TRUE()` |
| G9:I10 | `EQ[AAPL]` / `=$B$3` / 80；`EQ[AAPL]` / `=$B$2` / 80 |
| G13:I13 | `EQ[AAPL]` / `=$B$3+TIME(11,0,0)` / 80 |
| A17:B19 | 文本 `SCALE` / 文本 `2.0`；`=$B$3` / `x = SCALE * FIX(EQ[AAPL])`；`=$B$5` / `pay PAYS x + FIX(EQ[AAPL], 2026-09-15)` |
| A22:B22 | `=$B$5` / `pay PAYS SPOT()` |
| A25:B25 | `=$B$2` / `pay PAYS FIX(EQ[AAPL])` |
| A28:B30 | 文本 `SCALE` / 文本 `2.0`；`=$B$3` / `x = SCALE * FIX(EQ[AAPL])`；`=$B$5` / `pay PAYS x` |
| A33:B33 | `=$B$5` / `pay PAYS FIX(EQ[AAPL], 2026-09-15)` |
| D16:E17 | `evaluation_date` / `=$B$2`；`today_fixing` / `RequireHistorical` |
| D20:E20 | 两个实际空单元格，空设置表正例 |
| D23:E24 | `today_fixing`/`Model`；`TODAY_FIXING`/`RequireHistorical`，重复负例 |
| D27:E27 | `evaluation_date` / `=$B$2+TIME(11,0,0)`，日内估值负例 |
| D30:E30 | `enable_aad` / **文本** `TRUE`，bool 负例 |

### 8.2 Handles sheet

| 单元格 | 公式 |
| --- | --- |
| B2 | `=SCRIPTPRODUCTSETTINGS.NEW("contract",Inputs!D2:E2)` |
| B3 | `=MARKETFIXINGSNAPSHOT.NEW(Inputs!G9:G10,Inputs!H9:H10,Inputs!I9:I10)` |
| B4 | `=SCRIPTVALUATIONSETTINGS.NEW("valuation",Inputs!D5:E6,Inputs!G5:H5,B3)` |
| B5 | `=MONTECARLOSETTINGS.NEW("simulation",Inputs!D9:E13)` |
| B6 | `=BSMODELDATA.NEW("zero-vol",100,0,0,0)` |
| B7 | `=PRODUCT.NEWWITHSETTINGS("mixed",Inputs!A17:A19,Inputs!B17:B19,B2)` |
| B8 | `=PRODUCT.NEW("legacy",Inputs!A22,Inputs!B22)` |
| B9 | `=SCRIPTPRODUCTSETTINGS.NEW("default-contract",Inputs!D20:E20)` |
| B10 | `=MONTECARLOSETTINGS.NEW("default-simulation")` |
| B11 | `=SCRIPTVALUATIONSETTINGS.NEW("default-valuation")` |
| B12 | `=MARKETFIXINGSNAPSHOT.NEW(,,)` |
| B13 | `=SCRIPTVALUATIONSETTINGS.NEW("empty-history",Inputs!D5:E6,Inputs!G5:H5,B12)` |
| B14 | `=MARKETFIXINGSNAPSHOT.NEW(Inputs!G13,Inputs!H13,Inputs!I13)` |
| B15 | `=SCRIPTVALUATIONSETTINGS.NEW("only-11am",Inputs!D5:E6,Inputs!G5:H5,B14)` |
| B16 | `=PRODUCT.NEW("today",Inputs!A25,Inputs!B25)` |
| B17 | `=SCRIPTVALUATIONSETTINGS.NEW("require-today",Inputs!D16:E17,Inputs!G5:H5,B3)` |
| B18 | `=PRODUCT.NEWWITHSETTINGS("history",Inputs!A28:A30,Inputs!B28:B30,B2)` |
| B19 | `=PRODUCT.NEW("retained-future",Inputs!A33,Inputs!B33)` |
| B20 | `=BSMODELDATA.NEW("nonzero-carry",100,0,0.05,0.02)` |

Control sheet：B2 `=INIT.GLOBALDATA(1)`，B3 `=EVALUATIONDATE.SET(Inputs!B2)`。runner 先在 manual calculation 下逐个 Calculate 并检查成功，然后把 B2/B3 结果保存为 Value2，避免后续全表重算再次执行状态 setter。先算 Inputs，再按依赖算 Handles/结果；不依赖 Excel 的无关单元格计算顺序来设置 D。不要重算 Init 以测试 repricing。

### 8.3 结果与必须断言

Values sheet 每个起点占独立区域；当前 BS+SCALE AAD 表为6行×2列，BS-only AAD为5×2，非AAD为1×2。测试仍按实际 spill 枚举、检查两列并用 key 查值，不按 PV 行号读取。

| 起点 | 公式 | 独立预期 |
| --- | --- | --- |
| A2 | `=MONTECARLO.VALUEWITHSETTINGS(Handles!B7,Handles!B6,257,Handles!B4,Handles!B5)` | PV260，d_SCALE80，d_spot1；有四个模型风险及 SCALE，别把此混合例的 d_vol 固定为0 |
| D2 | `=MONTECARLO.VALUE(Handles!B8,Handles!B6,257,"sobol",FALSE,FALSE,0.01)` | 原七输入表，PV100 |
| G2 | `=MONTECARLO.VALUEWITHSETTINGS(Handles!B8,Handles!B6,257,,)` | 两个 null handle 默认，PV100 |
| J2 | `=MONTECARLO.VALUEWITHSETTINGS(Handles!B8,Handles!B6,257,Handles!B11,Handles!B10)` | 非空默认设置对象，PV100 |
| M2 | `=MONTECARLO.VALUEWITHSETTINGS(Handles!B7,Handles!B6,257,Handles!B4,)` | simulation null，PV260且仅1×2 |
| A20 | `=MONTECARLO.VALUEWITHSETTINGS(Handles!B16,Handles!B6,1,Handles!B4,)` | 当天 Model，PV100 |
| D20 | `=MONTECARLO.VALUEWITHSETTINGS(Handles!B16,Handles!B6,1,Handles!B17,)` | 当天 RequireHistorical，PV80 |
| G20 | `=MONTECARLO.VALUEWITHSETTINGS(Handles!B18,Handles!B20,257,Handles!B4,Handles!B5)` | 历史-only 非零率 oracle，见第9节 |
| J20 | `=MONTECARLO.VALUEWITHSETTINGS(Handles!B19,Handles!B20,257,Handles!B4,Handles!B5)` | F 无事件、P付款的 carry oracle，见第9节 |
| A40 | `=MONTECARLO.VALUEWITHSETTINGS(Handles!B7,Handles!B6,1,Handles!B13,)` | MissingFixing，ExplicitSnapshot，H午夜，非0价格 |
| D40 | `=MONTECARLO.VALUEWITHSETTINGS(Handles!B7,Handles!B6,1,Handles!B15,)` | 只有11:00仍 MissingFixing H午夜 |
| G40 | `=SCRIPTVALUATIONSETTINGS.NEW("bad-duplicate",Inputs!D23:E24)` | duplicate key，row2/column1、first row1 |
| J40 | `=SCRIPTVALUATIONSETTINGS.NEW("bad-date",Inputs!D27:E27)` | evaluation_date，row1/column2，integral constraint |
| M40 | `=MONTECARLOSETTINGS.NEW("bad-bool",Inputs!D30:E30)` | enable_aad，row1/column2，boolean/0/1 constraint |

另将 G2 分别换成省略最后参数的三参公式、两个 `""`、两处空单元格引用，以及 null valuation+显式 simulation，验证 nullable 的全部形状。给 Product_NewWithSettings 传 B9 验证空产品设置 handle 合法；传空 handle 则应拒绝。

Diagnostics sheet：A2 `=PRODUCT.DESCRIBE(Handles!B7)`；D2 `=SCRIPTVALUATION.EXPLAIN(Handles!B7,Handles!B6,Handles!B4)`。拼接后分别 parse `/2`、`/1`。mixed 的历史H与模型F各一请求，sample_dates 是 F/P，timeline 是 3/365、10/365，live_events 指向原 dated event 1，event_to_sample 为 `[1]`；付款 numeraire sample=1、模型F sample=0；历史值80，模型 value=null。检验 Describe 保留两条 dated events，不能因 H<D 丢掉 H。

runner 再将 global D 改成 2026-09-23，仅重新 Calculate 显式 valuation 的 A2/诊断：结果仍260、Explain D仍2026-09-12；随后恢复 global D再测默认入口。不要让 legacy 默认在 global D>P 时仍期待100。

工作簿错误是 DAL 的 `#Error:` **文本**，也要拒绝真实 Excel `#NAME?`、`#VALUE!`、`#SPILL!` 和 COM error variants。不能仅靠 Excel ISERROR 判断 DAL 负例，更不能让注册失败产生的 #NAME? 充当 MissingFixing 通过。

## 9. 独立 PV/AAD oracle 与跨语言数据

共同历史表含 `(EQ[AAPL],2026-09-11 00:00,80)`、`(EQ[AAPL],2026-09-12 00:00,80)`；三语言分别通过真实 snapshot constructor 建立相同内容（跨进程不能是同一个内存对象）。使用同一事件原文、D、绑定、模型参数、RNG/路径和 MC 设置；保存标准输入 manifest 与哈希。Python 调用现有 F7 新入口，C++ 调 typed Value，Excel 走真实生成 wrapper；检查 native/模块/XLL 的精确构建身份。

跨语言相等只作补充，以下预期独立来自算式，不能调用 DAL 自己计算 expected：

- 零率混合：历史 `2*80` + F日确定性 spot100 = **260**，d_SCALE=80，d_spot=1。
- 当天：Model=100、RequireHistorical=80；历史查询由 native 计数接缝分别证明0/1，不从价格相等猜计数。
- 纯历史付款（Handles B18/B20）：`T=10/365`，`PV=160*exp(-.05*T)=159.78097197125695`；`d_SCALE=79.89048598562847`；`d_rate=-T*PV=-4.377560875924848`；d_spot=d_vol=d_div=0，无 fixing 风险键。
- retained future（B19/B20）：`tF=3/365`、`tP=10/365`，`PV=100*exp((.05-.02)*tF-.05*tP)=99.88773429802069`；d_spot=PV/100，d_rate=(tF-tP)*PV，d_div=-tF*PV。错误地使用付款日 spot 会得到99.94522048890788，因此此例能识别两个语言共同用错 sample 的问题。
- 零波动使 primal 和上述 spot/rate/div/SCALE 导数确定，但有限路径的 pathwise d_vol 不必恰为0；mixed/future 不作虚假的零 vega 断言。历史-only 的 vol 风险才独立为0。

PV 容差 `1e-12*max(1,abs(expected))`；解析风险绝对 `1e-10`；随机 fixed-path 均值比较 `1e-8`。FD 在光滑点同路径/epsilon做中心差分；hard切换点不作可微验收。除257外，portable 覆盖1/8193及多线程/多批，tree/compiled、double/AAD，BS与Dupire合法对照；保留原 fixed-scenario F120/P999 的 native 测试作补充。

## 10. 测试落点与三类证据

现有 Excel `test_value.cpp` 主要是路径转换，`test_excel_api.cpp` 有 snapshot并行输入及typed binding测试，`test_registration.cpp` 的完整 registration/export检查在 Windows。新测试建议 `test_script_settings.cpp` 与 `__script_test_api.hpp`；沿 `__excel_test_api.hpp` / `__curvepricing_test_api.hpp` 的 test-only 导出模式，共用实际内部 wrapper，不复制一份测试实现。

| T | F8 的直接证据 |
| --- | --- |
| T07 TodayPolicy | workbook A20/D20及缺当天历史负例；portable 调 Excel typed Value/Explain，设置 global D与显式D不同，native Fixing/History counters有正向对照；BS/Dupire、tree/compiled、double/AAD。 |
| T08 ExactTimestamp | workbook B14/D40；真实 snapshot时间保留11:00，午夜Find缺失，准备报MissingFixing；合法午夜对照。非法显式日期表达式/带时间脚本文法拒绝；evaluation_date小数失败。 |
| T27 Legacy | 保留旧两个注册的名称、argTypes、argNames、输入数、帮助和导出；旧未来SPOT原fixtures及独立解析值；新旧两列表；历史无绑定SPOT、混合无default、具参SPOT/FIX()失败；有default时同identity/time共享request。 |
| T28 Settings | 三constructor全部字段/默认/类型/拒绝矩阵；unknown/duplicate大小写键、重复asset、半空、首部/中间/尾部整空行后仍读取、3列包括全空、headers误入；bool真假/0/1及错误文本；Date上下界/小数/非有限，smooth坏值，paths边界，错类/坏tag/null handles；复制不别名。 |
| T28 Snapshot | portable在global确有H80时传非null空snapshot必须MissingFixing；null snapshot成功取global，ExplicitSnapshot无globalHistory。worksheet `MARKETFIXINGSNAPSHOT.NEW(,,)`返回实际非空tag并触发ExplicitSnapshot缺值。不得用另一个进程的global store来冒充Excel进程市场。 |
| T29 Diagnostics/archive | Describe在会触发的history/model/worker故障接缝下为0，改D/H不变；Explain每次prepare、finalFixing按唯一请求、每序列History至多一次、无路径/worker，无缓存；JSON chunk重组与真实schema/slots/uses对照。受影响public v1 golden/v2原文及无runtime归档回归继续执行。 |
| T30 ExcelContract | portable执行真实typed binding；Windows执行raw OPER到wrapper的边界（尤其blank/error/shape），注册 metadata/导出地址；真正Excel载入本次XLL、执行完整fixture，记录所有成功/错误单元格及长JSON。 |

已有可复用 public 测试：`test_script_diagnostics.cpp` 的 `TestTodayPolicy`、`TestRepricingAndExplainNeverCache`、`TestDefaultSpotDeduplicatesAndLegacyGuards`、`TestExplainRetainedFixingAndPaymentSamples`；`test_script_contract.cpp` 的历史风险/归档与 interleaved model/inverse-FX request IDs；`test_script_archive.cpp` 的 v1/v2。这些是**读到的落点，不是本次运行结果**。

### 10.1 Linux portable

配置 `DAL_BUILD_EXCEL_PORTABLE_TESTS=ON`、`DAL_EXCEL_BUILD_TESTS=ON`、GTest可用；执行 `dal_excel_portable_tests`。该目标排除 `_excel.cpp` / `_xlcall.cpp` 且 `_WIN32` 生成入口未编译，因此不能证明 raw Excel 类型转换或注册。使用 native fixing/simulation observers 与 actual typed wrappers 证明调用语义；public/core/Python受影响回归、consumer、sanitizer按父任务要求单独记结果。

### 10.2 Windows 构建与注册

先确认 Excel.exe PE位数与编译目标一致；Windows源树/依赖身份必须对应本次实现 head，不使用旧安装XLL或旧wheel。Visual Studio developer shell 中按仓库 `Release-windows`（Ninja、静态 MSVC runtime）构建：

```powershell
cmake --preset Release-windows
cmake --build build/Release-windows --target dal_excel dal_excel_tests --parallel
ctest --test-dir build/Release-windows --output-on-failure
```

使用仓库现有 Office DLL路径配置；记录实际 `OFFICE_MSO_DLL`、`OFFICE_VBE_OLB`、`OFFICE_EXCEL_EXE`，cl/CMake/Ninja版本、x64/x86、AAD backend、运行库、build SHA/tree，XLL/依赖DLL全路径及SHA256。Windows `RegisteredFunctionsForTest` 验 exact新旧名/输入/help≤255、nullable标记、无重复、`GetProcAddress` 全导出。原型只生成文本不能替代该步骤。

### 10.3 真实 Excel 工作簿

S2 runner 必须在独立 Excel.Application 实例执行，记录其 PID/Hwnd、Application.Version/Build、Office位数、实际加载XLL路径及文件hash。调用 `Application.RegisterXLL(absoluteXllPath)` 检查 true，再写/打开本次fixture、按第8节初始化和计算；不能把注册true当全部公式执行成功。[Microsoft RegisterXLL](https://learn.microsoft.com/en-us/office/vba/api/excel.application.registerxll)

采用 Formula2 获得动态数组spill；Formula2的数组计算行为有官方说明。[Microsoft Formula2](https://learn.microsoft.com/en-us/office/vba/api/excel.range.formula2)
读 actual spill 的 Value2并保存JSON/CSV；检查每个handle、完整N×2/N×1形状、key集合、oracle、指定负例标识、JSON末尾与重组。fixture保存 `.xlsx` 并提供执行脚本、raw结果、验证日志与截图（截图是补充，不替代可解析值）。以实际支持的host为准，不假设所有Office16都提供相同Formula2功能。

runner 在 finally 关闭自建workbook、Quit自建Excel并释放COM对象；不终止用户已有Excel进程。所有运行在前台收齐结果。失败时保存具体阶段/异常/已生成fixture，不减少验收。**Linux portable、Windows编译/导出、真实Excel公式三类证据分别报告；前两者都不能替代第三者。** 性能仅advisory，不新增性能gate。

## 11. 已执行的 S1 核对与限制

- 当前头文件 C++17 `-fsyntax-only` consumer exit0：三 native type关系、todayFixingPolicy_、const snapshot、既有storable、旧Value/typedValue/product/diagnostics声明均通过；没有链接或运行DAL定价。
- 按仓库 gitlink `e76b0ef243ca9bd506bb2f3743d8c9b52ee8d01d` 在独立 workdir 取得/构建 Machinist，使用真实 `dal.ifc`/`dal.mgl` 和 `Public.mgt`，对8个原型markup生成16个inc/htm。自动核对8个exact name/args/Q数量/help长度、greedy/nullable、源插入点，全部通过。新局部helper在本轮只有声明设计，没有定义或XLL链接证明。
- 初次原型没预建auto目录，工具声称write但文件不存在；建目录后重新生成并以真实文件存在及内容验证。未把第一次exit0当成功证据。没有运行仓库dal_generate，产品生成树未改。
- Python3独立日期/零波动算式及ASCII JSON chunk原型通过；标准日期serial、历史/未来PV/AAD算式见evidence/probe-contract.json。此脚本不import DAL，不实现设置parser，不运行Excel。
- fresh Windows可用性探针（2026-09-15T10:08:27Z）：WSL2 `5.15.167.4-microsoft-standard-WSL2`；Windows `10.0.26200.0`，PowerShell `5.1.26100.9444`（该PS进程64bit）；Excel COM注册true、`C:\Program Files\Microsoft Office\Root\Office16\EXCEL.EXE` 存在、file version `16.0.20326.20144`；vswhere找到 VS Community2022 `17.14.37411.7` 且具C++工具组件；CMake和Ninja在PATH可解析。第一次VS列表投影产生null字段，修正PS数组枚举后得到上述实值，未据首次输出声称缺VS。
- Linux实跑工具版本：GCC15.2.0、CMake4.2.3、Git2.53.0、Python3.13.9、multica0.4.43（2ae2dbbb8）。Windows CMake/Ninja只查到路径，未执行其版本命令；VS安装版本不是cl编译版本，PS位数不是Excel位数。
- 未构建产品core/public/Python/XLL、未启动Excel、未执行工作簿、产品tests、sanitizer、installed consumer或CI；F7旧结果未计成本轮测试。Windows可用性不是验收通过。没有重绑任何agent/runtime。

命令、原型、实际stdout/exit code及读源输入SHA256见随附evidence。探索中的无效checkout `--output`、不存在路径查找和一次日志编排变量错误没有改变产品；最终成功调用另有明确日志。不用这些失败代替功能测试，也不隐去关键限制。

## 12. 取舍与后继事项

采用：七个新函数、三 immutable handles、两列原始矩阵、现有nullable生成规则加局部raw边界、snapshot三并行optional、原core JSON的等价ASCII分块。典型定价只需product/model/path和至多两个设置handle，不再扩旧七输入。

拒绝：把所有字段塞进旧Value；JSON/分隔符设置字符串；dictionary静默覆盖；按首空行裁掉设置；从model名或唯一FIX推断身份；null/显式空snapshot合并；临时改global D实现显式D；手改生成inc；Describe借用prepare；Explain缓存Value；单格长JSON截断；用parity代替独立oracle；以portable或XLL编译冒充真实工作簿通过。

S2需落实的新写入包括 `__script.cpp` / `__value.cpp`、必要本地storable/转换/test helpers、`__curveprotocol.cpp` 的snapshot **markup/help**、相应生成物、直接测试与fixture/Windows runner。无需修改core/public/Python合同或Machinist子模块。若实现发现真正core/public缺陷，先给父最小复现路由，不在Excel wrapper伪造价格。

没有待用户决定的API阻断。尚待的是新helper/包装的真实编译与运行、矩阵/错误/数字oracle测试、Windows host位数/Office配置/XLL及实际工作簿、文档/CHANGELOG决定和独立review。父验收关闭DAL-243后再启动S2；本note只完成S1，不代表F8已实现、已验收、可合并或可以启动F9。
